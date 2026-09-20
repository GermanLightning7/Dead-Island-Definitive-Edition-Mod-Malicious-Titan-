                                                                                
std::atomic<float> jumpScale{1.f},runScale{1.f},swingScale{1.f};
std::atomic_bool jumpReady{false},runReady{false},swingReady{false},damageReady{false};
std::atomic<unsigned> jumpChanges{0},runChanges{0},swingChanges{0},damageChanges{0};
std::atomic<float> jumpBefore{0},jumpAfter{0},runBefore{0},runAfter{0},swingBefore{0},swingAfter{0},damageBefore{0};
using JumpFn=void(*)(void*);using RunFn=void(*)(void*,float);
using SwingFn=float(*)(void*,void*);using DamageFn=float(*)(void*,float,void*);
JumpFn nativeJump=nullptr;RunFn nativeRun=nullptr;SwingFn nativeSwing=nullptr;DamageFn nativeDamage=nullptr;
using SpeedCapFn=float(*)(void*,float);
SpeedCapFn nativeSpeedCap=nullptr;
std::atomic<unsigned> speedCapChanges{0},sprintChanges{0};
std::atomic<float> speedCapBefore{0},speedCapAfter{0};
bool LocalMove(void* who);
                                                                                 
                                                                                
float ModifiedSpeedCap(void* who,float parameter){
 auto caller=(uintptr_t)_ReturnAddress();float value=nativeSpeedCap(who,parameter),scale=runScale.load();
 uintptr_t p=0,owner=0,controller=0,move=0;
 if(caller==gameBase+0x66656B&&runReady.load()&&scale!=1.f&&TrueGodPlayer(p)&&Read(p+0x900,controller)&&controller==(uintptr_t)who&&Read(controller+0x58,owner)&&owner==p&&Read(p+0x810,move)&&LocalMove((void*)move)&&std::isfinite(value)&&value>0&&value<20){
  speedCapBefore=value;speedCapAfter=value*scale;++speedCapChanges;return value*scale;
 }return value;
}
bool LocalMove(void* who){
 uintptr_t p=0,vt=0,owner=0;unsigned char active=0;auto c=(uintptr_t)who;
 return TrueGodPlayer(p)&&Read(c,vt)&&vt==gameBase+0xED72C8&&Read(c+0x58,owner)&&owner==p&&Read(c+0x50,active)&&active;
}
bool LocalMelee(uintptr_t p){
 for(int i=16;i<=17;i++){uintptr_t c=0,vt=0,owner=0;unsigned char active=0;
  if(Read(p+0x810+i*8,c)&&Read(c,vt)&&(vt==gameBase+0xEE2238||vt==gameBase+0xED7EA8)&&Read(c+0x58,owner)&&owner==p&&Read(c+0x50,active)&&active)return true;
 }return false;
}
#include "DoubleJump.inl"
void ModifiedJump(void* who){
 auto c=(uintptr_t)who;float stamp=0;bool valid=LocalMove(who)&&Read(c+0x94,stamp);
 nativeJump(who);float scale=jumpScale.load(),afterStamp=0,height=0;
 if(valid&&scale!=1.f&&Read(c+0x94,afterStamp)&&std::isfinite(afterStamp)&&afterStamp!=stamp&&Read(c+0x8C,height)&&std::isfinite(height)&&height>0&&height<20){
  if(Write(c+0x8C,height*scale)){jumpBefore=height;jumpAfter=height*scale;++jumpChanges;}
 }
}
void ModifiedRun(void* who,float dt){
 nativeRun(who,dt);DoubleJumpFrame(who);PumpBlueprintChange(who);PumpSpawnItem(who);PumpKickLaunchInput(who);PumpFunMovement(who,dt);float scale=runScale.load();if(scale==1.f||!LocalMove(who))return;
 auto c=(uintptr_t)who;unsigned char active=0;V v{};
 if(!Read(c+0x28,active)||!active||!Read(c+0x70,v)||!Finite(v))return;
 float speed=sqrtf(v.x*v.x+v.z*v.z);if(!std::isfinite(speed)||speed<=0||speed>100)return;
 v.x*=scale;v.z*=scale;if(Write(c+0x70,v)){runBefore=speed;runAfter=speed*scale;++runChanges;unsigned char sprint=0;if(Read(c+0x7C,sprint)&&sprint)++sprintChanges;}
}
float ModifiedSwing(void* who,void* item){
 float value=nativeSwing(who,item),scale=swingScale.load();uintptr_t p=0;
 if(scale!=1.f&&TrueGodPlayer(p)&&p==(uintptr_t)who&&LocalMelee(p)&&std::isfinite(value)&&value>0&&value<20){swingBefore=value;swingAfter=value*scale;++swingChanges;return value*scale;}return value;
}
float ModifiedDamage(void* who,float damage,void* item){
 uintptr_t caller=(uintptr_t)_ReturnAddress(),p=0;float result=nativeDamage(who,damage,item);
 if(caller>=gameBase+0x7A57E0&&caller<gameBase+0x7A7B00&&oneShotMenuState&&oneShotMenuState()==1&&TrueGodPlayer(p)&&p==(uintptr_t)who&&std::isfinite(result)&&result>0){damageBefore=result;++damageChanges;return 999999.f;}return result;
}
bool InstallFeatureHook(uintptr_t rva,void* hook,void** original,const unsigned char(&expected)[16]){
 unsigned char actual[16]{};auto target=(void*)(gameBase+rva);
 if(!ReadBytes((uintptr_t)target,actual,16)||memcmp(actual,expected,16))return false;
 if(MH_CreateHook(target,hook,original)!=MH_OK)return false;
 if(MH_EnableHook(target)!=MH_OK){MH_RemoveHook(target);return false;}return true;
}
void InstallMovementCombat(){
 if(!TrueGodBuildMatches())return;auto init=MH_Initialize();if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED)return;
 const unsigned char j[16]={0x48,0x89,0x7C,0x24,0x18,0x55,0x48,0x8D,0x6C,0x24,0xA9,0x48,0x81,0xEC,0xA0,0x00};
 const unsigned char r[16]={0x40,0x57,0x48,0x81,0xEC,0x00,0x01,0x00,0x00,0x44,0x0F,0x29,0x7C,0x24,0x60,0x48};
 const unsigned char s[16]={0x48,0x89,0x5C,0x24,0x10,0x56,0x48,0x83,0xEC,0x20,0x48,0x8B,0xF2,0x48,0x8B,0xD9};
 const unsigned char d[16]={0x48,0x89,0x5C,0x24,0x18,0x56,0x48,0x83,0xEC,0x40,0x0F,0x29,0x7C,0x24,0x20,0x49};
 const unsigned char c[16]={0x40,0x53,0x48,0x83,0xEC,0x40,0x48,0x8B,0xD9,0x48,0x8B,0x49,0x58,0x0F,0x29,0x74};
 const unsigned char g[16]={0x40,0x53,0x48,0x83,0xEC,0x30,0x48,0x8B,0x41,0x58,0x48,0x8B,0xD9,0x48,0x83,0xB8};
 const unsigned char i[16]={0x48,0x85,0xC9,0x0F,0x84,0x0E,0x02,0x00,0x00,0x53,0x48,0x83,0xEC,0x30,0x4C,0x8B};
 jumpReady=InstallFeatureHook(0x762A50,(void*)ModifiedJump,(void**)&nativeJump,j);
 runReady=InstallFeatureHook(0x759F20,(void*)ModifiedRun,(void**)&nativeRun,r);
 bool capReady=InstallFeatureHook(0x729AA0,(void*)ModifiedSpeedCap,(void**)&nativeSpeedCap,c);
 runReady=runReady.load()&&capReady;Log(capReady?"SPEED_CAP_READY_NEUTRAL":"SPEED_CAP_HOOK_FAILED");
 swingReady=InstallFeatureHook(0x67FD10,(void*)ModifiedSwing,(void**)&nativeSwing,s);
 damageReady=InstallFeatureHook(0x67F4C0,(void*)ModifiedDamage,(void**)&nativeDamage,d);
 bool physicalReady=PreparePhysicalAirJump();
 bool inputReady=InstallFeatureHook(0x75B2C0,(void*)ModifiedMoveInput,(void**)&nativeMoveInput,i);
 doubleJumpReady=jumpReady.load()&&nativeRun&&physicalReady&&inputReady;Log(doubleJumpReady?"AIR_JUMPS_READY_DISABLED":"AIR_JUMPS_HOOK_FAILED");
 Log(jumpReady?"JUMP_READY_NEUTRAL":"JUMP_HOOK_FAILED");Log(runReady?"RUN_READY_NEUTRAL":"RUN_HOOK_FAILED");Log(swingReady?"SWING_READY_NEUTRAL":"SWING_HOOK_FAILED");Log(damageReady?"MELEE_DAMAGE_READY_DISABLED":"MELEE_DAMAGE_HOOK_FAILED");
}
void LogMovementCombat(){
 static ULONGLONG next=0;auto now=GetTickCount64();if(now<next)return;next=now+1000;
 std::ofstream f(LogPath(),std::ios::app);f<<now<<",MOVEMENT_COMBAT,jump_scale="<<jumpScale.load()<<",jump_n="<<jumpChanges.load()<<",jump_before="<<jumpBefore.load()<<",jump_after="<<jumpAfter.load()<<",run_scale="<<runScale.load()<<",run_n="<<runChanges.load()<<",run_before="<<runBefore.load()<<",run_after="<<runAfter.load()<<",sprint_n="<<sprintChanges.load()<<",cap_n="<<speedCapChanges.load()<<",cap_before="<<speedCapBefore.load()<<",cap_after="<<speedCapAfter.load()<<",swing_scale="<<swingScale.load()<<",swing_n="<<swingChanges.load()<<",swing_before="<<swingBefore.load()<<",swing_after="<<swingAfter.load()<<",double_on="<<doubleJumpOn.load()<<",double_n="<<doubleJumpCount.load()<<",double_landings="<<doubleJumpLandings.load()<<",damage_n="<<damageChanges.load()<<",damage_before="<<damageBefore.load()<<'\n';
}


