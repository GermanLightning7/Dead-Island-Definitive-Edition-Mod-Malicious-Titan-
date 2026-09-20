
bool ResolveCharacterBody(uintptr_t player,uintptr_t&body,int&failStep){
 body=0;failStep=0;
 if(!physicalEngine){failStep=1;return false;}                                                                          
 uintptr_t control=0,self=0,link=0,component=0,vt=0,physics=0;int type=0;
 if(!Read(player+0x20,control)||!control){failStep=2;return false;}
 if(!Read(control+0x250,self)||self!=control){failStep=3;return false;}
 if(!Read(control+0x258,link)||!link){failStep=4;return false;}
 if(!Read(link+0x28,component)||!component){failStep=5;return false;}
 if(!Read(component,vt)||vt!=physicalEngine+0x882BD8){failStep=6;return false;}
 if(!Read(component+0x38,type)||type!=3){failStep=7;return false;}
 physics=component+0x6F0;
 if(!Read(physics,vt)||vt!=physicalEngine+0x882D68){failStep=8;return false;}
 if(!Pointer(physics+0x10,body)){failStep=9;return false;}
 return true;
}
bool Unit(V v){float l=v.x*v.x+v.y*v.y+v.z*v.z;return std::isfinite(l)&&l>.5f&&l<1.5f;}

V MotionVelocity(V from,V to,float seconds,float maximum){
 if(!std::isfinite(from.x)||!std::isfinite(from.y)||!std::isfinite(from.z)||!std::isfinite(to.x)||!std::isfinite(to.y)||!std::isfinite(to.z)||!std::isfinite(seconds)||seconds<=0||!std::isfinite(maximum)||maximum<=0)return {};
 V v{(to.x-from.x)/seconds,(to.y-from.y)/seconds,(to.z-from.z)/seconds};
 float speed=sqrtf(v.x*v.x+v.y*v.y+v.z*v.z);if(!std::isfinite(speed))return {};
 if(speed>maximum){float scale=maximum/speed;v={v.x*scale,v.y*scale,v.z*scale};}return v;
}
