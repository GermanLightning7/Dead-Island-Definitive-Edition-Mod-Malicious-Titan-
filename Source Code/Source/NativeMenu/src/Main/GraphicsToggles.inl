                                                                                
namespace GraphicsToggles {
constexpr unsigned All=127,ParamCount=21,ContextCount=32;
struct Param{const char*name;unsigned group;unsigned offset;bool integer;};
Param params[ParamCount]={
 {"f_pp_chromatic_aberration_scale",0,0,false},{"f_pp_aberration_on",0,0,false},
 {"f_pp_blur_motion_object",1,0,false},{"f_pp_blur_motion_camera",1,0,false},{"f_pp_blur_camera_motion_factor",1,0,false},{"f_pp_prv_lrp",1,0,false},
 {"f_pp_blur_near",2,0,false},{"f_pp_blur_far",2,0,false},{"i_nvidia_features_dof_bokeh",2,0,true},
 {"f_pp_noise",3,0,false},{"f_pp_filmgrain_4",3,0,false},{"f_pp_filmgrain_8",3,0,false},{"f_pp_filmgrain_16",3,0,false},
 {"f_pp_screen_border",4,0,false},
 {"f_pp_glow_factor",5,0,false},{"f_pp_sun_glow_on",5,0,false},{"f_pp_scattering_shafts",5,0,false},{"f_scattering_shafts_factor",5,0,false},
 {"f_pp_sun_flare_on",6,0,false},{"f_sun_flare_on",6,0,false},{"f_sun_flare_intensity",6,0,false}
};
struct Context{uintptr_t vars=0,data=0;unsigned side=0,owned=0;unsigned saved[ParamCount]{};};
Context contexts[ContextCount];SRWLOCK lock=SRWLOCK_INIT;
std::atomic<unsigned> disabled{0},available{0},active{0},pending{0},fault{0};
std::atomic<unsigned long long> passes{0},writes{0},restores{0},destroyed{0},refused{0};
std::atomic<uintptr_t> lastVars{0},lastData{0};std::atomic<unsigned> lastSide{0};
uintptr_t boundManager=0;bool hookInstalled=false,destroyInstalled=false;
using Evaluate=void(*)(uintptr_t,uintptr_t,uintptr_t);Evaluate original=nullptr;
using Destroy=void(*)(uintptr_t);Destroy originalDestroy=nullptr;
using Find=uintptr_t(*)(const char*,uintptr_t,int);Find find=nullptr;
using Address=uintptr_t(*)(uintptr_t,unsigned,unsigned);Address address=nullptr;
using ThreadKind=int(*)();ThreadKind threadKind=nullptr;
uintptr_t Q(uintptr_t a){uintptr_t x=0;Read(a,x);return x;}
unsigned U(uintptr_t a){unsigned x=0;Read(a,x);return x;}
unsigned Hash(const char*s){unsigned h=0;for(;*s;++s){unsigned char c=*s;if(c>='A'&&c<='Z')c+=32;h=h*41+c;}return h;}
void Recount(){unsigned n=0;for(auto&c:contexts)if(c.owned)++n;pending=n;}
void Fail(unsigned reason){fault=reason;disabled=0;++refused;}
bool Buffer(uintptr_t vars,uintptr_t&data,unsigned&side,unsigned&size){
 if(Q(vars)!=engineBase+0x8d2250)return false;
 int kind=threadKind();if(kind<0||kind>2)return false;
 side=kind<=1?0x10:0x38;data=Q(vars+side);size=U(vars+side+8);
 return data>0x10000&&size>0&&size<=1024*1024;
}
void Restore(uintptr_t vars){
 uintptr_t data=0;unsigned side=0,size=0;
 if(!Buffer(vars,data,side,size)){Fail(1);return;}
 for(auto&c:contexts){
  if(c.vars!=vars||c.side!=side)continue;
  if(c.data!=data){c=Context{};continue;}
  for(unsigned i=0;i<ParamCount;++i){
   if(!(c.owned&(1u<<i)))continue;
   unsigned off=params[i].offset;
   if(off>size||size-off<4){Fail(2);continue;}
   uintptr_t value=address(vars,off,4);
   if(value!=data+off){Fail(3);continue;}
                                                                                  
   if(U(value)==0){*(unsigned*)value=c.saved[i];++restores;}
  }
  c=Context{};
 }
 Recount();
}
void Apply(uintptr_t vars){
 unsigned mask=disabled.load()&available.load();if(!mask||fault)return;
 if(Q(engineBase+0xa52110)!=boundManager){Fail(4);return;}
 uintptr_t data=0;unsigned side=0,size=0;if(!Buffer(vars,data,side,size)){Fail(5);return;}
 lastVars=vars;lastData=data;lastSide=side;
 Context*slot=nullptr;for(auto&c:contexts)if(!c.vars){slot=&c;break;}
 if(!slot){Fail(6);return;}
 uintptr_t locations[ParamCount]{};unsigned values[ParamCount]{};
                                                        
 for(unsigned i=0;i<ParamCount;++i){
  if(!(mask&(1u<<params[i].group)))continue;
  unsigned off=params[i].offset;if(off>size||size-off<4){Fail(7);return;}
  uintptr_t value=address(vars,off,4);if(value!=data+off){Fail(8);return;}
  unsigned bits=U(value);float f=0;memcpy(&f,&bits,4);
  if((params[i].integer&&bits>1)||(!params[i].integer&&(!std::isfinite(f)||fabsf(f)>1000000.f))){Fail(9);return;}
  locations[i]=value;values[i]=bits;
 }
 slot->vars=vars;slot->data=data;slot->side=side;
 for(unsigned i=0;i<ParamCount;++i)if(locations[i]){
  slot->saved[i]=values[i];if(values[i])slot->owned|=1u<<i;
  *(unsigned*)locations[i]=0;++writes;
 }
 if(!slot->owned)*slot=Context{};
 Recount();
}
void SafeRestore(uintptr_t vars){
 AcquireSRWLockExclusive(&lock);
 __try{__try{Restore(vars);}__except(EXCEPTION_EXECUTE_HANDLER){Fail(10);}}
 __finally{ReleaseSRWLockExclusive(&lock);}
}
void SafeApply(uintptr_t vars){
 AcquireSRWLockExclusive(&lock);
 __try{__try{Apply(vars);}__except(EXCEPTION_EXECUTE_HANDLER){Fail(11);}}
 __finally{ReleaseSRWLockExclusive(&lock);}
}
void Hook(uintptr_t script,uintptr_t renderContext,uintptr_t vars){
 ++active;
 __try{
  bool render=script==boundManager+0x220&&Q(engineBase+0xa52110)==boundManager;
  if(render)SafeRestore(vars);
  original(script,renderContext,vars);
  if(render){++passes;SafeApply(vars);}
 }__finally{--active;}
}
void DestroyHook(uintptr_t vars){
 ++active;
 __try{
  AcquireSRWLockExclusive(&lock);
  for(auto&c:contexts)if(c.vars==vars){c=Context{};++destroyed;}
  Recount();ReleaseSRWLockExclusive(&lock);
  originalDestroy(vars);
 }__finally{--active;}
}
bool Ready(unsigned group){return group<7&&(available.load()&(1u<<group))&&!fault;}
void Toggle(unsigned group){if(Ready(group))disabled.fetch_xor(1u<<group);}
void Reset(){disabled=0;}
void DisableAll(){if(!fault)disabled=available.load();}
                                                                               
bool initPending=true,checkedBuild=false;unsigned initAttempts=0;
bool BindReadyManager(){
 uintptr_t manager=Q(engineBase+0xa52110);
 if(Q(manager)!=engineBase+0x8d21f8)return false;
 unsigned renderCount=U(manager+0x230);
 if(!renderCount||renderCount>10000)return false;
 unsigned offsets[ParamCount]{};
 for(unsigned i=0;i<ParamCount;++i){
  auto&p=params[i];uintptr_t registry=manager+(p.integer?0x128:0x140);
  unsigned count=U(registry+8);if(!count||count>20000)return false;
  uintptr_t desc=find(p.name,registry,0);unsigned short type=0;
  if(!desc||!Read(desc+0x5c,type)||type!=(p.integer?1:2)||U(desc+0x54)!=Hash(p.name))return false;
  offsets[i]=U(desc+0x50);if(offsets[i]>1024*1024-4)return false;
 }
 if(Q(engineBase+0xa52110)!=manager)return false;
 for(unsigned i=0;i<ParamCount;++i)params[i].offset=offsets[i];
 boundManager=manager;return true;
}
void Install(){
 if(!initPending)return;
 if(!checkedBuild){
  if(!CameraFeatures::HashModule(engineBase,"7cf1f0153a2748da55a36d89fcf2e451ba444468260355836722d3ef76b27c79")){initPending=false;Log("GRAPHICS_ENGINE_MISMATCH");return;}
  const BYTE evalBytes[]={0x4c,0x8b,0xdc,0x53,0x41,0x54,0x41,0x55,0x48,0x83,0xec,0x70,0x8b,0x41,0x10};
  const BYTE destroyBytes[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57};
  if(memcmp((void*)(engineBase+0x7204e0),evalBytes,sizeof(evalBytes))||memcmp((void*)(engineBase+0x71e580),destroyBytes,sizeof(destroyBytes))){initPending=false;Log("GRAPHICS_CODE_MISMATCH");return;}
  find=(Find)(engineBase+0x7232b0);address=(Address)(engineBase+0x71fa50);threadKind=(ThreadKind)(engineBase+0x17b00);
  checkedBuild=true;
 }
 ++initAttempts;
 if(!BindReadyManager()){if(initAttempts==1)Log("GRAPHICS_WAITING_FOR_SCRIPTS");return;}
 initPending=false;
 auto mh=MH_Initialize();if(mh!=MH_OK&&mh!=MH_ERROR_ALREADY_INITIALIZED)return;
 void*dest=(void*)(engineBase+0x71e580);void*eval=(void*)(engineBase+0x7204e0);
 if(MH_CreateHook(dest,(void*)DestroyHook,(void**)&originalDestroy)!=MH_OK)return;
 if(MH_EnableHook(dest)!=MH_OK){MH_RemoveHook(dest);return;}destroyInstalled=true;
 if(MH_CreateHook(eval,(void*)Hook,(void**)&original)!=MH_OK){Log("GRAPHICS_EVAL_HOOK_FAILED");return;}
 if(MH_EnableHook(eval)!=MH_OK){MH_RemoveHook(eval);Log("GRAPHICS_EVAL_ENABLE_FAILED");return;}
 hookInstalled=true;available=All;Log("GRAPHICS_READY_DISABLED");
}
bool CanStop(){
 initPending=false;disabled=0;for(int n=0;n<100&&(pending.load()||active.load());++n)Sleep(1);
 if(pending.load()||active.load())return false;
 available=0;
 if(hookInstalled){MH_DisableHook((void*)(engineBase+0x7204e0));hookInstalled=false;}
 if(destroyInstalled){MH_DisableHook((void*)(engineBase+0x71e580));destroyInstalled=false;}
 for(int n=0;n<100&&active.load();++n)Sleep(1);return active.load()==0;
}
void Diagnostic(){static ULONGLONG next=0;auto now=GetTickCount64();if(now<next)return;next=now+1000;if(initPending)Install();char line[384];sprintf_s(line,"GRAPHICS_DIAG,available=%u,disabled=%u,passes=%llu,writes=%llu,restores=%llu,pending=%u,refused=%llu,fault=%u,vars=%llx,data=%llx,side=%x",available.load(),disabled.load(),passes.load(),writes.load(),restores.load(),pending.load(),refused.load(),fault.load(),(unsigned long long)lastVars.load(),(unsigned long long)lastData.load(),lastSide.load());Log(line);}
}
