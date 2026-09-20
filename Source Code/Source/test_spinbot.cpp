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
#include "test_pose_fixture.h"


uintptr_t Allocate(size_t n){auto p=(uintptr_t)VirtualAlloc(nullptr,n,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);assert(p);return p;}
bool specialCamera=false;unsigned char FakeModelCamera(void*){return specialCamera?1:0;}
bool poisonAngles=false;unsigned basisWrites=0;M34 renderedWorld{};
const M34* FakeGetWorld(void*){return &renderedWorld;}
void FakeBasis(void*p,const M34*m){renderedWorld=*m;++basisWrites;if(poisonAngles)*(float*)((uintptr_t)p-0x18+0xc7c)+=1;}
uintptr_t hitControl=0;unsigned char hitResult=2;V observedRayEnd{};unsigned rayCalls=0;bool wallOnShot=false;
unsigned char FakeVisibility(void*,unsigned char,void*c,const V*,V*,unsigned short,void*,unsigned,unsigned,int){++rayCalls;memcpy((char*)c+0x18,&hitControl,8);return hitResult;}
unsigned char FakeShot(void*,V*,V*end,void*c,void*,char){observedRayEnd=*end;uintptr_t result=wallOnShot?0:hitControl;memcpy((char*)c+0x18,&result,8);if(wallOnShot){*end={0,1,4};return 1;}return 2;}
uintptr_t Rtti(unsigned base,const char*name){uintptr_t vt=gameBase+base,col=gameBase+base+0x100;Write(vt-8,col);Write(col+4,0u);Write(col+16,base+0x200);Write(gameBase+base+0x208,1);Write(gameBase+base+0x20c,base+0x300);Write(gameBase+base+0x300,base+0x400);Write(gameBase+base+0x400,base+0x500);strcpy_s((char*)(gameBase+base+0x510),192,name);return vt;}
int main(){
 SetEnvironmentVariableW(L"LOCALAPPDATA",L"../Evidence/BodySpinFixture");
 gameBase=Allocate(0x1290000);gameSize=0x1290000;engineBase=Allocate(0x900000);
 auto hud=Allocate(0x700),p=Allocate(0x2000),v=Allocate(0x4000),g=Allocate(0x500),session=Allocate(0x500),m=Allocate(0x100),t=Allocate(0x100),f=Allocate(0x100),tree=Allocate(0x40),node1=Allocate(0x40)+16,node2=Allocate(0x40)+16;
 Write(gameBase+0x12822f8,hud);Write(hud+0x628,p);Write(p,gameBase+0xebb6d8);Write(p+0x18,gameBase+0xebbe08);Write(p+0x268,v);Write(v,gameBase+0xebedb8);Write(v+0x60,p);Write(v+0xec0,gameBase+0xebf9d8);
 Write(gameBase+0x12822a8,g);Write(g+0x4a0,session);Write(session+0xc8,m);Write(m,gameBase+0xceb628);Write(m+0x68,2);Write(m+0x60,f);Write(f,gameBase+0xcead58);Write(t,gameBase+0xcedd78);Write(f+0x60,v+0xec0);Write(t+0x60,v+0xec0);Write(m+0x50,tree);Write(tree,node1);Write(node1-16,1);Write(node1-8,t);Write(node1+16,node2);Write(node2-16,2);Write(node2-8,f);
 Write(gameBase+0xebb6d8+0x150,gameBase+0x834910);Write(gameBase+0xebb6d8+0x460,gameBase+0x664820);
 CameraFeatures::heldManager=m;CameraFeatures::heldPlayer=p;CameraFeatures::heldVis=v;CameraFeatures::heldFpp=f;CameraFeatures::heldTpp=t;CameraFeatures::ready=true;
 Write(p+0x578,V{100,10,100});Spinbot::setWorld=FakeBasis;Spinbot::getWorld=FakeGetWorld;auto modelEngine=Allocate(0x200),modelDescriptor=Allocate(0x40);Write(p+0x20,modelEngine);Write(modelEngine+0xe8,modelDescriptor);Spinbot::modelCamera=FakeModelCamera;Spinbot::fault=0;FeatureBindings::Defaults();menuOpen=false;stop=false;captureHotkey=-1;
 M34 identity{{1,0,0,100,0,1,0,10,0,0,1,100}};M34 physics{{1,0,0,0,0,1,0,0,0,0,1,0}};Write(p+0x590,physics);renderedWorld=identity;assert(Spinbot::ValidBasis(identity));auto invalid=identity;invalid.v[0]=2;assert(!Spinbot::ValidBasis(invalid));
 M34 oldPhysics=Spinbot::Rotated(physics,1.2);Write(p+0x590,oldPhysics);assert(Spinbot::SameBasis(renderedWorld,identity));Write(p+0x590,physics);
 for(int hz:{30,60,120,1000}){
  Spinbot::Stop();Spinbot::Restore();renderedWorld=identity;testTime=1000;Spinbot::speed=3;Spinbot::enabled=true;Spinbot::Update(m);
  for(int i=1;i<=hz;++i){testTime=1000+(ULONGLONG)i*1000/hz;Spinbot::Update(m);if(!Spinbot::Running()){uintptr_t rp=0,rt=0,rf=0,rv=0;std::cerr<<"body fail hz="<<hz<<" i="<<i<<" fault="<<Spinbot::fault<<" owns="<<Spinbot::owns<<" ready="<<Spinbot::PlayerReady(p)<<" resolve="<<CameraFeatures::Resolve(m,rp,rt,rf,rv)<<" allowed="<<CombatLock::Allowed()<<" mode="<<CameraFeatures::I(m+0x68)<<" writes="<<basisWrites<<"\n";}assert(Spinbot::Running());}
  M34 actual{};actual=renderedWorld;assert(Spinbot::SameBasis(actual,identity));M34 unchanged{};ReadBytes(p+0x590,&unchanged,sizeof(unchanged));assert(Spinbot::SameBasis(unchanged,physics));assert(*(float*)(p+0xc7c)==0&&*(float*)(p+0xc80)==0);
 }
 testTime+=10;Spinbot::Update(m);M34 spinning{};spinning=renderedWorld;assert(!Spinbot::SameBasis(spinning,identity));assert(spinning.v[3]==100&&spinning.v[7]==10&&spinning.v[11]==100);
 FeatureBindings::keys[70]='Z';testKeys['Z']=(SHORT)0x8000;testTime+=10;Spinbot::Update(m);assert(Spinbot::Running());
 DIMOUSESTATE state{};state.lX=17;state.lY=-4;CameraLockOn::InjectMouseState(&state);assert(state.lX==17&&state.lY==-4);
 menuOpen=true;Spinbot::Update(m);M34 restored{};restored=renderedWorld;assert(!Spinbot::active&&!Spinbot::owns&&Spinbot::SameBasis(restored,identity));menuOpen=false;Spinbot::Update(m);
 testTime+=100;Spinbot::Update(m);assert(!Spinbot::active&&!Spinbot::owns);Spinbot::Update(m);testTime+=10;Spinbot::Update(m);assert(Spinbot::active);
 Write(m+0x68,1);Write(m+0x60,t);testTime+=10;Spinbot::Update(m);assert(Spinbot::Running());Write(m+0x68,2);Write(m+0x60,f);

 specialCamera=true;Spinbot::Update(m);assert(!Spinbot::Running()&&!Spinbot::owns);specialCamera=false;Spinbot::Update(m);assert(Spinbot::Running());
 auto enemy=Allocate(0x900),complete=Allocate(0x100);Write(enemy,gameBase+ZombieState1Rva);Write(enemy+0x58,complete);Write(enemy+0x6f8,V{0,0,10});Write(enemy+0x734,100.f);
 MakePose(enemy,complete,{0,0,10});PoseCamera();auto cameraOwner=CameraFeatures::Q(CameraFeatures::Q(gameBase+CameraViewRva)+0x90390);Write(f+8,cameraOwner);Write(cameraOwner,engineBase+0x845f68);
 EspEntity entity{enemy,complete,1,{0,0,10},false,true};EspFrame frame{};frame.entities[0]=entity;frame.count=1;frame.tick=testTime;PublishEsp(frame);
 CombatLock::Snapshot snap{true,entity,p,{0,1,0},{0,1,10},testTime};CombatLock::ray=FakeVisibility;hitControl=complete+0x18;assert(CombatLock::Visible(snap));CombatLock::Publish(snap);CombatLock::Snapshot copy{};assert(CombatLock::Copy(copy));
 hitControl=0;assert(!CombatLock::Visible(snap));hitControl=complete+0x18;frame.entities[0].generation=2;PublishEsp(frame);assert(!CombatLock::Copy(copy));frame.entities[0]=entity;PublishEsp(frame);
 original=FakeShot;alignas(16)unsigned char collision[0x60]{};V begin{0,1,0},end{20,1,0};wallOnShot=false;assert(CombatLock::LockedRay(nullptr,&begin,&end,collision,(void*)(p+0x18),0)==2);assert(observedRayEnd.z==10&&observedRayEnd.x==0);
 end={20,1,0};wallOnShot=true;assert(CombatLock::LockedRay(nullptr,&begin,&end,collision,(void*)(p+0x18),0)==1);assert(end.z==4);uintptr_t wall=1;memcpy(&wall,collision+0x18,8);assert(wall==0);wallOnShot=false;
                                                                                 
 PublishTarget({enemy+16,8,{90,1,0},90,testTime});end={20,1,0};CombatLock::LockedRay(nullptr,&begin,&end,collision,(void*)(p+0x18),0);assert(observedRayEnd.z==10);
 auto item=Allocate(0x100),desc=Allocate(0x700);Write(p+0xff0,item);Write(item,Rtti(0x10000,"InventoryItem"));Write(desc,Rtti(0x11000,"ItemDescFirearm"));Write(item+0x68,desc);Write(item+0x50,10);Write(desc+0x598,0);assert(CombatLock::FirearmReady());
 unsigned char cameraBefore[0x180]{},cameraAfter[0x180]{};ReadBytes(cameraOwner,cameraBefore,sizeof(cameraBefore));CameraLockOn::active=true;CameraLockOn::Update(m);ReadBytes(cameraOwner,cameraAfter,sizeof(cameraAfter));assert(memcmp(cameraBefore,cameraAfter,sizeof(cameraBefore))==0&&!CameraLockOn::pending.valid&&!CameraLockOn::haveError);
 V toward{0-renderedWorld.v[3],0,10-renderedWorld.v[11]};float norm=sqrtf(toward.x*toward.x+toward.z*toward.z);assert(fabsf(renderedWorld.v[2]-toward.x/norm)<.001f&&fabsf(renderedWorld.v[10]-toward.z/norm)<.001f);assert(renderedWorld.v[3]==100&&renderedWorld.v[7]==10&&renderedWorld.v[11]==100);
                                                                               
 testTime+=30;frame.tick=testTime;PublishEsp(frame);Spinbot::Update(m);CameraLockOn::Update(m);M34 resumed=renderedWorld;assert(fabsf(resumed.v[2]-toward.x/norm)>.01f);ReadBytes(cameraOwner,cameraAfter,sizeof(cameraAfter));assert(memcmp(cameraBefore,cameraAfter,sizeof(cameraBefore))==0);
                                                                                    
 AutoFire::started=0;AutoFire::target=0;AutoFire::generation=0;snap.stamp=testTime;CombatLock::Publish(snap);
 AutoFire::down=false;DIDEVICEOBJECTDATA data[4]{};DWORD n=0;AutoFire::Events(data,&n,4,false);assert(n==1&&data[0].dwOfs==DIMOFS_BUTTON0&&data[0].dwData==0x80&&!AutoFire::CanStop());
 n=0;AutoFire::Events(data,&n,4,false);assert(n==0);state={};state.lX=3;state.lY=5;AutoFire::State(&state,false);assert(state.rgbButtons[0]==0x80&&state.lX==3&&state.lY==5);
 testTime+=30;snap.stamp=testTime;frame.tick=testTime;PublishEsp(frame);CombatLock::Publish(snap);n=0;AutoFire::Events(data,&n,0,false);assert(n==0&&AutoFire::down);AutoFire::Events(data,&n,4,false);assert(n==1&&data[0].dwData==0&&!AutoFire::down&&AutoFire::CanStop());
 testTime+=50;snap.stamp=testTime;frame.tick=testTime;PublishEsp(frame);CombatLock::Publish(snap);n=0;AutoFire::Events(data,&n,4,false);assert(n==1&&data[0].dwData==0x80);
 Write(item+0x50,0);n=0;AutoFire::Events(data,&n,4,false);assert(n==1&&data[0].dwData==0);Write(item+0x50,10);
 n=0;AutoFire::Events(data,&n,4,false);assert(n==1&&data[0].dwData==0x80);CombatLock::Clear();n=0;AutoFire::Events(data,&n,4,false);assert(n==1&&data[0].dwData==0);
 CombatLock::Publish(snap);testTime+=41;assert(!CombatLock::Copy(copy));
                                                                                
 Spinbot::Stop();testKeys['Z']=0;CameraLockOn::active=false;frame.tick=testTime;PublishEsp(frame);PublishTarget({enemy,1,{0,1,10},10,testTime});
                                                                          
 end={20,1,0};wallOnShot=true;assert(CombatLock::StandaloneRay(nullptr,&begin,&end,collision,(void*)(p+0x18),0)==1&&end.z==4&&observedRayEnd.z==10&&!pendingRedirectExplosion.valid);wallOnShot=false;
 Spinbot::Stop();Spinbot::Update(m);assert(!Spinbot::owns);Spinbot::enabled=true;Spinbot::Update(m);poisonAngles=true;testTime+=10;Spinbot::Update(m);assert(Spinbot::fault==2&&!Spinbot::enabled);poisonAngles=false;
 std::cout<<"PASS production rendered-world rotation/restoration with separate physics storage (old-path failure reproduced), position preserved, body faces shared shot target then resumes spin, production combined-update leaves camera bytes unchanged, rates30/60/120/1000Hz, look angles untouched, heldlock does not pause body, no mouse spin, menu/stall recovery, native first-hit wall collision, shared target identity/generation, stale expiry, trigger edge/release/capacity/ammo guards, fault disables spin. Native callbacks mocked; gameplay pending.\n";
}
