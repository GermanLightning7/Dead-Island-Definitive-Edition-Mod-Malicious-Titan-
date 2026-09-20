namespace Spinbot {bool FaceTarget(const CombatLock::Snapshot& shot);}
namespace AutoFire {bool AimWindow();}
                                                                                
                                                                                      
namespace CameraLockOn {
std::atomic_bool active{false},haveError{false};std::atomic_int targetSource{0},fault{0};
std::atomic_int smoothing{1};
std::atomic<unsigned long long> ticks{0},appliedTicks{0},staleDrops{0},poseMisses{0},lockChanges{0};
std::atomic<float>yawError{0},pitchError{0},rawR{0},rawF{0},rawU{0};std::atomic<long>lastDx{0},lastDy{0};
constexpr float DEADZONE_RAD=.002f;
struct AxisResponse {
 float radiansPerCount=0;
 void Observe(float angle,LONG counts){
  if(std::abs(counts)<2||!std::isfinite(angle))return;
  if(std::fabs(angle)>.3f){radiansPerCount=0;return;}                                                                   
  if(std::fabs(angle)<.00005f)return;
  float value=angle/(float)counts;
  if(value<.00001f||value>.1f)return;
                                                                              
                                                                             
  radiansPerCount=radiansPerCount==0||value>radiansPerCount?value:.75f*radiansPerCount+.25f*value;
 }
 LONG Command(float error)const {
  if(!std::isfinite(error)||std::fabs(error)<=DEADZONE_RAD)return 0;
  float factor=(float)std::clamp(smoothing.load(),1,10);
  float gain=(radiansPerCount>0?.7f/radiansPerCount:64.f)/factor;
  float cap=radiansPerCount>0?std::min(64.f,.12f/radiansPerCount):4.f;
  cap=std::max(cap/factor,1.f);
  float value=std::clamp(error*gain,-cap,cap);
  return (LONG)(value>=0?std::floor(value+.5f):std::ceil(value-.5f));
 }
};
struct Sample {bool valid=false;ULONGLONG stamp=0;uintptr_t player=0,target=0;int generation=0,mode=0;V pos{},forward{},right{},up{},aim{};float yaw=0,pitch=0;int options=AimTargeting::Options();};
struct Sent {bool valid=false,physical=false;Sample sample{};LONG dx=0,dy=0;ULONGLONG stamp=0;};
std::mutex sampleMutex;Sample pending{},diagnosticSample{};Sent sent{};AxisResponse yawResponse[3]{},pitchResponse[3]{};
EspEntity locked{};bool hasLock=false;int lockSource=-1;uintptr_t lockPlayer=0;
bool InputAllowed(){return fault.load()!=5&&!stop.load()&&!menuOpen.load()&&ForegroundGame()&&captureHotkey.load()<0;}
bool Held(){int key=FeatureBindings::keys[70].load();return ((key&&(GetAsyncKeyState(key)&0x8000))||Spinbot::Running())&&InputAllowed();}
float Dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
bool Normalize(V&v){float n=sqrtf(Dot(v,v));if(!Finite(v)||!std::isfinite(n)||n<.05f)return false;v={v.x/n,v.y/n,v.z/n};return true;}
bool Compute(Sample&s){
 if(!Finite(s.pos)||!Finite(s.aim)||!Normalize(s.forward)||!Normalize(s.right)||!Normalize(s.up))return false;
 if(std::fabs(Dot(s.forward,s.right))>.05f||std::fabs(Dot(s.forward,s.up))>.05f||std::fabs(Dot(s.right,s.up))>.05f)return false;
 V delta{s.aim.x-s.pos.x,s.aim.y-s.pos.y,s.aim.z-s.pos.z};float distance=sqrtf(Dot(delta,delta));
 if(!std::isfinite(distance)||distance<.25f||distance>2000.f)return false;
 float r=Dot(delta,s.right),f=Dot(delta,s.forward),u=Dot(delta,s.up),h=sqrtf(f*f+r*r);
 if(h<.05f)return false;                                            
 s.yaw=atan2f(r,f);s.pitch=atan2f(u,h);return std::isfinite(s.yaw)&&std::isfinite(s.pitch);
}
void Clear(){CombatLock::Clear();std::lock_guard<std::mutex>g(sampleMutex);pending.valid=false;sent.valid=false;haveError=false;}
bool Same(const EspEntity&a,const EspEntity&b){return a.state==b.state&&a.complete==b.complete&&a.generation==b.generation;}
bool AimPoint(uintptr_t player,V&point){
 static AimTargeting::State selection{};
 if(!hasLock)selection.hasLock=false;
 EspEntity chosen{};bool selected=AimTargeting::Select(player,targetSource.load(),selection,point,chosen);
 if(selected&&(!hasLock||!Same(locked,chosen)))++lockChanges;
 hasLock=selected;if(selected)locked=chosen;poseMisses=AimTargeting::poseMisses.load();return selected;
}

void Publish(Sample s){
 std::lock_guard<std::mutex>g(sampleMutex);
 if(sent.valid){
  auto old=sent;sent.valid=false;
  if(!old.physical&&s.player==old.sample.player&&s.mode==old.sample.mode&&s.stamp>=old.stamp&&s.stamp-old.stamp<=40){
   float r=Dot(s.forward,old.sample.right),f=Dot(s.forward,old.sample.forward),u=Dot(s.forward,old.sample.up);
   yawResponse[s.mode].Observe(atan2f(r,f),old.dx);
   pitchResponse[s.mode].Observe(atan2f(u,sqrtf(r*r+f*f)),-old.dy);
  }
 }
 s.valid=true;pending=s;diagnosticSample=s;haveError=true;yawError=s.yaw;pitchError=s.pitch;
 V d{s.aim.x-s.pos.x,s.aim.y-s.pos.y,s.aim.z-s.pos.z};Normalize(d);rawR=Dot(d,s.right);rawF=Dot(d,s.forward);rawU=Dot(d,s.up);
}
void Update(uintptr_t m){
 ++ticks;if(!active.load()||!Held()){hasLock=false;Clear();return;}
 uintptr_t p=0,t=0,f=0,v=0;if(!CameraFeatures::Resolve(m,p,t,f,v)||m!=CameraFeatures::heldManager||p!=CameraFeatures::heldPlayer||v!=CameraFeatures::heldVis){hasLock=false;Clear();return;}
 int mode=CameraFeatures::I(m+0x68);uintptr_t camera=CameraFeatures::Q(m+0x60);
 if(((mode!=1||camera!=t)&&(mode!=2||camera!=f))||CameraFeatures::Q(camera+0x60)!=v+0xec0){Clear();return;}
 uintptr_t owner=CameraFeatures::Q(camera+8);if(CameraFeatures::Q(owner)!=engineBase+0x845f68){Clear();return;}
 Sample s{};s.mode=mode;s.player=p;s.stamp=GetTickCount64();
 unsigned char bytes[0x180]{};if(!ReadBytes(owner,bytes,sizeof(bytes))){fault=1;Clear();return;}
 auto F=[&](int offset){float x=0;memcpy(&x,bytes+offset,4);return x;};
                                                                         
 s.pos={F(0x4C)+F(0x168),F(0x5C)+F(0x16C),F(0x6C)+F(0x170)};
 s.forward={-F(0x48),-F(0x58),-F(0x68)};s.right={F(0x40),F(0x50),F(0x60)};s.up={F(0x44),F(0x54),F(0x64)};
 const bool bodyAim=Spinbot::Running();
 if(!AimPoint(p,s.aim)||(!bodyAim&&!Compute(s))){Clear();return;}
 s.target=locked.state;s.generation=locked.generation;
 CombatLock::Snapshot shot{true,locked,p,s.pos,s.aim,s.stamp};shot.options=s.options;
 if(!CombatLock::Visible(shot)){hasLock=false;Clear();return;}
 if(bodyAim){
                                                                                   
  std::lock_guard<std::mutex>g(sampleMutex);pending.valid=false;sent.valid=false;haveError=false;diagnosticSample=s;
 }else Publish(s);
                                                                                
 if(GetTickCount64()-s.stamp>40){Clear();return;}CombatLock::Publish(shot);
 if(bodyAim&&AutoFire::AimWindow()&&!Spinbot::FaceTarget(shot))Clear();
}
void SafeUpdate(uintptr_t m){__try{Update(m);}__except(EXCEPTION_EXECUTE_HANDLER){fault=5;active=false;Clear();}}
bool Take(LONG&dx,LONG&dy,bool physical,DWORD space){
 if(Spinbot::Running())return false;
 if(!active.load()||!Held()){Clear();return false;}
 std::lock_guard<std::mutex>g(sampleMutex);if(physical)sent.valid=false;if(!pending.valid)return false;
 auto now=GetTickCount64();if(pending.options!=AimTargeting::Options()||now<pending.stamp||now-pending.stamp>40){pending.valid=false;haveError=false;++staleDrops;return false;}
 dx=yawResponse[pending.mode].Command(pending.yaw);dy=-pitchResponse[pending.mode].Command(pending.pitch);
 DWORD needed=(dx!=0)+(dy!=0);if(needed>space)return false;
 auto used=pending;pending.valid=false;haveError=false;if(!needed)return false;
 sent={true,physical,used,dx,dy,now};lastDx=dx;lastDy=dy;++appliedTicks;return true;
}
void InjectMouseState(void*raw){if(!raw)return;auto*s=(DIMOUSESTATE*)raw;LONG dx=0,dy=0;if(Take(dx,dy,s->lX!=0||s->lY!=0,2)){s->lX+=dx;s->lY+=dy;}}
void InjectBufferedMouseData(void*raw,DWORD*count,DWORD capacity){
 if(!raw||!count||*count>capacity)return;auto*data=(DIDEVICEOBJECTDATA*)raw;bool physical=false;
 for(DWORD i=0;i<*count;++i)if((data[i].dwOfs==DIMOFS_X||data[i].dwOfs==DIMOFS_Y)&&(LONG)data[i].dwData!=0)physical=true;
 LONG dx=0,dy=0;if(!Take(dx,dy,physical,capacity-*count))return;DWORD n=*count;auto stamp=(DWORD)GetTickCount64();
 auto add=[&](DWORD axis,LONG value){if(!value)return;data[n]={};data[n].dwOfs=axis;data[n].dwData=(DWORD)value;data[n].dwTimeStamp=stamp;data[n].dwSequence=n?data[n-1].dwSequence+1:1;++n;};
 add(DIMOFS_X,dx);add(DIMOFS_Y,dy);*count=n;
}
void Install(){Log("CAMERA_LOCK_ON_READY_DISABLED_FRESH_POSE_SINGLE_USE_BODY_ONLY_SPIN_AIM");}
void Diagnostic(){static ULONGLONG next=0;auto now=GetTickCount64();if(now<next)return;next=now+1000;std::lock_guard<std::mutex>g(sampleMutex);auto&s=diagnosticSample;char line[900];
 sprintf_s(line,"CAMERA_LOCK_ON_DIAG,active=%d,source=%d,priority=%d,body_part=%d,ticks=%llu,applied=%llu,fault=%d,haveError=%d,mode=%d,target=%p,generation=%d,sample_age=%llu,yaw_deg=%.2f,pitch_deg=%.2f,cam=%.3f/%.3f/%.3f,aim=%.3f/%.3f/%.3f,last_dx=%ld,last_dy=%ld,yaw_response=%.6f,pitch_response=%.6f,pose_misses=%llu,lock_changes=%llu,stale_drops=%llu",(int)active.load(),targetSource.load(),AimTargeting::priority.load(),AimTargeting::bodyPart.load(),ticks.load(),appliedTicks.load(),fault.load(),(int)haveError.load(),s.mode,(void*)s.target,s.generation,now-s.stamp,s.yaw*57.29578f,s.pitch*57.29578f,s.pos.x,s.pos.y,s.pos.z,s.aim.x,s.aim.y,s.aim.z,lastDx.load(),lastDy.load(),yawResponse[s.mode].radiansPerCount,pitchResponse[s.mode].radiansPerCount,poseMisses.load(),lockChanges.load(),staleDrops.load());Log(line);
}
}
void PollCameraLockOnInput(){CameraLockOn::active=CameraLockOn::Held();}

