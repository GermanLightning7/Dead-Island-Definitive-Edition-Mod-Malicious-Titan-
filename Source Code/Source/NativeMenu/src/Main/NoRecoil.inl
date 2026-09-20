                                                                      
                                                                            
namespace NoRecoil {
std::atomic_bool enabled{false},ready{false};
std::atomic<unsigned long long> blocked{0},passed{0};
using ApplyFn=void(*)(void*,const float*,float);ApplyFn original=nullptr;
bool Local(void*controller){
 uintptr_t p=0,owner=0,held=0;int slot=-1;unsigned char active=0;auto c=(uintptr_t)controller;
 if(!c||!TrueGodPlayer(p)||!Read(c+0x58,owner)||owner!=p||!Read(c+0x50,active)||!active||!Read(c+0x1b4,slot)||slot<0||slot>1)return false;
 return Read(p+0x8a0+(uintptr_t)slot*8,held)&&held==c;
}
void Apply(void*controller,const float*impulse,float duration){
 if(ready.load()&&enabled.load()&&!stop.load()&&Local(controller)) {++blocked;return;}
 ++passed;if(original)original(controller,impulse,duration);
}
bool Set(bool value){if(value&&!ready){SetStatus("NO RECOIL UNAVAILABLE");return false;}enabled=value;SetStatus(value?"NO RECOIL ON":"NO RECOIL OFF");return true;}
void Install(){
 if(!TrueGodBuildMatches())return;
 const unsigned char signature[16]={0x48,0x83,0xec,0x28,0x83,0xb9,0xb4,0x01,0x00,0x00,0x01,0x0f,0x28,0xda,0x75,0x2f};
 ready=InstallFeatureHook(0x78d770,(void*)Apply,(void**)&original,signature);
 Log(ready?"NO_RECOIL_READY_DISABLED":"NO_RECOIL_HOOK_REFUSED");
}
void Diagnostic(){static ULONGLONG next=0;auto now=GetTickCount64();if(now<next)return;next=now+1000;char line[180];sprintf_s(line,"NO_RECOIL_DIAG,ready=%d,on=%d,blocked=%llu,passed=%llu",(int)ready.load(),(int)enabled.load(),blocked.load(),passed.load());Log(line);}
}
