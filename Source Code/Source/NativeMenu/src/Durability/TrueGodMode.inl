                                                                            
                                                                                  
#ifndef TRUE_GOD_VALIDATION_MS
#define TRUE_GOD_VALIDATION_MS 0
#endif
using TrueGodDamageFn = void(*)(void*, void*);
TrueGodDamageFn trueGodOriginal = nullptr;
bool trueGodHookReady = false;
std::atomic<unsigned long long> trueGodBlocked{0}, trueGodOther{0}, trueGodHealing{0}, trueGodInvalid{0};
std::atomic<unsigned long long> trueGodDeadline{0};
std::atomic<uintptr_t> trueGodLastVictim{0}, trueGodLastPlayer{0};

bool TrueGodBuildMatches() {
 wchar_t path[32768]{};
 if (!GetModuleFileNameW((HMODULE)gameBase, path, 32768)) return false;
 std::ifstream file(std::filesystem::path(path), std::ios::binary);
 if (!file) return false;
 HCRYPTPROV provider=0; HCRYPTHASH hash=0;
 if (!CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)) return false;
 bool ok=CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)!=FALSE;
 char buffer[65536];
 while (ok && file) {
  file.read(buffer,sizeof(buffer));
  auto n=file.gcount();
  if(n>0) ok=CryptHashData(hash,(BYTE*)buffer,(DWORD)n,0)!=FALSE;
 }
 BYTE digest[32]{}; DWORD size=32;
 const BYTE expected[32]={0xD1,0x0C,0x7F,0x59,0xAD,0x3B,0xF6,0x2E,0x7F,0x2D,0xD6,0x9C,0xD7,0xE4,0x9E,0xB6,0x34,0xD4,0xE8,0xD0,0x46,0xE4,0xF4,0xC0,0x74,0x63,0xB5,0xD5,0x89,0x83,0x35,0x45};
 ok=ok && file.eof() && CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0) && size==32 && !memcmp(digest,expected,32);
 if(hash) CryptDestroyHash(hash);
 CryptReleaseContext(provider,0);
 return ok;
}

bool TrueGodPlayer(uintptr_t& owner) {
 uintptr_t hud=0,view=0,vt=0,primary=0,control=0;
                                                               
                                                                            
 if(!Pointer(gameBase+0x12822F8,hud) || !Pointer(hud+0x628,view) || !Read(view,vt)) return false;
                                                                             
 if(vt==gameBase+0xEBB6D8) owner=view;
 else if(vt==gameBase+0xEBBE08) owner=view-0x18;
 else if(vt==gameBase+0xEBC168) owner=view-0x28;
 else return false;
 return Read(owner,primary) && primary==gameBase+0xEBB6D8 &&
        Read(owner+0x18,control) && control==gameBase+0xEBBE08;
}

void TrueGodDamageHook(void* victim,void* info) {
 if(godModeEnabled.load(std::memory_order_acquire)) {
  uintptr_t player=0; float amount=0;
  const uintptr_t address=(uintptr_t)victim;
  trueGodLastVictim=address;
  if(GetTickCount64()>=trueGodDeadline.load() || !TrueGodPlayer(player)) {
   ++trueGodInvalid; godModeEnabled=false;
  } else {
   trueGodLastPlayer=player;
   if(address!=player+0x18) ++trueGodOther;
   else if(!info || !Read((uintptr_t)info+0x18,amount) || !std::isfinite(amount)) {
    ++trueGodInvalid; godModeEnabled=false;
   } else if(amount>=0.0f) {
                                                                                    
    ++trueGodBlocked;
    return;
   } else ++trueGodHealing;                                                
  }
 }
 trueGodOriginal(victim,info);
}

void TrueGodReport() {
 std::ofstream f(LogPath(),std::ios::app);
 f<<GetTickCount64()<<",TRUE_GOD_DIAG,enabled="<<godModeEnabled.load()
  <<",blocked="<<trueGodBlocked.load()<<",other="<<trueGodOther.load()
  <<",healing="<<trueGodHealing.load()<<",invalid="<<trueGodInvalid.load()
  <<",victim=0x"<<std::hex<<trueGodLastVictim.load()
  <<",player=0x"<<trueGodLastPlayer.load()<<std::dec<<'\n';
}

bool SetGodModeNative(bool on) {
 if(!on) { godModeEnabled=false; TrueGodReport(); SetStatus("TRUE GOD MODE DISABLED"); return true; }
 uintptr_t player=0;
 if(!TrueGodPlayer(player)) { SetStatus("TRUE GOD REFUSED - PLAYER IDENTITY"); return false; }
 static const BYTE healthOriginal[9]={0xF3,0x0F,0x11,0x8A,0x2C,0x0D,0,0,0xC3};
 BYTE health[9]{};
 if(!ReadBytes(gameBase+GodModeRva,health,9) || memcmp(health,healthOriginal,9)) {
  SetStatus("TRUE GOD REFUSED - RESTORE OLD GOD PATCH"); return false;
 }
 if(!trueGodHookReady) {
  const BYTE entry[16]={0x40,0x55,0x56,0x41,0x54,0x41,0x55,0x48,0x8D,0xAC,0x24,0x68,0xFF,0xFF,0xFF,0x48};
  BYTE actual[16]{}; void* target=(void*)(gameBase+0x667950);
  if(!TrueGodBuildMatches() || !ReadBytes((uintptr_t)target,actual,16) || memcmp(actual,entry,16)) {
   SetStatus("TRUE GOD REFUSED - BUILD OR HOOK CONFLICT"); return false;
  }
  auto init=MH_Initialize();
  if((init!=MH_OK && init!=MH_ERROR_ALREADY_INITIALIZED) ||
     MH_CreateHook(target,(void*)&TrueGodDamageHook,(void**)&trueGodOriginal)!=MH_OK) {
   SetStatus("TRUE GOD HOOK CREATE FAILED"); return false;
  }
  if(MH_EnableHook(target)!=MH_OK) {
   MH_RemoveHook(target); trueGodOriginal=nullptr;
   SetStatus("TRUE GOD HOOK ENABLE FAILED"); return false;
  }
  trueGodHookReady=true;
 }
 trueGodBlocked=0; trueGodOther=0; trueGodHealing=0; trueGodInvalid=0;
 trueGodLastPlayer=player; trueGodLastVictim=0;
 trueGodDeadline=TRUE_GOD_VALIDATION_MS ? GetTickCount64()+TRUE_GOD_VALIDATION_MS : ~0ULL;
 godModeEnabled.store(true,std::memory_order_release);
 TrueGodReport(); SetStatus(TRUE_GOD_VALIDATION_MS ? "TRUE GOD ON - TIMED VALIDATION" : "TRUE GOD MODE ENABLED"); return true;
}

void TrueGodTick() {
 static unsigned long long next=0;
 if(!trueGodHookReady) return;
 auto now=GetTickCount64();
 if(godModeEnabled && now>=trueGodDeadline) SetGodModeNative(false);
 if(now>=next) {
  TrueGodReport(); next=now+1000;
  if(trueGodInvalid.load()) SetStatus("TRUE GOD STOPPED - IDENTITY OR RECORD INVALID");
 }
}
