                                                                                       
namespace Spinbot { void SafeUpdate(uintptr_t m); }
namespace CameraLockOn { void SafeUpdate(uintptr_t m); }
namespace CameraFeatures {
std::atomic_bool ready{false},boundsReady{false},third{false},bounds{false},customFov{false},active{false};
std::atomic_int firstFov{75},thirdFov{75},distanceCm{300},fault{0};
std::atomic<unsigned long long> ticks{0},fovWrites{0};std::atomic<float> observedFov{0.f};
std::atomic_flag busy=ATOMIC_FLAG_INIT;
using Update=void(*)(uintptr_t);Update originalUpdate=nullptr;
using SetMode=void(*)(uintptr_t,int,uintptr_t);
using SetFov=void(*)(uintptr_t,float);using GetFov=float(*)(uintptr_t);
SetFov setFov=nullptr;GetFov getFov=nullptr;
uintptr_t heldManager=0,heldPlayer=0,heldVis=0,heldTpp=0,heldFpp=0,fovCamera=0,fovOwner=0;
float nativeDistance=3.f,nativeFov=75.f;bool ownMode=false,ownFov=false;
constexpr unsigned originalTable[8]={0x658407,0x6584a7,0x658614,0x658407,0x658552,0x658614,0x658441,0x65849a};
constexpr unsigned bypassTable[8]={0x658614,0x658614,0x658614,0x658407,0x658552,0x658614,0x658441,0x65849a};
uintptr_t Q(uintptr_t p){uintptr_t v=0;Read(p,v);return v;}
int I(uintptr_t p){int v=-1;Read(p,v);return v;}
bool Vt(uintptr_t p,uintptr_t rva){return p>0x10000&&Q(p)==gameBase+rva;}
uintptr_t FindCamera(uintptr_t m,int key){uintptr_t n=Q(Q(m+0x50));for(int i=0;n&&i<8;i++){int id=I(n-16);if(id<1||id>5)return 0;if(id==key)return Q(n-8);n=Q(n+(id<key?16:0));}return 0;}
bool Resolve(uintptr_t m,uintptr_t&p,uintptr_t&t,uintptr_t&f,uintptr_t&v){
 if(!Vt(m,0xceb628)||Q(Q(Q(gameBase+0x12822a8)+0x4a0)+0xc8)!=m)return false;
 p=Q(Q(gameBase+0x12822f8)+0x628);if(Q(p)==gameBase+0xebbe08)p-=0x18;else if(Q(p)==gameBase+0xebc168)p-=0x28;
 if(!Vt(p,0xebb6d8)||!Vt(p+0x18,0xebbe08))return false;
 v=Q(p+0x268);if(!Vt(v,0xebedb8)||Q(v+0x60)!=p||!Vt(v+0xec0,0xebf9d8))return false;
 t=FindCamera(m,1);f=FindCamera(m,2);return Vt(t,0xcedd78)&&Vt(f,0xcead58);
}
bool HashModule(uintptr_t mod,const char*expected){
 wchar_t path[32768];if(!GetModuleFileNameW((HMODULE)mod,path,32768))return false;
 HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
 HCRYPTPROV provider=0;HCRYPTHASH hash=0;bool ok=CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&CryptCreateHash(provider,CALG_SHA_256,0,0,&hash);
 BYTE buf[65536],digest[32];DWORD n=0,size=32;while(ok){if(!ReadFile(file,buf,sizeof(buf),&n,nullptr)){ok=false;break;}if(!n)break;ok=CryptHashData(hash,buf,n,0)!=0;}
 ok=ok&&CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0)&&size==32;char actual[65]{};if(ok)for(int i=0;i<32;i++)sprintf_s(actual+i*2,3,"%02x",digest[i]);
 if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);CloseHandle(file);return ok&&!strcmp(actual,expected);
}
bool SetBounds(bool on){
 if(!boundsReady)return false;void*address=(void*)(gameBase+0x658670);const auto expected=bounds.load()?bypassTable:originalTable;const auto target=on?bypassTable:originalTable;
 if(memcmp(address,expected,sizeof(originalTable))){fault=3;return false;}DWORD old=0,ignored=0;
 if(!VirtualProtect(address,8,PAGE_EXECUTE_READWRITE,&old))return false;
                                                                                    
 LONG64 value=0;memcpy(&value,target,8);InterlockedExchange64((volatile LONG64*)address,value);
 bool ok=VirtualProtect(address,8,old,&ignored)!=FALSE;FlushInstructionCache(GetCurrentProcess(),address,8);
 bounds=on;Log(on?"PLAYABLE_AREA_BYPASS_ON":"PLAYABLE_AREA_BYPASS_OFF");if(!ok)fault=3;return ok;
}
void RestoreFov(){
 if(ownFov&&fovCamera&&(fovCamera==heldTpp||fovCamera==heldFpp)&&Q(fovCamera+8)==fovOwner&&Q(fovOwner)==engineBase+0x845f68)setFov(fovCamera,nativeFov);
 ownFov=false;fovCamera=0;fovOwner=0;
}
void Before(uintptr_t m){
 uintptr_t p=0,t=0,f=0,v=0;if(!Resolve(m,p,t,f,v)){active=false;ownMode=false;ownFov=false;return;}
 bool same=m==heldManager&&p==heldPlayer&&v==heldVis&&t==heldTpp&&f==heldFpp;
 if(!same){if(t==heldTpp&&f==heldFpp){RestoreFov();if(ownMode)*(float*)(t+0x88)=nativeDistance;}ownMode=false;ownFov=false;heldManager=m;heldPlayer=p;heldVis=v;heldTpp=t;heldFpp=f;Read(t+0x88,nativeDistance);if(!std::isfinite(nativeDistance)||nativeDistance<.1f||nativeDistance>100.f){fault=1;return;}}
 RestoreFov();int mode=I(m+0x68);uintptr_t camera=Q(m+0x60);
 if(mode!=1&&mode!=2){active=false;return;}                                                      
 if((mode==1&&camera!=t)||(mode==2&&camera!=f)||Q(camera+0x60)!=v+0xec0){active=false;return;}
 if(third.load()&&mode==2){((SetMode)(gameBase+0x93fc0))(m,1,p);ownMode=I(m+0x68)==1&&Q(m+0x60)==t;Log(ownMode?"CAMERA_THIRD_PERSON_BOUND":"CAMERA_SWITCH_FAILED");}
 if(third.load()&&mode==1&&camera==t)ownMode=true;
 if(!third.load()&&ownMode){if(I(m+0x68)==1&&Q(m+0x60)==t)((SetMode)(gameBase+0x93fc0))(m,2,p);*(float*)(t+0x88)=nativeDistance;ownMode=false;Log("CAMERA_FIRST_PERSON_RESTORED");}
 active=third.load()&&I(m+0x68)==1&&Q(m+0x60)==t;
 if(active)*(float*)(t+0x88)=std::clamp(distanceCm.load(),100,600)*.01f;
}
void After(uintptr_t m){
 if(!customFov.load()||fault.load())return;
 uintptr_t p=0,t=0,f=0,v=0;if(!Resolve(m,p,t,f,v)||m!=heldManager||p!=heldPlayer||v!=heldVis)return;
 int mode=I(m+0x68);uintptr_t camera=Q(m+0x60);if((mode!=1||camera!=t)&&(mode!=2||camera!=f))return;
 if(Q(camera+0x60)!=v+0xec0)return;uintptr_t owner=Q(camera+8);if(Q(owner)!=engineBase+0x845f68)return;
 float baseline=getFov(camera);if(!std::isfinite(baseline)||baseline<5.f||baseline>170.f){fault=2;return;}
 nativeFov=baseline;fovCamera=camera;fovOwner=owner;ownFov=true;
 float requested=(float)std::clamp(mode==1?thirdFov.load():firstFov.load(),50,120);setFov(camera,requested);
 observedFov=getFov(camera);++fovWrites;
}
void SafeBefore(uintptr_t m){__try{Before(m);}__except(EXCEPTION_EXECUTE_HANDLER){fault=1;third=false;customFov=false;active=false;}}
void SafeAfter(uintptr_t m){__try{After(m);}__except(EXCEPTION_EXECUTE_HANDLER){fault=2;customFov=false;}}
void Hook(uintptr_t m){
 ++ticks;
 if(busy.test_and_set(std::memory_order_acquire)){originalUpdate(m);return;}
 __try{if(ready)SafeBefore(m);originalUpdate(m);if(ready)SafeAfter(m);if(ready)Spinbot::SafeUpdate(m);if(ready)CameraLockOn::SafeUpdate(m);}
 __finally{busy.clear(std::memory_order_release);}
}
void Reset(){third=false;customFov=false;firstFov=75;thirdFov=75;distanceCm=300;if(bounds)SetBounds(false);}
void Diagnostic(){static ULONGLONG next=0;auto now=GetTickCount64();if(now<next)return;next=now+1000;char line[320];sprintf_s(line,"CAMERA_DIAG,ready=%d,third=%d,active=%d,bounds=%d,custom_fov=%d,first_fov=%d,third_fov=%d,observed_fov=%.2f,ticks=%llu,fov_writes=%llu,fault=%d",(int)ready.load(),(int)third.load(),(int)active.load(),(int)bounds.load(),(int)customFov.load(),firstFov.load(),thirdFov.load(),observedFov.load(),ticks.load(),fovWrites.load(),fault.load());Log(line);}
void Install(){
 if(!HashModule(gameBase,"d10c7f59ad3bf62e7f2dd69cd7e49eb634d4e8d046e4f4c07463b5d589833545")||!HashModule(engineBase,"7cf1f0153a2748da55a36d89fcf2e451ba444468260355836722d3ef76b27c79")){Log("CAMERA_BUILD_MISMATCH");return;}
 boundsReady=!memcmp((void*)(gameBase+0x658670),originalTable,sizeof(originalTable));
 const BYTE prologue[]={0x48,0x83,0xec,0x28,0x48,0x8b,0x41,0x60,0x48,0x85,0xc0,0x74,0x0f,0x80,0x78,0x68};
 if(memcmp((void*)(gameBase+0x94200),prologue,sizeof(prologue)))return;
 setFov=(SetFov)GetProcAddress((HMODULE)engineBase,"?SetFOV@IBaseCamera@@QEAAXM@Z");getFov=(GetFov)GetProcAddress((HMODULE)engineBase,"?GetFOV@IBaseCamera@@QEAAMXZ");if(!setFov||!getFov)return;
 auto init=MH_Initialize();if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED)return;
 void*target=(void*)(gameBase+0x94200);if(MH_CreateHook(target,(void*)Hook,(void**)&originalUpdate)!=MH_OK)return;
 ready=true;if(MH_EnableHook(target)!=MH_OK){ready=false;MH_RemoveHook(target);return;}Log("CAMERA_MENU_READY_DISABLED");
}
bool CanStop(){
 third=false;customFov=false;if(bounds&&!SetBounds(false))return false;
                                                                                  
 for(int n=0;n<100;n++){if(!busy.test_and_set(std::memory_order_acquire)){bool clean=!ownMode&&!ownFov;busy.clear(std::memory_order_release);if(clean)return true;}Sleep(5);}
 Log("CAMERA_UNLOAD_WAITING_FOR_NATIVE_UPDATE");return false;
}
}
