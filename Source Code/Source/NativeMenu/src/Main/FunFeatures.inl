                                                                              
                                                                                   
bool FunInputAllowed(){return !stop.load()&&!menuOpen.load()&&ForegroundGame()&&captureHotkey.load()<0;}
V FunScaled(V v,float s){return {v.x*s,v.y*s,v.z*s};}
V FunAdd(V a,V b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
V FunSubtract(V a,V b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
bool FunDirection(V&v){float length=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);if(!std::isfinite(length)||length<.01f)return false;v=FunScaled(v,1.f/length);return true;}
bool FunCameraForward(V&direction){V camera{},right{},up{},back{};float sx=0,sy=0;if(!ReadD3DCamera(camera,right,up,back,sx,sy))return false;direction=FunScaled(back,-1.f);return FunDirection(direction);}
bool FunFreshEntity(const EspEntity&e,V&position){uintptr_t root=0;return !e.humanType&&ValidateEspEntity(e,position)&&Read(e.state+0x58,root)&&root==e.complete;}
                                                                              
                                                                           
bool FunZombieBody(const EspEntity&e,uintptr_t&body){
 V position{};uintptr_t vt=0,model=0,link=0,component=0,owner=0;int offset=-1,fail=0;
 return FunFreshEntity(e,position)&&Read(e.complete,vt)&&TypeContains(vt,"ZombieAI",offset)&&offset==0&&
  ResolveCharacterBody(e.complete,body,fail)&&Read(e.complete+0x20,model)&&Read(model+0x258,link)&&Read(link+0x28,component)&&
  Read(body+0x18,owner)&&owner==component;
}
struct FunMotion{EspEntity entity{};uintptr_t player=0,body=0;V target{},velocity{};ULONGLONG until=0;bool pull=false;ULONGLONG epoch=0;};
SRWLOCK funMotionLock=SRWLOCK_INIT;FunMotion funMotions[8]{};
void QueueFunMotion(const EspEntity&e,uintptr_t player,V target,V velocity,bool pull){
 const auto epoch=funResetEpoch.load();
 uintptr_t body=0;if(!FunZombieBody(e,body)){static unsigned refusals=0;if(++refusals<=5)Log("FUN_MOTION_REFUSED_UNKNOWN_AI_BODY");return;}
 AcquireSRWLockExclusive(&funMotionLock);auto now=GetTickCount64();
 if(epoch==funResetEpoch.load()){
                                                                
  FunMotion* destination=nullptr;
  for(auto&slot:funMotions)if(slot.until&&slot.entity.complete==e.complete){if(!destination)destination=&slot;else slot={};}
  if(!destination)for(auto&slot:funMotions)if(slot.until<=now||slot.epoch!=epoch){destination=&slot;break;}
  if(destination)*destination={e,player,body,target,velocity,now+(pull?700:900),pull,epoch};
 }
 ReleaseSRWLockExclusive(&funMotionLock);
}
struct FunChain{uintptr_t player=0;V center{};EspEntity candidates[MaxEspEntities]{};unsigned count=0,hops=0;uintptr_t visited[8]{};ULONGLONG next=0,until=0,epoch=0;};
SRWLOCK funChainLock=SRWLOCK_INIT;FunChain funChain{};thread_local bool funChainSpawning=false;
void NotifyFunExplosion(uintptr_t owner,V position){
 if(!chainExplosionsOn.load()||funChainSpawning||!Finite(position))return;
 uintptr_t player=0;if(!TrueGodPlayer(player)||owner!=player+0x18)return;
 EspFrame frame{};if(!LoadEsp(frame))return;
 AcquireSRWLockExclusive(&funChainLock);auto now=GetTickCount64();
 if(funChain.until<=now||funChain.epoch!=funResetEpoch.load()){funChain={};funChain.epoch=funResetEpoch.load();funChain.player=player;funChain.center=position;funChain.next=now+100;funChain.until=now+2500;
  for(unsigned i=0;i<frame.count&&funChain.count<MaxEspEntities;++i){V p{};if(FunFreshEntity(frame.entities[i],p)&&Distance(p,position)>1.5f)funChain.candidates[funChain.count++]=frame.entities[i];}}
 ReleaseSRWLockExclusive(&funChainLock);
}
void PumpFunChain(uintptr_t player){
 FunChain chain{};auto now=GetTickCount64();AcquireSRWLockExclusive(&funChainLock);
 if(!chainExplosionsOn.load()||funChain.epoch!=funResetEpoch.load()||funChain.player!=player||funChain.until<=now||funChain.hops>=8){funChain={};ReleaseSRWLockExclusive(&funChainLock);return;}
 if(now<funChain.next){ReleaseSRWLockExclusive(&funChainLock);return;}chain=funChain;funChain.next=now+120;ReleaseSRWLockExclusive(&funChainLock);
 int best=-1;float nearest=10.f;V chosen{};
 for(unsigned i=0;i<chain.count;i++){const auto&e=chain.candidates[i];bool used=false;for(unsigned j=0;j<chain.hops;j++)if(chain.visited[j]==e.complete)used=true;
  V p{};if(used||!FunFreshEntity(e,p))continue;float distance=Distance(p,chain.center);if(distance<nearest){nearest=distance;best=(int)i;chosen=p;}}
 AcquireSRWLockExclusive(&funChainLock);
                                                                        
 if(funChain.epoch!=chain.epoch||chain.epoch!=funResetEpoch.load()||funChain.until!=chain.until||funChain.player!=chain.player||funChain.hops!=chain.hops||funChain.hops>=8){ReleaseSRWLockExclusive(&funChainLock);return;}
 if(best<0){funChain={};ReleaseSRWLockExclusive(&funChainLock);return;}
 funChain.visited[funChain.hops++]=chain.candidates[best].complete;funChain.center=chosen;ReleaseSRWLockExclusive(&funChainLock);
 chosen.y+=.8f;funChainSpawning=true;bool ok=SpawnRedirectedRocket(player+0x28,player+0x18,chosen,V{0,1,0});funChainSpawning=false;
 if(!ok){chainExplosionsOn=false;SetStatus("CHAIN EXPLOSION FAILED - DISABLED");}else Log("FUN_CHAIN_HOP");
}
FunMeleeResult BeforeFunMelee(uintptr_t caller,uintptr_t attacker,uintptr_t victim,uintptr_t info){
 FunMeleeResult result{};
 if((!homerunKicksOn.load()&&!gravityPunchOn.load()&&!chaosHitsOn.load())||meleeExplosionActive||!IsMeleeDamageCaller(caller))return result;
 uintptr_t player=0,vt=0;int offset=-1,type=-1;float amount=0;
 if(!TrueGodPlayer(player)||player!=attacker||!Read(victim,vt)||!TypeContains(vt,"ZombieAI",offset)||offset!=0||
  !Read(info+0x18,amount)||!std::isfinite(amount)||amount<=0||!Read(info+0x3C,type)||!Read(info+0x20,result.position)||!Finite(result.position))return result;
 static thread_local MeleeExplosionStamp stamps[16]{};static thread_local unsigned cursor=0;auto now=GetTickCount64();
 for(const auto&s:stamps)if(s.player==player&&s.victim==victim&&now>=s.tick&&now-s.tick<150)return result;
 stamps[cursor++%16]={player,victim,now};result.valid=true;result.player=player;result.victim=victim;
                                                                
 result.launch=homerunKicksOn.load()&&type==35;result.pull=gravityPunchOn.load();
 if(chaosHitsOn.load()){
  static thread_local unsigned rng=0x51A7F03Du;rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;
  unsigned choice=rng%3;if(choice==0)result.explode=true;else if(choice==1)result.launch=true;else{
   const int kinds[3]={11,5,28};result.originalType=type;result.changedType=Write(info+0x3C,kinds[(rng>>8)%3]);
  }
 }
 return result;
}
void AfterFunMelee(const FunMeleeResult&hit,uintptr_t info){
 if(hit.changedType)Write(info+0x3C,hit.originalType);
 if(!hit.valid)return;uintptr_t player=0;if(!TrueGodPlayer(player)||player!=hit.player)return;
 if(hit.explode&&chaosHitsOn.load()&&!explosiveMeleeOn.load()){
  meleeExplosionActive=true;bool ok=SpawnRedirectedRocket(player+0x28,player+0x18,hit.position,V{0,1,0});meleeExplosionActive=false;
  if(!ok){chaosHitsOn=false;Log("CHAOS_EXPLOSION_REFUSED_DISABLED");}
 }
 EspFrame frame{};if((!hit.launch&&!hit.pull)||!LoadEsp(frame))return;V direction{};bool haveDirection=FunCameraForward(direction);
 for(unsigned i=0;i<frame.count;i++){const auto&e=frame.entities[i];V p{};if(!FunFreshEntity(e,p))continue;
  if(hit.launch&&e.complete==hit.victim&&haveDirection){V velocity=FunScaled(direction,35.f);velocity.y=std::max(velocity.y,9.f);QueueFunMotion(e,player,{},velocity,false);}
  else if(hit.pull&&e.complete!=hit.victim&&Distance(p,hit.position)<9.f)QueueFunMotion(e,player,hit.position,{},true);
 }
}
struct FunPlayerMotion{uintptr_t player=0,body=0;V velocity{};ULONGLONG until=0,epoch=0;};FunPlayerMotion funMomentum{};
struct FunSlam{uintptr_t player=0,body=0;V target{},surface{};ULONGLONG until=0,epoch=0;};FunSlam funSlam{};
float CurrentGrappleSpeed(){return grapplePullItem.load()&&grappleUpgradesOn.load()?std::clamp(grappleSpeed.load(),10.f,120.f):kKickLaunchSpeed;}
void ReleaseUpgradedGrapple(void*who){
 if(!LocalMove(who)||!grapplePullItem.load()||!grappleReleaseRequested.exchange(false))return;
 uintptr_t player=0,body=0;int fail=0;V position{};
 if(grappleMomentumOn.load()&&grappleUpgradesOn.load()&&TrueGodPlayer(player)&&ResolveCharacterBody(player,body,fail)&&body==kickBurst.body&&Read(body+0xBC,position)){
  V direction=FunSubtract(kickBurst.target,position);if(FunDirection(direction))funMomentum={player,body,FunScaled(direction,CurrentGrappleSpeed()),GetTickCount64()+600,funResetEpoch.load()};
 }
 kickLaunchPending=0;EndKickBurst("grapple_released");
}
void PumpFunMovement(void*who,float dt){
 if(!LocalMove(who))return;uintptr_t player=0;if(!TrueGodPlayer(player))return;
 auto now=GetTickCount64();
 if(!FunInputAllowed()||!std::isfinite(dt)||dt<=0||dt>.1f){funSlam={};funMomentum={};return;}
 PumpFunChain(player);
 FunMotion pending[8]{};AcquireSRWLockExclusive(&funMotionLock);
 for(unsigned i=0;i<8;i++){pending[i]=funMotions[i];funMotions[i]={};}
 ReleaseSRWLockExclusive(&funMotionLock);
 for(auto&motion:pending){
  if(!motion.until)continue;
  if(motion.epoch!=funResetEpoch.load()||now>=motion.until||motion.player!=player||(motion.pull?!gravityPunchOn.load():(!homerunKicksOn.load()&&!chaosHitsOn.load()))){motion={};continue;}
  uintptr_t body=0;V pos{};if(!FunZombieBody(motion.entity,body)||body!=motion.body||!Read(body+0xBC,pos)||!Finite(pos)){motion={};continue;}
  V next{};bool arrived=false;
  if(motion.pull)arrived=KickPullStep(pos,motion.target,18.f*dt,next);
  else{motion.velocity.y-=18.f*dt;next=FunAdd(pos,FunScaled(motion.velocity,dt));}
  if(!Finite(next)||Distance(pos,next)>12.f){motion={};continue;}
  if(motion.epoch!=funResetEpoch.load())continue;
  V velocity=motion.pull?MotionVelocity(pos,motion.target,dt,18.f):motion.velocity;vehicleStopConnected((void*)body,&velocity);
  if(arrived)continue;
                                                                        
  V root{};if(Read(motion.entity.state+0x6F8,root)&&Finite(root))motion.entity.root=root;
  AcquireSRWLockExclusive(&funMotionLock);
                                                                           
  bool replaced=false;for(const auto&slot:funMotions)if(slot.until&&slot.entity.complete==motion.entity.complete)replaced=true;
  if(!replaced&&motion.epoch==funResetEpoch.load())for(auto&slot:funMotions)if(!slot.until){slot=motion;break;}
  ReleaseSRWLockExclusive(&funMotionLock);
 }
 uintptr_t body=0;int fail=0;V pos{};
 if(!ResolveCharacterBody(player,body,fail)||!Read(body+0xBC,pos)||!Finite(pos)){funSlam={};funMomentum={};return;}
 bool air=false;
 auto request=slamRequested.exchange(0);
 if(request&&groundSlamOn.load()&&now>=request&&now-request<500&&!kickLaunchUntil.load()&&DoubleJumpAir(who,air)&&air){
  V end=pos;end.y-=300.f;
  if(kickRay((void*)(player+0x28),0x37,nullptr,&pos,&end,0x11,(void*)(player+0x18),0,0,(int)0x80000000u)&&Finite(end)&&pos.y-end.y>1.f&&pos.y-end.y<299.f){V target=end;target.y+=.9f;funSlam={player,body,target,end,now+5000,funResetEpoch.load()};funMomentum={};}
 }
 if(funSlam.until){
  if(funSlam.epoch!=funResetEpoch.load()||!groundSlamOn.load()||funSlam.player!=player||funSlam.body!=body||now>=funSlam.until||kickLaunchUntil.load()){funSlam={};return;}
  bool arrived=Distance(pos,funSlam.target)<.35f;V velocity=arrived?V{}:MotionVelocity(pos,funSlam.target,dt,65.f);kickSetVelocity((void*)body,&velocity);
  if(arrived){auto surface=funSlam.surface;funSlam={};if(!SpawnRedirectedRocket(player+0x28,player+0x18,surface,V{0,-1,0})){groundSlamOn=false;Log("SLAM_EXPLOSION_REFUSED_DISABLED");}else Log("GROUND_SLAM_LANDED");}return;
 }
 if(funMomentum.until){
  if(funMomentum.epoch!=funResetEpoch.load()||!grappleUpgradesOn.load()||!grappleMomentumOn.load()||funMomentum.player!=player||funMomentum.body!=body||now>=funMomentum.until||kickLaunchUntil.load()){funMomentum={};return;}
  funMomentum.velocity.y-=18.f*dt;V next=FunAdd(pos,FunScaled(funMomentum.velocity,dt));if(Finite(next)&&Distance(pos,next)<15.f){kickSetVelocity((void*)body,&funMomentum.velocity);}else funMomentum={};
 }
}
void PumpVehicleBoost(void*who,float dt){
 static uintptr_t previousCar=0;static ULONGLONG until=0,epoch=0;static V direction{};
 uintptr_t car=0,get=0,set=0,body=0;unsigned count=0;auto now=GetTickCount64();
 if(epoch!=funResetEpoch.load()){until=0;epoch=funResetEpoch.load();}
                                                        
 if(chainExplosionsOn.load()&&FunInputAllowed()&&ResolveOccupiedDrivenCar(who,car,get,set)){uintptr_t player=0;if(TrueGodPlayer(player))PumpFunChain(player);}
 bool boosting=false;
 if(!vehicleBoostOn.load()||!FunInputAllowed()||!vehicleBoostReady.load()||!std::isfinite(dt)||dt<=0||dt>.1f||!ResolveOccupiedDrivenCar(who,car,get,set)||!ResolveVehicleBoostBody(car,body,count))until=0;
 else{
  auto request=boostRequested.exchange(0);
  if(request&&now>=request&&now-request<500&&FunCameraForward(direction)){until=now+500;previousCar=car;Log("VEHICLE_BOOST_STARTED");}
  boosting=now<until&&previousCar==car&&epoch==funResetEpoch.load();
 }
                                                                                   
 if(!boosting||epoch!=funResetEpoch.load())return;
 V velocity=FunScaled(direction,25.f);
 if(Finite(velocity))vehicleStopConnected((void*)body,&velocity);

}
                                                                                                    
bool DriveByLocalVehicle(uintptr_t controller,uintptr_t expectedPlayer=0){
 uintptr_t player=0,actual=0,owner=0,car=0,view=0,seat=0,definition=0,vt=0,iface=0;unsigned char active=0,input=0;
 if(!TrueGodPlayer(player)||(expectedPlayer&&player!=expectedPlayer)||!Read(player+0x830,actual)||actual!=controller||
 !Read(controller,vt)||vt!=gameBase+0xEDE7F8||!Read(controller+0x58,owner)||owner!=player||
 !Read(controller+0x50,active)||!active||!Read(controller+0x28,input)||!input||
 !Read(controller+0xB8,view)||!Read(controller+0xC0,car)||!car||view!=car+0x60||
 !Read(car,vt)||!Read(car+0x60,iface)||!Read(controller+0x198,seat)||!seat||!Read(seat+0x70,definition)||!definition)return false;
 return (vt==gameBase+0xF6BA38&&iface==gameBase+0xF6CC48)||(vt==gameBase+0xF71768&&iface==gameBase+0xF72978);
}
using SeatPredicateFn=unsigned char(*)(void*);SeatPredicateFn nativeSeatPredicate=nullptr;
using DriveByPolicyFn=uintptr_t(*)(void*);DriveByPolicyFn nativeDriveByPolicy=nullptr;
std::atomic<unsigned long long> driveByEquipPass{0},driveByFirePass{0},driveByPolicyPass{0};
unsigned char DriveBySeatPredicate(void*who){
 auto caller=(uintptr_t)_ReturnAddress();
 if((caller==gameBase+0x78A884||caller==gameBase+0x676532)&&driveByOn.load()){
  uintptr_t controller=0;if(Read((uintptr_t)who+0x830,controller)&&DriveByLocalVehicle(controller,(uintptr_t)who)){
   if(caller==gameBase+0x676532)++driveByEquipPass;else ++driveByFirePass;return 0;
  }
 }
 return nativeSeatPredicate(who);
}
uintptr_t DriveByWeaponPolicy(void*controller){
 if(driveByOn.load()&&DriveByLocalVehicle((uintptr_t)controller)){++driveByPolicyPass;return 0;}
 return nativeDriveByPolicy(controller);
}
using CarHolsterFn=unsigned char(*)(void*);CarHolsterFn nativeCarHolster=nullptr;
std::atomic<unsigned long long> driveByCarHolsterPass{0};
unsigned char DriveByCarHolster(void*who){
                                                                                                
 if((uintptr_t)_ReturnAddress()==gameBase+0x676522&&driveByOn.load()){
  uintptr_t player=0,controller=0,car=0;
  if(TrueGodPlayer(player)&&Read(player+0x830,controller)&&DriveByLocalVehicle(controller,player)&&
     Read(controller+0xC0,car)&&(uintptr_t)who==car+0x28){++driveByCarHolsterPass;return 0;}
 }
 return nativeCarHolster(who);
}
void LogDriveBy(){static ULONGLONG next=0;auto now=GetTickCount64();if(now<next)return;next=now+1000;char line[240];sprintf_s(line,"DRIVE_BY_DIAG,ready=%d,on=%d,equip_pass=%llu,fire_pass=%llu,policy_pass=%llu,car_holster_pass=%llu",(int)driveByReady.load(),(int)driveByOn.load(),driveByEquipPass.load(),driveByFirePass.load(),driveByPolicyPass.load(),driveByCarHolsterPass.load());Log(line);}
void InstallFunFeatures(){
 if(!physicalEngine||!TrueGodBuildMatches())return;
 const unsigned char expected[16]={0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0x41,0x28,0x48,0x8B,0xD9,0x48,0x83,0xC1};
 const unsigned char policyExpected[16]={0x48,0x8B,0x81,0x98,0x01,0x00,0x00,0x48,0x85,0xC0,0x74,0x14,0x48,0x8B,0x40,0x70};
 bool seatReady=InstallFeatureHook(0x833610,(void*)DriveBySeatPredicate,(void**)&nativeSeatPredicate,expected);
 bool policyReady=seatReady&&InstallFeatureHook(0x787440,(void*)DriveByWeaponPolicy,(void**)&nativeDriveByPolicy,policyExpected);
 const unsigned char carExpected[16]={0xB0,0x01,0xC3,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC};
 bool carReady=policyReady&&InstallFeatureHook(0xAAAEE0,(void*)DriveByCarHolster,(void**)&nativeCarHolster,carExpected);
 driveByReady=seatReady&&policyReady&&carReady;
 Log(driveByReady?"DRIVE_BY_SEAT_GATE_READY_DISABLED":"DRIVE_BY_SEAT_GATE_REFUSED");
}
void PollFunInput(){
 static bool previousG=false,previousB=false,previousR=false;
 bool g=(GetAsyncKeyState(FeatureBindings::keys[258].load())&0x8000)!=0,b=(GetAsyncKeyState(FeatureBindings::keys[259].load())&0x8000)!=0,r=(GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0;
 if(FunInputAllowed()){
  if(g&&!previousG&&groundSlamOn.load())slamRequested=GetTickCount64();
  if(b&&!previousB&&vehicleBoostOn.load())boostRequested=GetTickCount64();
  if(previousR&&!r&&grappleUpgradesOn.load()&&grapplePullItem.load())grappleReleaseRequested=true;
 }else{slamRequested=0;boostRequested=0;}
 previousG=g;previousB=b;previousR=r;
}
void ResetFunFeatures(){++funResetEpoch;homerunKicksOn=false;groundSlamOn=false;chainExplosionsOn=false;gravityPunchOn=false;vehicleBoostOn=false;grappleUpgradesOn=false;grappleMomentumOn=false;chaosHitsOn=false;driveByOn=false;slamRequested=0;boostRequested=0;grappleReleaseRequested=false;grappleSpeed=45.f;}
std::atomic_bool* FunToggle(int row){switch(row){case 0:return &homerunKicksOn;case 1:return &groundSlamOn;case 2:return &chainExplosionsOn;case 3:return &gravityPunchOn;case 4:return &vehicleBoostOn;case 5:return &grappleUpgradesOn;case 7:return &grappleMomentumOn;case 8:return &chaosHitsOn;case 9:return &driveByOn;default:return nullptr;}}
bool FunReady(int row){if(row==9)return driveByReady.load();if(row==4)return vehicleBoostReady.load();if(row==5||row==6||row==7)return grappleReady.load();return explosiveMeleeReady.load()&&kickLaunchReady.load()&&vehicleBoostReady.load();}
void AdjustFunMenu(int row,bool increase){
 if(!FunReady(row)){SetStatus("FEATURE HOOK UNAVAILABLE");return;}
 if(row==6){grappleSpeed=std::clamp(grappleSpeed.load()+(increase?5.f:-5.f),10.f,120.f);SetStatus("GRAPPLE SPEED UPDATED");return;}
 if(auto toggle=FunToggle(row)){*toggle=!toggle->load();SetStatus(toggle->load()?"FEATURE ON":"FEATURE OFF");}
}
