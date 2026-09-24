                                                                                       
namespace CameraLockOn { bool Held(); }
namespace Spinbot { bool Requested(); bool Running(); }
bool TrueGodPlayer(uintptr_t&player);
namespace CombatLock {
struct Snapshot {bool valid=false;EspEntity entity{};uintptr_t player=0;V camera{},aim{};ULONGLONG stamp=0;int options=AimTargeting::Options();};
std::mutex mutex;Snapshot published{};
using NativeRay=unsigned char(*)(void*,unsigned char,void*,const V*,V*,unsigned short,void*,unsigned,unsigned,int);
NativeRay ray=nullptr;
std::atomic<unsigned long long> visibleChecks{0},blocked{0},shots{0},explosiveShots{0};
std::atomic<unsigned long long> lastExplosiveShot{0};
bool Wanted(){return Spinbot::Requested()||CameraLockOn::Held();}
bool Allowed(){return !stop.load()&&!menuOpen.load()&&captureHotkey.load()<0&&ForegroundGame();}
void Clear(){std::lock_guard<std::mutex>g(mutex);published={};}
void Publish(const Snapshot&s){std::lock_guard<std::mutex>g(mutex);published=s;}
bool Identity(const EspEntity&e){
 V root{};if(!ValidateEspEntity(e,root))return false;
 if(e.humanType&&!AiAllegiance::HumanHostile(e.complete,gameBase,[](uintptr_t p,auto&v){return Read(p,v);}))return false;
 EspFrame f{};if(!LoadEsp(f))return false;
 for(unsigned i=0;i<f.count&&i<MaxEspEntities;++i)if(f.entities[i].state==e.state&&f.entities[i].complete==e.complete&&f.entities[i].generation==e.generation)return true;
 return false;
}
bool Copy(Snapshot&s){
 if(!Wanted()||!Allowed())return false;
 {std::lock_guard<std::mutex>g(mutex);s=published;}
 auto now=GetTickCount64();uintptr_t p=0;
 return s.valid&&s.options==AimTargeting::Options()&&now>=s.stamp&&now-s.stamp<=40&&TrueGodPlayer(p)&&p==s.player&&Identity(s.entity);
}
bool Visible(const Snapshot&s){
 if(s.options!=AimTargeting::Options()||!ray||!s.player||!Finite(s.camera)||!Finite(s.aim)||!Identity(s.entity))return false;
 float d=Distance(s.camera,s.aim);if(!std::isfinite(d)||d<.25f||d>NativeMaxDistance())return false;
                                                                               
 alignas(16)unsigned char collision[0x40]{};V end=s.aim;unsigned char result=0;++visibleChecks;
 __try{result=ray((void*)(s.player+0x28),0x37,collision,&s.camera,&end,0x11,(void*)(s.player+0x18),0,0,(int)0x80000000u);}
 __except(EXCEPTION_EXECUTE_HANDLER){return false;}
 uintptr_t hit=0;memcpy(&hit,collision+0x18,8);
 bool ok=s.options==AimTargeting::Options()&&result==2&&hit==s.entity.complete+0x18&&Finite(end)&&Identity(s.entity);
 if(!ok)++blocked;return ok;
}
                                                                                
                                                                               
unsigned char LockedRay(void*w,V*start,V*end,void*collision,void*ignore,char flags){
 Snapshot s{};pendingRedirectExplosion.valid=false;
 if(!start||!end||!collision||!Copy(s)||(uintptr_t)ignore!=s.player+0x18||!Visible(s))return original(w,start,end,collision,ignore,flags);
 V rayStart=*start,rayEnd=s.aim;float d=Distance(rayStart,rayEnd);
 if(!Finite(rayStart)||!std::isfinite(d)||d<.01f||d>NativeMaxDistance())return original(w,start,end,collision,ignore,flags);
 unsigned char result=original(w,&rayStart,&rayEnd,collision,ignore,flags);
                                                                                
 *end=rayEnd;uintptr_t hit=0;memcpy(&hit,(unsigned char*)collision+0x18,8);
 if(result==2&&hit==s.entity.complete+0x18){
  if(!Identity(s.entity))return 0;++shots;
  if(explosiveSilentAimEnabled.load()&&enabled.load()&&Spinbot::Running()){
   auto now=GetTickCount64();
   if(now-lastExplosiveShot.load()>=150){lastExplosiveShot=now;if(SpawnRedirectedRocket(s.player+0x28,s.player+0x18,s.aim,V{0,1,0}))++explosiveShots;}
  }
 }
 return result;
}
unsigned char StandaloneRay(void*w,V*start,V*end,void*collision,void*ignore,char flags){
 pendingRedirectExplosion.valid=false;uintptr_t p=0;
 if(!Allowed()||!start||!end||!collision||!TrueGodPlayer(p)||(uintptr_t)ignore!=p+0x18)return original(w,start,end,collision,ignore,flags);
 thread_local AimTargeting::State selection{};Snapshot s{};s.player=p;s.stamp=GetTickCount64();
 if(!AimTargeting::Select(p,0,selection,s.aim,s.entity)||!ReadCamera(s.camera)||GetTickCount64()-s.stamp>40||!Visible(s)||!Finite(*start)||Distance(*start,s.aim)>NativeMaxDistance())return original(w,start,end,collision,ignore,flags);
 uintptr_t ec=s.entity.complete+0x18;
 V origin=*start,endpoint=s.aim;unsigned char result=original(w,&origin,&endpoint,collision,ignore,flags);*end=endpoint;uintptr_t hit=0;memcpy(&hit,(unsigned char*)collision+0x18,8);
 if(result==2&&hit==ec&&Identity(s.entity)){
  ++redirects;if(explosiveSilentAimEnabled.load())pendingRedirectExplosion={endpoint,GetTickCount64(),true};
 }
 return result;
}
bool FirearmReady(){
 uintptr_t hud=0,manager=0,item=0,vt=0,desc=0;int offset=0,loaded=0,type=-1;
 if(!Pointer(gameBase+AmmoRootRva,hud)||!Pointer(hud+0x628,manager)||!Pointer(manager+0xff0,item)||!Read(item,vt)||!TypeContains(vt,"InventoryItem",offset)||offset!=0||!Read(item+0x50,loaded)||loaded<=0)return false;
 return Pointer(item+0x68,desc)&&Read(desc,vt)&&TypeContains(vt,"ItemDescFirearm",offset)&&offset==0&&Read(desc+0x598,type)&&type>=0&&type<=7;
}
void Install(){ray=(NativeRay)GetProcAddress((HMODULE)engineBase,"?Raytrace@IGSObject@@QEAAEEPEAUSCollision@@AEBVvec3@@AEAV3@GPEAVIControlObject@@IIH@Z");}
}
