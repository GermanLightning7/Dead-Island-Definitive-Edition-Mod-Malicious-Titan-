                                                                                 
                                                                                       
std::atomic_bool kickLaunchReady{false};
std::atomic<uintptr_t> grapplePullItem{0};
bool GrapplePullValid(uintptr_t item);
std::atomic<ULONGLONG> kickLaunchPending{0},kickLaunchUntil{0};
constexpr ULONGLONG kKickLaunchRequestMs=1000;
constexpr float kKickLaunchSpeed=30.f;
using KickCommandFn=void(*)(void*,const V*);
KickCommandFn nativeKickCommand=nullptr;
bool KickLaunchVector(V v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&fabsf(v.x)<=100000&&fabsf(v.y)<=100000&&fabsf(v.z)<=100000;}
bool KickLaunchAllowed(){auto item=grapplePullItem.load();return item&&GrapplePullValid(item)&&kickLaunchReady.load()&&!stop.load()&&!menuOpen.load()&&ForegroundGame()&&captureHotkey.load()<0;}
struct KickBurstState{uintptr_t player=0,physics=0,body=0;V target{},origin{};unsigned ticks=0;ULONGLONG lastTick=0;};
KickBurstState kickBurst;                                                          
void EndKickBurst(const char* reason,uintptr_t freshBody=0){
 if(kickBurst.ticks){
  V position{};bool valid=freshBody&&freshBody==kickBurst.body&&Read(freshBody+0xBC,position)&&KickLaunchVector(position);
  std::ofstream f(LogPath(),std::ios::app);
  f<<GetTickCount64()<<",KICK_COMMAND_END,reason="<<reason<<",ticks="<<kickBurst.ticks
   <<",position_valid="<<valid<<",origin="<<kickBurst.origin.x<<':'<<kickBurst.origin.y<<':'<<kickBurst.origin.z
   <<",position="<<position.x<<':'<<position.y<<':'<<position.z<<'\n';
 }
 kickLaunchUntil=0;grapplePullItem=0;kickBurst={};
}
bool KickCommandOwner(void* who,uintptr_t& p,uintptr_t& body){
 uintptr_t model=0,link=0,component=0,vehicle=0,owner=0;unsigned char active=0;int fail=0;
 if(!TrueGodPlayer(p)||!Read(p+0x830,vehicle))return false;
 if(vehicle&&(!Read(vehicle+0x58,owner)||!Read(vehicle+0x50,active)||(owner==p&&active)))return false;
 return ResolveCharacterBody(p,body,fail)&&Read(p+0x20,model)&&Read(model+0x258,link)&&
  Read(link+0x28,component)&&component+0x6F0==(uintptr_t)who;
}
using KickRayFn=unsigned char(*)(void*,unsigned char,void*,const V*,V*,unsigned short,void*,unsigned,unsigned,int);
KickRayFn kickRay=nullptr;
KickCommandFn kickSetVelocity=nullptr;
                                                                                   
bool CaptureKickTarget(uintptr_t player,V bodyPosition,V& target){
 V camera{},right{},up{},back{};float sx=0,sy=0;
 if(!kickRay||!ReadD3DCamera(camera,right,up,back,sx,sy)||!KickLaunchVector(camera)||!KickLaunchVector(back))return false;
 float length=sqrtf(back.x*back.x+back.y*back.y+back.z*back.z);
 if(length<.5f||length>1.5f)return false;
 V direction{-back.x/length,-back.y/length,-back.z/length};
                                                                                
 V end{camera.x+direction.x*10000.f,camera.y+direction.y*10000.f,camera.z+direction.z*10000.f};
 auto hit=kickRay((void*)(player+0x28),0x37,nullptr,&camera,&end,0x11,(void*)(player+0x18),0,0,(int)0x80000000u);
 if(!hit||!KickLaunchVector(end))return false;
 V delta{end.x-camera.x,end.y-camera.y,end.z-camera.z};
 float along=delta.x*direction.x+delta.y*direction.y+delta.z*direction.z;
 V off{delta.x-direction.x*along,delta.y-direction.y*along,delta.z-direction.z*along};
 if(along<.1f||along>=9999.f||off.x*off.x+off.y*off.y+off.z*off.z>.25f)return false;
                                                                            
 target={end.x+bodyPosition.x-camera.x,end.y+bodyPosition.y-camera.y,end.z+bodyPosition.z-camera.z};
 return KickLaunchVector(target);
}
bool KickPullStep(V position,V target,float step,V& next){
 float distance=Distance(position,target);
 if(distance<=step||distance<.05f){next=target;return true;}
 float scale=step/distance;
 next={position.x+(target.x-position.x)*scale,position.y+(target.y-position.y)*scale,position.z+(target.z-position.z)*scale};
 return false;
}
                                                                                      
void SubmitKickCommand(void* who,const V* original,uintptr_t caller){
 if(caller!=gameBase+0xA1BDDB){nativeKickCommand(who,original);return;}
 auto now=GetTickCount64();uintptr_t p=0,body=0;
 if(!KickLaunchAllowed()){
  if(kickBurst.ticks)EndKickBurst("cancelled");
  nativeKickCommand(who,original);return;
 }
 if(!KickCommandOwner(who,p,body)){nativeKickCommand(who,original);return;}
 if(kickLaunchUntil.load()&&(kickBurst.player!=p||kickBurst.body!=body||kickBurst.physics!=(uintptr_t)who||now>=kickLaunchUntil.load()))EndKickBurst("expired_or_identity_changed",body);
 auto pending=kickLaunchPending.load();
 if(pending&&now>=pending&&now-pending<=kKickLaunchRequestMs&&!kickLaunchUntil.load()){
  V position{},target{};
  if(Read(body+0xBC,position)&&KickLaunchVector(position)&&kickLaunchPending.compare_exchange_strong(pending,0)){
   if(!CaptureKickTarget(p,position,target)){
    Log("KICK_PULL_NO_CROSSHAIR_SURFACE");grapplePullItem=0;nativeKickCommand(who,original);return;
   }
   float distance=Distance(position,target);
   if(!std::isfinite(distance)||distance<.1f){grapplePullItem=0;nativeKickCommand(who,original);return;}
   kickBurst={p,(uintptr_t)who,body,target,position,0,now};
                                                                                   
   kickLaunchUntil=now+(ULONGLONG)(distance/CurrentGrappleSpeed()*2000.f)+2000;
   std::ofstream f(LogPath(),std::ios::app);f<<now<<",KICK_PULL_START,distance="<<distance
    <<",target="<<target.x<<':'<<target.y<<':'<<target.z<<'\n';
  }
 }
 if(kickLaunchUntil.load()&&now<kickLaunchUntil.load()&&kickBurst.player==p&&kickBurst.body==body){
                                                                                          
                                                                                             
                                                                                             
                                                                                          
                                                                                            
                                       
  V neutral{};nativeKickCommand(who,&neutral);
 }else nativeKickCommand(who,original);
}
void ModifiedKickCommand(void* who,const V* velocity){SubmitKickCommand(who,velocity,(uintptr_t)_ReturnAddress());}
                                                                                      
                                                                                                
                                                                                   
void PumpKickLaunchInput(void* who){
 ReleaseUpgradedGrapple(who);
 if(!kickLaunchUntil.load())return;
 uintptr_t player=0;
 if(!TrueGodPlayer(player)||!LocalMove(who)){EndKickBurst("invalid_position");return;}
 auto now=GetTickCount64();
 if(kickBurst.player!=player||now>=kickLaunchUntil.load()){EndKickBurst("expired_or_identity_changed");return;}
 uintptr_t body=0;int failStep=0;
 if(!ResolveCharacterBody(player,body,failStep)||body!=kickBurst.body){EndKickBurst("invalid_position",body);return;}
 V position{};
 if(!Read(body+0xBC,position)||!KickLaunchVector(position)){EndKickBurst("invalid_position",body);return;}
 auto elapsed=now>=kickBurst.lastTick?now-kickBurst.lastTick:0;kickBurst.lastTick=now;
 if(elapsed>250){EndKickBurst("frame_stall",body);return;}
 bool arrived=Distance(position,kickBurst.target)<.35f;
 V neutral{};
 if(elapsed||arrived){
  V velocity=arrived?V{}:MotionVelocity(position,kickBurst.target,elapsed*.001f,CurrentGrappleSpeed());kickSetVelocity((void*)body,&velocity);++kickBurst.ticks;
 }
 if(arrived)EndKickBurst("arrived",body);
}
void InstallKickLaunch(){
 kickLaunchReady=false;kickLaunchPending=0;kickLaunchUntil=0;
 if(!physicalEngine||!TrueGodBuildMatches())return;
                                                                                    
 const unsigned char expected[16]={0x80,0xB9,0xE8,0,0,0,0,0x75,0x1A,0x8B,0x02,0x89,0x81,0x70,0x03,0};
 unsigned char actual[16]{};auto target=(void*)(physicalEngine+0x3E5440);
 if(!ReadBytes((uintptr_t)target,actual,16)||memcmp(actual,expected,16))return;
 if(MH_CreateHook(target,(void*)ModifiedKickCommand,(void**)&nativeKickCommand)!=MH_OK)return;
 if(MH_EnableHook(target)!=MH_OK){MH_RemoveHook(target);return;}
 kickRay=(KickRayFn)(physicalEngine+0x24E770);
 kickSetVelocity=(KickCommandFn)(physicalEngine+0x300FC0);
 kickLaunchReady=true;Log("KICK_PULL_READY_DISABLED_NATIVE_3D_TARGET");
}
void PollKickLaunchInput(){
 if(!KickLaunchAllowed()){kickLaunchPending=0;kickLaunchUntil=0;grapplePullItem=0;}
 auto pending=kickLaunchPending.load();
 if(pending&&GetTickCount64()-pending>kKickLaunchRequestMs&&kickLaunchPending.compare_exchange_strong(pending,0))Log("KICK_COMMAND_EXPIRED_NO_VALID_NATIVE_SETTER");
}
