                                                                                       
void MenuRound(std::vector<D3DVertex>&v,float x,float y,float rw,float rh,float w,float h,float r,float g,float b,float a){
 AddRect(v,x+3,y,rw-6,rh,w,h,r,g,b,a);AddRect(v,x+1,y+1,2,rh-2,w,h,r,g,b,a);AddRect(v,x+rw-3,y+1,2,rh-2,w,h,r,g,b,a);AddRect(v,x,y+3,1,rh-6,w,h,r,g,b,a);AddRect(v,x+rw-1,y+3,1,rh-6,w,h,r,g,b,a);
}
void MenuPanel(std::vector<D3DVertex>&v,float w,float h,float br,float bg,float bb,float ar,float ag,float ab,float tr,float tg,float tb){
 MenuRound(v,-209,1,718,537,w,h,br,bg,bb,.99f);
 MenuRound(v,-204,75,198,405,w,h,.105f,.075f,.15f,1);
 AddRect(v,-208,1,716,2,w,h,ar,ag,ab,1);
 MenuRound(v,-190,17,35,35,w,h,.37f,.19f,.56f,1);AddText(v,-180,24,"M",2.1f,w,h,tr,tg,tb);
 AddText(v,-140,16,"Malicious Titan's Mod Menu",2.4f,w,h,tr,tg,tb);
 AddText(v,-139,42,"DEAD ISLAND / DEFINITIVE EDITION",.85f,w,h,.73f,.64f,.87f);
 AddText(v,-186,495,"INSERT",1.2f,w,h,ar,ag,ab);AddText(v,-186,514,"OPEN / CLOSE MENU",.85f,w,h,.68f,.60f,.78f);
 MenuRound(v,475,8,26,26,w,h,.24f,.14f,.34f,1);AddText(v,484,17,"X",1.4f,w,h,tr,tg,tb);
}
void MenuValue(std::vector<D3DVertex>&v,float x,float y,const char*value,float w,float h,float ar,float ag,float ab){
 if(!*value)return;
 bool on=strcmp(value,"ON")==0,off=strcmp(value,"OFF")==0,na=strcmp(value,"N/A")==0;
 float bx=std::min(x,385.f),bw=494.f-bx;float scale=std::min(1.25f,(bw-12.f)/(6.f*strlen(value)));
 MenuRound(v,bx,y-2,bw,19,w,h,on?.28f:.115f,on?.095f:.095f,on?.13f:.105f,1);
 if(on||off)AddRect(v,bx+6,y+5,3,3,w,h,on?ar:.44f,on?ag:.39f,on?ab:.39f,1);
 float tx=bx+(bw-SmoothFont::WidthOf(value,scale))*.5f+(on||off?3.f:0.f);
 AddText(v,tx,y+3,value,scale,w,h,na?.50f:on?.98f:.80f,na?.45f:on?.85f:.74f,na?.45f:on?.83f:.72f);
}

                                                                                   
void MenuHighlight(std::vector<D3DVertex>&v,float x,float y,float rw,float rh,float w,float h,unsigned long long now){
 float wave=.5f+.5f*std::sin((now%100000)/650.f),r=.60f+.20f*wave,g=.30f+.18f*wave,b=.86f+.12f*wave;
 unsigned tint=(unsigned)themeColors[4];MenuRound(v,x,y,rw,rh,w,h,.06f+((tint>>16)&255)/255.f*.65f,((tint>>8)&255)/255.f*.50f,.02f+(tint&255)/255.f*.55f,1);
 for(int i=3;i>=1;i--){float alpha=.06f*(4-i);AddRect(v,x+3,y+i,rw-6,1,w,h,r,g,b,alpha);AddRect(v,x+3,y+rh-i-1,rw-6,1,w,h,r,g,b,alpha);}
 AddRect(v,x+3,y,rw-6,1,w,h,r,g,b,.75f);AddRect(v,x+3,y+rh-1,rw-6,1,w,h,r,g,b,.75f);
 AddRect(v,x,y+3,2,rh-6,w,h,r,g,b,1);AddRect(v,x+rw-1,y+3,1,rh-6,w,h,r,g,b,.6f);
 float travel=(now%1800)/1800.f*(rw+70)-70;
 for(int i=0;i<14;i++){float px=x+travel+i*5;if(px<x+3||px+5>x+rw-3)continue;float a=1.f-std::abs(i-6.5f)/7.f;AddRect(v,px,y,5,1.3f,w,h,.90f,.76f,1.f,a);AddRect(v,x+rw-(px-x)-5,y+rh-1.3f,5,1.3f,w,h,.90f,.76f,1.f,a);}
}
