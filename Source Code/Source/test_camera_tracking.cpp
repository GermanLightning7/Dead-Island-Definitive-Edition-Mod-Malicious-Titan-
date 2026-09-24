#include <windows.h>
#include <array>
std::array<SHORT,256> testKeys{};
SHORT TestKey(int key){return key>0&&key<256?testKeys[key]:0;}
#define GetAsyncKeyState TestKey
bool testForeground=true;HWND TestForegroundWindow(){return (HWND)1;}DWORD TestWindowThreadProcessId(HWND,DWORD*pid){if(pid)*pid=testForeground?GetCurrentProcessId():0;return 1;}
#define GetForegroundWindow TestForegroundWindow
#define GetWindowThreadProcessId TestWindowThreadProcessId
ULONGLONG testTime=1000;ULONGLONG CameraTestNow(){return testTime;}
#define GetTickCount64 CameraTestNow
#include "NativeMenu/src/Main/PersistentCollisionRedirect.cpp"
#include <cassert>
#include <iostream>

int main(){
 using namespace CameraLockOn;
 Sample s{};s.player=1;s.target=2;s.generation=1;s.mode=1;s.stamp=testTime;s.pos={1000,62,-700};s.aim={1000,62,-600};s.forward={0,0,1};s.right={1,0,0};s.up={0,1,0};
 assert(Compute(s)&&std::fabs(s.yaw)<1e-6f&&std::fabs(s.pitch)<1e-6f);
 auto wrong=s;wrong.pos={0,2,0};assert(Compute(wrong)&&std::fabs(wrong.yaw)>2.f);
 auto nearSample=s;nearSample.aim={1001,62,-690};auto farSample=s;farSample.aim={1010,62,-600};assert(Compute(nearSample)&&Compute(farSample)&&std::fabs(nearSample.yaw-farSample.yaw)<1e-6f);
 auto invalid=s;invalid.up=invalid.right;assert(!Compute(invalid));invalid=s;invalid.aim=invalid.pos;assert(!Compute(invalid));invalid=s;invalid.aim={1000,90,-700};assert(!Compute(invalid));
 FeatureBindings::keys[70]='Z';testKeys['Z']=(SHORT)0x8000;active=true;menuOpen=false;stop=false;captureHotkey=-1;
 s.yaw=.1f;s.pitch=.1f;Publish(s);DIDEVICEOBJECTDATA data[8]{};DWORD count=0;InjectBufferedMouseData(data,&count,8);assert(count==2&&(LONG)data[0].dwData==4&&(LONG)data[1].dwData==-4);auto applied=appliedTicks.load();
 for(int i=0;i<1000;++i){count=0;InjectBufferedMouseData(data,&count,8);assert(count==0);}DIMOUSESTATE mouse{};InjectMouseState(&mouse);assert(mouse.lX==0&&mouse.lY==0&&appliedTicks==applied);
 Publish(s);count=0;InjectBufferedMouseData(data,&count,1);assert(count==0&&pending.valid);InjectBufferedMouseData(data,&count,2);assert(count==2&&!pending.valid);
 Publish(s);testTime+=41;count=0;InjectBufferedMouseData(data,&count,8);assert(count==0&&staleDrops==1);s.stamp=testTime;
 Publish(s);testKeys['Z']=0;count=0;InjectBufferedMouseData(data,&count,8);assert(count==0&&!pending.valid);testKeys['Z']=(SHORT)0x8000;
 Publish(s);menuOpen=true;InjectMouseState(&mouse);assert(mouse.lX==0&&!pending.valid);menuOpen=false;
 Publish(s);count=1;data[0].dwOfs=DIMOFS_X;data[0].dwData=17;InjectBufferedMouseData(data,&count,8);assert(count==3&&data[0].dwData==17&&sent.physical);
                                                                             
 for(float response:{.0002f,.001f,.006f,.02f}){
  AxisResponse axis{};float error=1.f;
  for(int i=0;i<180;++i){LONG n=axis.Command(error);assert(std::abs(n)<=64);float turned=(float)n*response;axis.Observe(turned,n);error-=turned;assert(std::isfinite(error)&&std::fabs(error)<=1.001f);}
  assert(std::fabs(error)<.021f);std::cout<<"response="<<response<<" residual="<<error<<'\n';
 }
 AxisResponse changing{};float err=1.f;
 for(int i=0;i<260;++i){float response=i<90?.0002f:.02f;LONG n=changing.Command(err);float turn=n*response;changing.Observe(turn,n);err-=turn;if(i==89)err=1.f;assert(std::isfinite(err)&&std::fabs(err)<1.5f);}assert(std::fabs(err)<.021f);
 AxisResponse axis{};axis.Observe(-.1f,4);assert(axis.radiansPerCount==0);axis.Observe(.02f,4);assert(std::fabs(axis.radiansPerCount-.005f)<1e-6);axis.Observe(.08f,4);assert(std::fabs(axis.radiansPerCount-.02f)<1e-6);
 yawResponse[1]={};yawResponse[2]={};yawResponse[1].Observe(.08f,4);yawResponse[2].Observe(.004f,4);assert(yawResponse[1].Command(.1f)<yawResponse[2].Command(.1f));
 fault=5;PollCameraLockOnInput();assert(!active&&!Held());fault=0;
 EspEntity a{10,20,1,{0,0,0},false,true},b=a;assert(Same(a,b));b.generation=2;assert(!Same(a,b));b=a;b.complete=21;assert(!Same(a,b));
 std::cout<<"PASS world-translation regression, nearSample/farSample bearing equivalence, degenerate basis rejection, one-use correction across1000polls and both APIs, completeXYcapacity, stale expiry, immediate key/menu release, physical mouse preservation, measured-response convergence and per-mode isolation, identity/generation. Mocked geometry/response; gameplay unverified.\n";
}


