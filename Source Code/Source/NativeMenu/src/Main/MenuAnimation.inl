namespace MenuAnimation {
struct Frame{float opacity=1,content=1,row=0;};
struct State {
 bool open=false;int page=-1;unsigned long long opened=0,changed=0,last=0;float row=0;
 void Close(){open=false;page=-1;}
 Frame Sample(unsigned long long now,int nextPage,int selected){
  if(!open||now<last){open=true;page=nextPage;opened=changed=last=now;row=(float)selected;}
  if(page!=nextPage){page=nextPage;changed=now;row=(float)selected;}
  float dt=(float)std::min<unsigned long long>(now-last,100);last=now;
  row+=(selected-row)*(1.f-std::exp(-dt/45.f));if(std::abs(row-selected)<.001f)row=(float)selected;
  auto ease=[](float t){t=std::clamp(t,0.f,1.f);return 1.f-(1.f-t)*(1.f-t)*(1.f-t);};
  return {.18f+.82f*ease((now-opened)/180.f),.4f+.6f*ease((now-changed)/120.f),row};
 }
};
State state;
}
