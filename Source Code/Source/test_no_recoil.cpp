#include <windows.h>
#include <atomic>
#include <array>
#include <cstring>
#include <cassert>
#include <cstdio>
#include <iostream>
std::array<unsigned char,0x1000> player{};std::array<unsigned char,0x220> primary{},secondary{},npc{};
uintptr_t local=(uintptr_t)player.data();uintptr_t gameBase=0x180000000;
std::atomic_bool stop{false};bool build=true,hookOK=true;std::string status;
bool TrueGodPlayer(uintptr_t&p){p=local;return p!=0;}
bool TrueGodBuildMatches(){return build;}
template<class T>bool Read(uintptr_t a,T&v){for(auto r:{std::pair<uintptr_t,size_t>{(uintptr_t)player.data(),player.size()},{(uintptr_t)primary.data(),primary.size()},{(uintptr_t)secondary.data(),secondary.size()},{(uintptr_t)npc.data(),npc.size()}})if(a>=r.first&&a+sizeof(v)<=r.first+r.second){memcpy(&v,(void*)a,sizeof(v));return true;}return false;}
template<class T>void put(unsigned char*p,size_t off,T v){memcpy(p+off,&v,sizeof(v));}
void SetStatus(const char*s){status=s;}void Log(const char*){}
bool InstallFeatureHook(uintptr_t r,void*,void**,const unsigned char(&sig)[16]){assert(r==0x78d770&&sig[0]==0x48&&sig[15]==0x2f);return hookOK;}
#include "NativeMenu/src/Main/NoRecoil.inl"
int calls=0;void*last=nullptr;const float*lastImpulse=nullptr;float lastTime=0;
void native(void*c,const float*i,float t){++calls;last=c;lastImpulse=i;lastTime=t;}
int main(){
 for(auto pair:{std::pair<unsigned char*,int>{primary.data(),0},{secondary.data(),1}}){put(pair.first,0x58,local);put(pair.first,0x50,(unsigned char)1);put(pair.first,0x1b4,pair.second);put(player.data(),0x8a0+pair.second*8,(uintptr_t)pair.first);}
 NoRecoil::original=native;float kick[2]={.05f,-.02f};
 assert(!NoRecoil::Set(true));NoRecoil::Install();assert(NoRecoil::ready&&!NoRecoil::enabled);
 NoRecoil::Apply(primary.data(),kick,.4f);assert(calls==1&&last==primary.data()&&lastImpulse==kick&&lastTime==.4f);
 auto p=player;auto a=primary;auto b=secondary;assert(NoRecoil::Set(true));
 NoRecoil::Apply(primary.data(),kick,.4f);NoRecoil::Apply(secondary.data(),kick,.4f);assert(calls==1&&NoRecoil::blocked==2);assert(player==p&&primary==a&&secondary==b&&kick[0]==.05f&&kick[1]==-.02f);
 npc=primary;NoRecoil::Apply(npc.data(),kick,.4f);assert(calls==2);                                      
 put(primary.data(),0x58,(uintptr_t)0x12345);NoRecoil::Apply(primary.data(),kick,.4f);assert(calls==3);primary=a;
 put(primary.data(),0x1b4,2);NoRecoil::Apply(primary.data(),kick,.4f);assert(calls==4);primary=a;
 put(primary.data(),0x50,(unsigned char)0);NoRecoil::Apply(primary.data(),kick,.4f);assert(calls==5);primary=a;
 local=0;NoRecoil::Apply(primary.data(),kick,.4f);assert(calls==6);local=(uintptr_t)player.data();
 stop=true;NoRecoil::Apply(primary.data(),kick,.4f);assert(calls==7);stop=false;
 NoRecoil::Apply((void*)1,kick,.4f);assert(calls==8);
 NoRecoil::Set(false);NoRecoil::Apply(primary.data(),kick,.4f);assert(calls==9);                                      
 NoRecoil::ready=false;build=false;NoRecoil::Install();assert(!NoRecoil::ready);build=true;hookOK=false;NoRecoil::Install();assert(!NoRecoil::ready);
 std::cout<<"PASS no_recoil local primary/secondary only; disabled/stopped/stale/foreign/invalid pass-through; zero camera/controller/impulse mutation; signature/readiness guards\n";
}