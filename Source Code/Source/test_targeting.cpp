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
struct Enemy {EspEntity e;PoseFixture pose;bool visible=true,head=true,chest=true;};
std::vector<Enemy> enemies;unsigned checks=0;V shotEnd{};
unsigned char Visibility(void*,unsigned char,void*c,const V*,V*end,unsigned short,void*,unsigned,unsigned,int){
 ++checks;for(auto&e:enemies)if(Distance({e.e.root.x,end->y,e.e.root.z},*end)<.001f&&e.visible&&((fabsf(end->y-1.55f)<.001f&&e.head)||(fabsf(end->y-1.35f)<.001f&&e.chest))){Write((uintptr_t)c+0x18,e.e.complete+0x18);return 2;}return 0;
}
unsigned char Shot(void*,V*,V*end,void*c,void*,char){shotEnd=*end;for(auto&e:enemies)if(fabsf(end->x-e.e.root.x)<.001f&&fabsf(end->z-e.e.root.z)<.001f){Write((uintptr_t)c+0x18,e.e.complete+0x18);return 2;}return 1;}
void Add(V root,float hp){auto a=PoseAllocate(0x900),b=PoseAllocate(0x100);Write(a,gameBase+ZombieState1Rva);Write(a+0x58,b);Write(a+0x6f8,root);Write(a+0x734,hp);enemies.push_back({{a,b,1,root,false,true},MakePose(a,b,root)});}
void PublishEnemies(){EspFrame f{};f.tick=testTime;for(auto&e:enemies)f.entities[f.count++]=e.e;PublishEsp(f);}
int main(){
 SetEnvironmentVariableW(L"LOCALAPPDATA",L"../Evidence/TargetingFixture");gameBase=PoseAllocate(0x1290000);gameSize=0x1290000;PoseCamera();
 auto hud=PoseAllocate(0x700),player=PoseAllocate(0x2000);Write(gameBase+AmmoRootRva,hud);Write(hud+0x628,player);Write(player,gameBase+0xebb6d8);Write(player+0x18,gameBase+0xebbe08);
 menuOpen=false;stop=false;captureHotkey=-1;CombatLock::ray=Visibility;original=Shot;CameraLockOn::targetSource=0;fovAimEnabled=false;
 Add({2,0,10},100);Add({4,0,20},10);Add({0,0,30},50);PublishEnemies();
 V point{};assert(PoseNameHash("bip01 spine3")==0x1bfbfe9e&&PoseNameHash("bip01 head")==0xce79da5e);
 assert(ResolveAimPoseNow(enemies[0].e.state,enemies[0].e.root,point,0)&&fabsf(point.y-1.55f)<.001f);
 assert(ResolveAimPoseNow(enemies[0].e.state,enemies[0].e.root,point,1)&&fabsf(point.y-1.35f)<.001f);
                                                                                
 auto&p=enemies[0].pose;Write(p.map,-1);assert(!ResolveAimPoseNow(enemies[0].e.state,enemies[0].e.root,point,1));Write(p.map,17);
 const int chestKey=17*7%24;Write(p.flags+chestKey,(unsigned char)2);Write(p.descriptors+17*0x30,p.descriptors+17*0x30);assert(!ResolveAimPoseNow(enemies[0].e.state,enemies[0].e.root,point,1));
 Write(p.descriptors+17*0x30,p.descriptors+3*0x30);M34 local{{1,0,0,0,0,1,0,.2f,0,0,1,0}};Write(p.locals+chestKey*0x30,local);assert(ResolveAimPoseNow(enemies[0].e.state,enemies[0].e.root,point,1)&&fabsf(point.y-1.55f)<.001f);Write(p.flags+chestKey,(unsigned char)0);
 AimTargeting::State state{};EspEntity chosen{};
 for(int mode=0;mode<3;++mode){AimTargeting::priority=mode;checks=0;assert(AimTargeting::Select(player,0,state,point,chosen));assert(chosen.state==enemies[mode].e.state&&checks<=4);}
                                                                                    
 AimTargeting::priority=1;assert(AimTargeting::Select(player,0,state,point,chosen)&&chosen.state==enemies[1].e.state);Write(enemies[0].e.state+0x734,1.f);assert(AimTargeting::Select(player,0,state,point,chosen)&&chosen.state==enemies[1].e.state);
 AimTargeting::bodyPart=1;assert(AimTargeting::Select(player,0,state,point,chosen)&&chosen.state==enemies[0].e.state&&fabsf(point.y-1.35f)<.001f);
 enemies[0].chest=false;assert(AimTargeting::Select(player,0,state,point,chosen)&&chosen.state==enemies[1].e.state);enemies[0].chest=true;
                                                                                  
 FeatureBindings::keys[70]='Z';testKeys['Z']=(SHORT)0x8000;CameraLockOn::active=true;
 CombatLock::Snapshot snap{true,chosen,player,{0,1.55f,0},point,testTime};CombatLock::Publish(snap);CombatLock::Snapshot copy{};assert(CombatLock::Copy(copy));
 CameraLockOn::Sample sample{};sample.valid=true;sample.mode=1;sample.stamp=testTime;sample.yaw=.2f;CameraLockOn::Publish(sample);
 AimTargeting::bodyPart=0;assert(!CombatLock::Copy(copy)&&!CombatLock::Visible(snap));LONG dx=0,dy=0;assert(!CameraLockOn::Take(dx,dy,false,2));testKeys['Z']=0;
                                                                                          
 for(int part=0;part<2;++part){AimTargeting::priority=2;AimTargeting::bodyPart=part;V begin{0,1.55f,0},end{99,1,0};unsigned char collision[0x60]{};
  assert(CombatLock::StandaloneRay(nullptr,&begin,&end,collision,(void*)(player+0x18),0)==2);assert(fabsf(shotEnd.y-(part?1.35f:1.55f))<.001f&&shotEnd.z==30);
  CameraLockOn::hasLock=false;assert(CameraLockOn::AimPoint(player,point)&&CameraLockOn::locked.state==enemies[2].e.state&&Distance(point,shotEnd)<.001f);
 }
                                                                    
 AimTargeting::priority=0;AimTargeting::bodyPart=0;for(auto&e:enemies)e.visible=false;for(int i=0;i<6;++i)Add({float(i+1),0,float(40+i*10)},100);for(auto&e:enemies)e.visible=false;enemies.back().visible=true;PublishEnemies();state={};bool acquired=false;
 for(int i=0;i<3&&!acquired;++i){checks=0;acquired=AimTargeting::Select(player,0,state,point,chosen);assert(checks<=4);}assert(acquired&&chosen.state==enemies.back().e.state);
 testTime+=301;assert(!AimTargeting::Select(player,0,state,point,chosen));
 std::cout<<"PASS production priority selection, sticky lock and option reselect, semantic head/chest with permuted keys, missing mapping and parent-cycle rejection, parent transform, selected-part visibility, shared camera/projectile point, option expiry, four-ray budget and occluded-roster progress, stale roster. Native ray mocked; gameplay pending.\n";
}
