#pragma once
#include <atomic>
#include <vector>
#include <cmath>

namespace Crosshair {
inline std::atomic_bool enabled{false};

inline void Toggle(){
 bool next=!enabled.load();
 enabled=next;
 Log(next?"CROSSHAIR_ENABLED":"CROSSHAIR_DISABLED");
 SetStatus(next?"CROSSHAIR ENABLED":"CROSSHAIR DISABLED");
}

inline void Draw(std::vector<D3DVertex>&verts,float w,float h){
 if(!enabled.load()||w<=0.f||h<=0.f)return;
 const float cx=std::floor(w*0.5f);
 const float cy=std::floor(h*0.5f);
 const float gap=4.0f;
 const float len=7.0f;
 const float th=2.0f;
 const float halfTh=th*0.5f;

 // Reticle color: high visibility neon green
 const float cr=0.20f,cg=1.0f,cb=0.25f,ca=0.95f;
 // Border shadow for contrast on any background
 const float br=0.0f,bg=0.0f,bb=0.0f,ba=0.85f;

 // Left bar
 AddRect(verts,cx-gap-len-1.f,cy-halfTh-1.f,len+2.f,th+2.f,w,h,br,bg,bb,ba);
 AddRect(verts,cx-gap-len,cy-halfTh,len,th,w,h,cr,cg,cb,ca);

 // Right bar
 AddRect(verts,cx+gap-1.f,cy-halfTh-1.f,len+2.f,th+2.f,w,h,br,bg,bb,ba);
 AddRect(verts,cx+gap,cy-halfTh,len,th,w,h,cr,cg,cb,ca);

 // Top bar
 AddRect(verts,cx-halfTh-1.f,cy-gap-len-1.f,th+2.f,len+2.f,w,h,br,bg,bb,ba);
 AddRect(verts,cx-halfTh,cy-gap-len,th,len,w,h,cr,cg,cb,ca);

 // Bottom bar
 AddRect(verts,cx-halfTh-1.f,cy+gap-1.f,th+2.f,len+2.f,w,h,br,bg,bb,ba);
 AddRect(verts,cx-halfTh,cy+gap,th,len,w,h,cr,cg,cb,ca);

 // Center dot
 AddRect(verts,cx-1.5f,cy-1.5f,3.0f,3.0f,w,h,br,bg,bb,ba);
 AddRect(verts,cx-0.5f,cy-0.5f,1.0f,1.0f,w,h,cr,cg,cb,ca);
}
}
