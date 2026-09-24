namespace AimTargeting {
bool Same(const EspEntity&a,const EspEntity&b){return a.state==b.state&&a.complete==b.complete&&a.generation==b.generation;}
struct Candidate {EspEntity entity{};float score=0,distance=0;};
bool Before(const Candidate&a,const Candidate&b){if(a.score!=b.score)return a.score<b.score;if(a.distance!=b.distance)return a.distance<b.distance;return a.entity.state<b.entity.state;}
bool Select(uintptr_t player,int source,State&state,V&point,EspEntity&entity){
 const int options=Options(),mode=options&3,part=(options>>2)&1;
 if(state.player!=player||state.options!=options||state.source!=source){state={};state.player=player;state.options=options;state.source=source;}
 EspFrame frame{};V eye{},right{},up{},back{};float sx=0,sy=0;
 if(!LoadEsp(frame)||!ReadD3DCamera(eye,right,up,back,sx,sy)){state.hasLock=false;return false;}
 const float width=(float)presentWidth.load(),height=(float)presentHeight.load();
 const bool limit=source==0&&fovAimEnabled.load();unsigned work=0;
 auto projected=[&](V p,float&score){float x=0,y=0;if(width<=0||height<=0||!ProjectD3D(p,eye,right,up,back,sx,sy,width,height,x,y))return false;float dx=x-width*.5f,dy=y-height*.5f;score=dx*dx+dy*dy;return std::isfinite(score);};
 auto basic=[&](const EspEntity&e,V&root,float&health){return !(source==1&&e.humanType)&&!(e.humanType&&!e.hostile)&&ValidateEspEntity(e,root,&health)&&Distance(eye,root)<=NativeMaxDistance();};
 auto eligible=[&](const EspEntity&e){
  V root{};float hp=0;if(!basic(e,root,hp)||work>=4)return false;++work;
  if(!ResolveAimPoseNow(e.state,root,point,part)){++poseMisses;return false;}
  V check{};if(!ValidateEspEntity(e,check)||Distance(check,root)>=.25f||!Finite(point))return false;
  float screen=0;if((limit||mode==2)&&!projected(point,screen))return false;
  float radius=(float)fovRadiusPixels.load();if(limit&&screen>radius*radius)return false;
  CombatLock::Snapshot shot{true,e,player,eye,point,GetTickCount64()};shot.options=options;
  return options==Options()&&CombatLock::Visible(shot);
 };
 if(state.hasLock)for(unsigned i=0;i<frame.count&&i<MaxEspEntities;++i)if(Same(state.locked,frame.entities[i])&&eligible(frame.entities[i])){entity=state.locked;return true;}
 const uintptr_t failed=state.hasLock?state.locked.state:0;state.hasLock=false;
 std::array<Candidate,MaxEspEntities> candidates{};unsigned count=0;
 for(unsigned i=0;i<frame.count&&i<MaxEspEntities;++i){auto&e=frame.entities[i];V root{};float hp=0;if(e.state==failed||!basic(e,root,hp))continue;
  float distance=Distance(eye,root),score=mode==1?hp:distance;
                                                                                               
  if(mode==2&&!projected({root.x,root.y+(part?1.35f:1.55f),root.z},score))continue;
  candidates[count++]={e,score,distance};
 }
 std::sort(candidates.begin(),candidates.begin()+count,Before);
                                                                                                 
 for(unsigned i=0;i<count&&work<4;++i){auto&e=candidates[state.cursor++%count].entity;if(eligible(e)){state.locked=e;state.hasLock=true;state.cursor=0;entity=e;return true;}}
 return false;
}
}
