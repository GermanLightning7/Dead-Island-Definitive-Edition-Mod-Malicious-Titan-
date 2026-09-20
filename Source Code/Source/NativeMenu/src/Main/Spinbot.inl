                                                                           
                                                                        
namespace Spinbot {
std::atomic_bool enabled{false},active{false},owns{false};std::atomic_int speed{3},fault{0};
std::atomic<unsigned long long> applied{0};
using SetWorld=void(*)(void*,const M34*);SetWorld setWorld=nullptr;
using GetWorld=const M34*(*)(void*);GetWorld getWorld=nullptr;
using ModelCameraFn=unsigned char(*)(void*);ModelCameraFn modelCamera=nullptr;
uintptr_t bodyPlayer=0,modelOwner=0,modelDescriptor=0;M34 baseline{},lastApplied{};ULONGLONG previous=0;double phase=0;
bool Requested(){return enabled.load();}
bool Running(){return enabled.load()&&active.load()&&!fault.load();}
bool ValidBasis(const M34&m){
 for(float x:m.v)if(!std::isfinite(x)||fabsf(x)>100000.f)return false;
 for(int c=0;c<3;++c){float n=0;for(int r=0;r<3;++r)n+=m.v[r*4+c]*m.v[r*4+c];if(fabsf(n-1.f)>.15f)return false;}
 for(int a=0;a<3;++a)for(int b=a+1;b<3;++b){float n=0;for(int r=0;r<3;++r)n+=m.v[r*4+a]*m.v[r*4+b];if(fabsf(n)>.05f)return false;}
 return true;
}
bool SameBasis(const M34&a,const M34&b){for(int i=0;i<12;++i)if(fabsf(a.v[i]-b.v[i])>.0001f)return false;return true;}
M34 Rotated(M34 m,double yaw){
 float c=(float)cos(yaw),s=(float)sin(yaw);
 for(int col=0;col<3;++col){float x=m.v[col],z=m.v[8+col];m.v[col]=c*x+s*z;m.v[8+col]=-s*x+c*z;}
 return m;
}
void Reset(){previous=0;phase=0;}                         
void Stop(){enabled=false;CombatLock::Clear();}                                          
void Toggle(){
 if(enabled.load()){Stop();SetStatus("SPINBOT OFF");return;}
 uintptr_t p=0;if(!setWorld||!getWorld||!TrueGodPlayer(p)||!CameraFeatures::ready){SetStatus("SPINBOT NOT READY");return;}
 if(fault){SetStatus("SPINBOT FAULT - RESTART GAME");return;}
 enabled=true;SetStatus("BODY SPIN + VISIBLE TARGET AUTO FIRE");
}
void ChangeSpeed(int delta){speed=std::clamp(speed.load()+delta,1,10);SetStatus("BODY SPIN SPEED UPDATED");}
bool PlayerIdentity(uintptr_t p){
 uintptr_t current=0;if(!TrueGodPlayer(current)||current!=p||CameraFeatures::Q(p+0x268)!=CameraFeatures::heldVis)return false;
 return CameraFeatures::Q(CameraFeatures::Q(p)+0x150)==gameBase+0x834910&&CameraFeatures::Q(CameraFeatures::Q(p)+0x460)==gameBase+0x664820;
}
bool PlayerReady(uintptr_t p){
 if(!PlayerIdentity(p)||!modelCamera||modelCamera((void*)p))return false;unsigned char flags=0;if(!Read(p+0xaf8,flags)||(flags&4))return false;
 uintptr_t vehicle=0,owner=0;unsigned char on=0;
 if(!Read(p+0x830,vehicle))return false;
 if(vehicle&&(!Read(vehicle+0x58,owner)||!Read(vehicle+0x50,on)||(owner==p&&on)))return false;
 return CameraFeatures::Q(CameraFeatures::Q(p)+0x150)==gameBase+0x834910&&CameraFeatures::Q(CameraFeatures::Q(p)+0x460)==gameBase+0x664820;
}
bool World(uintptr_t p,M34&value){
 uintptr_t owner=CameraFeatures::Q(p+0x20),descriptor=CameraFeatures::Q(owner+0xe8);
 if(!getWorld||!setWorld||owner<0x10000||descriptor<0x10000)return false;
 if(bodyPlayer==p&&(modelOwner!=owner||modelDescriptor!=descriptor))return false;
 const M34* native=getWorld((void*)(p+0x18));
 return native&&ReadBytes((uintptr_t)native,&value,sizeof(value))&&ValidBasis(value)&&CameraFeatures::Q(p+0x20)==owner&&CameraFeatures::Q(owner+0xe8)==descriptor;
}
void Restore(){
 if(owns&&bodyPlayer&&PlayerIdentity(bodyPlayer)){
  M34 actual{};if(World(bodyPlayer,actual)&&SameBasis(actual,lastApplied))setWorld((void*)(bodyPlayer+0x18),&baseline);
 }
 owns=false;bodyPlayer=0;modelOwner=0;modelDescriptor=0;active=false;Reset();
}
void Update(uintptr_t m){
 uintptr_t p=0,t=0,f=0,v=0;int mode=CameraFeatures::I(m+0x68);
 bool allowed=enabled.load()&&!fault.load()&&CombatLock::Allowed()&&CameraFeatures::Resolve(m,p,t,f,v)&&m==CameraFeatures::heldManager&&p==CameraFeatures::heldPlayer&&v==CameraFeatures::heldVis&&((mode==1&&CameraFeatures::Q(m+0x60)==t)||(mode==2&&CameraFeatures::Q(m+0x60)==f))&&CameraFeatures::Q(CameraFeatures::Q(m+0x60)+0x60)==v+0xec0&&PlayerReady(p);
                                                                                   
 if(!allowed){Restore();return;}
 M34 actual{};float angles[2]{};V position{};
 if(!World(p,actual)||!ReadBytes(p+0xc7c,angles,sizeof(angles))||!std::isfinite(angles[0])||!std::isfinite(angles[1])||!Read(p+0x578,position)||!Finite(position)){enabled=false;fault=1;Restore();return;}
 auto now=GetTickCount64();
 if(p!=bodyPlayer){Restore();bodyPlayer=p;modelOwner=CameraFeatures::Q(p+0x20);modelDescriptor=CameraFeatures::Q(modelOwner+0xe8);baseline=actual;previous=now;phase=0;}
 else if(!owns||!SameBasis(actual,lastApplied))baseline=actual;                                                    
 auto elapsed=now>=previous?now-previous:0;previous=now;
 if(elapsed>50){Restore();return;}
 phase=fmod(phase+(double)elapsed*.001*std::clamp(speed.load(),1,10)*6.283185307179586,6.283185307179586);
 M34 changed=Rotated(baseline,phase);setWorld((void*)(p+0x18),&changed);lastApplied=changed;owns=true;
 float afterAngles[2]{};V afterPosition{};M34 after{};
 if(!PlayerReady(p)||!ReadBytes(p+0xc7c,afterAngles,sizeof(afterAngles))||memcmp(angles,afterAngles,sizeof(angles))||!Read(p+0x578,afterPosition)||Distance(position,afterPosition)>.001f||!World(p,after)||!SameBasis(after,changed)){enabled=false;fault=2;Restore();return;}
 active=true;++applied;
}
bool Facing(M34&model,V target){
 V delta{target.x-model.v[3],0,target.z-model.v[11]};float horizontal=std::sqrt(delta.x*delta.x+delta.z*delta.z);
 if(!Finite(target)||!std::isfinite(horizontal)||horizontal<.05f)return false;
 double yaw=std::atan2(delta.x,delta.z)-std::atan2(model.v[2],model.v[10]);model=Rotated(model,yaw);return ValidBasis(model);
}
bool FaceTarget(const CombatLock::Snapshot&shot){
 if(!Running()||!owns||shot.player!=bodyPlayer||shot.options!=AimTargeting::Options()||!CombatLock::Identity(shot.entity))return false;
 M34 before{};float angles[2]{};V position{};
 if(!PlayerReady(bodyPlayer)||!World(bodyPlayer,before)||!ReadBytes(bodyPlayer+0xc7c,angles,sizeof(angles))||!Read(bodyPlayer+0x578,position))return false;
 M34 aimed=before;if(!Facing(aimed,shot.aim))return false;
 setWorld((void*)(bodyPlayer+0x18),&aimed);lastApplied=aimed;
 M34 after{};float afterAngles[2]{};V afterPosition{};
 if(!World(bodyPlayer,after)||!SameBasis(after,aimed)||!ReadBytes(bodyPlayer+0xc7c,afterAngles,sizeof(afterAngles))||memcmp(angles,afterAngles,sizeof(angles))||!Read(bodyPlayer+0x578,afterPosition)||Distance(position,afterPosition)>.001f){fault=2;Stop();Restore();return false;}
 return true;
}
void SafeUpdate(uintptr_t m){__try{Update(m);}__except(EXCEPTION_EXECUTE_HANDLER){enabled=false;active=false;fault=3;CombatLock::Clear();}}
bool CanStop(){Stop();return !owns.load();}
void Install(){if(CameraFeatures::ready){setWorld=(SetWorld)GetProcAddress((HMODULE)engineBase,"?SetWorldXform@IControlObject@@QEAAXAEBVmtx34@@@Z");getWorld=(GetWorld)GetProcAddress((HMODULE)engineBase,"?GetWorldXform@IControlObject@@QEBAAEBVmtx34@@XZ");modelCamera=(ModelCameraFn)(gameBase+0x69ac40);}}
void Diagnostic(){static ULONGLONG next=0;auto now=GetTickCount64();if(now<next)return;next=now+1000;char line[256];sprintf_s(line,"BODY_SPIN_DIAG,path=MODEL_WORLD,enabled=%d,active=%d,fault=%d,owns=%d,applied=%llu,visible_checks=%llu,blocked=%llu,shots=%llu",(int)enabled.load(),(int)active.load(),fault.load(),(int)owns.load(),applied.load(),CombatLock::visibleChecks.load(),CombatLock::blocked.load(),CombatLock::shots.load());Log(line);}
}
namespace AutoFire {
std::mutex mutex;bool down=false;ULONGLONG started=0;uintptr_t target=0;int generation=0;
bool CanStop(){std::lock_guard<std::mutex>g(mutex);return !down;}
bool Desired(){
 CombatLock::Snapshot s{};if(!Spinbot::Running()||!CombatLock::Copy(s)||!CombatLock::FirearmReady()){started=0;target=0;generation=0;return false;}
 auto now=GetTickCount64();if(target!=s.entity.state||generation!=s.entity.generation||now<started){target=s.entity.state;generation=s.entity.generation;started=now;}
                                                                                 
                                                                                 
 return (now-started)%80<25;
}
bool AimWindow(){std::lock_guard<std::mutex>g(mutex);return Desired();}
void State(void*raw,bool capture){if(!raw)return;std::lock_guard<std::mutex>g(mutex);bool desired=!capture&&Desired();auto*s=(DIMOUSESTATE*)raw;if(desired)s->rgbButtons[0]|=0x80;}
void Events(void*raw,DWORD*count,DWORD capacity,bool capture){
 if(!raw||!count||*count>capacity)return;std::lock_guard<std::mutex>g(mutex);bool desired=!capture&&Desired();auto*existing=(DIDEVICEOBJECTDATA*)raw;if(desired)for(DWORD i=0;i<*count;++i)if(existing[i].dwOfs==DIMOFS_BUTTON0)existing[i].dwData|=0x80;if(desired==down)return;
 if(*count==capacity)return;auto*d=(DIDEVICEOBJECTDATA*)raw;DWORD n=*count;bool physical=!capture&&(GetAsyncKeyState(VK_LBUTTON)&0x8000);
 d[n]={};d[n].dwOfs=DIMOFS_BUTTON0;d[n].dwData=(desired||physical)?0x80:0;d[n].dwTimeStamp=(DWORD)GetTickCount64();d[n].dwSequence=n?d[n-1].dwSequence+1:1;*count=n+1;down=desired;
}
}
