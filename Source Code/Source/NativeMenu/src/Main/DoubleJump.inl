                                                                                       
std::atomic_bool doubleJumpOn{false},doubleJumpReady{false};
std::atomic<unsigned> doubleJumpCount{0},doubleJumpLandings{0};
std::atomic<uintptr_t> doubleJumpController{0};
std::atomic_bool doubleJumpWasAir{false};
using MoveInputFn=void(*)(void*,int,float,char);
using PhysicalJumpFn=unsigned char(*)(void*,const V*,float,unsigned char,unsigned char);
MoveInputFn nativeMoveInput=nullptr;
uintptr_t physicalEngine=0;
bool DoubleJumpAir(void* who,bool& air){
 uintptr_t p=0;if(!LocalMove(who)||!Read((uintptr_t)who+0x58,p))return false;
 air=((unsigned char(*)(void*))(gameBase+0x66F460))((void*)p)!=0;return true;
}
void DoubleJumpFrame(void* who){
 if(!doubleJumpOn.load()||!doubleJumpReady.load())return;
 bool air=false;if(!DoubleJumpAir(who,air))return;
 if(doubleJumpController.exchange((uintptr_t)who)!=(uintptr_t)who){doubleJumpWasAir=air;return;}
 if(air)doubleJumpWasAir=true;else if(doubleJumpWasAir.exchange(false))++doubleJumpLandings;
}
bool PreparePhysicalAirJump(){
 auto engine=GetModuleHandleW(L"engine_x64_rwdi.dll");if(!engine)return false;
 wchar_t path[32768]{};if(!GetModuleFileNameW(engine,path,32768))return false;
 std::ifstream file(std::filesystem::path(path),std::ios::binary);if(!file)return false;
 HCRYPTPROV provider=0;HCRYPTHASH hash=0;
 if(!CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT))return false;
 bool ok=CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)!=FALSE;char buffer[65536];
 while(ok&&file){file.read(buffer,sizeof(buffer));auto n=file.gcount();if(n>0)ok=CryptHashData(hash,(BYTE*)buffer,(DWORD)n,0)!=FALSE;}
 BYTE digest[32]{};DWORD size=32;
 const BYTE expected[32]={0x7c,0xf1,0xf0,0x15,0x3a,0x27,0x48,0xda,0x55,0xa3,0x6d,0x89,0xfc,0xf2,0xe4,0x51,0xba,0x44,0x44,0x68,0x26,0x03,0x55,0x83,0x67,0x22,0xd3,0xef,0x76,0xb2,0x7c,0x79};
 ok=ok&&file.eof()&&CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0)&&size==32&&!memcmp(digest,expected,32);
 if(hash)CryptDestroyHash(hash);CryptReleaseContext(provider,0);
 if(ok)physicalEngine=(uintptr_t)engine;Log(ok?"PHYSICAL_AIR_JUMP_API_VERIFIED":"PHYSICAL_AIR_JUMP_BUILD_REJECTED");return ok;
}
bool AirParameter(uintptr_t p,unsigned index,float& value){
 unsigned char shared=0;uintptr_t stats=0,holder=0,table=0,cell=0;
 return Read(p+0x50,shared)&&Read(shared?gameBase+0x1281B88:p+0xA18,stats)&&
  Read(stats+0xB0,holder)&&Read(holder,table)&&Read(table+index*8,cell)&&Read(cell+8,value)&&std::isfinite(value);
}
void PhysicalAirCommand(void* who){
 uintptr_t p=0,control=0,self=0,link=0,component=0,vt=0,body=0,fn=0,vehicle=0,state=0;
 unsigned char input=0,busy=0;int locks=-1,mode=0,type=0;
 if(!LocalMove(who)||!Read((uintptr_t)who+0x58,p)||!Read((uintptr_t)who+0x28,input)||!input||
  !Read(p+0xEC8,vehicle)||vehicle||!Read(p+0x700,locks)||locks||!Read(p+0x1EB8,busy)||busy||
  !Read(p+0x920,state)||!state||!Read(state+0x80,mode)||(mode>=2&&mode<=6))return;
 if(!Read(p+0x20,control)||!control||!Read(control+0x250,self)||self!=control||
  !Read(control+0x258,link)||!link||!Read(link+0x28,component)||!component||
  !Read(component,vt)||vt!=physicalEngine+0x882BD8||!Read(component+0x38,type)||type!=3)return;
 uintptr_t physics=component+0x6F0;
 if(!Read(physics,vt)||vt!=physicalEngine+0x882D68||!Read(vt+0x320,fn)||fn!=physicalEngine+0x3E0040||
  !Read(physics+0x10,body)||!body)return;
 V before{},after{};float height=0,cost=0,costScale=0,stamina=0,scale=jumpScale.load();
 if(!Read(body+0xD8,before)||!Finite(before)||!AirParameter(p,0xD6,height)||height<=0||height>20||
  !AirParameter(p,0xD8,cost)||cost<0||cost>10||!AirParameter(p,0x1D5,costScale)||costScale<0||costScale>100||
  !Read(p+0xDBC,stamina)||!std::isfinite(stamina)||(!staminaOn.load()&&stamina<=0)||
  !std::isfinite(scale)||scale<1||scale>20)return;
 static uintptr_t previousBody=0;static ULONGLONG previousTime=0;auto now=GetTickCount64();
 if(previousBody==body&&now-previousTime<80)return;previousBody=body;previousTime=now;
                                                                         
                                                                   
 V direction{};auto result=((PhysicalJumpFn)fn)((void*)physics,&direction,height*scale,0,0);
 bool measured=Read(body+0xD8,after)&&Finite(after)&&after.y>0;
 if(result&&measured){++doubleJumpCount;((StaminaConsumeFn)(gameBase+0x65BCE0))((void*)p,cost*costScale,0);}
 static unsigned records=0;if(records++<80){
  std::ofstream f(LogPath(),std::ios::app);f<<now<<",PHYSICAL_AIR_JUMP,result="<<(unsigned)result<<",measured="<<measured<<",vy_before="<<before.y<<",vy_after="<<after.y<<",height="<<height*scale<<",body="<<(void*)body<<'\n';
 }
}
void ModifiedMoveInput(void* who,int action,float amount,char consumed){
 bool air=false;
 if(doubleJumpOn.load()&&doubleJumpReady.load()&&action==0xB&&!consumed&&std::isfinite(amount)&&amount!=0&&DoubleJumpAir(who,air)&&air){
  PhysicalAirCommand(who);PumpKickLaunchInput(who);return;
 }
 nativeMoveInput(who,action,amount,consumed);if(!consumed)PumpKickLaunchInput(who);
}
void SetDoubleJump(bool on){doubleJumpOn=on&&doubleJumpReady.load();Log(doubleJumpOn?"PHYSICAL_AIR_JUMPS_ENABLED":"PHYSICAL_AIR_JUMPS_DISABLED");}
