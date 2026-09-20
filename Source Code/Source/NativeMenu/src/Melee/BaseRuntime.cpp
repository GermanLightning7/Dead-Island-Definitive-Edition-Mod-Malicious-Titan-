#include "../AiAllegiance.h"
#include <windows.h>
#include "../RoutineDiagnostics.h"
#include <wincrypt.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <MinHook.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <intrin.h>
#include <unordered_map>
#include <vector>
#include <tlhelp32.h>

namespace {
constexpr uintptr_t RayRva=0x3804D0,PrimaryReturnRva=0x37E9F3,GameRootRva=0x12822A8,AmmoRootRva=0x12822F8,CameraViewRva=0x12822D8;
constexpr uintptr_t ImpactRva=0x37F520,MeleeEventRva=0x7A0F30,RocketCreateIatRva=0xC7DA68,RocketActivateIatRva=0xC7D928,RocketRttiRva=0x12C9FA0,RocketInitRva=0x5FA450,RocketDetonateRva=0x5FA110;
constexpr uintptr_t RocketVtableRva=0xE896A8,RocketIgsVtableRva=0xE89C78;
constexpr uintptr_t GodModeRva=0x672774,ArenaVtableRva=0xCC3DE8,CompletionQueryRva=0x2685A0;
constexpr uintptr_t PlayerManagerOffset=0x4A0,StatsChainOffset=0xC0,StatsReadyOffset=0x60,StatsObjOffset=0x68;
constexpr uintptr_t CashPrimaryOffset=0xBC8,CashMirrorOffset=0x1C30,CashCategoryOffset=0xBD0;
constexpr uintptr_t AiDebugGate1Rva=0x1281718,AiDebugGate2Rva=0x12817A8;
constexpr uintptr_t ZombieState1Rva=0xD83CF8,ZombieState2Rva=0xD84538,HumanStateRva=0xD6DC08;
constexpr uintptr_t RendererGlobalRva=0xA51C98,DebugPassRva=0x76B730,DrawLineSlot=0x280;
constexpr size_t PatchLength=15;
struct V{float x,y,z;};
struct LineV{float x,y,z,w;};
struct V4{float r,g,b,a;};
struct D3DVertex{float x,y;float r,g,b,a;};
struct M34{float v[12];};
struct Target{uintptr_t address;int generation;V position;float distance;unsigned long long tick;};
struct EspEntity{uintptr_t state{};uintptr_t complete{};int generation{};V root{};bool humanType{};bool human{};bool hostile{};EspEntity()=default;EspEntity(uintptr_t s,uintptr_t c,int g,V r,bool isHuman,bool isHostile):state(s),complete(c),generation(g),root(r),humanType(isHuman),human(isHuman&&!isHostile),hostile(isHostile){}};
constexpr unsigned MaxEspEntities=12;
struct EspFrame{EspEntity entities[MaxEspEntities];unsigned count;unsigned long long tick;};
struct EspRosterItem{EspEntity entity;float distance;unsigned long long lastSeen;};
struct EspRenderTrack{int generation;V display;int consecutiveLargeSteps;unsigned long long lastSeen;};
struct Control{unsigned magic,version;volatile LONG ready,enabled;unsigned pid,reserved;volatile LONG64 redirects,refusals;volatile LONG skeletonIntervalMs,maxDistanceTenths,styleScaleHundredths;volatile LONG zombieArgb,friendlyArgb,hostileArgb;volatile LONG espEnabled,boxesEnabled,skeletonsEnabled;};
struct Life{int generation;unsigned long long lastScan;};
struct CallerSlot{std::atomic<uintptr_t> caller{0};std::atomic<unsigned long> count{0};};
using RaytraceFn=unsigned char(*)(void*,V*,V*,void*,void*,char);
using ImpactFn=unsigned long long(*)(void*,void*,V*,V*);
using MeleeEventFn=void(*)(void*,int,float,char);
using RocketCreateFn=void*(*)(void*,void*,bool,void*);
using RocketActivateFn=void(*)(void*);
using RocketRootFn=uintptr_t(*)(void*);
using RocketVecFn=void(*)(void*,V*);
using RocketInitFn=void(*)(void*,void*,V*,V*,V*,float,float);
using RocketDetonateFn=void(*)(void*,unsigned);
using DebugPassFn=void(*)(void*,void*,void*);
using DrawLineFn=void(*)(void*,const LineV*,const LineV*,const V4*,unsigned);

RaytraceFn original=nullptr;unsigned char*site=nullptr,saved[PatchLength]{};
ImpactFn originalImpact=nullptr;
MeleeEventFn originalMeleeEvent=nullptr;
DebugPassFn originalDebugPass=nullptr;unsigned char*debugSite=nullptr,debugSaved[20]{};
uintptr_t engineBase=0;size_t engineSize=0;std::atomic_bool espEnabled{false};
std::atomic_bool espBoxesEnabled{false},espSkeletonsEnabled{false};
uintptr_t cachedRenderer=0,cachedLineAddress=0;
std::atomic<unsigned long> espRenderThread{0};std::atomic_flag espDrawActive=ATOMIC_FLAG_INIT;
SRWLOCK espFrameLock=SRWLOCK_INIT;EspFrame currentEspFrame{};std::atomic<unsigned long long> lastEspBatch{0};
std::atomic<unsigned long long> debugPasses{0},espBatches{0},espLines{0},espProjectionRejects{0},espThreadRejects{0},espEntityRejects{0},espReentryRejects{0};
std::atomic_bool enabled{false},stop{false};std::atomic_long redirects{0},refusals{0};
using PresentFn=HRESULT(__stdcall*)(IDXGISwapChain*,UINT,UINT);
PresentFn originalPresent=nullptr;std::atomic<unsigned long long> presentCalls{0},presentDraws{0};
std::atomic<unsigned long long> presentForegroundRejects{0},presentDescRejects{0},presentDeviceRejects{0},presentResourceRejects{0},presentCameraRejects{0},presentFrameRejects{0},presentEntityRejects{0},presentProjectionRejects{0},presentHeightRejects{0},presentVertexFrames{0};
std::atomic<unsigned long long> poseHeadSuccess{0},poseHeadFallback{0};
std::atomic<unsigned long long> skeletonSamples{0},skeletonSampleSuccess{0},skeletonSampleMicros{0},skeletonSampleMaxMicros{0},skeletonSegments{0};
std::atomic<unsigned long long> targetMarkerFrames{0},targetSnaplineFrames{0},espSnaplineSegments{0};
std::atomic<unsigned> presentWidth{0},presentHeight{0};
std::atomic_bool menuOpen{false};
std::atomic_int menuIndex{0};
std::atomic_int menuPage{0};
std::atomic_bool fovAimEnabled{false};
std::atomic_int fovRadiusPixels{250};
std::atomic_bool crosshairPriorityEnabled{false},targetMarkerEnabled{false},targetSnaplineEnabled{false},espSnaplinesEnabled{false};
std::atomic_bool godModeEnabled{false},externalAimEnabled{false},externalEspEnabled{false};
std::atomic_int customCash{10000};
std::atomic_int hotkeys[5]{{VK_F11},{VK_F5},{VK_F6},{VK_INSERT},{VK_END}};
std::atomic_int captureHotkey{-1};
std::atomic_int ammoEditField{-1},ammoEditValue{0};
std::atomic_bool ammoEditHasDigits{false},ammoDisplayValid{false};
std::atomic_int ammoDisplayLoaded{0},ammoDisplayCapacity{0},ammoDisplayReserve{0},ammoDisplayType{-1};
uintptr_t ammoEditManager=0,ammoEditItem=0,ammoEditDescriptor=0;int ammoEditType=-1;
std::atomic_int weaponShotTypeMode{0};std::atomic_bool alwaysCriticalEnabled{false},explosiveSilentAimEnabled{false};
std::unordered_map<uintptr_t,float> criticalChanceOriginals;
struct FirearmShotOriginal{int shootMode=0,bulletsPerShot=0,ammoType=0;};
std::unordered_map<uintptr_t,FirearmShotOriginal> firearmShotOriginals;
std::atomic_int weaponProfileType{0};std::atomic_bool weaponFullAutoEnabled{false},weaponIntervalEnabled{false};
std::atomic_bool kickRocketEnabled{false};std::atomic<uintptr_t> cachedRocketWorld{0},cachedRocketControl{0};std::atomic<unsigned long long> cachedRocketAt{0},lastKickRocketAt{0};
std::atomic_int weaponIntervalHundredths{10};
struct WeaponProfile{float physics=0,force=0,legs=0,arms=0,headCut=0,headSmash=0,damageSize=0;unsigned ragdoll=0;unsigned char knockdown=0;int bullets=1;uintptr_t animPrefix=0;};
WeaponProfile learnedWeaponProfiles[3]{};bool learnedWeaponProfileValid[3]{};
std::unordered_map<uintptr_t,WeaponProfile> weaponProfileOriginals;
std::unordered_map<uintptr_t,int> weaponFullAutoOriginals;
std::unordered_map<uintptr_t,float> weaponIntervalOriginals;
HMODULE selfModule=nullptr;
uintptr_t cachedArena=0;
char menuStatus[96]="READY";
LONG themeColors[6]={(LONG)0xF20A0A0C,(LONG)0xFFEBEBF0,(LONG)0xFFDC2323,(LONG)0xFFDC2323,(LONG)0xFF823CBE,(LONG)0xFFE6DC1E};
PROCESS_INFORMATION managerProcess{},bridgeProcess{},overlayProcess{};
ID3D11VertexShader*d3dVs=nullptr;ID3D11PixelShader*d3dPs=nullptr;ID3D11InputLayout*d3dLayout=nullptr;ID3D11Buffer*d3dVertices=nullptr;ID3D11BlendState*d3dBlend=nullptr;ID3D11DepthStencilState*d3dDepth=nullptr;ID3D11RasterizerState*d3dRaster=nullptr;
std::atomic<unsigned long long> targetSequence{0};Target currentTarget{};
HANDLE toggleEvent=nullptr,restoreEvent=nullptr,espToggleEvent=nullptr,espBoxesToggleEvent=nullptr,espSkeletonsToggleEvent=nullptr,controlMap=nullptr;Control*control=nullptr;
uintptr_t gameBase=0;size_t gameSize=0;unsigned long long scanNumber=0;
std::unordered_map<uintptr_t,Life> lives;
std::unordered_map<uintptr_t,EspRosterItem> espRoster;
std::unordered_map<uintptr_t,EspRenderTrack> espRenderTracks;
struct PoseHeadCache{int generation;V head;unsigned long long sampled,lastGood;};
std::unordered_map<uintptr_t,PoseHeadCache> poseHeadCache;
struct SkeletonSegment{V a,b;};
struct SkeletonCache{int generation;std::vector<SkeletonSegment>segments;unsigned long long sampled,lastGood;V root;uintptr_t pose;};
std::unordered_map<uintptr_t,SkeletonCache> skeletonCache;
thread_local float currentLineThickness=2.2f;
struct PendingRedirectExplosion{V position{};unsigned long long tick{};bool valid{};};
thread_local PendingRedirectExplosion pendingRedirectExplosion{};
std::atomic<unsigned long long> redirectedExplosions{0},redirectedExplosionRefusals{0};
float NativeMaxDistance(){return control?std::clamp(control->maxDistanceTenths/10.f,10.f,800.f):400.f;}
float NativeStyleScale(){return control?std::clamp(control->styleScaleHundredths/100.f,0.f,2.f):0.f;}
void NativeColor(const EspEntity&entity,float&r,float&g,float&b){LONG packed=!entity.humanType?(control?control->zombieArgb:(LONG)0xEBFF2323):entity.hostile?(control?control->hostileArgb:(LONG)0xEBFF8C1E):(control?control->friendlyArgb:(LONG)0xEB00DCFF);r=((packed>>16)&255)/255.f;g=((packed>>8)&255)/255.f;b=(packed&255)/255.f;}
constexpr unsigned AiDebugBits=0x10002;
std::unordered_map<uintptr_t,unsigned> aiDebugOriginals;
uintptr_t aiDebugLevel=0;unsigned aiDebugLevelOriginal=0;bool aiDebugLevelSaved=false;
unsigned char aiDebugGate1Original=0,aiDebugGate2Original=0;bool aiDebugGatesSaved=false;
CallerSlot callerSlots[64];std::atomic_bool captureActive{false};std::atomic<unsigned long long> captureUntil{0};int captureLabel=0;

std::filesystem::path LogPath(){wchar_t p[MAX_PATH]{};GetEnvironmentVariableW(L"LOCALAPPDATA",p,MAX_PATH);auto d=std::filesystem::path(p)/L"JentaSpecialEdition"/L"MeleeSilentAim";std::filesystem::create_directories(d);return d/L"melee_silent_aim.txt";}
void Log(const char*s){RoutineDiagnostics::Emit(s);std::ofstream f(LogPath(),std::ios::app);f<<GetTickCount64()<<','<<s<<",redirects="<<redirects.load()<<",refusals="<<refusals.load()<<'\n';}
void RecordCaller(uintptr_t caller){if(!captureActive.load(std::memory_order_relaxed))return;for(auto&slot:callerSlots){auto found=slot.caller.load(std::memory_order_relaxed);if(found==caller){slot.count.fetch_add(1,std::memory_order_relaxed);return;}if(!found&&slot.caller.compare_exchange_strong(found,caller,std::memory_order_relaxed)){slot.count.store(1,std::memory_order_relaxed);return;}}}
void ArmCapture(){captureActive=false;for(auto&slot:callerSlots){slot.caller=0;slot.count=0;}captureLabel=captureLabel%3+1;captureUntil=GetTickCount64()+1500;captureActive=true;std::ofstream f(LogPath(),std::ios::app);f<<GetTickCount64()<<",MELEE_CAPTURE_ARM,label="<<captureLabel<<",meaning="<<(captureLabel==1?"empty_swing":captureLabel==2?"melee_hit":"enemy_attack")<<'\n';MessageBeep(MB_OK);}
void FinishCapture(){captureActive=false;std::ofstream f(LogPath(),std::ios::app);f<<GetTickCount64()<<",MELEE_CAPTURE_RESULT,label="<<captureLabel;for(auto&slot:callerSlots){auto caller=slot.caller.load();auto count=slot.count.load();if(caller&&count)f<<",rva_0x"<<std::hex<<(caller-gameBase)<<std::dec<<'='<<count;}f<<'\n';MessageBeep(MB_ICONASTERISK);}

template<class T>bool Read(uintptr_t address,T&value){__try{memcpy(&value,(const void*)address,sizeof(T));return true;}__except(EXCEPTION_EXECUTE_HANDLER){memset(&value,0,sizeof(T));return false;}}
template<class T>bool Write(uintptr_t address,const T&value){__try{memcpy((void*)address,&value,sizeof(T));return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool ReadBytes(uintptr_t address,void*out,size_t size){__try{memcpy(out,(const void*)address,size);return true;}__except(EXCEPTION_EXECUTE_HANDLER){memset(out,0,size);return false;}}
bool Pointer(uintptr_t address,uintptr_t&value){return Read(address,value)&&value>=0x10000;}
bool InGame(uintptr_t value){return value>=gameBase&&value<gameBase+gameSize;}
bool Finite(V value){return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z)&&fabsf(value.x)<=100000&&fabsf(value.y)<=100000&&fabsf(value.z)<=100000&&(value.x!=0||value.y!=0||value.z!=0);}
float Distance(V a,V b){float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return sqrtf(x*x+y*y+z*z);}
void SetStatus(const char*text){strncpy_s(menuStatus,text,_TRUNCATE);Log(text);}
struct NativeSettingsV1{unsigned magic=0x53444944,version=1;LONG theme[6]{};LONG zombie=0,friendly=0,hostile=0,interval=8,distance=4000,style=0,keys[5]{};};
struct NativeSettings{unsigned magic=0x53444944,version=3;LONG theme[6]{};LONG zombie=0,friendly=0,hostile=0,interval=8,distance=4000,style=0,keys[5]{};LONG fovEnabled=0,fovRadius=250,crosshairPriority=0,targetMarker=0,targetSnapline=0,espSnaplines=0;};
std::filesystem::path SettingsPath(){wchar_t p[MAX_PATH]{};GetEnvironmentVariableW(L"LOCALAPPDATA",p,MAX_PATH);auto d=std::filesystem::path(p)/L"JentaSpecialEdition"/L"NativeMenu";std::error_code e;std::filesystem::create_directories(d,e);return d/L"settings.bin";}
void SaveNativeSettings(){if(!control)return;NativeSettings s{};memcpy(s.theme,themeColors,sizeof(themeColors));s.zombie=control->zombieArgb;s.friendly=control->friendlyArgb;s.hostile=control->hostileArgb;s.interval=control->skeletonIntervalMs;s.distance=control->maxDistanceTenths;s.style=control->styleScaleHundredths;for(int i=0;i<5;i++)s.keys[i]=hotkeys[i].load();s.fovEnabled=fovAimEnabled?1:0;s.fovRadius=fovRadiusPixels.load();s.crosshairPriority=crosshairPriorityEnabled?1:0;s.targetMarker=targetMarkerEnabled?1:0;s.targetSnapline=targetSnaplineEnabled?1:0;s.espSnaplines=espSnaplinesEnabled?1:0;std::ofstream f(SettingsPath(),std::ios::binary|std::ios::trunc);if(f)f.write((char*)&s,sizeof(s));}
void LoadNativeSettings(){if(!control)return;NativeSettingsV1 base{};std::ifstream f(SettingsPath(),std::ios::binary);if(!f||!f.read((char*)&base,sizeof(base))||base.magic!=0x53444944||base.version<1||base.version>3)return;memcpy(themeColors,base.theme,sizeof(themeColors));control->zombieArgb=base.zombie;control->friendlyArgb=base.friendly;control->hostileArgb=base.hostile;control->skeletonIntervalMs=std::clamp<LONG>(base.interval,4,50);control->maxDistanceTenths=std::clamp<LONG>(base.distance,100,8000);control->styleScaleHundredths=std::clamp<LONG>(base.style,0,200);for(int i=0;i<5;i++)if(base.keys[i]>0&&base.keys[i]<255)hotkeys[i]=base.keys[i];if(base.version>=2){LONG values[2]{};if(!f.read((char*)values,sizeof(values)))return;fovAimEnabled=values[0]!=0;fovRadiusPixels=std::clamp<LONG>(values[1],50,800);}if(base.version>=3){LONG values[4]{};if(f.read((char*)values,sizeof(values))){crosshairPriorityEnabled=values[0]!=0;targetMarkerEnabled=values[1]!=0;targetSnaplineEnabled=values[2]!=0;espSnaplinesEnabled=values[3]!=0;}}}
bool WriteCode(uintptr_t address,const void*bytes,size_t size){DWORD old{};if(!VirtualProtect((void*)address,size,PAGE_EXECUTE_READWRITE,&old))return false;memcpy((void*)address,bytes,size);FlushInstructionCache(GetCurrentProcess(),(void*)address,size);DWORD ignored{};VirtualProtect((void*)address,size,old,&ignored);return true;}
#include "TrueGodMode.inl"
bool ResolveStats(uintptr_t&stats){uintptr_t game=0,manager=0,chain=0;LONG64 ready=0;return Pointer(gameBase+GameRootRva,game)&&Pointer(game+PlayerManagerOffset,manager)&&Pointer(manager+StatsChainOffset,chain)&&Read(chain+StatsReadyOffset,ready)&&ready&&Pointer(chain+StatsObjOffset,stats);}
bool GiveCashNative(int amount){uintptr_t stats=0;if(!ResolveStats(stats)){SetStatus("CASH REFUSED - LOAD A SAVE");return false;}const uintptr_t offsets[3]={CashPrimaryOffset,CashMirrorOffset,CashCategoryOffset};int before[3]{},after[3]{};for(int i=0;i<3;i++)if(!Read(stats+offsets[i],before[i])){SetStatus("CASH READ FAILED");return false;}for(int i=0;i<3;i++){after[i]=before[i]+amount;if(!Write(stats+offsets[i],after[i])){SetStatus("CASH WRITE FAILED");return false;}}for(int i=0;i<3;i++){int check=0;if(!Read(stats+offsets[i],check)||check!=after[i]){SetStatus("CASH VERIFY FAILED");return false;}}SetStatus("CASH APPLIED AND VERIFIED");return true;}
bool ValidArenaNative(uintptr_t arena,int&wave){uintptr_t table=0,children=0;int a=0,b=0,slots=0;if(!Read(arena,table)||table!=gameBase+ArenaVtableRva||!Read(arena+0x25C,a)||!Read(arena+0x260,b)||a!=b||a<1||a>=10000||!Read(arena+0x258,slots)||slots<1||slots>1024||!Pointer(arena+0x2A0,children))return false;wave=a;return true;}
bool FindArenaNative(uintptr_t&arena,int&wave){if(cachedArena&&ValidArenaNative(cachedArena,wave)){arena=cachedArena;return true;}SYSTEM_INFO si{};GetSystemInfo(&si);MEMORY_BASIC_INFORMATION mbi{};for(uintptr_t p=0x10000;p<(uintptr_t)si.lpMaximumApplicationAddress;){if(!VirtualQuery((void*)p,&mbi,sizeof(mbi)))break;bool readable=mbi.State==MEM_COMMIT&&!(mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS));if(readable){auto*data=(uintptr_t*)mbi.BaseAddress;size_t count=mbi.RegionSize/sizeof(uintptr_t);__try{for(size_t i=0;i<count;i++)if(data[i]==gameBase+ArenaVtableRva){uintptr_t candidate=(uintptr_t)&data[i];if(ValidArenaNative(candidate,wave)){if(arena&&arena!=candidate){SetStatus("ARENA REFUSED - MULTIPLE MATCHES");return false;}arena=candidate;}}}__except(EXCEPTION_EXECUTE_HANDLER){}}p=(uintptr_t)mbi.BaseAddress+mbi.RegionSize;}cachedArena=arena;return arena!=0;}
bool SkipWaveNative(){SetStatus("SKIP WAVE DISABLED - BLOCKING SCAN REMOVED");return false;}
std::filesystem::path BundleRoot(){wchar_t path[MAX_PATH]{};GetModuleFileNameW(selfModule,path,MAX_PATH);return std::filesystem::path(path).parent_path().parent_path();}
bool ChildRunning(PROCESS_INFORMATION&pi){if(!pi.hProcess)return false;if(WaitForSingleObject(pi.hProcess,0)==WAIT_TIMEOUT)return true;CloseHandle(pi.hProcess);if(pi.hThread)CloseHandle(pi.hThread);pi={};return false;}
void StopChild(PROCESS_INFORMATION&pi){if(!pi.hProcess)return;if(WaitForSingleObject(pi.hProcess,0)==WAIT_TIMEOUT)TerminateProcess(pi.hProcess,0);WaitForSingleObject(pi.hProcess,1000);CloseHandle(pi.hProcess);if(pi.hThread)CloseHandle(pi.hThread);pi={};}
bool StartChild(const std::filesystem::path&exe,const wchar_t*args,PROCESS_INFORMATION&pi){if(!std::filesystem::exists(exe))return false;std::wstring command=L"\""+exe.wstring()+L"\"";if(args&&*args){command+=L" ";command+=args;}STARTUPINFOW si{sizeof(si)};std::vector<wchar_t>mutableCommand(command.begin(),command.end());mutableCommand.push_back(0);return CreateProcessW(exe.c_str(),mutableCommand.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,exe.parent_path().c_str(),&si,&pi)!=FALSE;}
void ToggleExternalAimNative(){if(ChildRunning(bridgeProcess)){StopChild(bridgeProcess);if(!ChildRunning(overlayProcess))StopChild(managerProcess);externalAimEnabled=false;SetStatus("LEGACY TARGET PUBLISHER STOPPED");return;}enabled=false;StopChild(overlayProcess);StopChild(managerProcess);auto root=BundleRoot();if(!StartChild(root/L"Manager"/L"SpawnMonitor.exe",L"--manager-live",managerProcess)){SetStatus("SPAWN MONITOR START FAILED");return;}Sleep(750);if(!StartChild(root/L"Bridge"/L"TargetBridge.exe",L"",bridgeProcess)){StopChild(managerProcess);SetStatus("TARGET BRIDGE START FAILED");return;}externalAimEnabled=true;SetStatus("LEGACY PUBLISHER ON - DIAGNOSTIC ONLY");}
void ToggleExternalEspNative(){if(ChildRunning(overlayProcess)){StopChild(overlayProcess);if(!ChildRunning(bridgeProcess))StopChild(managerProcess);externalEspEnabled=false;SetStatus("EXTERNAL ESP STOPPED");return;}espEnabled=false;StopChild(bridgeProcess);StopChild(managerProcess);auto root=BundleRoot();if(!StartChild(root/L"Manager"/L"SpawnMonitor.exe",L"",managerProcess)){SetStatus("SPAWN MONITOR START FAILED");return;}Sleep(750);if(!StartChild(root/L"Overlay"/L"ExternalEspOverlay.exe",L"",overlayProcess)){StopChild(managerProcess);SetStatus("EXTERNAL OVERLAY START FAILED");return;}externalEspEnabled=true;SetStatus("EXTERNAL ESP ON - NATIVE ESP OFF");}

bool TypeContains(uintptr_t vtable,const char*needle,int&objectOffset){
 uintptr_t col=0;if(!InGame(vtable)||!Read(vtable-8,col)||!InGame(col))return false;
 unsigned char locator[24]{};if(!ReadBytes(col,locator,sizeof(locator)))return false;memcpy(&objectOffset,locator+4,4);
 unsigned hierarchyRva=0;memcpy(&hierarchyRva,locator+16,4);unsigned char hierarchy[16]{};if(!ReadBytes(gameBase+hierarchyRva,hierarchy,sizeof(hierarchy)))return false;
 int count=0;unsigned arrayRva=0;memcpy(&count,hierarchy+8,4);memcpy(&arrayRva,hierarchy+12,4);if(count<=0||count>256)return false;
 for(int i=0;i<count;i++){unsigned descriptorRva=0,typeRva=0;if(!Read(gameBase+arrayRva+i*4,descriptorRva)||!Read(gameBase+descriptorRva,typeRva))continue;char name[192]{};if(!ReadBytes(gameBase+typeRva+16,name,sizeof(name)-1))continue;if(strstr(name,needle))return true;}
 return false;
}

struct WeaponAmmoState{uintptr_t manager=0,item=0,descriptor=0,reserveAddress=0;int ammoType=-1,reserveAmmoType=-1,loaded=0,capacity=0,reserve=0;};
bool WritableInt(uintptr_t address){MEMORY_BASIC_INFORMATION mbi{};if(!VirtualQuery((void*)address,&mbi,sizeof(mbi))||mbi.State!=MEM_COMMIT||(mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS))||address+sizeof(int)>(uintptr_t)mbi.BaseAddress+mbi.RegionSize)return false;DWORD p=mbi.Protect&0xff;return p==PAGE_READWRITE||p==PAGE_WRITECOPY||p==PAGE_EXECUTE_READWRITE||p==PAGE_EXECUTE_WRITECOPY;}
bool ResolveWeaponAmmo(WeaponAmmoState&state){
 state={};uintptr_t root=0,itemVtable=0,descriptorVtable=0;int objectOffset=0;
 if(!Pointer(gameBase+AmmoRootRva,root)||!Pointer(root+0x628,state.manager)||!Pointer(state.manager+0xFF0,state.item)||!Read(state.item,itemVtable)||!TypeContains(itemVtable,"InventoryItem",objectOffset)||objectOffset!=0)return false;
 if(!Pointer(state.item+0x68,state.descriptor)||!Read(state.descriptor,descriptorVtable)||!TypeContains(descriptorVtable,"ItemDescFirearm",objectOffset)||objectOffset!=0)return false;
 if(!Read(state.descriptor+0x598,state.ammoType)||state.ammoType<0||state.ammoType>7)return false;
 state.reserveAmmoType=state.ammoType;auto savedShot=firearmShotOriginals.find(state.descriptor);if(weaponShotTypeMode.load()==2&&savedShot!=firearmShotOriginals.end())state.reserveAmmoType=savedShot->second.ammoType;
 state.reserveAddress=state.manager+0x117C+(uintptr_t)state.reserveAmmoType*4;
 return Read(state.item+0x50,state.loaded)&&Read(state.descriptor+0x59C,state.capacity)&&Read(state.reserveAddress,state.reserve)&&state.loaded>=0&&state.capacity>=0&&state.reserve>=0;
}
void RefreshWeaponAmmo(){WeaponAmmoState state{};bool valid=ResolveWeaponAmmo(state);ammoDisplayValid=valid;if(valid){ammoDisplayLoaded=state.loaded;ammoDisplayCapacity=state.capacity;ammoDisplayReserve=state.reserve;ammoDisplayType=state.ammoType;}else ammoDisplayType=-1;}
bool BeginAmmoEdit(int field){WeaponAmmoState state{};if(field<0||field>2||!ResolveWeaponAmmo(state)){SetStatus("AMMO EDIT REFUSED - EQUIP SUPPORTED FIREARM");return false;}ammoEditManager=state.manager;ammoEditItem=state.item;ammoEditDescriptor=state.descriptor;ammoEditType=state.ammoType;ammoEditValue=0;ammoEditHasDigits=false;ammoEditField=field;static const char*prompts[3]={"TYPE MAG AMMO - ENTER APPLY ESC CANCEL","TYPE MAG CAPACITY - ENTER APPLY ESC CANCEL","TYPE RESERVE AMMO - ENTER APPLY ESC CANCEL"};SetStatus(prompts[field]);return true;}
void CancelAmmoEdit(){ammoEditField=-1;ammoEditHasDigits=false;SetStatus("AMMO EDIT CANCELLED");}
bool CommitAmmoEdit(){
 int field=ammoEditField.load(),value=ammoEditValue.load();if(field<0||field>2||!ammoEditHasDigits.load()){SetStatus("AMMO EDIT NEEDS A NUMBER");return false;}
 WeaponAmmoState state{};if(!ResolveWeaponAmmo(state)||state.manager!=ammoEditManager||state.item!=ammoEditItem||state.descriptor!=ammoEditDescriptor||state.ammoType!=ammoEditType){ammoEditField=-1;ammoEditHasDigits=false;SetStatus("AMMO EDIT REFUSED - WEAPON CHANGED");return false;}
 uintptr_t address=field==0?state.item+0x50:field==1?state.descriptor+0x59C:state.reserveAddress;if(!WritableInt(address)||!Write(address,value)){ammoEditField=-1;SetStatus("AMMO WRITE FAILED");return false;}int verify=-1;if(!Read(address,verify)||verify!=value){ammoEditField=-1;SetStatus("AMMO VERIFY FAILED");return false;}
 ammoEditField=-1;ammoEditHasDigits=false;RefreshWeaponAmmo();static const char*results[3]={"MAGAZINE AMMO APPLIED","MAGAZINE CAPACITY APPLIED","RESERVE AMMO APPLIED"};SetStatus(results[field]);return true;
}
bool SameFloat(float a,float b){return memcmp(&a,&b,sizeof(float))==0;}
void RestoreOwnedFloats(std::unordered_map<uintptr_t,float>&originals,uintptr_t offset,float ownedFactor,bool critical){for(const auto&entry:originals){float current=0,owned=critical?1.f:entry.second*ownedFactor;if(Read(entry.first+offset,current)&&SameFloat(current,owned)&&WritableInt(entry.first+offset))Write(entry.first+offset,entry.second);}originals.clear();}
void RestoreAlwaysCritical(){RestoreOwnedFloats(criticalChanceOriginals,0x1A0,1.f,true);}
void RestoreShotType(int ownedMode){for(const auto&entry:firearmShotOriginals){int currentMode=0,currentBullets=0,currentAmmo=0;const int ownedShootMode=ownedMode==3?3:1,ownedBullets=ownedMode==1?12:1,ownedAmmo=ownedMode==2?7:entry.second.ammoType;if(Read(entry.first+0x238,currentMode)&&Read(entry.first+0x23C,currentBullets)&&Read(entry.first+0x598,currentAmmo)&&currentMode==ownedShootMode&&currentBullets==ownedBullets&&currentAmmo==ownedAmmo&&WritableInt(entry.first+0x238)&&WritableInt(entry.first+0x23C)&&WritableInt(entry.first+0x598)){Write(entry.first+0x238,entry.second.shootMode);Write(entry.first+0x23C,entry.second.bulletsPerShot);Write(entry.first+0x598,entry.second.ammoType);}}firearmShotOriginals.clear();}
void MaintainWeaponModifiers();
int WeaponFamilyIndex(int ammo){if(ammo==0||ammo==1)return 0;if(ammo==2||ammo==3||ammo==5||ammo==6)return 1;if(ammo==4)return 2;return -1;}
bool ReadWeaponProfile(uintptr_t d,WeaponProfile&p){unsigned char flags=0;if(!Read(d+0x170,p.physics)||!Read(d+0x174,p.force)||!Read(d+0x17C,p.legs)||!Read(d+0x180,p.arms)||!Read(d+0x184,p.headCut)||!Read(d+0x188,p.headSmash)||!Read(d+0x1A4,p.damageSize)||!Read(d+0x1A8,p.ragdoll)||!Read(d+0x198,flags)||!Read(d+0x23C,p.bullets)||!Read(d+0xE8,p.animPrefix))return false;p.knockdown=flags&2;return std::isfinite(p.physics)&&std::isfinite(p.force)&&std::isfinite(p.legs)&&std::isfinite(p.arms)&&std::isfinite(p.headCut)&&std::isfinite(p.headSmash)&&std::isfinite(p.damageSize)&&p.bullets>=1&&p.bullets<=32&&p.animPrefix>0x10000;}
bool PutWeaponKnockdown(uintptr_t d,unsigned char bit){unsigned char flags=0;return Read(d+0x198,flags)&&Write(d+0x198,(unsigned char)((flags&~2u)|(bit&2u)));}
bool WriteWeaponProfile(uintptr_t d,const WeaponProfile&p){return Write(d+0x170,p.physics)&&Write(d+0x174,p.force)&&Write(d+0x17C,p.legs)&&Write(d+0x180,p.arms)&&Write(d+0x184,p.headCut)&&Write(d+0x188,p.headSmash)&&Write(d+0x1A4,p.damageSize)&&Write(d+0x1A8,p.ragdoll)&&PutWeaponKnockdown(d,p.knockdown)&&Write(d+0x23C,p.bullets)&&Write(d+0xE8,p.animPrefix);}
void RestoreWeaponProfiles(){for(const auto&e:weaponProfileOriginals){WeaponProfile now{};if(ReadWeaponProfile(e.first,now)&&memcmp(&now,&learnedWeaponProfiles[std::clamp(weaponProfileType.load()-1,0,2)],sizeof(now))==0)WriteWeaponProfile(e.first,e.second);}weaponProfileOriginals.clear();}
void RestoreFullAuto(){for(const auto&e:weaponFullAutoOriginals){int now=0;if(Read(e.first+0x238,now)&&now==3)Write(e.first+0x238,e.second);}weaponFullAutoOriginals.clear();}
void RestoreWeaponInterval(){float owned=weaponIntervalHundredths.load()/100.f;for(const auto&e:weaponIntervalOriginals){float now=0;if(Read(e.first+0x220,now)&&SameFloat(now,owned))Write(e.first+0x220,e.second);}weaponIntervalOriginals.clear();}
void SetWeaponProfileType(int type){int next=std::clamp(type,0,3);RestoreWeaponProfiles();weaponProfileType=next;if(next)MaintainWeaponModifiers();static const char*labels[4]={"SHOT/ANIMATION NORMAL","SHOT/ANIMATION PISTOL","SHOT/ANIMATION RIFLE","SHOT/ANIMATION SHOTGUN"};SetStatus(labels[next]);}
void SetWeaponFullAuto(bool value){if(!value)RestoreFullAuto();weaponFullAutoEnabled=value;if(value)MaintainWeaponModifiers();SetStatus(value?"FULL AUTO ENABLED - REEQUIP WEAPON":"FULL AUTO RESTORED");}
void SetWeaponInterval(bool value){if(!value)RestoreWeaponInterval();weaponIntervalEnabled=value;if(value)MaintainWeaponModifiers();SetStatus(value?"FIRE INTERVAL OVERRIDE ENABLED":"FIRE INTERVAL RESTORED");}
void RestoreNativeGunStyles(){int oldShotType=std::clamp(weaponShotTypeMode.load(),0,3);RestoreWeaponProfiles();weaponProfileType=0;RestoreShotType(oldShotType);weaponShotTypeMode=0;SetStatus("NATIVE ANIMATIONS AND BULLET STYLE RESTORED");}
void MaintainWeaponModifiers();
void SetAlwaysCritical(bool enabledValue){if(!enabledValue)RestoreAlwaysCritical();alwaysCriticalEnabled=enabledValue;if(enabledValue)MaintainWeaponModifiers();SetStatus(enabledValue?"ALWAYS CRITICAL ENABLED":"ALWAYS CRITICAL DISABLED");}
void SetExplosiveSilentAim(bool enabledValue){explosiveSilentAimEnabled=enabledValue;if(!enabledValue)pendingRedirectExplosion.valid=false;SetStatus(enabledValue?"EXPLOSIVE SILENT AIM ENABLED":"EXPLOSIVE SILENT AIM DISABLED");}
void SetShotTypeMode(int mode){int old=std::clamp(weaponShotTypeMode.load(),0,3),next=std::clamp(mode,0,3);if(next==old)return;RestoreShotType(old);weaponShotTypeMode=next;if(next)MaintainWeaponModifiers();static const char*labels[4]={"NORMAL BEHAVIOR RESTORED","SHOTGUN BEHAVIOR ENABLED","ROCKET BEHAVIOR ENABLED","RIFLE BEHAVIOR ENABLED"};SetStatus(labels[next]);}
void MaintainWeaponModifiers(){
 WeaponAmmoState state{};if(!ResolveWeaponAmmo(state))return;
 if(alwaysCriticalEnabled.load()){auto found=criticalChanceOriginals.find(state.descriptor);float baseValue=0;if(found==criticalChanceOriginals.end()){if(!Read(state.descriptor+0x1A0,baseValue)||!std::isfinite(baseValue)||baseValue<0.f||baseValue>1.f)return;criticalChanceOriginals[state.descriptor]=baseValue;}float current=0,wanted=1.f;if(Read(state.descriptor+0x1A0,current)&&!SameFloat(current,wanted)&&WritableInt(state.descriptor+0x1A0)){Write(state.descriptor+0x1A0,wanted);float verify=0;if(!Read(state.descriptor+0x1A0,verify)||!SameFloat(verify,wanted))SetStatus("ALWAYS CRITICAL VERIFY FAILED");}}
 int shotType=std::clamp(weaponShotTypeMode.load(),0,3);if(shotType>0){auto found=firearmShotOriginals.find(state.descriptor);if(found==firearmShotOriginals.end()){FirearmShotOriginal originalPair{};if(!Read(state.descriptor+0x238,originalPair.shootMode)||!Read(state.descriptor+0x23C,originalPair.bulletsPerShot)||!Read(state.descriptor+0x598,originalPair.ammoType)||originalPair.shootMode<0||originalPair.shootMode>16||originalPair.bulletsPerShot<1||originalPair.bulletsPerShot>128)return;firearmShotOriginals.emplace(state.descriptor,originalPair);found=firearmShotOriginals.find(state.descriptor);}const int wantedMode=shotType==3?3:1,wantedBullets=shotType==1?12:1,wantedAmmo=shotType==2?7:found->second.ammoType;int currentMode=0,currentBullets=0,currentAmmo=0;if(Read(state.descriptor+0x238,currentMode)&&Read(state.descriptor+0x23C,currentBullets)&&Read(state.descriptor+0x598,currentAmmo)&&(currentMode!=wantedMode||currentBullets!=wantedBullets||currentAmmo!=wantedAmmo)&&WritableInt(state.descriptor+0x238)&&WritableInt(state.descriptor+0x23C)&&WritableInt(state.descriptor+0x598)){if(!Write(state.descriptor+0x238,wantedMode)||!Write(state.descriptor+0x23C,wantedBullets)||!Write(state.descriptor+0x598,wantedAmmo)){SetStatus("SHOT TYPE WRITE FAILED");return;}int verifyMode=0,verifyBullets=0,verifyAmmo=0;if(!Read(state.descriptor+0x238,verifyMode)||!Read(state.descriptor+0x23C,verifyBullets)||!Read(state.descriptor+0x598,verifyAmmo)||verifyMode!=wantedMode||verifyBullets!=wantedBullets||verifyAmmo!=wantedAmmo)SetStatus("SHOT TYPE VERIFY FAILED");}}
 int family=WeaponFamilyIndex(state.ammoType),profileType=std::clamp(weaponProfileType.load(),0,3);if(profileType==0&&family>=0&&!learnedWeaponProfileValid[family]){WeaponProfile natural{};if(ReadWeaponProfile(state.descriptor,natural)){learnedWeaponProfiles[family]=natural;learnedWeaponProfileValid[family]=true;}}
 if(profileType>0){int wantedFamily=profileType-1;if(!learnedWeaponProfileValid[wantedFamily]&&family==wantedFamily){WeaponProfile natural{};if(ReadWeaponProfile(state.descriptor,natural)){learnedWeaponProfiles[wantedFamily]=natural;learnedWeaponProfileValid[wantedFamily]=true;}}if(learnedWeaponProfileValid[wantedFamily]){auto found=weaponProfileOriginals.find(state.descriptor);if(found==weaponProfileOriginals.end()){WeaponProfile before{};if(ReadWeaponProfile(state.descriptor,before)){weaponProfileOriginals.emplace(state.descriptor,before);found=weaponProfileOriginals.find(state.descriptor);}}if(found!=weaponProfileOriginals.end()){WeaponProfile now{};const auto&wanted=learnedWeaponProfiles[wantedFamily];if(ReadWeaponProfile(state.descriptor,now)&&memcmp(&now,&wanted,sizeof(now))!=0){if(!WriteWeaponProfile(state.descriptor,wanted))SetStatus("SHOT/ANIMATION WRITE FAILED");}}}else SetStatus("EQUIP THAT WEAPON TYPE ONCE IN NORMAL MODE");}
 if(weaponFullAutoEnabled.load()){auto found=weaponFullAutoOriginals.find(state.descriptor);if(found==weaponFullAutoOriginals.end()){int before=0;if(Read(state.descriptor+0x238,before)&&before>=0&&before<=3){weaponFullAutoOriginals.emplace(state.descriptor,before);found=weaponFullAutoOriginals.find(state.descriptor);}}if(found!=weaponFullAutoOriginals.end()){int now=0;if(Read(state.descriptor+0x238,now)&&now!=3)Write(state.descriptor+0x238,3);}}
 if(weaponIntervalEnabled.load()){auto found=weaponIntervalOriginals.find(state.descriptor);if(found==weaponIntervalOriginals.end()){float before=0;if(Read(state.descriptor+0x220,before)&&std::isfinite(before)&&before>0){weaponIntervalOriginals.emplace(state.descriptor,before);found=weaponIntervalOriginals.find(state.descriptor);}}float wanted=std::clamp(weaponIntervalHundredths.load(),1,100)/100.f;if(found!=weaponIntervalOriginals.end()){float now=0;if(Read(state.descriptor+0x220,now)&&!SameFloat(now,wanted))Write(state.descriptor+0x220,wanted);}}
}

bool FindLevel(uintptr_t&level){
 uintptr_t root=0,manager=0,player=0;if(!Pointer(gameBase+GameRootRva,root)||!Pointer(root+0x4A0,manager)||!Pointer(manager+0xB8,player))return false;
 uintptr_t unique=0;
 for(size_t offset=0;offset<=0xAF8;offset+=8){uintptr_t igs=0,vtable=0,engine=0,back=0,candidate=0;if(!Pointer(player+offset,igs)||!Pointer(igs,vtable)||!InGame(vtable)||!Pointer(igs+0x20,engine)||!Read(engine+0x20,back)||back!=igs||!Pointer(engine+0x48,candidate))continue;if(unique&&unique!=candidate)return false;unique=candidate;}
 level=unique;return level>=0x10000;
}

void RestoreAiDebug(){for(const auto&pair:aiDebugOriginals){unsigned current=0;if(Read(pair.first+0x874,current)){unsigned restored=(current&~AiDebugBits)|(pair.second&AiDebugBits);Write(pair.first+0x874,restored);}}aiDebugOriginals.clear();if(aiDebugLevelSaved&&aiDebugLevel){unsigned current=0;if(Read(aiDebugLevel+0xA80,current)){unsigned restored=(current&~AiDebugBits)|(aiDebugLevelOriginal&AiDebugBits);Write(aiDebugLevel+0xA80,restored);}}if(aiDebugGatesSaved){Write(gameBase+AiDebugGate1Rva,aiDebugGate1Original);Write(gameBase+AiDebugGate2Rva,aiDebugGate2Original);}aiDebugLevel=0;aiDebugLevelOriginal=0;aiDebugLevelSaved=false;aiDebugGatesSaved=false;}
bool EnableAiDebugGates(){if(!aiDebugGatesSaved){if(!Read(gameBase+AiDebugGate1Rva,aiDebugGate1Original)||!Read(gameBase+AiDebugGate2Rva,aiDebugGate2Original))return false;aiDebugGatesSaved=true;}unsigned char on=1;return Write(gameBase+AiDebugGate1Rva,on)&&Write(gameBase+AiDebugGate2Rva,on);}
void ApplyAiDebug(uintptr_t level,uintptr_t complete){if(!EnableAiDebugGates())return;if(!aiDebugLevelSaved||aiDebugLevel!=level){RestoreAiDebug();if(!EnableAiDebugGates())return;unsigned mask=0;if(!Read(level+0xA80,mask))return;aiDebugLevel=level;aiDebugLevelOriginal=mask;aiDebugLevelSaved=true;Write(level+0xA80,mask|AiDebugBits);}if(!aiDebugOriginals.count(complete)){unsigned flags=0;if(!Read(complete+0x874,flags))return;aiDebugOriginals.emplace(complete,flags);}unsigned flags=0;if(Read(complete+0x874,flags))Write(complete+0x874,flags|AiDebugBits);}

bool ReadCamera(V&camera){
 uintptr_t view=0,internal=0;if(!Pointer(gameBase+CameraViewRva,view)||!Pointer(view+0x90390,internal))return false;unsigned char bytes[0x180]{};if(!ReadBytes(internal,bytes,sizeof(bytes)))return false;
 auto F=[&](int offset){float value=0;memcpy(&value,bytes+offset,4);return value;};camera={F(0x4C)+F(0x168),F(0x5C)+F(0x16C),F(0x6C)+F(0x170)};return Finite(camera);
}

bool ReadD3DCamera(V&pos,V&right,V&up,V&back,float&sx,float&sy);
bool ProjectD3D(V world,V pos,V right,V up,V back,float sx,float sy,float w,float h,float&x,float&y);

void PublishTarget(const Target&target){auto odd=targetSequence.fetch_add(1)+1;currentTarget=target;MemoryBarrier();targetSequence.store(odd+1);}
bool LoadTarget(Target&target){for(int attempt=0;attempt<3;attempt++){auto before=targetSequence.load();if(before&1)continue;Target copy=currentTarget;MemoryBarrier();auto after=targetSequence.load();if(before==after&&!(after&1)){target=copy;return target.address&&GetTickCount64()-target.tick<=300;}}return false;}
void PublishEsp(const EspFrame&frame){AcquireSRWLockExclusive(&espFrameLock);currentEspFrame=frame;ReleaseSRWLockExclusive(&espFrameLock);}
bool LoadEsp(EspFrame&frame){AcquireSRWLockShared(&espFrameLock);frame=currentEspFrame;ReleaseSRWLockShared(&espFrameLock);return frame.count&&frame.count<=MaxEspEntities&&GetTickCount64()-frame.tick<=300;}

bool ForegroundGame(){auto window=GetForegroundWindow();DWORD pid=0;if(window)GetWindowThreadProcessId(window,&pid);return pid==GetCurrentProcessId();}
bool ValidDebugCamera(uintptr_t camera){if(!camera)return false;unsigned char bytes[0x70]{};if(!ReadBytes(camera,bytes,sizeof(bytes)))return false;auto F=[&](int offset){float value=0;memcpy(&value,bytes+offset,4);return value;};float cx=F(0x4C),cy=F(0x5C),focal=F(0x6C);return std::isfinite(cx)&&std::isfinite(cy)&&std::isfinite(focal)&&cx>-4096&&cx<4096&&cy>-4096&&cy<4096&&focal>=100&&focal<=5000;}
bool ValidateEspEntity(const EspEntity&entity,V&freshRoot){if(entity.generation<=0)return false;uintptr_t vtable=0;if(!Read(entity.state,vtable))return false;bool correct=entity.humanType?vtable==gameBase+HumanStateRva:(vtable==gameBase+ZombieState1Rva||vtable==gameBase+ZombieState2Rva);if(!correct)return false;unsigned char vitals[0x40]{};if(!ReadBytes(entity.state+0x6F8,vitals,sizeof(vitals)))return false;float health=0;memcpy(&freshRoot,vitals,12);memcpy(&health,vitals+0x3C,4);float drift=Distance(freshRoot,entity.root);return Finite(freshRoot)&&std::isfinite(health)&&health>0&&health<=1000000&&std::isfinite(drift)&&drift<=5.0f;}

V SmoothEspRoot(const EspEntity&entity,V observed,unsigned long long now){auto&track=espRenderTracks[entity.state];if(track.generation!=entity.generation||!Finite(track.display)||now-track.lastSeen>750){track={entity.generation,observed,0,now};return observed;}float dx=observed.x-track.display.x,dy=observed.y-track.display.y,dz=observed.z-track.display.z;bool large=dx*dx+dy*dy+dz*dz>36.0f;if(large&&++track.consecutiveLargeSteps<3){track.lastSeen=now;return track.display;}if(large){track.display=observed;track.consecutiveLargeSteps=0;track.lastSeen=now;return observed;}track.consecutiveLargeSteps=0;track.display={track.display.x+dx*.35f,track.display.y+dy*.35f,track.display.z+dz*.35f};track.lastSeen=now;return track.display;}

bool ReadCameraRight(V&right){uintptr_t view=0,camera=0;if(!Pointer(gameBase+CameraViewRva,view)||!Pointer(view+0x90390,camera))return false;unsigned char c[0x70]{};if(!ReadBytes(camera,c,sizeof(c)))return false;auto F=[&](int o){float v=0;memcpy(&v,c+o,4);return v;};right={F(0x40),F(0x50),F(0x60)};right.y=0;float length=sqrtf(right.x*right.x+right.z*right.z);if(!Finite(right)||!std::isfinite(length)||length<.01f)return false;right.x/=length;right.z/=length;return true;}

bool ProjectToDebug(V world,uintptr_t debug,V&screen){uintptr_t view=0,camera=0;if(!Pointer(gameBase+CameraViewRva,view)||!Pointer(view+0x90390,camera)||!debug)return false;unsigned char c[0x180]{},d[0x70]{};if(!ReadBytes(camera,c,sizeof(c))||!ReadBytes(debug,d,sizeof(d)))return false;auto F=[&](const unsigned char*b,int o){float v=0;memcpy(&v,b+o,4);return v;};V pos{F(c,0x4C)+F(c,0x168),F(c,0x5C)+F(c,0x16C),F(c,0x6C)+F(c,0x170)};V delta{world.x-pos.x,world.y-pos.y,world.z-pos.z};float right=delta.x*F(c,0x40)+delta.y*F(c,0x50)+delta.z*F(c,0x60),up=delta.x*F(c,0x44)+delta.y*F(c,0x54)+delta.z*F(c,0x64),depth=-(delta.x*F(c,0x48)+delta.y*F(c,0x58)+delta.z*F(c,0x68));float cx=F(d,0x4C),cy=F(d,0x5C),focal=F(d,0x6C);if(!std::isfinite(depth)||depth<=.05f||!std::isfinite(cx)||!std::isfinite(cy)||!std::isfinite(focal)||focal<100||focal>5000)return false;float px=cx+focal*right/depth,py=cy-focal*up/depth;constexpr float z=8;screen={cx+(px-cx)*z/focal,cy+(py-cy)*z/focal,focal-z};return Finite(screen)&&screen.x>=-256&&screen.x<=1536&&screen.y>=-256&&screen.y<=976;}

void DebugPassHook(void*self,void*debugCamera,void*context){debugPasses++;if(!espEnabled.load(std::memory_order_acquire)||!ForegroundGame()||!ValidDebugCamera((uintptr_t)debugCamera)){originalDebugPass(self,debugCamera,context);return;}auto now=GetTickCount64(),previous=lastEspBatch.load(std::memory_order_relaxed);if(now<previous+33||!lastEspBatch.compare_exchange_strong(previous,now,std::memory_order_acq_rel)){originalDebugPass(self,debugCamera,context);return;}if(espDrawActive.test_and_set(std::memory_order_acquire)){espReentryRejects++;originalDebugPass(self,debugCamera,context);return;}espRenderThread=GetCurrentThreadId();EspFrame frame{};unsigned submitted=0;if(cachedRenderer&&cachedLineAddress&&LoadEsp(frame)){auto line=(DrawLineFn)cachedLineAddress;for(unsigned i=0;i<frame.count&&i<MaxEspEntities;i++){V root{};if(!ValidateEspEntity(frame.entities[i],root)){espEntityRejects++;continue;}root=SmoothEspRoot(frame.entities[i],root,now);V bottom{},topWorld=root,top{};topWorld.y+=1.85f;if(!ProjectToDebug(root,(uintptr_t)debugCamera,bottom)||!ProjectToDebug(topWorld,(uintptr_t)debugCamera,top)){espProjectionRejects++;continue;}float height=fabsf(bottom.y-top.y),half=height*.21f;if(height<.01f||height>20){espProjectionRejects++;continue;}LineV a{top.x-half,top.y,top.z,0},b{top.x+half,top.y,top.z,0},c{bottom.x+half,bottom.y,bottom.z,0},d{bottom.x-half,bottom.y,bottom.z,0};V4 color=frame.entities[i].human?V4{.05f,.9f,1,1}:V4{1,.08f,.02f,1};__try{line((void*)cachedRenderer,&a,&b,&color,2);line((void*)cachedRenderer,&b,&c,&color,2);line((void*)cachedRenderer,&c,&d,&color,2);line((void*)cachedRenderer,&d,&a,&color,2);submitted+=4;}__except(EXCEPTION_EXECUTE_HANDLER){espEntityRejects++;break;}}}for(auto it=espRenderTracks.begin();it!=espRenderTracks.end();){if(now-it->second.lastSeen>750)it=espRenderTracks.erase(it);else++it;}if(submitted){espBatches++;espLines+=submitted;}espDrawActive.clear(std::memory_order_release);originalDebugPass(self,debugCamera,context);}

bool EnumerateAndSelect(){
 uintptr_t level=0;if(!FindLevel(level)){PublishTarget({});PublishEsp({});return false;}V camera{};if(!ReadCamera(camera)){PublishTarget({});PublishEsp({});return false;}
 const bool useFov=fovAimEnabled.load(std::memory_order_acquire),useCrosshair=crosshairPriorityEnabled.load(std::memory_order_acquire),needProjection=useFov||useCrosshair;const float screenW=(float)presentWidth.load(),screenH=(float)presentHeight.load();V aimCamera{},aimRight{},aimUp{},aimBack{};float aimSx=0,aimSy=0;const bool projectionReady=!needProjection||(screenW>0&&screenH>0&&ReadD3DCamera(aimCamera,aimRight,aimUp,aimBack,aimSx,aimSy));
 ++scanNumber;Target selected{};float selectedScreenDistance=-1;EspFrame esp{};unsigned zombies=0,hostileHumans=0,friendlyHumans=0,recordsSeen=0,rejected=0,fovProjected=0,fovEligible=0;uintptr_t container=level+0x358;
 for(int partition=0;partition<5;partition++){
  uintptr_t part=container+partition*0x48;int declared=0,width=0,height=0;if(!Read(part+0x14,declared)||!Read(part+0x34,width)||!Read(part+0x38,height)||declared<0||declared>250000)continue;
  long long slots=partition>=3?1LL:(long long)width*height;if(slots<0||slots>1000000)continue;uintptr_t cells=0;if(declared&&!Pointer(part+0x28,cells))continue;
  for(long long cellIndex=0;cellIndex<slots;cellIndex++){
   uintptr_t cell=0;if(!Read(cells+cellIndex*8,cell)||!cell)continue;int recordCount=0;uintptr_t records=0;if(!Read(cell+0x14,recordCount)||recordCount<0||recordCount>100000||!Pointer(cell+0x28,records))continue;
   for(int record=0;record<recordCount;record++){
    recordsSeen++;uintptr_t engine=0,igs=0,back=0,vtable=0;if(!Read(records+record*0x30+0x20,engine)||engine<0x10000||!Pointer(engine+0x20,igs)||!Read(igs+0x20,back)||back!=engine||!Pointer(igs,vtable)){rejected++;continue;}
    int objectOffset=0;bool zombie=TypeContains(vtable,"ZombieAI",objectOffset),human=false;if(!zombie)human=TypeContains(vtable,"HumanAI",objectOffset);if(!zombie&&!human)continue;uintptr_t complete=igs-(intptr_t)objectOffset,state=0;
    for(size_t member=0;member<=0xFF8;member+=8){uintptr_t candidate=0,candidateVtable=0;if(!Pointer(complete+member,candidate)||!Pointer(candidate,candidateVtable))continue;bool stateMatches=zombie?(candidateVtable==gameBase+ZombieState1Rva||candidateVtable==gameBase+ZombieState2Rva):candidateVtable==gameBase+HumanStateRva;if(stateMatches){state=candidate;break;}}
    if(!state)continue;unsigned char vitals[0x40]{};if(!ReadBytes(state+0x6F8,vitals,sizeof(vitals)))continue;V position{};float health=0;memcpy(&position,vitals,12);memcpy(&health,vitals+0x3C,4);if(!Finite(position)||!std::isfinite(health)||health<=0||health>1000000)continue;
    bool hostile=!human;if(human)hostile=AiAllegiance::HumanHostile(complete,gameBase,[](uintptr_t p,auto& v){return Read(p,v);});auto&life=lives[state];if(!life.generation)life.generation=1;else if(life.lastScan+7<scanNumber)life.generation++;life.lastScan=scanNumber;if(zombie)zombies++;else if(hostile)hostileHumans++;else friendlyHumans++;float distance=Distance(camera,position);espRoster[state]={{state,complete,life.generation,position,human,hostile},distance,GetTickCount64()};
    if(hostile&&distance>.1f&&distance<2000){V aimPoint{position.x,position.y+1.55f,position.z};bool eligible=!useFov&&!useCrosshair;float screenDistance=-1;if(needProjection&&projectionReady){float px=0,py=0;if(ProjectD3D(aimPoint,aimCamera,aimRight,aimUp,aimBack,aimSx,aimSy,screenW,screenH,px,py)){fovProjected++;float dx=px-screenW*.5f,dy=py-screenH*.5f;screenDistance=sqrtf(dx*dx+dy*dy);eligible=std::isfinite(screenDistance)&&(!useFov||screenDistance<=fovRadiusPixels.load());if(useFov&&eligible)fovEligible++;}}const bool better=!selected.address||(useCrosshair?screenDistance<selectedScreenDistance:distance<selected.distance);if(eligible&&better){selected={state,life.generation,aimPoint,distance,GetTickCount64()};selectedScreenDistance=screenDistance;}}
   }
  }
 }
 auto now = GetTickCount64();
 for (auto it = espRoster.begin(); it != espRoster.end();) {
   if (now - it->second.lastSeen > 750)
     it = espRoster.erase(it);
   else
     ++it;
 }
 std::vector<EspRosterItem> ranked;
 ranked.reserve(espRoster.size());
 for (const auto &pair : espRoster)
   ranked.push_back(pair.second);
 std::sort(ranked.begin(), ranked.end(),
           [](const EspRosterItem &a, const EspRosterItem &b) {
             return a.distance < b.distance;
           });
 for (const auto &item : ranked) {
   if (esp.count >= MaxEspEntities)
     break;
   if (item.distance > .1f && item.distance <= 800)
     esp.entities[esp.count++] = item.entity;
 }
 esp.tick = now;
 PublishTarget(selected);
 PublishEsp(esp);
 static unsigned long long nextLog = 0;
 if (now >= nextLog) {
   std::ofstream f(LogPath(), std::ios::app);
   f << now << ",NATIVE_ENUM,scan=" << scanNumber << ",records=" << recordsSeen
     << ",zombies=" << zombies << ",hostile_humans=" << hostileHumans
     << ",roster=" << espRoster.size() << ",esp=" << esp.count << ",selected=0x"
     << std::hex << selected.address << std::dec
     << ",distance=" << selected.distance << ",rejected=" << rejected
     << ",fov_enabled=" << useFov << ",projection_ready=" << projectionReady
     << ",fov_radius_px=" << fovRadiusPixels.load()
     << ",fov_projected=" << fovProjected << ",fov_eligible=" << fovEligible
     << ",selected_screen_distance_px=" << selectedScreenDistance
     << ",crosshair_priority=" << useCrosshair
     << ",debug_passes=" << debugPasses.load()
     << ",esp_enabled=" << espEnabled.load()
     << ",render_thread=" << espRenderThread.load()
     << ",esp_batches=" << espBatches.load() << ",esp_lines=" << espLines.load()
     << ",projection_rejects=" << espProjectionRejects.load()
     << ",thread_rejects=" << espThreadRejects.load()
     << ",entity_rejects=" << espEntityRejects.load()
     << ",reentry_rejects=" << espReentryRejects.load() << '\n';
   nextLog = now + 1000;
 }
 return true;
}

void PublishControl(bool ready){if(!control)return;control->magic=0x45444944;control->version=1;InterlockedExchange(&control->ready,ready?1:0);InterlockedExchange(&control->enabled,enabled.load()?1:0);control->pid=GetCurrentProcessId();InterlockedExchange64(&control->redirects,redirects.load());InterlockedExchange64(&control->refusals,refusals.load());InterlockedExchange(&control->espEnabled,espEnabled.load()?1:0);InterlockedExchange(&control->boxesEnabled,espBoxesEnabled.load()?1:0);InterlockedExchange(&control->skeletonsEnabled,espSkeletonsEnabled.load()?1:0);}
bool ResolveTarget(Target&target,uintptr_t&entityControl,int&why){if(!LoadTarget(target)){why=4;return false;}uintptr_t vtable=0,root=0;if(!Read(target.address,vtable)){why=5;return false;}bool zombie=vtable==gameBase+ZombieState1Rva||vtable==gameBase+ZombieState2Rva,human=vtable==gameBase+HumanStateRva;if(!zombie&&!human){why=5;return false;}if(!Pointer(target.address+0x58,root)){why=6;return false;}if(human&&!AiAllegiance::HumanHostile(root,gameBase,[](uintptr_t p,auto& v){return Read(p,v);})){why=7;return false;}entityControl=root+0x18;why=0;return true;}

bool ResolveRocketContext(void*projectile,uintptr_t&world,uintptr_t&rocketControl){uintptr_t worldVtable=0,owner=0,ownerVtable=0,rootTarget=0,base=0;if(!Read((uintptr_t)projectile,world)||!Read(world,worldVtable)||!InGame(worldVtable)||!Read((uintptr_t)projectile+8,owner)||owner<0x10000||!Read(owner,ownerVtable)||!InGame(ownerVtable)||!Read(ownerVtable,rootTarget)||!InGame(rootTarget))return false;__try{base=((RocketRootFn)rootTarget)((void*)owner);}__except(EXCEPTION_EXECUTE_HANDLER){base=0;}if(base<0x10000)return false;rocketControl=base+0x18;uintptr_t controlVtable=0;return Read(rocketControl,controlVtable)&&InGame(controlVtable);}
bool ReadD3DCamera(V&pos,V&right,V&up,V&back,float&sx,float&sy);
bool SpawnKickRocket(uintptr_t world,uintptr_t rocketControl){V camera{},right{},up{},back{};float sx=0,sy=0;if(!ReadD3DCamera(camera,right,up,back,sx,sy))return false;V forward{-back.x,-back.y,-back.z};float length=sqrtf(forward.x*forward.x+forward.y*forward.y+forward.z*forward.z);if(!std::isfinite(length)||length<.01f)return false;forward={forward.x/length,forward.y/length,forward.z/length};V p{camera.x+forward.x*1.5f,camera.y+forward.y*1.5f-.25f,camera.z+forward.z*1.5f},direction{forward.x*10.f,forward.y*10.f,forward.z*10.f};__try{auto create=*(RocketCreateFn*)(gameBase+RocketCreateIatRva);auto activate=*(RocketActivateFn*)(gameBase+RocketActivateIatRva);if(!create||!activate)return false;void*igs=create((void*)world,(void*)(gameBase+RocketRttiRva),true,nullptr);if(!igs)return false;uintptr_t base=(uintptr_t)igs-0x28,vt=0,igsVt=0;if(!Read(base,vt)||vt!=gameBase+RocketVtableRva||!Read((uintptr_t)igs,igsVt)||igsVt!=gameBase+RocketIgsVtableRva)return false;activate(igs);auto setPosition=*(RocketVecFn*)(vt+0x1D8);auto setDirection=*(RocketVecFn*)(vt+0x220);if(!InGame((uintptr_t)setPosition)||!InGame((uintptr_t)setDirection))return false;setPosition((void*)base,&p);setDirection((void*)base,&direction);*(float*)(base+0x210)=100.f;*(float*)(base+0x214)=10000.f;*(float*)(base+0x218)=200.f;*(float*)(base+0x224)=7.5f;*(unsigned char*)(base+0x21d)=1;((RocketInitFn)(gameBase+RocketInitRva))((void*)base,(void*)rocketControl,&p,&direction,&forward,2.f,-1.f);return *(uintptr_t*)base==gameBase+RocketVtableRva;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool SpawnRedirectedRocket(uintptr_t world,uintptr_t rocketControl,const V&position,const V&incoming){__try{float length=sqrtf(incoming.x*incoming.x+incoming.y*incoming.y+incoming.z*incoming.z);if(!std::isfinite(length)||length<.0001f)return false;V velocity{incoming.x/length,incoming.y/length,incoming.z/length};V direction{velocity.x*10.f,velocity.y*10.f,velocity.z*10.f};auto create=*(RocketCreateFn*)(gameBase+RocketCreateIatRva);auto activate=*(RocketActivateFn*)(gameBase+RocketActivateIatRva);if(!create||!activate)return false;void*igs=create((void*)world,(void*)(gameBase+RocketRttiRva),true,nullptr);if(!igs)return false;uintptr_t base=(uintptr_t)igs-0x28,baseVtable=0,igsVtable=0;if(!Read(base,baseVtable)||baseVtable!=gameBase+RocketVtableRva||!Read((uintptr_t)igs,igsVtable)||igsVtable!=gameBase+RocketIgsVtableRva)return false;activate(igs);auto setPosition=*(RocketVecFn*)(baseVtable+0x1D8);auto setDirection=*(RocketVecFn*)(baseVtable+0x220);if(!InGame((uintptr_t)setPosition)||!InGame((uintptr_t)setDirection))return false;V p=position;setPosition((void*)base,&p);setDirection((void*)base,&direction);*(float*)(base+0x210)=100.f;*(float*)(base+0x214)=10000.f;*(float*)(base+0x218)=200.f;*(float*)(base+0x224)=7.5f;*(unsigned char*)(base+0x21d)=1;((RocketInitFn)(gameBase+RocketInitRva))((void*)base,(void*)rocketControl,&p,&direction,&velocity,2.f,-1.f);if(*(uintptr_t*)base!=gameBase+RocketVtableRva)return false;((RocketDetonateFn)(gameBase+RocketDetonateRva))((void*)base,18);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
unsigned long long ImpactHook(void*projectile,void*collision,V*position,V*velocity){uintptr_t world=0,rocketControl=0;bool contextOk=ResolveRocketContext(projectile,world,rocketControl);if(contextOk){cachedRocketWorld=world;cachedRocketControl=rocketControl;cachedRocketAt=GetTickCount64();Log("NATIVE_ROCKET_CONTEXT_CACHED");}if(!explosiveSilentAimEnabled.load()){pendingRedirectExplosion.valid=false;return originalImpact(projectile,collision,position,velocity);}auto result=originalImpact(projectile,collision,position,velocity);auto pending=pendingRedirectExplosion;pendingRedirectExplosion.valid=false;auto now=GetTickCount64();if(!result||!contextOk||!velocity||!pending.valid||now<pending.tick||now-pending.tick>250||!Finite(pending.position)||!SpawnRedirectedRocket(world,rocketControl,pending.position,*velocity)){if(pending.valid){auto n=++redirectedExplosionRefusals;if(n<=10||n%25==0)Log("REDIRECT_EXPLOSION_REFUSED");}return result;}auto n=++redirectedExplosions;if(n<=10||n%25==0)Log("REDIRECT_EXPLOSION_REASON18_AT_TARGET");return result;}
void MeleeEventHook(void*self,int id,float value,char blocked){if(kickRocketEnabled.load()&&id==0xA2&&value==1.f&&!blocked){auto now=GetTickCount64();if(now-lastKickRocketAt.load()>=250){lastKickRocketAt=now;auto cached=cachedRocketAt.load();if(!cached||now-cached>120000)SetStatus("KICK ROCKET NEEDS ONE NATURAL ROCKET IMPACT");else if(SpawnKickRocket(cachedRocketWorld.load(),cachedRocketControl.load()))Log("KICK_ROCKET_LAUNCHED");else SetStatus("KICK ROCKET SPAWN REFUSED");}}originalMeleeEvent(self,id,value,blocked);}

unsigned char Hook(void*w,V*start,V*end,void*collision,void*ignore,char flags){
 auto caller=(uintptr_t)_ReturnAddress();RecordCaller(caller);unsigned char result=original(w,start,end,collision,ignore,flags);if(!enabled.load()||caller-gameBase!=PrimaryReturnRva||!collision||!start)return result;
 Target target{};uintptr_t entityControl=0;int why=0;if(ResolveTarget(target,entityControl,why)){auto c=(unsigned char*)collision;V impact=target.position;V d{impact.x-start->x,impact.y-start->y,impact.z-start->z};float length=sqrtf(d.x*d.x+d.y*d.y+d.z*d.z);if(std::isfinite(length)&&length>.01f){*(uintptr_t*)(c+0x18)=entityControl;*(uintptr_t*)(c+0x28)=0;*(unsigned short*)(c+0x38)=10;*(unsigned char*)(c+0x3a)=0;*(int*)(c+0x3c)=-1;*(V*)(c+0x44)=impact;*(V*)(c+0x50)=V{-d.x/length,-d.y/length,-d.z/length};if(explosiveSilentAimEnabled.load())pendingRedirectExplosion={impact,GetTickCount64(),true};else pendingRedirectExplosion.valid=false;long n=++redirects;if(n<=10||n%25==0)Log("REDIRECT_PROGRESS_NATIVE_ENUM");return 2;}}
 long bad=++refusals;const char*reason=why==4?"REFUSED_NO_NATIVE_TARGET":why==5?"REFUSED_INVALID_CLASS":why==6?"REFUSED_ROOT_MISSING":why==7?"REFUSED_HUMAN_NOT_HOSTILE":"REFUSED_BAD_GEOMETRY";if(bad<=10||bad%25==0)Log(reason);return result;
}

void Jump(unsigned char*address,void*target){unsigned char jump[14]={0xFF,0x25,0,0,0,0};auto pointer=(unsigned long long)target;memcpy(jump+6,&pointer,8);memcpy(address,jump,14);}
bool Install(){site=(unsigned char*)(gameBase+RayRva);const unsigned char expected[PatchLength]={0x4C,0x89,0x4C,0x24,0x20,0x4C,0x89,0x44,0x24,0x18,0x48,0x89,0x54,0x24,0x10};if(memcmp(site,expected,PatchLength))return false;memcpy(saved,site,PatchLength);auto trampoline=(unsigned char*)VirtualAlloc(nullptr,64,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);if(!trampoline)return false;memcpy(trampoline,saved,PatchLength);Jump(trampoline+PatchLength,site+PatchLength);original=(RaytraceFn)trampoline;DWORD old{};if(!VirtualProtect(site,PatchLength,PAGE_EXECUTE_READWRITE,&old))return false;Jump(site,(void*)&Hook);site[14]=0x90;FlushInstructionCache(GetCurrentProcess(),site,PatchLength);DWORD ignored{};VirtualProtect(site,PatchLength,old,&ignored);return true;}
bool InstallImpact(){const unsigned char expected[8]={0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x48};auto target=(void*)(gameBase+ImpactRva);if(memcmp(target,expected,sizeof(expected))){Log("IMPACT_INSTALL_BYTE_MISMATCH");return false;}auto init=MH_Initialize();if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED){Log("IMPACT_INSTALL_INIT_FAILED");return false;}if(MH_CreateHook(target,(void*)&ImpactHook,(void**)&originalImpact)!=MH_OK||MH_CreateHook((void*)(gameBase+MeleeEventRva),(void*)&MeleeEventHook,(void**)&originalMeleeEvent)!=MH_OK){Log("IMPACT_MELEE_CREATE_FAILED");return false;}if(MH_EnableHook(target)!=MH_OK||MH_EnableHook((void*)(gameBase+MeleeEventRva))!=MH_OK){Log("IMPACT_MELEE_ENABLE_FAILED");return false;}Log("IMPACT_AND_KICK_INSTALL_OK");return true;}
bool InstallDebug() {
  uintptr_t renderer = 0, vtable = 0, lineAddress = 0;
  if (!Pointer(engineBase + RendererGlobalRva, renderer) ||
      !Pointer(renderer, vtable) ||
      !Pointer(vtable + DrawLineSlot, lineAddress) ||
      lineAddress < engineBase || lineAddress >= engineBase + engineSize)
    return false;
  cachedRenderer = renderer;
  cachedLineAddress = lineAddress;
  debugSite = (unsigned char *)(engineBase + DebugPassRva);
  const unsigned char expected[20] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89,
                                      0x74, 0x24, 0x18, 0x48, 0x89, 0x7C, 0x24,
                                      0x20, 0x48, 0x89, 0x54, 0x24, 0x10};
  if (memcmp(debugSite, expected, sizeof(expected)))
    return false;
  memcpy(debugSaved, debugSite, sizeof(debugSaved));
  auto trampoline = (unsigned char *)VirtualAlloc(
      nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  if (!trampoline)
    return false;
  memcpy(trampoline, debugSaved, sizeof(debugSaved));
  Jump(trampoline + sizeof(debugSaved), debugSite + sizeof(debugSaved));
  originalDebugPass = (DebugPassFn)trampoline;
  DWORD old{};
  if (!VirtualProtect(debugSite, sizeof(debugSaved), PAGE_EXECUTE_READWRITE,
                      &old))
    return false;
  Jump(debugSite, (void *)&DebugPassHook);
  memset(debugSite + 14, 0x90, sizeof(debugSaved) - 14);
  FlushInstructionCache(GetCurrentProcess(), debugSite, sizeof(debugSaved));
  DWORD ignored{};
  VirtualProtect(debugSite, sizeof(debugSaved), old, &ignored);
  return true;
}

void ReleaseD3D(){if(d3dRaster)d3dRaster->Release();if(d3dDepth)d3dDepth->Release();if(d3dBlend)d3dBlend->Release();if(d3dVertices)d3dVertices->Release();if(d3dLayout)d3dLayout->Release();if(d3dPs)d3dPs->Release();if(d3dVs)d3dVs->Release();d3dRaster=nullptr;d3dDepth=nullptr;d3dBlend=nullptr;d3dVertices=nullptr;d3dLayout=nullptr;d3dPs=nullptr;d3dVs=nullptr;}
bool EnsureD3D(ID3D11Device *d) {
  if (d3dVs)
    return true;
  static const char *vs = "struct V{float2 p:POSITION;float4 c:COLOR;};struct "
                          "O{float4 p:SV_Position;float4 c:COLOR;};O main(V "
                          "v){O o;o.p=float4(v.p,0,1);o.c=v.c;return o;}";
  static const char *ps =
      "float4 main(float4 p:SV_Position,float4 c:COLOR):SV_Target{return c;}";
  ID3DBlob *vb = nullptr, *pb = nullptr, *err = nullptr;
  if (FAILED(D3DCompile(vs, strlen(vs), nullptr, nullptr, nullptr, "main",
                        "vs_5_0", 0, 0, &vb, &err))) {
    if (err)
      err->Release();
    return false;
  }
  if (FAILED(D3DCompile(ps, strlen(ps), nullptr, nullptr, nullptr, "main",
                        "ps_5_0", 0, 0, &pb, &err))) {
    vb->Release();
    if (err)
      err->Release();
    return false;
  }
  d->CreateVertexShader(vb->GetBufferPointer(), vb->GetBufferSize(), nullptr,
                        &d3dVs);
  d->CreatePixelShader(pb->GetBufferPointer(), pb->GetBufferSize(), nullptr,
                       &d3dPs);
  D3D11_INPUT_ELEMENT_DESC il[] = {{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
                                    0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                                   {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,
                                    0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0}};
  d->CreateInputLayout(il, 2, vb->GetBufferPointer(), vb->GetBufferSize(),
                       &d3dLayout);
  vb->Release();
  pb->Release();
  D3D11_BUFFER_DESC bd{};
  bd.ByteWidth = sizeof(D3DVertex) * 32768;
  bd.Usage = D3D11_USAGE_DYNAMIC;
  bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  d->CreateBuffer(&bd, nullptr, &d3dVertices);
  D3D11_BLEND_DESC bl{};
  bl.RenderTarget[0].BlendEnable = TRUE;
  bl.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
  bl.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
  bl.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  bl.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
  bl.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
  bl.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  bl.RenderTarget[0].RenderTargetWriteMask = 15;
  d->CreateBlendState(&bl, &d3dBlend);
  D3D11_DEPTH_STENCIL_DESC ds{};
  ds.DepthEnable = FALSE;
  ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
  d->CreateDepthStencilState(&ds, &d3dDepth);
  D3D11_RASTERIZER_DESC rs{};
  rs.FillMode = D3D11_FILL_SOLID;
  rs.CullMode = D3D11_CULL_NONE;
  rs.DepthClipEnable = TRUE;
  rs.ScissorEnable = FALSE;
  d->CreateRasterizerState(&rs, &d3dRaster);
  return d3dVs && d3dPs && d3dLayout && d3dVertices && d3dBlend && d3dDepth &&
         d3dRaster;
}

uint32_t PoseNameHash(const char*text){uint32_t h=0;for(;*text;text++)h=h*0x29u+(unsigned char)*text;return h;}
bool ReadM34(uintptr_t address,M34&matrix){if(!ReadBytes(address,&matrix,sizeof(matrix)))return false;for(float f:matrix.v)if(!std::isfinite(f))return false;return true;}
M34 MultiplyM34(const M34&p,const M34&c){M34 o{};for(int r=0;r<3;r++){for(int q=0;q<3;q++)o.v[r*4+q]=p.v[r*4]*c.v[q]+p.v[r*4+1]*c.v[4+q]+p.v[r*4+2]*c.v[8+q];o.v[r*4+3]=p.v[r*4]*c.v[3]+p.v[r*4+1]*c.v[7]+p.v[r*4+2]*c.v[11]+p.v[r*4+3];}return o;}
bool ResolvePoseHeadNow(uintptr_t entity,V root,V&head){std::vector<uintptr_t>models;auto add=[&](uintptr_t address,size_t offset){uintptr_t p=0,m=0;if(Pointer(address,p)&&Pointer(p+offset,m)&&std::find(models.begin(),models.end(),m)==models.end())models.push_back(m);};add(entity+0x58,0x08);add(entity+0x58,0x20);add(entity+0x58,0x48);add(entity+0x20,0x30);const uint32_t wanted=PoseNameHash("bip01 head");for(auto model:models)for(size_t poseOffset:{size_t(0x398),size_t(0x3A8)}){uintptr_t pose=0;if(!Pointer(model+poseOffset,pose))continue;unsigned char ph[0x40]{};if(!ReadBytes(pose,ph,sizeof(ph)))continue;uintptr_t owner=0,skeleton=0;unsigned short count=0;memcpy(&owner,ph+0x20,8);memcpy(&count,ph+0x28,2);memcpy(&skeleton,ph+0x38,8);if(count<20||count>255||!(ph[0x2C]&1)||owner<0x10000||skeleton<0x10000)continue;uintptr_t descriptors=0,names=0,map=0;uint32_t nameCount=0,mapCount=0;if(!Pointer(owner+0x38,descriptors)||!Pointer(skeleton+0x28,names)||!Read(skeleton+0x30,nameCount)||!Pointer(skeleton+0x58,map)||!Read(skeleton+0x60,mapCount)||!nameCount||nameCount>2048||!mapCount||mapCount>2048)continue;int element=-1;for(uint32_t i=0;i<nameCount;i++){int candidate=-1;uint32_t hash=0;if(Read(names+i*8,candidate)&&Read(names+i*8+4,hash)&&hash==wanted){element=candidate;break;}}if(element<0||element>=count)continue;bool mapped=false;for(uint32_t i=0;i<mapCount;i++){int candidate=-1;if(Read(map+i*4,candidate)&&candidate==element){mapped=true;break;}}if(!mapped)continue;std::vector<int>parents(count,-1),keys(count);std::vector<uintptr_t>components(count);bool layout=true;for(unsigned i=0;i<count;i++){unsigned char row[0x30]{};if(!ReadBytes(descriptors+i*0x30,row,sizeof(row))){layout=false;break;}uintptr_t parent=0;memcpy(&parent,row,8);memcpy(&components[i],row+0x20,8);memcpy(&keys[i],row+0x28,4);keys[i]&=0xFFF;auto delta=(intptr_t)parent-(intptr_t)descriptors;if(delta>=0&&delta%0x30==0&&delta/0x30<count)parents[i]=(int)(delta/0x30);if(components[i]<0x10000){layout=false;break;}}if(!layout)continue;std::vector<M34>matrices(count);std::vector<unsigned char>states(count);std::function<bool(int)>solve=[&](int i){if(i<0||i>=count)return false;if(states[i]==2)return true;if(states[i]==1)return false;states[i]=1;uintptr_t locals=0,worlds=0,flags=0;if(!Pointer(components[i]+0x20,locals)||!Pointer(components[i]+0x30,worlds)||!Pointer(components[i]+0x40,flags))return false;unsigned char fl=0;if(!Read(flags+keys[i],fl))return false;uintptr_t source=(fl&2)?locals:worlds;M34 m{};if(!ReadM34(source+(uintptr_t)keys[i]*0x30,m))return false;if((fl&2)&&parents[i]>=0){if(!solve(parents[i]))return false;m=MultiplyM34(matrices[parents[i]],m);}matrices[i]=m;states[i]=2;return true;};if(!solve(element))continue;V candidate{matrices[element].v[3],matrices[element].v[7],matrices[element].v[11]};float span=Distance(candidate,root);if(Finite(candidate)&&std::isfinite(span)&&span>.25f&&span<3.5f){head=candidate;return true;}}return false;}
bool ResolvePoseHead(const EspEntity&entity,V root,unsigned long long now,V&head){auto&cache=poseHeadCache[entity.state];if(cache.generation!=entity.generation){cache={entity.generation,{},0,0};}if(now-cache.sampled>=25){cache.sampled=now;V candidate{};if(ResolvePoseHeadNow(entity.state,root,candidate)){cache.head=candidate;cache.lastGood=now;}}if(cache.lastGood&&now-cache.lastGood<=125&&Finite(cache.head)){head=cache.head;return true;}return false;}
bool ResolveSkeletonNow(uintptr_t entity, V root, uintptr_t preferredPose,
                        std::vector<SkeletonSegment> &out,
                        uintptr_t &selectedPose) {
  std::vector<uintptr_t> models;
  auto add = [&](uintptr_t a, size_t o) {
    uintptr_t p = 0, m = 0;
    if (Pointer(a, p) && Pointer(p + o, m) &&
        std::find(models.begin(), models.end(), m) == models.end())
      models.push_back(m);
  };
  add(entity + 0x58, 8);
  add(entity + 0x58, 0x20);
  add(entity + 0x58, 0x48);
  add(entity + 0x20, 0x30);
  float bestScore = FLT_MAX;
  uintptr_t bestPose = 0;
  std::vector<SkeletonSegment> best;
  for (auto model : models)
    for (size_t po : {size_t(0x398), size_t(0x3A8)}) {
      uintptr_t pose = 0;
      if (!Pointer(model + po, pose))
        continue;
      unsigned char ph[0x40]{};
      if (!ReadBytes(pose, ph, sizeof(ph)))
        continue;
      uintptr_t owner = 0;
      unsigned short count = 0;
      memcpy(&owner, ph + 0x20, 8);
      memcpy(&count, ph + 0x28, 2);
      if (count < 20 || count > 255 || !(ph[0x2C] & 1) || owner < 0x10000)
        continue;
      uintptr_t descriptors = 0;
      if (!Pointer(owner + 0x38, descriptors))
        continue;
      std::vector<int> parents(count, -1), keys(count);
      std::vector<uintptr_t> components(count);
      bool ok = true;
      for (unsigned i = 0; i < count; i++) {
        unsigned char row[0x30]{};
        if (!ReadBytes(descriptors + i * 0x30, row, sizeof(row))) {
          ok = false;
          break;
        }
        uintptr_t parent = 0;
        memcpy(&parent, row, 8);
        memcpy(&components[i], row + 0x20, 8);
        memcpy(&keys[i], row + 0x28, 4);
        keys[i] &= 0xFFF;
        auto delta = (intptr_t)parent - (intptr_t)descriptors;
        if (delta >= 0 && delta % 0x30 == 0 && delta / 0x30 < count)
          parents[i] = (int)(delta / 0x30);
        if (components[i] < 0x10000) {
          ok = false;
          break;
        }
      }
      if (!ok)
        continue;
      std::vector<M34> matrices(count);
      std::vector<unsigned char> states(count);
      std::function<bool(int)> solve = [&](int i) {
        if (i < 0 || i >= count)
          return false;
        if (states[i] == 2)
          return true;
        if (states[i] == 1)
          return false;
        states[i] = 1;
        uintptr_t locals = 0, worlds = 0, flags = 0;
        if (!Pointer(components[i] + 0x20, locals) ||
            !Pointer(components[i] + 0x30, worlds) ||
            !Pointer(components[i] + 0x40, flags))
          return false;
        unsigned char fl = 0;
        if (!Read(flags + keys[i], fl))
          return false;
        M34 m{};
        if (!ReadM34(((fl & 2) ? locals : worlds) + (uintptr_t)keys[i] * 0x30,
                     m))
          return false;
        if ((fl & 2) && parents[i] >= 0) {
          if (!solve(parents[i]))
            return false;
          m = MultiplyM34(matrices[parents[i]], m);
        }
        matrices[i] = m;
        states[i] = 2;
        return true;
      };
      std::vector<SkeletonSegment> candidate;
      candidate.reserve(128);
      for (int i = 0; i < count && candidate.size() < 128; i++) {
        int parent = parents[i];
        if (parent < 0 || !solve(i) || !solve(parent))
          continue;
        V a{matrices[parent].v[3], matrices[parent].v[7],
            matrices[parent].v[11]},
            b{matrices[i].v[3], matrices[i].v[7], matrices[i].v[11]};
        float da = Distance(a, root), db = Distance(b, root),
              length = Distance(a, b);
        if (Finite(a) && Finite(b) && da < 4 && db < 4 && length > .005f &&
            length < 3)
          candidate.push_back({a, b});
      }
      if (candidate.size() >= 8) {
        if (preferredPose && pose == preferredPose) {
          selectedPose = pose;
          out.swap(candidate);
          return true;
        }
        float score = FLT_MAX;
        for (const auto &segment : candidate)
          score = std::min(score, std::min(Distance(segment.a, root),
                                           Distance(segment.b, root)));
        if (std::isfinite(score) && score < bestScore) {
          bestScore = score;
          bestPose = pose;
          best.swap(candidate);
        }
      }
    }
  if (best.size() < 8)
    return false;
  selectedPose = bestPose;
  out.swap(best);
  return true;
}
bool ResolveSkeleton(const EspEntity &entity, V root, unsigned long long now,
                     const std::vector<SkeletonSegment> *&segments) {
  auto &cache = skeletonCache[entity.state];
  if (cache.generation != entity.generation)
    cache = {entity.generation, {}, 0, 0, {}};
  unsigned interval =
      control ? (unsigned)std::clamp((LONG)control->skeletonIntervalMs, 4L, 50L)
              : 8;
  if (now - cache.sampled >= interval) {
    cache.sampled = now;
    LARGE_INTEGER frequency{}, started{}, finished{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&started);
    std::vector<SkeletonSegment> fresh;
    uintptr_t selectedPose = 0;
    bool resolved = ResolveSkeletonNow(entity.state, root, cache.pose, fresh,
                                       selectedPose);
    QueryPerformanceCounter(&finished);
    unsigned long long micros =
        frequency.QuadPart
            ? (unsigned long long)((finished.QuadPart - started.QuadPart) *
                                   1000000 / frequency.QuadPart)
            : 0;
    skeletonSamples++;
    skeletonSampleMicros += micros;
    auto previous = skeletonSampleMaxMicros.load();
    while (micros > previous &&
           !skeletonSampleMaxMicros.compare_exchange_weak(previous, micros)) {
    }
    if (resolved) {
      skeletonSampleSuccess++;
      skeletonSegments += fresh.size();
      cache.segments.swap(fresh);
      cache.lastGood = now;
      cache.root = root;
      cache.pose = selectedPose;
    }
  }
  if (cache.lastGood && now - cache.lastGood <= 2000 &&
      !cache.segments.empty()) {
    V delta{root.x - cache.root.x, root.y - cache.root.y,
            root.z - cache.root.z};
    if (Finite(delta) && Distance(root, cache.root) < 2.f) {
      for (auto &segment : cache.segments) {
        segment.a.x += delta.x;
        segment.a.y += delta.y;
        segment.a.z += delta.z;
        segment.b.x += delta.x;
        segment.b.y += delta.y;
        segment.b.z += delta.z;
      }
      cache.root = root;
    }
    segments = &cache.segments;
    return true;
  }
  segments = nullptr;
  return false;
}
bool ReadD3DCamera(V&pos,V&right,V&up,V&back,float&sx,float&sy){uintptr_t view=0,camera=0,pub=0,owner=0;if(!Pointer(gameBase+CameraViewRva,view)||!Pointer(view+0x90390,camera)||!Pointer(camera+0x2B0,pub)||!Read(pub+8,owner)||owner!=camera)return false;unsigned char b[0x180]{};if(!ReadBytes(camera,b,sizeof(b)))return false;auto F=[&](int o){float f=0;memcpy(&f,b+o,4);return f;};pos={F(0x4C)+F(0x168),F(0x5C)+F(0x16C),F(0x6C)+F(0x170)};right={F(0x40),F(0x50),F(0x60)};up={F(0x44),F(0x54),F(0x64)};back={F(0x48),F(0x58),F(0x68)};sx=F(0x70);sy=F(0x84);return Finite(pos)&&std::isfinite(sx)&&std::isfinite(sy)&&sx>.1f&&sx<4&&sy>.1f&&sy<4;}
bool ProjectD3D(V world,V pos,V right,V up,V back,float sx,float sy,float w,float h,float&x,float&y){V d{world.x-pos.x,world.y-pos.y,world.z-pos.z};float depth=-(d.x*back.x+d.y*back.y+d.z*back.z);if(!std::isfinite(depth)||depth<=.05f)return false;float nx=sx*(d.x*right.x+d.y*right.y+d.z*right.z)/depth,ny=sy*(d.x*up.x+d.y*up.y+d.z*up.z)/depth;if(!std::isfinite(nx)||!std::isfinite(ny)||fabsf(nx)>4||fabsf(ny)>4)return false;x=(nx+1)*.5f*w;y=(1-ny)*.5f*h;return true;}
bool ComputeEspBox(const EspEntity&entity,V root,unsigned long long now,V camera,V cameraRight,V cameraUp,V cameraBack,float sx,float sy,float w,float h,float&left,float&topY,float&rightEdge,float&bottomY){V head{};bool semantic=ResolvePoseHead(entity,root,now,head);if(!semantic){poseHeadFallback++;head=root;head.y+=1.85f;}else{poseHeadSuccess++;head.y+=.12f;}constexpr float radius=.34f;V points[10]={root,head,{root.x-radius,root.y,root.z-radius},{root.x-radius,root.y,root.z+radius},{root.x+radius,root.y,root.z-radius},{root.x+radius,root.y,root.z+radius},{head.x-radius,head.y,head.z-radius},{head.x-radius,head.y,head.z+radius},{head.x+radius,head.y,head.z-radius},{head.x+radius,head.y,head.z+radius}};left=FLT_MAX;topY=FLT_MAX;rightEdge=-FLT_MAX;bottomY=-FLT_MAX;unsigned projected=0;for(const auto&point:points){float x=0,y=0;if(!ProjectD3D(point,camera,cameraRight,cameraUp,cameraBack,sx,sy,w,h,x,y))continue;left=std::min(left,x);rightEdge=std::max(rightEdge,x);topY=std::min(topY,y);bottomY=std::max(bottomY,y);projected++;}if(projected<4)return false;float boxWidth=rightEdge-left,boxHeight=bottomY-topY;if(boxWidth<4||boxHeight<4||boxWidth>w||boxHeight>h*1.5f)return false;float pad=std::clamp(boxHeight*.035f,2.f,8.f);left-=pad;rightEdge+=pad;topY-=pad;bottomY+=pad;return true;}
void AddLine(std::vector<D3DVertex>&v,float x1,float y1,float x2,float y2,float w,float h,float r,float g,float b){float dx=x2-x1,dy=y2-y1,length=sqrtf(dx*dx+dy*dy);if(!std::isfinite(length)||length<.01f)return;float half=std::clamp(currentLineThickness*.5f,.5f,22.f),px=-dy/length*half,py=dx/length*half;auto add=[&](float x,float y){v.push_back({x/w*2-1,1-y/h*2,r,g,b,1});};add(x1+px,y1+py);add(x2+px,y2+py);add(x2-px,y2-py);add(x1+px,y1+py);add(x2-px,y2-py);add(x1-px,y1-py);}
void AddCircle(std::vector<D3DVertex>&v,float cx,float cy,float radius,float w,float h,float r,float g,float b){constexpr int segments=96;constexpr float tau=6.28318530718f;for(int i=0;i<segments;i++){float a=tau*i/segments,next=tau*(i+1)/segments;AddLine(v,cx+cosf(a)*radius,cy+sinf(a)*radius,cx+cosf(next)*radius,cy+sinf(next)*radius,w,h,r,g,b);}}
void AddRect(std::vector<D3DVertex>&v,float x,float y,float rw,float rh,float w,float h,float r,float g,float b,float a){auto add=[&](float px,float py){v.push_back({px/w*2-1,1-py/h*2,r,g,b,a});};add(x,y);add(x+rw,y);add(x+rw,y+rh);add(x,y);add(x+rw,y+rh);add(x,y+rh);}
const unsigned char* Glyph(char c){
 static const unsigned char blank[5]={0,0,0,0,0};
#define G(ch,a,b,c,d,e) case ch:{static const unsigned char g[5]={a,b,c,d,e};return g;}
 switch(c){
 G('0',0x3e,0x51,0x49,0x45,0x3e) G('1',0,0x42,0x7f,0x40,0) G('2',0x42,0x61,0x51,0x49,0x46)
 G('3',0x21,0x41,0x45,0x4b,0x31) G('4',0x18,0x14,0x12,0x7f,0x10) G('5',0x27,0x45,0x45,0x45,0x39)
 G('6',0x3c,0x4a,0x49,0x49,0x30) G('7',0x01,0x71,0x09,0x05,0x03) G('8',0x36,0x49,0x49,0x49,0x36)
 G('9',0x06,0x49,0x49,0x29,0x1e) G('A',0x7e,0x11,0x11,0x11,0x7e) G('B',0x7f,0x49,0x49,0x49,0x36)
 G('C',0x3e,0x41,0x41,0x41,0x22) G('D',0x7f,0x41,0x41,0x22,0x1c) G('E',0x7f,0x49,0x49,0x49,0x41)
 G('F',0x7f,0x09,0x09,0x09,0x01) G('G',0x3e,0x41,0x49,0x49,0x7a) G('H',0x7f,0x08,0x08,0x08,0x7f)
 G('I',0,0x41,0x7f,0x41,0) G('J',0x20,0x40,0x41,0x3f,0x01) G('K',0x7f,0x08,0x14,0x22,0x41)
 G('L',0x7f,0x40,0x40,0x40,0x40) G('M',0x7f,0x02,0x0c,0x02,0x7f) G('N',0x7f,0x04,0x08,0x10,0x7f)
 G('O',0x3e,0x41,0x41,0x41,0x3e) G('P',0x7f,0x09,0x09,0x09,0x06) G('Q',0x3e,0x41,0x51,0x21,0x5e)
 G('R',0x7f,0x09,0x19,0x29,0x46) G('S',0x46,0x49,0x49,0x49,0x31) G('T',0x01,0x01,0x7f,0x01,0x01)
 G('U',0x3f,0x40,0x40,0x40,0x3f) G('V',0x1f,0x20,0x40,0x20,0x1f) G('W',0x3f,0x40,0x38,0x40,0x3f)
 G('X',0x63,0x14,0x08,0x14,0x63) G('Y',0x07,0x08,0x70,0x08,0x07) G('Z',0x61,0x51,0x49,0x45,0x43)
 G(':',0,0x36,0x36,0,0) G('-',0x08,0x08,0x08,0x08,0x08) G('.',0,0x60,0x60,0,0)
 default:return blank;
 }
#undef G
}
void AddText(std::vector<D3DVertex>&v,float x,float y,const char*text,float scale,float w,float h,float r,float g,float b){for(;*text;text++,x+=6*scale){auto glyph=Glyph(*text);for(int col=0;col<5;col++)for(int row=0;row<7;row++)if(glyph[col]&(1<<row))AddRect(v,x+col*scale,y+row*scale,scale,scale,w,h,r,g,b,1);}}
void AddMenu(std::vector<D3DVertex>&v,float w,float h){
 auto rgb=[](LONG c,int shift){return ((c>>shift)&255)/255.f;};float br=rgb(themeColors[0],16),bg=rgb(themeColors[0],8),bb=rgb(themeColors[0],0),tr=rgb(themeColors[1],16),tg=rgb(themeColors[1],8),tb=rgb(themeColors[1],0),ar=rgb(themeColors[2],16),ag=rgb(themeColors[2],8),ab=rgb(themeColors[2],0),hr=rgb(themeColors[4],16),hg=rgb(themeColors[4],8),hb=rgb(themeColors[4],0);
 const int page=std::clamp(menuPage.load(),0,9);const char*pages[10]={"MAIN","PLAYER","ARENA","EXTERNAL AIM","NATIVE AIM","EXTERNAL ESP","NATIVE ESP","STYLE","HOTKEYS","WEAPON"};
 const char*mainRows[2]={"RESET DEFAULTS","UNLOAD"};const char*playerRows[6]={"GOD MODE","CASH AMOUNT","GIVE CUSTOM CASH","GIVE 1000 CASH","GIVE 200000 CASH","RESTORE GOD BYTES"};const char*arenaRows[1]={"SKIP WAVE DISABLED"};const char*externalAimRows[1]={"LEGACY TARGET PUBLISHER"};const char*nativeAimRows[8]={"NATIVE AIM","FOV AIM LIMIT","FOV RADIUS","CROSSHAIR PRIORITY","TARGET MARKER","TARGET SNAPLINE","ALWAYS CRITICAL","EXPLOSIVE SILENT AIM"};const char*externalEspRows[1]={"EXTERNAL ESP FALLBACK"};const char*nativeEspRows[7]={"NATIVE ESP","SKELETONS","BOXES","ALL HOSTILE SNAPLINES","DISTANCE","REFRESH","RENDER STYLE"};const char*styleRows[9]={"BACKGROUND","TEXT","ACCENT","BORDER","HIGHLIGHT","HEALTH BAR","ZOMBIE","FRIENDLY HUMAN","HOSTILE HUMAN"};const char*hotkeyRows[5]={"GOD MODE KEY","SKIP WAVE DISABLED","CANCEL WAVE KEY","MENU KEY","CLOSE OVERLAY KEY"};const char*weaponRows[10]={"MAGAZINE AMMO","MAGAZINE CAPACITY","RESERVE AMMO","SHOT TYPE","SHOT/ANIMATION TYPE","FULL AUTO","FIRE INTERVAL","INTERVAL VALUE","RESTORE NATIVE GUN","KICK ROCKET"};
 const char**rows=mainRows;int count=2;if(page==1){rows=playerRows;count=6;}else if(page==2){rows=arenaRows;count=1;}else if(page==3){rows=externalAimRows;count=1;}else if(page==4){rows=nativeAimRows;count=8;}else if(page==5){rows=externalEspRows;count=1;}else if(page==6){rows=nativeEspRows;count=7;}else if(page==7){rows=styleRows;count=9;}else if(page==8){rows=hotkeyRows;count=5;}else if(page==9){rows=weaponRows;count=10;}
 const int selected=std::clamp(menuIndex.load(),0,count-1);constexpr float pw=510,panelH=450,rowH=31,margin=36;const float x=std::max(margin,w-pw-margin),y=std::max(margin,(h-panelH)*.5f);
 AddRect(v,x,y,pw,panelH,w,h,br,bg,bb,.94f);AddRect(v,x,y,pw,42,w,h,ar,ag,ab,.96f);AddText(v,x+16,y+14,"DIDE INTERNAL MENU",2,w,h,tr,tg,tb);AddText(v,x+16,y+50,pages[page],2,w,h,ar,ag,ab);
 for(int i=0;i<count;i++){float ry=y+83+i*rowH;if(i==selected)AddRect(v,x+10,ry-7,pw-20,27,w,h,hr,hg,hb,.9f);AddText(v,x+22,ry,rows[i],2,w,h,tr,tg,tb);char value[32]{};float valueX=x+385;if(page==1&&i==0)strcpy_s(value,godModeEnabled?"ON":"OFF");else if(page==1&&i==1)sprintf_s(value,"%d",customCash.load());else if(page==3)strcpy_s(value,externalAimEnabled?"ON":"OFF");else if(page==4&&i==0)strcpy_s(value,enabled?"ON":"OFF");else if(page==4&&i==1)strcpy_s(value,fovAimEnabled?"ON":"OFF");else if(page==4&&i==2){const float trackX=x+270,trackW=145,fill=trackW*(fovRadiusPixels.load()-50)/750.f;AddRect(v,trackX,ry+3,trackW,5,w,h,.18f,.2f,.24f,1);AddRect(v,trackX,ry+3,fill,5,w,h,ar,ag,ab,1);sprintf_s(value,"%d",fovRadiusPixels.load());valueX=x+430;}else if(page==4&&i==3)strcpy_s(value,crosshairPriorityEnabled?"ON":"OFF");else if(page==4&&i==4)strcpy_s(value,targetMarkerEnabled?"ON":"OFF");else if(page==4&&i==5)strcpy_s(value,targetSnaplineEnabled?"ON":"OFF");else if(page==5)strcpy_s(value,externalEspEnabled?"ON":"OFF");else if(page==6&&i<3)strcpy_s(value,(i==0?espEnabled.load():i==1?espSkeletonsEnabled.load():espBoxesEnabled.load())?"ON":"OFF");else if(page==6&&i==3)strcpy_s(value,espSnaplinesEnabled?"ON":"OFF");else if(page==6&&i==4)sprintf_s(value,"%d M",control?control->maxDistanceTenths/10:400);else if(page==6&&i==5)sprintf_s(value,"%d MS",control?control->skeletonIntervalMs:8);else if(page==6&&i==6)sprintf_s(value,"%d",control?control->styleScaleHundredths/10:0);else if(page==8)sprintf_s(value,"%d",hotkeys[i].load());AddText(v,valueX,ry,value,2,w,h,ar,ag,ab);}
 if(page==4){char value[8]{};strcpy_s(value,alwaysCriticalEnabled.load()?"ON":"OFF");AddText(v,x+385,y+83+6*rowH,value,2,w,h,ar,ag,ab);strcpy_s(value,explosiveSilentAimEnabled.load()?"ON":"OFF");AddText(v,x+385,y+83+7*rowH,value,2,w,h,ar,ag,ab);}if(page==9)for(int i=0;i<10;i++){char value[32]{};if(i<3&&ammoEditField.load()==i)sprintf_s(value,ammoEditHasDigits.load()?">%d_":">_",ammoEditValue.load());else if(i<3&&ammoDisplayValid.load()){const int displayed[3]={ammoDisplayLoaded.load(),ammoDisplayCapacity.load(),ammoDisplayReserve.load()};sprintf_s(value,"%d",displayed[i]);}else if(i<3)strcpy_s(value,"N/A");else if(i==3){static const char*shotLabels[4]={"NORMAL","SHOTGUN","ROCKET","RIFLE"};strcpy_s(value,shotLabels[std::clamp(weaponShotTypeMode.load(),0,3)]);}else if(i==4){static const char*profileLabels[4]={"NORMAL","PISTOL","RIFLE","SHOTGUN"};strcpy_s(value,profileLabels[std::clamp(weaponProfileType.load(),0,3)]);}else if(i==5)strcpy_s(value,weaponFullAutoEnabled.load()?"ON":"OFF");else if(i==6)strcpy_s(value,weaponIntervalEnabled.load()?"ON":"OFF");else if(i==8)strcpy_s(value,"APPLY");else if(i==9)strcpy_s(value,kickRocketEnabled.load()?"ON":"OFF");else{float trackX=x+270,trackW=105,fill=trackW*(weaponIntervalHundredths.load()-1)/99.f;AddRect(v,trackX,y+83+i*rowH+3,trackW,5,w,h,.18f,.2f,.24f,1);AddRect(v,trackX,y+83+i*rowH+3,fill,5,w,h,ar,ag,ab,1);sprintf_s(value,"%.2f S",weaponIntervalHundredths.load()/100.f);}AddText(v,x+382,y+83+i*rowH,value,2,w,h,ar,ag,ab);}
 AddText(v,x+16,y+414,menuStatus,1,w,h,.85f,.65f,.25f);AddText(v,x+16,y+432,ammoEditField.load()>=0?"DIGITS TYPE   BACKSPACE EDIT   ENTER APPLY   ESC CANCEL":"PGUP PGDN PAGE   ARROWS CHANGE   ENTER SELECT",1,w,h,.55f,.66f,.72f);
}
bool AppendSkeleton(std::vector<D3DVertex>&verts,const EspEntity&entity,V root,unsigned long long now,V camera,V right,V up,V back,float sx,float sy,float w,float h,float r,float g,float b,float baseThickness,float style){const std::vector<SkeletonSegment>*segments=nullptr;if(!ResolveSkeleton(entity,root,now,segments)||!segments)return false;unsigned added=0;for(const auto&segment:*segments){float ax=0,ay=0,bx=0,by=0;if(!ProjectD3D(segment.a,camera,right,up,back,sx,sy,w,h,ax,ay)||!ProjectD3D(segment.b,camera,right,up,back,sx,sy,w,h,bx,by))continue;if(std::max(ax,bx)<0||std::min(ax,bx)>w||std::max(ay,by)<0||std::min(ay,by)>h)continue;float boneLength=Distance(segment.a,segment.b),scaledTaper=std::clamp(boneLength/.2f,.3f,1.3f);currentLineThickness=baseThickness*(1.f+style*(scaledTaper-1.f));AddLine(verts,ax,ay,bx,by,w,h,r,g,b);if(++added>=128)break;}return added>=4;}
void DrawNativeEsp(IDXGISwapChain *sc) {
  const bool aimActive=enabled.load(std::memory_order_acquire),drawEsp=espEnabled.load(std::memory_order_acquire),drawMenu=menuOpen.load(std::memory_order_acquire),drawFov=fovAimEnabled.load(std::memory_order_acquire),drawTargetMarker=aimActive&&targetMarkerEnabled.load(std::memory_order_acquire),drawTargetLine=aimActive&&targetSnaplineEnabled.load(std::memory_order_acquire);
  if (!drawEsp&&!drawMenu&&!drawFov&&!drawTargetMarker&&!drawTargetLine)
    return;
  if (!ForegroundGame()) {
    presentForegroundRejects++;
    return;
  }
  DXGI_SWAP_CHAIN_DESC sd{};
  if (FAILED(sc->GetDesc(&sd)) || !sd.BufferDesc.Width ||
      !sd.BufferDesc.Height) {
    presentDescRejects++;
    return;
  }
  presentWidth = sd.BufferDesc.Width;
  presentHeight = sd.BufferDesc.Height;
  ID3D11Device *d = nullptr;
  if (FAILED(sc->GetDevice(__uuidof(ID3D11Device), (void **)&d)) || !d) {
    presentDeviceRejects++;
    return;
  }
  ID3D11DeviceContext *ctx = nullptr;
  d->GetImmediateContext(&ctx);
  if (!ctx || !EnsureD3D(d)) {
    presentResourceRejects++;
    if (ctx)
      ctx->Release();
    d->Release();
    return;
  }
  float w = (float)sd.BufferDesc.Width, h = (float)sd.BufferDesc.Height;
  std::vector<D3DVertex> verts;
  verts.reserve(32768);
  auto now = GetTickCount64();
  if(drawEsp){V pos{}, right{}, up{}, back{};float sx=0,sy=0;EspFrame frame{};
   if(!ReadD3DCamera(pos,right,up,back,sx,sy))presentCameraRejects++;
   else if(!LoadEsp(frame))presentFrameRejects++;
   else {float maxDistance = NativeMaxDistance(), style = NativeStyleScale();
   for (unsigned i = 0; i < frame.count; i++) {
    V root{};
    if (!ValidateEspEntity(frame.entities[i], root)) {
      presentEntityRejects++;
      continue;
    }
    V observedRoot = root;
    V displayRoot = SmoothEspRoot(frame.entities[i], observedRoot, now);
    float distance = Distance(root, pos);
    if (!std::isfinite(distance) || distance > maxDistance)
      continue;
    float r = 1.f, g = 1.f, b = 1.f;
    NativeColor(frame.entities[i], r, g, b);
    float scaledThickness = std::clamp(220.f / std::max(distance, 1.f), 3.f, 22.f);
    float baseThickness = 2.2f + style * (scaledThickness - 2.2f);
    currentLineThickness = baseThickness;
    bool rendered = false;
    if (espSkeletonsEnabled.load(std::memory_order_relaxed))
      rendered = AppendSkeleton(verts, frame.entities[i], observedRoot, now, pos, right,
                                up, back, sx, sy, w, h, r, g, b,
                                baseThickness, style);
    if (espBoxesEnabled.load(std::memory_order_relaxed)) {
      float boxLeft = 0, boxTop = 0, boxRight = 0, boxBottom = 0;
      if (ComputeEspBox(frame.entities[i], displayRoot, now, pos, right, up, back, sx,
                        sy, w, h, boxLeft, boxTop, boxRight, boxBottom)) {
        AddLine(verts, boxLeft, boxTop, boxRight, boxTop, w, h, r, g, b);
        AddLine(verts, boxRight, boxTop, boxRight, boxBottom, w, h, r, g, b);
        AddLine(verts, boxRight, boxBottom, boxLeft, boxBottom, w, h, r, g, b);
        AddLine(verts, boxLeft, boxBottom, boxLeft, boxTop, w, h, r, g, b);
        rendered = true;
      }
    }
    if (espSnaplinesEnabled.load(std::memory_order_relaxed) && (!frame.entities[i].humanType || frame.entities[i].hostile)) {
      float lineX=0,lineY=0;
      if(ProjectD3D(observedRoot,pos,right,up,back,sx,sy,w,h,lineX,lineY)){
        currentLineThickness=2.f;AddLine(verts,w*.5f,h-1.f,lineX,lineY,w,h,r,g,b);espSnaplineSegments++;rendered=true;
      }
    }
    if (!rendered)
      presentProjectionRejects++;
   }}
  }
  if(drawFov){currentLineThickness=2.f;AddCircle(verts,w*.5f,h*.5f,(float)fovRadiusPixels.load(),w,h,.86f,.14f,.14f);}
  if(drawTargetMarker||drawTargetLine){V pos{},right{},up{},back{};float sx=0,sy=0;Target target{};uintptr_t entityControl=0;int why=0;if(ReadD3DCamera(pos,right,up,back,sx,sy)&&ResolveTarget(target,entityControl,why)){V liveTarget{};if(Read(target.address+0x6F8,liveTarget)&&Finite(liveTarget)){liveTarget.y+=1.55f;float tx=0,ty=0;if(ProjectD3D(liveTarget,pos,right,up,back,sx,sy,w,h,tx,ty)){if(drawTargetLine){currentLineThickness=2.5f;AddLine(verts,w*.5f,h-1.f,tx,ty,w,h,1.f,.86f,.12f);targetSnaplineFrames++;}if(drawTargetMarker){currentLineThickness=2.5f;AddCircle(verts,tx,ty,14.f,w,h,1.f,.86f,.12f);targetMarkerFrames++;}}}}}
  if(drawMenu)AddMenu(verts,w,h);
  if (verts.empty()) {
    ctx->Release();
    d->Release();
    return;
  }
  presentVertexFrames++;
  ID3D11Texture2D *bb = nullptr;
  ID3D11RenderTargetView *rtv = nullptr;
  if (SUCCEEDED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&bb)))
    d->CreateRenderTargetView(bb, nullptr, &rtv);
  if (!rtv) {
    if (bb)
      bb->Release();
    ctx->Release();
    d->Release();
    return;
  }
  D3D11_MAPPED_SUBRESOURCE map{};
  if (FAILED(ctx->Map(d3dVertices, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
    rtv->Release();
    bb->Release();
    ctx->Release();
    d->Release();
    return;
  }
  memcpy(map.pData, verts.data(), verts.size() * sizeof(D3DVertex));
  ctx->Unmap(d3dVertices, 0);
  ID3D11RenderTargetView *oldRtv = nullptr;
  ID3D11DepthStencilView *oldDsv = nullptr;
  ID3D11BlendState *oldBlend = nullptr;
  ID3D11DepthStencilState *oldDepth = nullptr;
  ID3D11RasterizerState *oldRaster = nullptr;
  ID3D11VertexShader *oldVs = nullptr;
  ID3D11PixelShader *oldPs = nullptr;
  ID3D11GeometryShader *oldGs = nullptr;
  ID3D11HullShader *oldHs = nullptr;
  ID3D11DomainShader *oldDs = nullptr;
  ID3D11InputLayout *oldLayout = nullptr;
  ID3D11Buffer *oldVb = nullptr;
  D3D11_PRIMITIVE_TOPOLOGY oldTopo{};
  UINT oldStride = 0, oldOffset = 0, stencil = 0, sampleMask = 0,
       viewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
  FLOAT factor[4]{};
  D3D11_VIEWPORT
      viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
  ctx->OMGetRenderTargets(1, &oldRtv, &oldDsv);
  ctx->OMGetBlendState(&oldBlend, factor, &sampleMask);
  ctx->OMGetDepthStencilState(&oldDepth, &stencil);
  ctx->RSGetState(&oldRaster);
  ctx->RSGetViewports(&viewportCount, viewports);
  ctx->VSGetShader(&oldVs, nullptr, nullptr);
  ctx->PSGetShader(&oldPs, nullptr, nullptr);
  ctx->GSGetShader(&oldGs, nullptr, nullptr);
  ctx->HSGetShader(&oldHs, nullptr, nullptr);
  ctx->DSGetShader(&oldDs, nullptr, nullptr);
  ctx->IAGetInputLayout(&oldLayout);
  ctx->IAGetVertexBuffers(0, 1, &oldVb, &oldStride, &oldOffset);
  ctx->IAGetPrimitiveTopology(&oldTopo);
  UINT stride = sizeof(D3DVertex), offset = 0;
  D3D11_VIEWPORT vp{0, 0, w, h, 0, 1};
  ctx->OMSetRenderTargets(1, &rtv, nullptr);
  ctx->OMSetBlendState(d3dBlend, nullptr, 0xffffffff);
  ctx->OMSetDepthStencilState(d3dDepth, 0);
  ctx->RSSetState(d3dRaster);
  ctx->RSSetViewports(1, &vp);
  ctx->IASetInputLayout(d3dLayout);
  ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  ctx->IASetVertexBuffers(0, 1, &d3dVertices, &stride, &offset);
  ctx->VSSetShader(d3dVs, nullptr, 0);
  ctx->PSSetShader(d3dPs, nullptr, 0);
  ctx->GSSetShader(nullptr, nullptr, 0);
  ctx->HSSetShader(nullptr, nullptr, 0);
  ctx->DSSetShader(nullptr, nullptr, 0);
  ctx->Draw((UINT)verts.size(), 0);
  ctx->DSSetShader(oldDs, nullptr, 0);
  ctx->HSSetShader(oldHs, nullptr, 0);
  ctx->GSSetShader(oldGs, nullptr, 0);
  ctx->PSSetShader(oldPs, nullptr, 0);
  ctx->VSSetShader(oldVs, nullptr, 0);
  ctx->IASetPrimitiveTopology(oldTopo);
  ctx->IASetVertexBuffers(0, 1, &oldVb, &oldStride, &oldOffset);
  ctx->IASetInputLayout(oldLayout);
  if (viewportCount)
    ctx->RSSetViewports(viewportCount, viewports);
  ctx->RSSetState(oldRaster);
  ctx->OMSetDepthStencilState(oldDepth, stencil);
  ctx->OMSetBlendState(oldBlend, factor, sampleMask);
  ctx->OMSetRenderTargets(1, &oldRtv, oldDsv);
  if (oldVb)
    oldVb->Release();
  if (oldLayout)
    oldLayout->Release();
  if (oldDs)
    oldDs->Release();
  if (oldHs)
    oldHs->Release();
  if (oldGs)
    oldGs->Release();
  if (oldPs)
    oldPs->Release();
  if (oldVs)
    oldVs->Release();
  if (oldRaster)
    oldRaster->Release();
  if (oldDepth)
    oldDepth->Release();
  if (oldBlend)
    oldBlend->Release();
  if (oldDsv)
    oldDsv->Release();
  if (oldRtv)
    oldRtv->Release();
  rtv->Release();
  bb->Release();
  ctx->Release();
  d->Release();
  presentDraws++;
}
HRESULT __stdcall PresentHook(IDXGISwapChain*sc,UINT sync,UINT flags){presentCalls++;__try{DrawNativeEsp(sc);}__except(EXCEPTION_EXECUTE_HANDLER){espEnabled=false;}return originalPresent(sc,sync,flags);}
bool InstallPresent(){WNDCLASSEXW wc{sizeof(wc),CS_CLASSDC,DefWindowProcW,0,0,GetModuleHandleW(nullptr),nullptr,nullptr,nullptr,nullptr,L"DIDE_PresentProbe",nullptr};RegisterClassExW(&wc);HWND wnd=CreateWindowW(wc.lpszClassName,L"",WS_OVERLAPPEDWINDOW,0,0,8,8,nullptr,nullptr,wc.hInstance,nullptr);DXGI_SWAP_CHAIN_DESC sd{};sd.BufferCount=1;sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.OutputWindow=wnd;sd.SampleDesc.Count=1;sd.Windowed=TRUE;IDXGISwapChain*sc=nullptr;ID3D11Device*d=nullptr;D3D_FEATURE_LEVEL fl{};ID3D11DeviceContext*ctx=nullptr;HRESULT hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&sd,&sc,&d,&fl,&ctx);bool ok=false;if(SUCCEEDED(hr)&&sc){void*target=(*(void***)sc)[8];auto status=MH_Initialize();if(status==MH_OK||status==MH_ERROR_ALREADY_INITIALIZED)ok=MH_CreateHook(target,(void*)&PresentHook,(void**)&originalPresent)==MH_OK&&MH_EnableHook(target)==MH_OK;}if(ctx)ctx->Release();if(d)d->Release();if(sc)sc->Release();if(wnd)DestroyWindow(wnd);UnregisterClassW(wc.lpszClassName,wc.hInstance);return ok;}
void RestorePresent(){if(originalPresent||originalImpact){MH_DisableHook(MH_ALL_HOOKS);MH_Uninitialize();originalPresent=nullptr;originalImpact=nullptr;}ReleaseD3D();}
void Restore(){SetExplosiveSilentAim(false);RestoreAlwaysCritical();RestoreWeaponProfiles();RestoreFullAuto();RestoreWeaponInterval();weaponProfileType=0;weaponFullAutoEnabled=false;weaponIntervalEnabled=false;RestoreShotType(weaponShotTypeMode.load());weaponShotTypeMode=0;alwaysCriticalEnabled=false;if(!site)return;DWORD old{};if(VirtualProtect(site,PatchLength,PAGE_EXECUTE_READWRITE,&old)){memcpy(site,saved,PatchLength);FlushInstructionCache(GetCurrentProcess(),site,PatchLength);DWORD ignored{};VirtualProtect(site,PatchLength,old,&ignored);}site=nullptr;}
void RestoreDebug(){if(!debugSite)return;DWORD old{};if(VirtualProtect(debugSite,sizeof(debugSaved),PAGE_EXECUTE_READWRITE,&old)){memcpy(debugSite,debugSaved,sizeof(debugSaved));FlushInstructionCache(GetCurrentProcess(),debugSite,sizeof(debugSaved));DWORD ignored{};VirtualProtect(debugSite,sizeof(debugSaved),old,&ignored);}debugSite=nullptr;}

void HandleMenuInput(){
 TrueGodTick();
 static bool oldMenu=false,oldUp=false,oldDown=false,oldLeft=false,oldRight=false,oldEnter=false,oldPgUp=false,oldPgDown=false;
 static bool oldGod=false,oldSkip=false,oldCancel=false,oldClose=false;
 static bool captureReady=false;
 int capture=captureHotkey.load();if(capture>=0){if(!captureReady){bool anyDown=false;for(int key=1;key<255;key++)if(GetAsyncKeyState(key)&0x8000){anyDown=true;break;}if(!anyDown)captureReady=true;return;}if(GetAsyncKeyState(VK_ESCAPE)&0x8000){captureHotkey=-1;captureReady=false;SetStatus("HOTKEY CAPTURE CANCELLED");return;}for(int key=1;key<255;key++)if(key!=VK_ESCAPE&&(GetAsyncKeyState(key)&0x8000)){hotkeys[capture]=key;captureHotkey=-1;captureReady=false;SaveNativeSettings();SetStatus("HOTKEY UPDATED");break;}return;}
 static bool ammoReady=false;
 if(ammoEditField.load()>=0){bool enter=(GetAsyncKeyState(VK_RETURN)&0x8000)!=0,escape=(GetAsyncKeyState(VK_ESCAPE)&0x8000)!=0,back=(GetAsyncKeyState(VK_BACK)&0x8000)!=0,any=enter||escape||back;int digit=-1;for(int i=0;i<10;i++)if((GetAsyncKeyState('0'+i)&0x8000)||(GetAsyncKeyState(VK_NUMPAD0+i)&0x8000)){any=true;digit=i;break;}if(!ammoReady){if(!any)ammoReady=true;return;}if(escape){ammoReady=false;CancelAmmoEdit();return;}if(back){ammoEditValue=ammoEditValue.load()/10;ammoEditHasDigits=ammoEditValue.load()!=0;ammoReady=false;return;}if(digit>=0){int before=ammoEditValue.load();if(before<214748364||(before==214748364&&digit<=7)){ammoEditValue=before*10+digit;ammoEditHasDigits=true;}else SetStatus("AMMO VALUE TOO LARGE");ammoReady=false;return;}if(enter){ammoReady=false;CommitAmmoEdit();return;}return;}
 static unsigned long long nextAmmoRefresh=0,nextModifierRefresh=0;auto inputNow=GetTickCount64();if(inputNow>=nextModifierRefresh){MaintainWeaponModifiers();nextModifierRefresh=inputNow+100;}if(menuOpen.load()&&menuPage.load()==9&&inputNow>=nextAmmoRefresh){RefreshWeaponAmmo();nextAmmoRefresh=inputNow+100;}
 const bool menuKey=(GetAsyncKeyState(hotkeys[3].load())&0x8000)!=0,up=(GetAsyncKeyState(VK_UP)&0x8000)!=0,down=(GetAsyncKeyState(VK_DOWN)&0x8000)!=0,left=(GetAsyncKeyState(VK_LEFT)&0x8000)!=0,right=(GetAsyncKeyState(VK_RIGHT)&0x8000)!=0,enter=(GetAsyncKeyState(VK_RETURN)&0x8000)!=0,pgup=(GetAsyncKeyState(VK_PRIOR)&0x8000)!=0,pgdown=(GetAsyncKeyState(VK_NEXT)&0x8000)!=0;
 if(menuKey&&!oldMenu){menuOpen=!menuOpen.load();Log(menuOpen?"MENU_OPEN":"MENU_CLOSED");}
 const bool god=(GetAsyncKeyState(hotkeys[0].load())&0x8000)!=0,skip=(GetAsyncKeyState(hotkeys[1].load())&0x8000)!=0,cancel=(GetAsyncKeyState(hotkeys[2].load())&0x8000)!=0,close=(GetAsyncKeyState(hotkeys[4].load())&0x8000)!=0;
 if(!menuOpen.load()){if(god&&!oldGod)SetGodModeNative(!godModeEnabled.load());if(skip&&!oldSkip)SkipWaveNative();if(cancel&&!oldCancel)SetStatus("WAVE ACTION CANCELLED");if(close&&!oldClose&&externalEspEnabled.load())ToggleExternalEspNative();}
 if(menuOpen.load()){
  int page=menuPage.load();if(pgup&&!oldPgUp){page=(page+9)%10;menuPage=page;menuIndex=0;}if(pgdown&&!oldPgDown){page=(page+1)%10;menuPage=page;menuIndex=0;}const int counts[10]={2,6,1,1,8,1,7,9,5,10};int count=counts[page],index=menuIndex.load();if(up&&!oldUp)index=(index+count-1)%count;if(down&&!oldDown)index=(index+1)%count;menuIndex=index;
  const bool activate=(enter&&!oldEnter)||(right&&!oldRight),decrease=left&&!oldLeft;
  if(page==0&&index==0&&enter&&!oldEnter){LONG defaults[6]={(LONG)0xF20A0A0C,(LONG)0xFFEBEBF0,(LONG)0xFFDC2323,(LONG)0xFFDC2323,(LONG)0xFF823CBE,(LONG)0xFFE6DC1E};memcpy(themeColors,defaults,sizeof(defaults));int keys[5]={VK_F11,VK_F5,VK_F6,VK_INSERT,VK_END};for(int i=0;i<5;i++)hotkeys[i]=keys[i];fovAimEnabled=false;fovRadiusPixels=250;crosshairPriorityEnabled=false;targetMarkerEnabled=false;targetSnaplineEnabled=false;espSnaplinesEnabled=false;SetExplosiveSilentAim(false);if(control){control->skeletonIntervalMs=8;control->maxDistanceTenths=4000;control->styleScaleHundredths=0;control->zombieArgb=(LONG)0xEBFF2323;control->friendlyArgb=(LONG)0xEB00DCFF;control->hostileArgb=(LONG)0xEBFF8C1E;}SaveNativeSettings();SetStatus("DEFAULTS RESTORED");}
  else if(page==0&&index==1&&enter&&!oldEnter){menuOpen=false;stop=true;Log("UNLOAD_MENU");}
  else if(page==1&&index==0&&(activate||decrease))SetGodModeNative(!godModeEnabled.load());
  else if(page==1&&index==1&&(activate||decrease)){int step=(GetAsyncKeyState(VK_SHIFT)&0x8000)?10000:1000;customCash=std::clamp(customCash.load()+(activate?step:-step),-1000000000,1000000000);}
  else if(page==1&&index==2&&enter&&!oldEnter)GiveCashNative(customCash.load());
  else if(page==1&&index==3&&enter&&!oldEnter)GiveCashNative(1000);
  else if(page==1&&index==4&&enter&&!oldEnter)GiveCashNative(200000);
  else if(page==1&&index==5&&enter&&!oldEnter){unsigned char bytes[8]{};ReadBytes(gameBase+GodModeRva,bytes,8);static const unsigned char nops[8]={0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90},godOriginal[8]={0xF3,0x0F,0x11,0x8A,0x2C,0x0D,0,0};if(!memcmp(bytes,nops,8)&&WriteCode(gameBase+GodModeRva,godOriginal,8)){godModeEnabled=false;SetStatus("GOD BYTES RESTORED");}else SetStatus("RESTORE REFUSED - NOT OUR PATCH");}
  else if(page==2&&enter&&!oldEnter)SkipWaveNative();
  else if(page==3&&enter&&!oldEnter)ToggleExternalAimNative();else if(page==4&&index==0&&(activate||decrease)){enabled=!enabled.load();SetStatus(enabled?"NATIVE AIM ENABLED":"NATIVE AIM DISABLED");}
  else if(page==4&&index==1&&(activate||decrease)){fovAimEnabled=!fovAimEnabled.load();SaveNativeSettings();SetStatus(fovAimEnabled?"FOV AIM LIMIT ENABLED":"FULL MAP AIM RESTORED");}
  else if(page==4&&index==2&&(activate||decrease)){int step=(GetAsyncKeyState(VK_SHIFT)&0x8000)?100:25;fovRadiusPixels=std::clamp(fovRadiusPixels.load()+(activate?step:-step),50,800);SaveNativeSettings();SetStatus("FOV RADIUS UPDATED");}
  else if(page==4&&index==3&&(activate||decrease)){crosshairPriorityEnabled=!crosshairPriorityEnabled.load();SaveNativeSettings();SetStatus(crosshairPriorityEnabled?"CROSSHAIR PRIORITY ENABLED":"NEAREST DISTANCE RESTORED");}
  else if(page==4&&index==4&&(activate||decrease)){targetMarkerEnabled=!targetMarkerEnabled.load();SaveNativeSettings();SetStatus(targetMarkerEnabled?"TARGET MARKER ENABLED":"TARGET MARKER DISABLED");}
  else if(page==4&&index==5&&(activate||decrease)){targetSnaplineEnabled=!targetSnaplineEnabled.load();SaveNativeSettings();SetStatus(targetSnaplineEnabled?"TARGET SNAPLINE ENABLED":"TARGET SNAPLINE DISABLED");}
  else if(page==4&&index==6&&(activate||decrease))SetAlwaysCritical(!alwaysCriticalEnabled.load());
  else if(page==4&&index==7&&(activate||decrease))SetExplosiveSilentAim(!explosiveSilentAimEnabled.load());
  else if(page==5&&enter&&!oldEnter)ToggleExternalEspNative();
  else if(page==6&&index==0&&(activate||decrease)){espEnabled=!espEnabled.load();SetStatus(espEnabled?"NATIVE ESP ENABLED":"NATIVE ESP DISABLED");}
  else if(page==6&&index==1&&(activate||decrease))espSkeletonsEnabled=!espSkeletonsEnabled.load();else if(page==6&&index==2&&(activate||decrease))espBoxesEnabled=!espBoxesEnabled.load();else if(page==6&&index==3&&(activate||decrease)){espSnaplinesEnabled=!espSnaplinesEnabled.load();SetStatus(espSnaplinesEnabled?"ALL HOSTILE SNAPLINES ENABLED":"ALL HOSTILE SNAPLINES DISABLED");}
  else if(control&&page==6&&index==4&&(activate||decrease))control->maxDistanceTenths=std::clamp<LONG>(control->maxDistanceTenths+(activate?250:-250),100,8000);else if(control&&page==6&&index==5&&(activate||decrease))control->skeletonIntervalMs=std::clamp<LONG>(control->skeletonIntervalMs+(activate?1:-1),4,50);else if(control&&page==6&&index==6&&(activate||decrease))control->styleScaleHundredths=std::clamp<LONG>(control->styleScaleHundredths+(activate?10:-10),0,200);
  else if(page==7&&(activate||decrease)){LONG*color=index<6?&themeColors[index]:index==6&&control?(LONG*)&control->zombieArgb:index==7&&control?(LONG*)&control->friendlyArgb:index==8&&control?(LONG*)&control->hostileArgb:nullptr;if(color){static const LONG palette[8]={(LONG)0xF20A0A0C,(LONG)0xFFEBEBF0,(LONG)0xFFDC2323,(LONG)0xFF823CBE,(LONG)0xFF1E90FF,(LONG)0xFF45FF60,(LONG)0xFFFF8C1E,(LONG)0xFFFFFFFF};int found=0;for(int i=0;i<8;i++)if((*color&0x00FFFFFF)==(palette[i]&0x00FFFFFF))found=i;*color=palette[(found+(activate?1:7))%8];SaveNativeSettings();SetStatus("COLOR UPDATED");}}
  else if(page==8&&enter&&!oldEnter){captureHotkey=index;SetStatus("PRESS A NEW KEY - ESC CANCELS");}
 else if(page==9&&index<3&&enter&&!oldEnter)BeginAmmoEdit(index);
  else if(page==9&&index==3&&(activate||decrease)){int mode=weaponShotTypeMode.load();SetShotTypeMode((mode+(activate?1:3))%4);}
  else if(page==9&&index==4&&(activate||decrease)){int mode=weaponProfileType.load();SetWeaponProfileType((mode+(activate?1:3))%4);}
  else if(page==9&&index==5&&(activate||decrease))SetWeaponFullAuto(!weaponFullAutoEnabled.load());
  else if(page==9&&index==6&&(activate||decrease))SetWeaponInterval(!weaponIntervalEnabled.load());
  else if(page==9&&index==7&&(activate||decrease)){bool enabledRate=weaponIntervalEnabled.load();if(enabledRate)RestoreWeaponInterval();weaponIntervalHundredths=std::clamp(weaponIntervalHundredths.load()+(activate?1:-1),1,100);if(enabledRate)MaintainWeaponModifiers();SetStatus("FIRE INTERVAL VALUE UPDATED");}
  else if(page==9&&index==8&&enter&&!oldEnter)RestoreNativeGunStyles();
  else if(page==9&&index==9&&(activate||decrease)){kickRocketEnabled=!kickRocketEnabled.load();SetStatus(kickRocketEnabled.load()?"KICK ROCKET ENABLED - CACHE NATURAL ROCKET":"KICK ROCKET DISABLED");}
  if((activate||decrease)&&page==6)SaveNativeSettings();
 }
 oldMenu=menuKey;oldUp=up;oldDown=down;oldLeft=left;oldRight=right;oldEnter=enter;oldPgUp=pgup;oldPgDown=pgdown;oldGod=god;oldSkip=skip;oldCancel=cancel;oldClose=close;
}

DWORD WINAPI Worker(void*){
 DeleteFileW(LogPath().c_str());gameBase=(uintptr_t)GetModuleHandleW(L"gamedll_x64_rwdi.dll");engineBase=(uintptr_t)GetModuleHandleW(L"engine_x64_rwdi.dll");if(!gameBase||!engineBase){Log("STOP,required_module_missing");return 0;}auto dos=(IMAGE_DOS_HEADER*)gameBase;auto nt=(IMAGE_NT_HEADERS64*)(gameBase+dos->e_lfanew);gameSize=nt->OptionalHeader.SizeOfImage;auto edos=(IMAGE_DOS_HEADER*)engineBase;auto ent=(IMAGE_NT_HEADERS64*)(engineBase+edos->e_lfanew);engineSize=ent->OptionalHeader.SizeOfImage;
 toggleEvent=CreateEventW(nullptr,FALSE,FALSE,L"Local\\JentaSE_SilentAimToggle_v1");restoreEvent=CreateEventW(nullptr,FALSE,FALSE,L"Local\\JentaSE_SilentAimRestore_v1");espToggleEvent=CreateEventW(nullptr,FALSE,FALSE,L"Local\\JentaSE_NativeEspToggle_v1");espBoxesToggleEvent=CreateEventW(nullptr,FALSE,FALSE,L"Local\\JentaSE_NativeEspBoxesToggle_v1");espSkeletonsToggleEvent=CreateEventW(nullptr,FALSE,FALSE,L"Local\\JentaSE_NativeEspSkeletonsToggle_v1");controlMap=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Control),L"Local\\JentaSE_SilentAimControl_v1");if(controlMap)control=(Control*)MapViewOfFile(controlMap,FILE_MAP_ALL_ACCESS,0,0,sizeof(Control));if(control){control->skeletonIntervalMs=8;control->maxDistanceTenths=4000;control->styleScaleHundredths=0;control->zombieArgb=(LONG)0xEBFF2323;control->friendlyArgb=(LONG)0xEB00DCFF;control->hostileArgb=(LONG)0xEBFF8C1E;LoadNativeSettings();}enabled=false;espEnabled=false;espBoxesEnabled=false;espSkeletonsEnabled=false;fovAimEnabled=false;crosshairPriorityEnabled=false;targetMarkerEnabled=false;targetSnaplineEnabled=false;espSnaplinesEnabled=false;alwaysCriticalEnabled=false;explosiveSilentAimEnabled=false;weaponShotTypeMode=0;weaponProfileType=0;weaponFullAutoEnabled=false;weaponIntervalEnabled=false;kickRocketEnabled=false;
 if(!Install()){PublishControl(false);Log("STOP,aim_hook_failed");return 0;}bool presentReady=InstallPresent();if(!InstallImpact()){RestorePresent();Restore();PublishControl(false);Log("STOP,redirect_explosion_hook_failed_restored");return 0;}enabled=false;espEnabled=false;PublishControl(true);Log(presentReady?"READY_DISABLED_BY_DEFAULT,native_manager_v1,redirect_explosion_reason18_target_v1":"READY_DISABLED_BY_DEFAULT,native_manager_v1,redirect_explosion_reason18_target_v1,present_failed");bool previousF7=false,previousF8=false,previousF9=false;unsigned long long nextScan=0,nextPresentLog=0;
 while(!stop){HandleMenuInput();auto now=GetTickCount64();if(now>=nextScan){EnumerateAndSelect();nextScan=now+100;}if(now>=nextPresentLog){auto samples=skeletonSamples.load(),micros=skeletonSampleMicros.load();std::ofstream f(LogPath(),std::ios::app);f<<now<<",PRESENT_DIAG,calls="<<presentCalls.load()<<",draws="<<presentDraws.load()<<",esp_enabled="<<espEnabled.load()<<",menu_open="<<menuOpen.load()<<",target_marker="<<targetMarkerEnabled.load()<<",target_snapline="<<targetSnaplineEnabled.load()<<",esp_snaplines="<<espSnaplinesEnabled.load()<<",target_marker_frames="<<targetMarkerFrames.load()<<",target_snapline_frames="<<targetSnaplineFrames.load()<<",esp_snapline_segments="<<espSnaplineSegments.load()<<",fg_reject="<<presentForegroundRejects.load()<<",desc_reject="<<presentDescRejects.load()<<",device_reject="<<presentDeviceRejects.load()<<",resource_reject="<<presentResourceRejects.load()<<",camera_reject="<<presentCameraRejects.load()<<",frame_reject="<<presentFrameRejects.load()<<",entity_reject="<<presentEntityRejects.load()<<",projection_reject="<<presentProjectionRejects.load()<<",height_reject="<<presentHeightRejects.load()<<",vertex_frames="<<presentVertexFrames.load()<<",pose_head="<<poseHeadSuccess.load()<<",pose_fallback="<<poseHeadFallback.load()<<",skeleton_samples="<<samples<<",skeleton_success="<<skeletonSampleSuccess.load()<<",skeleton_avg_us="<<(samples?micros/samples:0)<<",skeleton_max_us="<<skeletonSampleMaxMicros.load()<<",skeleton_segments="<<skeletonSegments.load()<<",width="<<presentWidth.load()<<",height="<<presentHeight.load()<<'\n';nextPresentLog=now+1000;}if(captureActive&&now>=captureUntil)FinishCapture();bool modifiers=(GetAsyncKeyState(VK_CONTROL)&0x8000)&&(GetAsyncKeyState(VK_SHIFT)&0x8000),f7=modifiers&&(GetAsyncKeyState(VK_F7)&0x8000),f8=modifiers&&(GetAsyncKeyState(VK_F8)&0x8000),f9=modifiers&&(GetAsyncKeyState(VK_F9)&0x8000);bool eventToggle=toggleEvent&&WaitForSingleObject(toggleEvent,0)==WAIT_OBJECT_0,eventRestore=restoreEvent&&WaitForSingleObject(restoreEvent,0)==WAIT_OBJECT_0,eventEsp=espToggleEvent&&WaitForSingleObject(espToggleEvent,0)==WAIT_OBJECT_0,eventBoxes=espBoxesToggleEvent&&WaitForSingleObject(espBoxesToggleEvent,0)==WAIT_OBJECT_0,eventSkeletons=espSkeletonsToggleEvent&&WaitForSingleObject(espSkeletonsToggleEvent,0)==WAIT_OBJECT_0;if(eventToggle||(f7&&!previousF7)){enabled=!enabled.load();Log(enabled?"ENABLED":"DISABLED");MessageBeep(enabled?MB_OK:MB_ICONHAND);}if(eventEsp){bool next=!espEnabled.load();espEnabled=next;Log(next?"ESP_ENABLED":"ESP_DISABLED");}if(eventBoxes){espBoxesEnabled=!espBoxesEnabled.load();Log(espBoxesEnabled?"ESP_BOXES_ENABLED":"ESP_BOXES_DISABLED");}if(eventSkeletons){espSkeletonsEnabled=!espSkeletonsEnabled.load();Log(espSkeletonsEnabled?"ESP_SKELETONS_ENABLED":"ESP_SKELETONS_DISABLED");}if(f8&&!previousF8&&!captureActive)ArmCapture();if(eventRestore||(f9&&!previousF9)){stop=true;Log("RESTORE_REQUESTED");}PublishControl(true);previousF7=f7;previousF8=f8;previousF9=f9;Sleep(5);}
 if(godModeEnabled.load())SetGodModeNative(false);StopChild(overlayProcess);StopChild(bridgeProcess);StopChild(managerProcess);enabled=false;espEnabled=false;RestoreAiDebug();RestorePresent();Restore();PublishControl(false);Log("STOP,restored");if(control)UnmapViewOfFile(control);if(controlMap)CloseHandle(controlMap);if(toggleEvent)CloseHandle(toggleEvent);if(restoreEvent)CloseHandle(restoreEvent);if(espToggleEvent)CloseHandle(espToggleEvent);if(espBoxesToggleEvent)CloseHandle(espBoxesToggleEvent);if(espSkeletonsToggleEvent)CloseHandle(espSkeletonsToggleEvent);return 0;
}
}

BOOL APIENTRY DllMain(HMODULE module,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){selfModule=module;DisableThreadLibraryCalls(module);CreateThread(nullptr,0,Worker,nullptr,0,nullptr);}return TRUE;}
