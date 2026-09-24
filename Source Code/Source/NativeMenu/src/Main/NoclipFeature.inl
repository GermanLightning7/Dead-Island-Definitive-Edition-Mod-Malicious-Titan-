// Player noclip/fly: reuses the same verified IPhBody pointer chain as ResolveCharacterBody
// (CharacterPhysics.inl) rather than duplicating the offset chain a second time.
using SetPositionFn=void(*)(void*,const V*);
std::atomic_bool noclipOn{false},noclipReady{false};
std::atomic<unsigned long long> noclipTicks{0},noclipWriteHeld{0},noclipWriteReverted{0},noclipAutoDisables{0};
std::atomic<float> kNoclipBaseSpeed{8.f};
constexpr float kNoclipSpeedMin=1.f,kNoclipSpeedMax=200.f;
constexpr float kNoclipSpeedStepFactor=1.2f;
constexpr float kNoclipFastMultiplier=2.f;
constexpr float kNoclipMaxDtSeconds=0.25f;
constexpr float kNoclipMaxStepPerTick=50.f;
constexpr float kNoclipRevertEpsilon=0.05f;
std::atomic<ULONGLONG> noclipLastTick{0},noclipRecoverySince{0},noclipRetryAt{0};
std::atomic<unsigned> noclipRecoveries{0};
constexpr ULONGLONG kNoclipRecoveryTimeoutMs=2000,kNoclipRetryMs=100;
void SuspendNoclip(int failStep){
 auto now=GetTickCount64();ULONGLONG empty=0;
 if(noclipRecoverySince.compare_exchange_strong(empty,now)){
  std::ofstream f(LogPath(),std::ios::app);
  f<<now<<",NOCLIP_RECOVERY_WAIT,step="<<failStep<<",gravity_unchanged=1\n";
 }
 noclipRetryAt=now+kNoclipRetryMs;
}
void ToggleNoclip(){
 bool next=!noclipOn.load();noclipLastTick=GetTickCount64();noclipRecoverySince=0;noclipRetryAt=0;noclipOn=next;Log(next?"NOCLIP_ENABLED":"NOCLIP_DISABLED");SetStatus(next?"NOCLIP ENABLED - WASD SPACE CTRL SHIFT":"NOCLIP DISABLED");
}
void AdjustNoclipSpeed(int direction){
 float next=std::clamp(direction>0?kNoclipBaseSpeed.load()*kNoclipSpeedStepFactor:kNoclipBaseSpeed.load()/kNoclipSpeedStepFactor,kNoclipSpeedMin,kNoclipSpeedMax);
 kNoclipBaseSpeed=next;SetStatus("NOCLIP SPEED UPDATED");
 std::ofstream f(LogPath(),std::ios::app);f<<GetTickCount64()<<",NOCLIP_SPEED_CHANGED,value="<<next<<'\n';
}
void PollNoclipInput(){
 auto last=noclipLastTick.load(),now=GetTickCount64();
 if(noclipOn.load()&&last&&now-last>500&&!noclipRecoverySince.load())SuspendNoclip(10);
 auto recovery=noclipRecoverySince.load();
 if(noclipOn.load()&&recovery&&now-recovery>=kNoclipRecoveryTimeoutMs){
  if(noclipOn.exchange(false)){++noclipAutoDisables;Log("NOCLIP_RECOVERY_TIMEOUT_DISABLED_GRAVITY_UNCHANGED");}
  noclipRecoverySince=0;noclipRetryAt=0;
 }
}
void PumpNoclip(void*who,float dt){
 static uintptr_t lastBody=0,lastPlayer=0;
 static V lastWritten{};
 static bool lastWrittenValid=false,revertLoggedThisActivation=false;
 uintptr_t player=0;
 if(!TrueGodPlayer(player)||!LocalMove(who))return;
 noclipLastTick=GetTickCount64();
 if(!noclipOn.load()){
  if(lastWrittenValid&&lastBody){
   V zeroVelocity{0,0,0};
   if(physicalEngine){
    ((SetPositionFn)(physicalEngine+0x300FC0))((void*)lastBody,&zeroVelocity);
    ((SetPositionFn)(physicalEngine+0x3010A0))((void*)lastBody,&zeroVelocity);
   }
   Write(lastBody+0xD8,zeroVelocity);
   Write((uintptr_t)who+0x70,zeroVelocity);
  }
  lastBody=0;lastPlayer=0;lastWrittenValid=false;revertLoggedThisActivation=false;
  return;
 }
 if(noclipRecoverySince.load()&&GetTickCount64()<noclipRetryAt.load())return;
 uintptr_t body=0;int failStep=0;
 if(!ResolveCharacterBody(player,body,failStep)){
  lastWrittenValid=false;SuspendNoclip(failStep);return;
 }
 if(noclipRecoverySince.exchange(0)){
  ++noclipRecoveries;lastWrittenValid=false;noclipRetryAt=0;
  Log("NOCLIP_RECOVERED_FRESH_BODY_VALIDATED");
 }
 if(!noclipOn.load())return;
 if(body!=lastBody||player!=lastPlayer){lastWrittenValid=false;lastBody=body;lastPlayer=player;revertLoggedThisActivation=false;}
 bool foreground=ForegroundGame();
 ++noclipTicks;
 V position{};
 if(!Read(body+0xBC,position)||!Finite(position))return;
 if(lastWrittenValid){
  float dx=position.x-lastWritten.x,dy=position.y-lastWritten.y,dz=position.z-lastWritten.z;
  float drift=sqrtf(dx*dx+dy*dy+dz*dz);
  if(std::isfinite(drift)&&drift>kNoclipMaxStepPerTick){
   lastWrittenValid=false;
  }else if(std::isfinite(drift)&&drift<=kNoclipRevertEpsilon){
   ++noclipWriteHeld;
  }else if(std::isfinite(drift)){
   ++noclipWriteReverted;
   if(!revertLoggedThisActivation){Log("NOCLIP_POSITION_REVERT_DETECTED_SOLVER_MAY_BE_FIGHTING_WRITE");revertLoggedThisActivation=true;}
  }
 }
 V camPos{},camRight{},camUp{},camBack{};float sx=0,sy=0;
 if(!ReadD3DCamera(camPos,camRight,camUp,camBack,sx,sy)||!Unit(camRight)||!Unit(camUp)||!Unit(camBack))return;
 float speed=kNoclipBaseSpeed.load()*((foreground&&(GetAsyncKeyState(VK_LSHIFT)&0x8000))?kNoclipFastMultiplier:1.f);
 float step=speed*std::clamp(dt,0.f,kNoclipMaxDtSeconds);
 V delta{0,0,0};
 bool moveForward=foreground&&(GetAsyncKeyState('W')&0x8000),moveBack=foreground&&(GetAsyncKeyState('S')&0x8000);
 bool moveLeft=foreground&&(GetAsyncKeyState('A')&0x8000),moveRight=foreground&&(GetAsyncKeyState('D')&0x8000);
 bool moveUp=foreground&&(GetAsyncKeyState(VK_SPACE)&0x8000),moveDown=foreground&&(GetAsyncKeyState(VK_LCONTROL)&0x8000);
 if(moveForward){delta.x-=camBack.x*step;delta.y-=camBack.y*step;delta.z-=camBack.z*step;}
 if(moveBack){delta.x+=camBack.x*step;delta.y+=camBack.y*step;delta.z+=camBack.z*step;}
 if(moveRight){delta.x+=camRight.x*step;delta.y+=camRight.y*step;delta.z+=camRight.z*step;}
 if(moveLeft){delta.x-=camRight.x*step;delta.y-=camRight.y*step;delta.z-=camRight.z*step;}
 if(moveUp)delta.y+=step;
 if(moveDown)delta.y-=step;
 float deltaLen=sqrtf(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);
 if(!std::isfinite(deltaLen)||deltaLen>kNoclipMaxStepPerTick)return;
 V origin=(lastWrittenValid&&Distance(position,lastWritten)<=kNoclipMaxStepPerTick)?lastWritten:position;
 V newPos{origin.x+delta.x,origin.y+delta.y,origin.z+delta.z};
 if(!Finite(newPos))return;
 ((SetPositionFn)(physicalEngine+0x300e80))((void*)body,&newPos);
 lastWritten=newPos;lastWrittenValid=true;
 V zeroVelocity{0,0,0};
 if(physicalEngine){
  ((SetPositionFn)(physicalEngine+0x300FC0))((void*)body,&zeroVelocity);
  ((SetPositionFn)(physicalEngine+0x3010A0))((void*)body,&zeroVelocity);
 }
 Write(body+0xD8,zeroVelocity);
 Write((uintptr_t)who+0x70,zeroVelocity);
}
bool InstallNoclip(){
 noclipReady=TrueGodBuildMatches()&&runReady.load()&&physicalEngine!=0;
 Log(noclipReady?"NOCLIP_READY_DISABLED":"NOCLIP_UNAVAILABLE_HOOK_OR_ENGINE_BUILD_MISMATCH");
 return noclipReady;
}
void LogNoclip(){
 static ULONGLONG next=0;auto now=GetTickCount64();if(now<next)return;next=now+1000;
 std::ofstream f(LogPath(),std::ios::app);
 f<<now<<",NOCLIP_DIAG,ready="<<noclipReady.load()<<",on="<<noclipOn.load()<<",ticks="<<noclipTicks.load()<<",write_held="<<noclipWriteHeld.load()<<",write_reverted="<<noclipWriteReverted.load()<<",auto_disables="<<noclipAutoDisables.load()<<",speed="<<kNoclipBaseSpeed.load()<<",recovering="<<(noclipRecoverySince.load()!=0)<<",recoveries="<<noclipRecoveries.load()<<",last_tick="<<noclipLastTick.load()<<'\n';
}