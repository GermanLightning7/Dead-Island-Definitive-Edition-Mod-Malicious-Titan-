#define DllMain UnusedDurabilityBaselineDllMain
#include "BaseRuntime.cpp"
#undef DllMain
namespace {
using WearFn=void(*)(void*,float);WearFn wearOriginal=nullptr;
std::atomic_bool repairPending{false},wearOn{false},wearReady{false},wearObserved{false};
std::atomic<unsigned> wearCount{0};
void WearHook(void*item,float amount){
 uintptr_t player=0,owner=0;float before=0,after=0;
 uintptr_t caller=(uintptr_t)_ReturnAddress();
 bool valid=(caller==gameBase+0x7A5456||caller==gameBase+0x767A73)&&TrueGodPlayer(player)&&Read((uintptr_t)item+0xA8,owner)&&owner==player&&Read((uintptr_t)item+0x54,before)&&std::isfinite(before)&&before>=0&&std::isfinite(amount)&&amount>0;
 bool blocked=valid&&wearOn.load();
 if(!blocked)wearOriginal(item,amount);
 if((valid||(caller==gameBase+0x7A5456||caller==gameBase+0x767A73))&&Read((uintptr_t)item+0x54,after)){
  if(valid&&before>after&&!blocked)wearObserved=true;
  std::ofstream f(LogPath(),std::ios::app);
  f<<GetTickCount64()<<",MELEE_WEAR,n="<<++wearCount<<",item=0x"<<std::hex<<(uintptr_t)item<<",owner=0x"<<owner<<",caller_rva=0x"<<(caller-gameBase)<<std::dec<<",valid="<<valid<<",amount="<<amount<<",before="<<before<<",after="<<after<<",blocked="<<blocked<<'\n';
 }
}
void RepairOnMeleeThread(){
 if(!repairPending.exchange(false))return;
 uintptr_t player=0,item=0,owner=0,vt=0,fn=0;float before=0,maximum=0,after=0;
 if(!TrueGodPlayer(player)||!Read(player+0xFF0,item)||!Read(item+0xA8,owner)||owner!=player||!Read(item+0x54,before)||!std::isfinite(before)||before<0||!Read(item+0x60,vt)||!Read(vt+0x5E8,fn)||!InGame(fn)){wearOn=false;Log("REPAIR_REFUSED_INVALID_WEAPON");return;}
                                                                          
 maximum=((float(*)(void*))fn)((void*)(item+0x60));
 if(!std::isfinite(maximum)||maximum<=0||maximum>1000000||before>maximum+.01f){wearOn=false;Log("REPAIR_REFUSED_INVALID_MAXIMUM");return;}
                                                                          
 if(before<maximum)wearOriginal((void*)item,before-maximum);
 bool ok=Read(item+0x54,after)&&std::isfinite(after)&&fabsf(after-maximum)<.01f;
 if(!ok)wearOn=false;
 std::ofstream f(LogPath(),std::ios::app);f<<GetTickCount64()<<",MELEE_REPAIR,item=0x"<<std::hex<<item<<std::dec<<",before="<<before<<",maximum="<<maximum<<",after="<<after<<",ok="<<ok<<'\n';
}
int SetWear(int value){if(!wearReady)return -1;wearOn=value!=0;repairPending=true;return value?1:0;}
DWORD WINAPI WearWorker(void*){
 gameBase=(uintptr_t)GetModuleHandleW(L"gamedll_x64_rwdi.dll");if(!gameBase)return 0;
 auto dos=(IMAGE_DOS_HEADER*)gameBase;gameSize=((IMAGE_NT_HEADERS64*)(gameBase+dos->e_lfanew))->OptionalHeader.SizeOfImage;
 if(!TrueGodBuildMatches()){Log("WEAR_BUILD_MISMATCH");return 0;}
 unsigned char expected[16]={0x41,0x54,0x41,0x55,0x48,0x83,0xEC,0x78,0x4C,0x8D,0x61,0x60,0x44,0x0F,0x29,0x54},actual[16]{};
 void*target=(void*)(gameBase+0x35D9A0);
 if(!ReadBytes((uintptr_t)target,actual,16)||memcmp(expected,actual,16)){Log("WEAR_HOOK_CONFLICT");return 0;}
 if(MH_Initialize()!=MH_OK||MH_CreateHook(target,(void*)WearHook,(void**)&wearOriginal)!=MH_OK||MH_EnableHook(target)!=MH_OK){Log("WEAR_INSTALL_FAILED");return 0;}
 auto melee=GetModuleHandleW(L"DideMeleeNative.dll");
 auto connect=melee?(void(*)(void(*)()))GetProcAddress(melee,"DideMeleeSetTick"):nullptr;
 if(!connect){MH_DisableHook(target);Log("WEAR_REPAIR_BRIDGE_MISSING");return 0;}connect(RepairOnMeleeThread);
 wearReady=true;Log("WEAR_READY_DISABLED_CTRL_SHIFT_F12_TOGGLE");
 bool old=false;
 for(;;){bool key=ForegroundGame()&&(GetAsyncKeyState(VK_CONTROL)&0x8000)&&(GetAsyncKeyState(VK_SHIFT)&0x8000)&&(GetAsyncKeyState(VK_F12)&0x8000);
 if(key&&!old){if(wearOn){SetWear(0);Log("WEAR_DISABLED_REPAIR_QUEUED");}else {SetWear(1);Log("WEAR_ENABLED_REPAIR_QUEUED");}}old=key;Sleep(10);}
}
}
extern "C" __declspec(dllexport) int DideWearState(){return !wearReady?-1:wearOn?1:0;}
extern "C" __declspec(dllexport) int DideWearSet(int value){return SetWear(value);}

BOOL APIENTRY DllMain(HMODULE module,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(module);HANDLE t=CreateThread(nullptr,0,WearWorker,nullptr,0,nullptr);if(t)CloseHandle(t);}return TRUE;}
