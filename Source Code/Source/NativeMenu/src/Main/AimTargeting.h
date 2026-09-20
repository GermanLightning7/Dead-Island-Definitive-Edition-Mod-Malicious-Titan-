#pragma once
namespace AimTargeting {
std::atomic_int priority{0},bodyPart{0};
int Options(){return priority.load()|(bodyPart.load()<<2)|(CameraLockOn::targetSource.load()<<3);}
const char* PriorityLabel(){static const char* names[]={"CLOSEST","LOWEST HP","CROSSHAIR"};return names[std::clamp(priority.load(),0,2)];}
const char* PartLabel(){return bodyPart.load()?"CHEST":"HEAD";}
struct State {EspEntity locked{};bool hasLock=false;uintptr_t player=0;int options=-1,source=-1;unsigned cursor=0;};
std::atomic<unsigned long long> poseMisses{0};
bool Select(uintptr_t player,int source,State&state,V&point,EspEntity&entity);
}
