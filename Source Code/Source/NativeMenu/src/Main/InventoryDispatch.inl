namespace InventoryDispatch {
using UpdateFn=void(__fastcall*)(void*);
UpdateFn original=nullptr;
std::atomic_bool ready{false},accepting{false};
std::atomic<unsigned> callbacks{0};
bool created=false,installed=false;
struct Lifetime {Lifetime(){callbacks.fetch_add(1);}~Lifetime(){callbacks.fetch_sub(1);}};
bool PlayerLevel(uintptr_t&player,uintptr_t&level){
 uintptr_t backing=0,levelBacking=0,view=0;
 return TrueGodPlayer(player)&&Pointer(player+0x48,backing)&&Pointer(backing+0x48,levelBacking)&&Pointer(levelBacking+0xA8,view)&&(level=view+0x10)!=0;
}
bool Context(uintptr_t manager){
 uintptr_t current=0,address=0,player=0,level=0,vt=0,backing=0,owner=0,self=0;DWORD thread=0;uint64_t flags=0;
 return Read(engineBase+0xA4FA38,current)&&current==manager&&
 Read(engineBase+0x7C64D8,address)&&Read(address,thread)&&thread==GetCurrentThreadId()&&
 PlayerLevel(player,level)&&Read(level,vt)&&vt==gameBase+0xD048D8&&Pointer(level+0x20,backing)&&
 Read(backing+0x20,owner)&&owner==level&&Read(backing+0x40,flags)&&!(flags&0x300000000ull)&&
 Read(backing+0x48,self)&&self==backing;
}
void __fastcall Update(void*manager){
 Lifetime lifetime;original(manager);
 if(!ready||!accepting||stop||menuOpen||!ForegroundGame()||!HasPendingInventoryCommands())return;
 if(!Context((uintptr_t)manager))return;
 PumpInventoryCommands();
}
bool Shutdown(){
 accepting=false;
 if(installed){auto status=MH_DisableHook((void*)(engineBase+0x227050));if(status!=MH_OK&&status!=MH_ERROR_DISABLED)return false;installed=false;}
 auto until=GetTickCount64()+2000;
 while(callbacks&&GetTickCount64()<until)Sleep(1);
 if(callbacks)return false;
 ready=false;return true;
}
bool CanStop(){return Shutdown();}
bool KeepTrampolines(){return created;}
void Install(){
 if(!TrueGodBuildMatches()||!physicalEngine)return;
 const unsigned char expected[16]={0x48,0x83,0xEC,0x28,0xFF,0x81,0xA8,0x06,0x00,0x00,0x48,0x83,0x3D,0x86,0xAE,0x82};
 unsigned char actual[16]{};
 if(!ReadBytes(engineBase+0x227050,actual,16)||memcmp(actual,expected,16))return;
 auto init=MH_Initialize();if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED)return;
 if(MH_CreateHook((void*)(engineBase+0x227050),(void*)Update,(void**)&original)!=MH_OK)return;
 created=true;
 if(MH_EnableHook((void*)(engineBase+0x227050))!=MH_OK)return;
 installed=true;ready=true;accepting=true;Log("INVENTORY_CALLBACK_READY");
}
}
