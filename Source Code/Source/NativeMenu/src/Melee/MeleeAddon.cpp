                                                                            
                                                                                   
#define DllMain UnusedBaselineDllMain
#include "BaseRuntime.cpp"
#undef DllMain
#include <algorithm>
#include <vector>
#include "../RoutineDiagnostics.h"
namespace {
struct ContactList { unsigned char* data; unsigned count,capacity,tag; };
using QueryFn=int(*)(void*,V*,V*,V*,V*,float,ContactList*,char);
QueryFn meleeOriginal=nullptr;
using GameTickFn=void(*)();std::atomic<GameTickFn> meleeGameTick{nullptr};
std::atomic_bool meleeReady{false},meleeOn{false},meleeStop{false},meleePrerequisites{false};
std::atomic_bool meleeOneShotOn{false},meleeChainOn{false};
constexpr float kChainHopRadius=450.f;
constexpr int kChainMaxHops=8;
std::atomic<unsigned> meleeQueries{0},meleeRedirects{0},meleeMisses{0},meleeInvalid{0};
std::atomic<unsigned> hostileHumanContacts{0};
bool MeleeHostile(const EspEntity&e,V&root){
 uintptr_t owner=0;if(!ValidateEspEntity(e,root)||!Read(e.state+0x58,owner)||owner!=e.complete)return false;
 if(!e.humanType)return true;
 return AiAllegiance::HumanHostile(e.complete,gameBase,[](uintptr_t p,auto& v){return Read(p,v);});
}
struct Observation {
 unsigned long long tick=0; unsigned query=0; uintptr_t owner=0,target=0,first=0;
 int generation=0,result=0,mesh=-1; unsigned count=0;
 V origin{},forward{},left{},right{},camera{},root{},head{},relocated{},impact{};
 float radius=0,distance=0,health=0; bool active=false,pose=false,redirected=false,inputsOk=false,listOk=false,geometryOk=false,human=false; int alarm=-1; uintptr_t state=0,stateClass=0;
 long long micros=0;
};
SRWLOCK observationLock=SRWLOCK_INIT; Observation latestObservation;
void Observe(const Observation&o){
 if(o.active){
  const char*reason=!o.inputsOk?"invalid_input":!o.target?"no_eligible_target":!o.pose?"pose_unavailable":!o.listOk?"invalid_contact_list":!o.geometryOk?"invalid_wedge":!o.redirected?"native_passthrough":o.count==0?"native_no_target_contact":"target_contact";
  char row[1024]{};sprintf_s(row,"MELEE_RESULT reason=%s query=%u target=0x%llx state=0x%llx class_rva=0x%llx generation=%d human=%d alarm_level_not_faction=%d enabled=%d chain=%d one_shot=%d count=%u mesh=%d pose=%d radius=%.3f distance=%.3f health=%.3f root=%.3f/%.3f/%.3f point=%.3f/%.3f/%.3f origin=%.3f/%.3f/%.3f impact=%.3f/%.3f/%.3f us=%lld",reason,o.query,(unsigned long long)o.target,(unsigned long long)o.state,(unsigned long long)o.stateClass,o.generation,(int)o.human,o.alarm,(int)meleeOn.load(),(int)meleeChainOn.load(),(int)meleeOneShotOn.load(),o.count,o.mesh,(int)o.pose,o.radius,o.distance,o.health,o.root.x,o.root.y,o.root.z,o.head.x,o.head.y,o.head.z,o.relocated.x,o.relocated.y,o.relocated.z,o.impact.x,o.impact.y,o.impact.z,o.micros);RoutineDiagnostics::Emit(row);
 }
AcquireSRWLockExclusive(&observationLock);latestObservation=o;ReleaseSRWLockExclusive(&observationLock);}
bool Unit(V v){float l=v.x*v.x+v.y*v.y+v.z*v.z;return std::isfinite(l)&&l>.5f&&l<1.5f;}
bool Normalize(V&v){float l=sqrtf(v.x*v.x+v.y*v.y+v.z*v.z);if(!std::isfinite(l)||l<.001f)return false;v={v.x/l,v.y/l,v.z/l};return true;}

                                                                        
                                                                                   
bool NativeMeleePoint(uintptr_t actor,V root,V&point){
 __try{
  using Getter=uintptr_t(*)(void*);
  using Mesh=int(*)(void*,unsigned char);
  struct Bounds{V low,high;};
  using Extents=Bounds*(*)(void*,Bounds*,int);
  uintptr_t vt=0,fn=0,animation=0,view=0,model=0;
  if(!Read(actor,vt)||!Read(vt+0x3A8,fn)||!InGame(fn))return false;
  animation=((Getter)fn)((void*)actor);if(animation<0x10000)return false;
  view=animation+0x50;if(!Read(view,vt)||!Read(vt+8,fn)||!InGame(fn))return false;
  model=((Getter)fn)((void*)view);if(model<0x10000)return false;
  auto meshFn=*(Mesh*)(gameBase+0xC7F860);auto extentsFn=*(Extents*)(gameBase+0xC7E770);
  if(!meshFn||!extentsFn)return false;
  for(int i=0;i<3;i++){
   unsigned bone=0;if(!Read(gameBase+0x12673C0+i*4,bone)||bone>255)return false;
   int mesh=meshFn((void*)model,(unsigned char)bone);if(mesh<0||mesh>4095)continue;
   Bounds b{};extentsFn((void*)actor,&b,mesh);
   V size{b.high.x-b.low.x,b.high.y-b.low.y,b.high.z-b.low.z};
   point={(b.low.x+b.high.x)*.5f,(b.low.y+b.high.y)*.5f,(b.low.z+b.high.z)*.5f};
   float span=Distance(root,point);
   if(Finite(point)&&std::isfinite(span)&&span>.1f&&span<4.f&&size.x>0&&size.x<4&&size.y>0&&size.y<4&&size.z>0&&size.z<4)return true;
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
 return false;
}

int MeleeQuery(void*owner,V*origin,V*forward,V*left,V*right,float radius,ContactList*out,char extra){
 uintptr_t player=0;
 if(!TrueGodPlayer(player)||(uintptr_t)owner!=player)return meleeOriginal(owner,origin,forward,left,right,radius,out,extra);
 if(auto tick=meleeGameTick.load())tick();
 Observation o{};o.active=meleeOn.load()||meleeChainOn.load()||meleeOneShotOn.load();o.tick=GetTickCount64();o.query=++meleeQueries;o.owner=player;o.radius=radius;
 LARGE_INTEGER begin{},end{},freq{};QueryPerformanceCounter(&begin);QueryPerformanceFrequency(&freq);
 bool inputs=origin&&forward&&left&&right&&out&&Read((uintptr_t)origin,o.origin)&&Read((uintptr_t)forward,o.forward)&&Read((uintptr_t)left,o.left)&&Read((uintptr_t)right,o.right)&&Finite(o.origin)&&Unit(o.forward)&&Unit(o.left)&&Unit(o.right)&&std::isfinite(radius)&&radius>.05f&&radius<10.f;
 o.inputsOk=inputs;EspEntity chosen{};V head{};
 if(inputs&&EnumerateAndSelect()){
  float best=2000.f;V cameraRight{},cameraUp{},cameraBack{};float sx=0,sy=0;
  if(ReadD3DCamera(o.camera,cameraRight,cameraUp,cameraBack,sx,sy)){
   for(const auto&pair:espRoster){const auto&e=pair.second.entity;V root{};
    if(!MeleeHostile(e,root))continue;
    float distance=Distance(o.camera,root);
    if(distance>.1f&&distance<best){chosen=e;chosen.root=root;best=distance;}
   }
   if(chosen.state){o.state=chosen.state;o.human=chosen.humanType;uintptr_t vt=0;if(Read(chosen.state,vt))o.stateClass=vt-gameBase;if(o.human)Read(chosen.state+0x728,o.alarm);o.target=chosen.complete;o.generation=chosen.generation;o.root=chosen.root;o.distance=best;Read(chosen.state+0x734,o.health);o.pose=NativeMeleePoint(chosen.complete,chosen.root,head);o.head=head;}
  }
 }
 ContactList before{};bool listOk=out&&Read((uintptr_t)out,before)&&before.count<=before.capacity&&before.capacity<=4096&&(!before.capacity||before.data);
 V bisector{o.left.x+o.right.x,o.left.y+o.right.y,o.left.z+o.right.z};
 bool geometry=inputs&&listOk&&chosen.state&&o.pose&&Normalize(bisector);
 o.listOk=listOk;o.geometryOk=geometry;
 bool chainMode=meleeChainOn.load()&&geometry&&before.count==0;
 bool redirect=!chainMode&&meleeOn.load()&&geometry&&before.count==0;
 if(redirect){
                                                                               
  float reach=radius*.65f;
  o.relocated={head.x-bisector.x*reach,head.y-bisector.y*reach,head.z-bisector.z*reach};
  redirect=Finite(o.relocated);
 }
 if(chainMode){
                                                                              
                                                                                   
  EspEntity currentTarget=chosen;V currentHead=head,currentRoot=chosen.root;
  std::vector<uintptr_t> hitAlready;hitAlready.reserve(kChainMaxHops);
  unsigned keptTotal=0;
  for(int hop=0;hop<kChainMaxHops;hop++){
   LARGE_INTEGER clock{};QueryPerformanceCounter(&clock);
   if((clock.QuadPart-begin.QuadPart)*1000000/freq.QuadPart>40000){meleeChainOn=false;break;}
   V verifiedRoot{};if(!MeleeHostile(currentTarget,verifiedRoot))break;
   float reach=radius*.65f;
   V relocated{currentHead.x-bisector.x*reach,currentHead.y-bisector.y*reach,currentHead.z-bisector.z*reach};
   if(!Finite(relocated))break;o.relocated=relocated;
   meleeOriginal(owner,&relocated,forward,left,right,radius,out,extra);
   ContactList after{};
   if(!Read((uintptr_t)out,after)||after.count<keptTotal||after.count>after.capacity||after.capacity>4096||after.count>256||!after.data){meleeChainOn=false;++meleeInvalid;break;}
   bool found=false,bad=false;
   for(unsigned i=keptTotal;i<after.count;i++){
    uintptr_t victim=0;if(!Read((uintptr_t)after.data+i*0x30,victim)){bad=true;break;}
    if(victim==currentTarget.complete){
     if(i!=keptTotal)memmove(after.data+keptTotal*0x30,after.data+i*0x30,0x30);
     found=true;break;
    }
   }
   if(found){++keptTotal;++meleeRedirects;if(currentTarget.humanType)++hostileHumanContacts;hitAlready.push_back(currentTarget.complete);}
   out->count=keptTotal;
   if(bad){meleeChainOn=false;++meleeInvalid;break;}if(!found){++meleeMisses;break;}
   EspEntity nextTarget{};float best=kChainHopRadius;
   for(const auto&pair:espRoster){const auto&e=pair.second.entity;V root{};
    if(!MeleeHostile(e,root)||std::find(hitAlready.begin(),hitAlready.end(),e.complete)!=hitAlready.end())continue;
    float distance=Distance(currentRoot,root);if(distance>.1f&&distance<best){nextTarget=e;nextTarget.root=root;best=distance;}
   }
   if(!nextTarget.state)break;V nextHead{};if(!NativeMeleePoint(nextTarget.complete,nextTarget.root,nextHead))break;
   currentTarget=nextTarget;currentHead=nextHead;currentRoot=nextTarget.root;
  }
                                                                                    
  out->count=keptTotal;o.result=(int)keptTotal;o.redirected=true;
 }else if(redirect){
  o.result=meleeOriginal(owner,&o.relocated,forward,left,right,radius,out,extra);
  ContactList after{};
  if(Read((uintptr_t)out,after)&&after.count<=after.capacity&&after.count<=256&&(!after.count||after.data)){
   unsigned kept=0;
   for(unsigned i=0;i<after.count;i++){
    uintptr_t victim=0;if(!Read((uintptr_t)after.data+i*0x30,victim)){meleeOn=false;++meleeInvalid;break;}
    if(victim==chosen.complete){if(kept!=i)memmove(after.data+kept*0x30,after.data+i*0x30,0x30);++kept;}
   }
   out->count=kept;o.result=(int)kept;o.redirected=true;
   if(kept){++meleeRedirects;if(chosen.humanType)++hostileHumanContacts;}else ++meleeMisses;
  }else{meleeOn=false;++meleeInvalid;}
 }else o.result=meleeOriginal(owner,origin,forward,left,right,radius,out,extra);
 ContactList observed{};
 if(out&&Read((uintptr_t)out,observed)&&observed.count<=observed.capacity&&observed.count<=256){
  o.count=observed.count;
  if(observed.count&&observed.data){Read((uintptr_t)observed.data,o.first);Read((uintptr_t)observed.data+8,o.mesh);Read((uintptr_t)observed.data+0x18,o.impact);}
  if(geometry)meleePrerequisites=true;
 }else{meleeOn=false;++meleeInvalid;}
 QueryPerformanceCounter(&end);o.micros=(end.QuadPart-begin.QuadPart)*1000000/freq.QuadPart;
 if(o.micros>50000){meleeOn=false;meleeChainOn=false;++meleeInvalid;}
 Observe(o);return o.result;
}
void LogObservation(const Observation&o){
 std::ofstream f(LogPath(),std::ios::app);
 auto vec=[&](const char*name,V v){f<<','<<name<<'='<<v.x<<'/'<<v.y<<'/'<<v.z;};
 f<<o.tick<<",MELEE_QUERY,n="<<o.query<<",enabled="<<meleeOn.load()<<",owner=0x"<<std::hex<<o.owner<<",target=0x"<<o.target<<",first=0x"<<o.first<<std::dec<<",generation="<<o.generation<<",radius="<<o.radius<<",distance="<<o.distance<<",health="<<o.health<<",pose="<<o.pose<<",redirected="<<o.redirected<<",result="<<o.result<<",count="<<o.count<<",mesh="<<o.mesh<<",us="<<o.micros;
 vec("origin",o.origin);vec("forward",o.forward);vec("left",o.left);vec("right",o.right);vec("camera",o.camera);vec("root",o.root);vec("head",o.head);vec("relocated",o.relocated);vec("impact",o.impact);f<<",hostile_human_contacts="<<hostileHumanContacts.load()<<'\n';
}
DWORD WINAPI MeleeWorker(void*){
 RoutineDiagnostics::Start("melee");RoutineDiagnostics::WorkerScope diagnosticScope;RoutineDiagnostics::Module(L"DideMeleeNative.dll","melee");RoutineDiagnostics::Pump(true);
 gameBase=(uintptr_t)GetModuleHandleW(L"gamedll_x64_rwdi.dll");if(!gameBase)return 0;
 auto dos=(IMAGE_DOS_HEADER*)gameBase;auto nt=(IMAGE_NT_HEADERS64*)(gameBase+dos->e_lfanew);gameSize=nt->OptionalHeader.SizeOfImage;
 if(!TrueGodBuildMatches()){Log("MELEE_STOP_BUILD_MISMATCH");return 0;}
 unsigned char expected[16]={0x48,0x8B,0xC4,0x4C,0x89,0x48,0x20,0x4C,0x89,0x40,0x18,0x48,0x89,0x50,0x10,0x48},actual[16]{};
 void*target=(void*)(gameBase+0x7A8FF0);
 if(!ReadBytes((uintptr_t)target,actual,16)||memcmp(actual,expected,16)){Log("MELEE_STOP_HOOK_CONFLICT");return 0;}
 if(MH_Initialize()!=MH_OK||MH_CreateHook(target,(void*)&MeleeQuery,(void**)&meleeOriginal)!=MH_OK||MH_EnableHook(target)!=MH_OK){Log("MELEE_STOP_INSTALL_FAILED");return 0;}
 meleeReady=true;Log("MELEE_READY_DISABLED_F10_TOGGLE_F11_STOP");
 bool oldToggle=false,oldStop=false;unsigned lastObservation=0,lastRate=0;auto nextRate=GetTickCount64()+1000;
 while(!meleeStop){
  RoutineDiagnostics::Pump();static unsigned long long nextSummary=0;if(GetTickCount64()>=nextSummary){nextSummary=GetTickCount64()+10000;char row[400]{};sprintf_s(row,"MELEE_STATUS ready=%d enabled=%d chain=%d one_shot=%d queries=%u redirects=%u misses=%u invalid=%u human_contacts=%u dropped=%llu io_errors=%llu",(int)meleeReady.load(),(int)meleeOn.load(),(int)meleeChainOn.load(),(int)meleeOneShotOn.load(),meleeQueries.load(),meleeRedirects.load(),meleeMisses.load(),meleeInvalid.load(),hostileHumanContacts.load(),RoutineDiagnostics::dropped.load(),RoutineDiagnostics::errors.load());RoutineDiagnostics::Emit(row);}
  bool mods=ForegroundGame()&&(GetAsyncKeyState(VK_CONTROL)&0x8000)&&(GetAsyncKeyState(VK_SHIFT)&0x8000);
  bool toggle=mods&&(GetAsyncKeyState(VK_F10)&0x8000),stopKey=mods&&(GetAsyncKeyState(VK_F11)&0x8000);
  if(toggle&&!oldToggle){if(meleeOn){meleeOn=false;Log("MELEE_DISABLED");}else {meleeOn=true;Log("MELEE_ENABLED");}MessageBeep(MB_OK);}
  if(stopKey&&!oldStop){meleeOn=false;meleeStop=true;}
  oldToggle=toggle;oldStop=stopKey;
  Observation o{};AcquireSRWLockShared(&observationLock);o=latestObservation;ReleaseSRWLockShared(&observationLock);
  if(o.query!=lastObservation){LogObservation(o);lastObservation=o.query;}
  auto now=GetTickCount64();if(now>=nextRate){unsigned n=meleeQueries.load();if(n-lastRate>60){meleeOn=false;meleeChainOn=false;Log("MELEE_STOP_EXCESSIVE_QUERY_RATE");}lastRate=n;nextRate=now+1000;}
  Sleep(10);
 }
 meleeOn=false;meleeChainOn=false;meleeOneShotOn=false;
 meleeReady=false;MH_DisableHook(target);                                                       
 Log("MELEE_STOPPED_NATIVE_QUERY_RESTORED");return 0;
}
}
extern "C" __declspec(dllexport) void DideMeleeSetTick(GameTickFn fn){meleeGameTick=fn;}
extern "C" __declspec(dllexport) int DideMeleeState(){return !meleeReady.load()?-1:meleeOn.load()?1:0;}
extern "C" __declspec(dllexport) int DideMeleeSet(int value){
 if(!value){meleeOn=false;return 0;}
 if(!meleeReady.load()||meleeStop.load())return -1;
                                                                     
                                                                       
                                                                
 meleeOn=true;return 1;
}
extern "C" __declspec(dllexport) int DideMeleeOneShotState(){return !meleeReady.load()?-1:meleeOneShotOn.load()?1:0;}
extern "C" __declspec(dllexport) int DideMeleeOneShotSet(int value){
 if(!value){meleeOneShotOn=false;return 0;}
 if(!meleeReady.load()||meleeStop.load())return -1;
 meleeOneShotOn=true;return 1;
}
extern "C" __declspec(dllexport) int DideMeleeChainState(){return !meleeReady.load()?-1:meleeChainOn.load()?1:0;}
extern "C" __declspec(dllexport) int DideMeleeChainSet(int value){
 if(!value){meleeChainOn=false;return 0;}
 if(!meleeReady.load()||meleeStop.load())return -1;
                                                          
 meleeChainOn=true;return 1;
}
extern "C" __declspec(dllexport) void DideMeleeStop(){meleeOn=false;meleeChainOn=false;meleeOneShotOn=false;meleeStop=true;}
BOOL APIENTRY DllMain(HMODULE module,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(module);auto thread=CreateThread(nullptr,0,MeleeWorker,nullptr,0,nullptr);if(thread)CloseHandle(thread);}return TRUE;}
