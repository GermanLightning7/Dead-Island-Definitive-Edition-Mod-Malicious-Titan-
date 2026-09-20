#pragma once
#include <cstdint>
namespace AiAllegiance {
                                                                       
                                                                       
constexpr bool HostileSide(int side){return side==3||side==4||side==6||side==7||side==8;}
template<class Reader> bool HumanHostile(uintptr_t actor,uintptr_t game,Reader read){
 uintptr_t vt=0;int side=0;
 return actor&&read(actor,vt)&&vt==game+0xD68D88&&read(actor+0x170,side)&&HostileSide(side);
}
}
