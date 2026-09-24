                                                                                      
namespace EspVitals {
struct Sample { uintptr_t state=0,owner=0; int generation=0; unsigned long long tick=0; float peak=0; };
Sample history[48]{};
float Fraction(const EspEntity& e,float hp,unsigned long long now){
 if(!e.state||!e.complete||e.generation<=0||!std::isfinite(hp)||hp<=0||hp>1000000)return -1.f;
 Sample* slot=&history[0];
 for(auto& entry:history){
  if(entry.state==e.state){slot=&entry;break;}
  if(entry.tick<slot->tick)slot=&entry;
 }
 if(slot->state!=e.state||slot->owner!=e.complete||slot->generation!=e.generation||now<slot->tick||now-slot->tick>750)
  *slot={e.state,e.complete,e.generation,now,hp};
 slot->peak=std::max(slot->peak,hp);slot->tick=now;
 return std::clamp(hp/slot->peak,0.f,1.f);
}
}
bool AppendEspVitals(std::vector<D3DVertex>& v,const EspEntity& e,float hp,float distance,unsigned long long now,
 float left,float top,float right,float bottom,float w,float h){
 if(!std::isfinite(left)||!std::isfinite(top)||!std::isfinite(right)||!std::isfinite(bottom)||
    !std::isfinite(w)||!std::isfinite(h)||w<320||h<200||right<=left||bottom<=top||
    right<0||left>w||bottom<0||top>h||!std::isfinite(distance)||distance<0)return false;
 bool health=espHealthEnabled.load(),dist=espDistanceEnabled.load();
 if(!health&&!dist)return false;
 float fraction=health?EspVitals::Fraction(e,hp,now):-1.f;
 health=health&&fraction>=0;
 if(!health&&!dist)return false;
 const float scale=std::clamp(h/720.f,1.f,2.f);
 char label[64]{};
 if(health&&dist)sprintf_s(label,"%.0f HP*  %.1f M",hp,distance);
 else if(health)sprintf_s(label,"%.0f HP*",hp);
 else sprintf_s(label,"%.1f M",distance);
 const float textWidth=TextWidth(label,scale);
 const float panelWidth=std::max(textWidth+8.f,72.f*scale);
 const float panelHeight=(health?25.f:18.f)*scale;
 const float x=std::clamp((left+right-panelWidth)*.5f,2.f,w-panelWidth-2.f);
 const float y=std::clamp(top-panelHeight-4.f,2.f,h-panelHeight-2.f);
 AddRect(v,x,y,panelWidth,panelHeight,w,h,.025f,.025f,.035f,.88f);
 AddText(v,x+(panelWidth-textWidth)*.5f,y+4.f*scale,label,scale,w,h,1.f,1.f,1.f);
 if(health){
  const float bx=x+4.f,by=y+17.f*scale,bw=panelWidth-8.f,bh=4.f*scale;
  AddRect(v,bx,by,bw,bh,w,h,.18f,.18f,.18f,1.f);
  unsigned color=(unsigned)themeColors[5];
  AddRect(v,bx,by,bw*fraction,bh,w,h,((color>>16)&255)/255.f,((color>>8)&255)/255.f,(color&255)/255.f,1.f);
  espHealthDraws++;
 }
 if(dist)espDistanceDraws++;
 return true;
}
