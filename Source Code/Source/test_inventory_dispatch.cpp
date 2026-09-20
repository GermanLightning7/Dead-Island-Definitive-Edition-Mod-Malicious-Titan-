#include <windows.h>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <cstring>
#include <cassert>
#include <iostream>
uintptr_t gameBase=0x10000000,engineBase=0x20000000,physicalEngine=0x20000000;
std::atomic_bool stop{false},menuOpen{false};bool foreground=true,work=true;unsigned pumps=0,originals=0;DWORD threadId=1;
std::unordered_map<uintptr_t,std::vector<char>> memory;
template<class T>bool Read(uintptr_t p,T&v){auto i=memory.find(p);if(i==memory.end()||i->second.size()!=sizeof(T))return false;memcpy(&v,i->second.data(),sizeof(T));return true;}
template<class T>void Put(uintptr_t p,T v){auto&b=memory[p];b.resize(sizeof(T));memcpy(b.data(),&v,sizeof(T));}
bool Pointer(uintptr_t p,uintptr_t&v){return Read(p,v)&&v;}
bool TrueGodPlayer(uintptr_t&p){p=0x30000000;return true;}
bool ForegroundGame(){return foreground;}
bool HasPendingInventoryCommands(){return work;}void PumpInventoryCommands(){++pumps;}
DWORD TestThread(){return threadId;}
#define GetCurrentThreadId TestThread
bool TrueGodBuildMatches(){return true;}bool ReadBytes(uintptr_t,void*,size_t){return false;}void Log(const char*){}
constexpr int MH_OK=0,MH_ERROR_DISABLED=1,MH_ERROR_ALREADY_INITIALIZED=2;
int MH_Initialize(){return 0;}int MH_CreateHook(void*,void*,void**){return 0;}int MH_EnableHook(void*){return 0;}int MH_DisableHook(void*){return 0;}
#include "NativeMenu/src/Main/InventoryDispatch.inl"
void Original(void*){++originals;}
int main(){
 Put(engineBase+0xA4FA38,uintptr_t(0x40000000));Put(engineBase+0x7C64D8,uintptr_t(0x50000000));Put(0x50000000,DWORD(1));
 Put(0x30000048,uintptr_t(0x60000000));Put(0x60000048,uintptr_t(0x70000000));Put(0x700000A8,uintptr_t(0x80000000));
 Put(0x80000010,gameBase+0xD048D8);Put(0x80000030,uintptr_t(0x90000000));Put(0x90000020,uintptr_t(0x80000010));Put(0x90000040,uint64_t(0));Put(0x90000048,uintptr_t(0x90000000));
 InventoryDispatch::original=Original;InventoryDispatch::ready=true;InventoryDispatch::accepting=true;
 InventoryDispatch::Update((void*)0x40000000);assert(pumps==1&&originals==1&&InventoryDispatch::callbacks==0);
 threadId=2;InventoryDispatch::Update((void*)0x40000000);assert(pumps==1);
 Put(0x50000000,DWORD(2));InventoryDispatch::Update((void*)0x40000000);assert(pumps==2);
 menuOpen=true;InventoryDispatch::Update((void*)0x40000000);assert(pumps==2);menuOpen=false;
 Put(0x90000040,uint64_t(0x300000000ull));InventoryDispatch::Update((void*)0x40000000);assert(pumps==2);Put(0x90000040,uint64_t(0));
 InventoryDispatch::Update((void*)0x40000001);assert(pumps==2);
 work=false;InventoryDispatch::Update((void*)0x40000000);assert(pumps==2);
 assert(InventoryDispatch::Shutdown()&&!InventoryDispatch::ready);
 std::cout<<"PASS inventory-only callback: fresh manager/level/owner, migrating control thread, paused input, invalid context, idle skip, callback lifetime and shutdown\n";
}
