                                                                                          
namespace VisualToggles {
std::atomic_bool ready{false},infiniteFlashlight{false};
std::atomic<unsigned> active{0};
std::atomic<unsigned long long> applied{0},refused{0};
using Update=void(*)(uintptr_t,float);Update original=nullptr;
uintptr_t Q(uintptr_t a){uintptr_t v=0;Read(a,v);return v;}
bool Eligible(uintptr_t component,float delta,float&capacity,float&drain){
 if(!std::isfinite(delta)||delta<0.f||delta>1.f||Q(component)!=gameBase+0xecedc8)return false;
 uintptr_t player=Q(Q(gameBase+AmmoRootRva)+0x628);
 if(Q(player)==gameBase+0xebbe08)player-=0x18;else if(Q(player)==gameBase+0xebc168)player-=0x28;
 if(Q(player)!=gameBase+0xebb6d8||Q(player+0x8e0)!=component||Q(component+0x58)!=player)return false;
 unsigned char initialized=0,live=0;float charge=0;
 if(!Read(component+0x28,initialized)||!initialized||!Read(component+0x50,live)||!live||!Read(component+0x64,capacity)||!Read(component+0x68,charge)||!Read(component+0x6c,drain))return false;
 return std::isfinite(capacity)&&capacity>0.f&&capacity<=100000.f&&std::isfinite(charge)&&charge>=0.f&&charge<=capacity&&std::isfinite(drain)&&drain>=0.f&&drain<=100000.f;
}
bool Prepare(uintptr_t component,float delta,float&capacity,float&drain){
 __try{
  if(!Eligible(component,delta,capacity,drain))return false;
  *(float*)(component+0x68)=capacity;
  *(float*)(component+0x6c)=0.f;
  return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){infiniteFlashlight=false;return false;}
}
void Hook(uintptr_t component,float delta){
 ++active;float capacity=0,drain=0;bool changed=false;
 __try{
  if(ready&&infiniteFlashlight){changed=Prepare(component,delta,capacity,drain);if(!changed)++refused;}
  original(component,delta);
 }__finally{
  if(changed){*(float*)(component+0x6c)=drain;++applied;}
  --active;
 }
}
void Reset(){infiniteFlashlight=false;}
void Install(){
 if(!CameraFeatures::HashModule(gameBase,"d10c7f59ad3bf62e7f2dd69cd7e49eb634d4e8d046e4f4c07463b5d589833545"))return;
 const BYTE expected[]={0x40,0x57,0x48,0x81,0xec,0xa0,0,0,0,0x80,0x79,0x28,0,0x48,0x8b,0xf9};
 void*target=(void*)(gameBase+0x738d30);
 if(memcmp(target,expected,sizeof(expected))){Log("FLASHLIGHT_CODE_MISMATCH");return;}
 auto result=MH_Initialize();if(result!=MH_OK&&result!=MH_ERROR_ALREADY_INITIALIZED)return;
 if(MH_CreateHook(target,(void*)Hook,(void**)&original)!=MH_OK)return;
 ready=true;
 if(MH_EnableHook(target)!=MH_OK){ready=false;MH_RemoveHook(target);return;}
 Log("FLASHLIGHT_READY_DISABLED");
}
bool CanStop(){
 infiniteFlashlight=false;
 if(ready.exchange(false))MH_DisableHook((void*)(gameBase+0x738d30));
 for(int i=0;i<100&&active.load();++i)Sleep(1);
 return active.load()==0;
}
void Diagnostic(){static ULONGLONG next=0;auto now=GetTickCount64();if(now<next)return;next=now+1000;char text[192];sprintf_s(text,"FLASHLIGHT_DIAG,ready=%d,enabled=%d,applied=%llu,refused=%llu",(int)ready.load(),(int)infiniteFlashlight.load(),applied.load(),refused.load());Log(text);}
}
