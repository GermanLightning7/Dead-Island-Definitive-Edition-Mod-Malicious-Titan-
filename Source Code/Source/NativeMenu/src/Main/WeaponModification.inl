#include "BlueprintCatalog.inl"
std::atomic<int> blueprintSelection{0};
std::atomic<bool> blueprintMenuReady{false};
SRWLOCK blueprintCommandLock=SRWLOCK_INIT;
struct BlueprintCommand{bool pending=false;int selection=-1;uintptr_t item=0,base=0,owner=0;ULONGLONG queued=0;};
BlueprintCommand blueprintCommand;
bool BlueprintName(uintptr_t desc,char(&out)[96]){
 uintptr_t vt=0,getter=0,text=0;out[0]=0;
 if(!Read(desc,vt)||!InGame(vt)||!Read(vt+0x68,getter)||getter!=gameBase+0x85B000||!Read(desc+0x10,text)||!text)return false;
 for(int i=0;i<95;i++){if(!Read(text+i,out[i]))return false;if(!out[i])return i>0;}out[95]=0;return false;
}
struct HeldBlueprintState{uintptr_t item=0,base=0,owner=0,blueprint=0;unsigned attributes=0;int quantity=0;float condition=0;};
struct HeldRepairCommand{bool pending=false;HeldBlueprintState before{};ULONGLONG queued=0;};HeldRepairCommand heldRepairCommand;
bool ResolveHeldBlueprint(HeldBlueprintState&s){
 uintptr_t root=0,manager=0,vt=0,wrapper=0,player=0;int offset=0;
 if(!TrueGodPlayer(player)||!Pointer(gameBase+AmmoRootRva,root)||!Pointer(root+0x628,manager)||!Pointer(manager+0xFF0,s.item))return false;
 if(!Read(s.item,vt)||!TypeContains(vt,"InventoryItem",offset)||offset!=0||!Read(s.item+0xA8,s.owner)||s.owner!=player)return false;
 if(!Read(s.item+0x60,wrapper)||wrapper!=gameBase+0xD9AEC8||!Read(s.item+0x68,s.base)||!Read(s.item+0x78,s.blueprint))return false;
 char name[96]{};if(!BlueprintName(s.base,name))return false;
 if(strncmp(name,"Melee_",6)&&strncmp(name,"Firearm_",8)&&strncmp(name,"zzz_Melee_",10)&&strncmp(name,"zzz_Firearm_",12))return false;
 return Read(s.item+0x70,s.attributes)&&s.attributes!=0&&Read(s.item+0x50,s.quantity)&&Read(s.item+0x54,s.condition)&&std::isfinite(s.condition);
}
                                                                            
float BlueprintClampedCondition(float requested,float maximum){
 return maximum>0.f?std::clamp(requested,0.f,maximum):requested;
}
bool BlueprintConditionLimit(uintptr_t wrapper,float& maximum){
 uintptr_t vt=0,fn=0;
 if(!Read(wrapper,vt)||vt!=gameBase+0xD9AEC8||!Read(vt+0x5E8,fn)||!InGame(fn))return false;
 maximum=((float(*)(void*))fn)((void*)wrapper);
 return std::isfinite(maximum);
}
bool BlueprintStateMatches(const HeldBlueprintState& before,const HeldBlueprintState& after,uintptr_t blueprint,float expected){
 return after.item==before.item&&after.base==before.base&&after.owner==before.owner&&after.blueprint==blueprint&&after.attributes==before.attributes&&after.quantity==before.quantity&&std::isfinite(after.condition)&&std::fabs(after.condition-expected)<=0.001f;
}
void QueueBlueprintChange(bool remove){
 if(!blueprintMenuReady){SetStatus("BLUEPRINT EDIT UNAVAILABLE");return;}
 HeldBlueprintState s{};if(!ResolveHeldBlueprint(s)){SetStatus("EQUIP A GUN OR MELEE WEAPON");return;}
 AcquireSRWLockExclusive(&blueprintCommandLock);
 if(blueprintCommand.pending||heldRepairCommand.pending){ReleaseSRWLockExclusive(&blueprintCommandLock);SetStatus("BLUEPRINT CHANGE ALREADY QUEUED");return;}
 blueprintCommand={true,remove?-1:blueprintSelection.load(),s.item,s.base,s.owner,GetTickCount64()};
 ReleaseSRWLockExclusive(&blueprintCommandLock);menuOpen=false;SetStatus("BLUEPRINT CHANGE REQUESTED");
}
void ConsumeBlueprintChange(){
 BlueprintCommand cmd{};AcquireSRWLockExclusive(&blueprintCommandLock);cmd=blueprintCommand;blueprintCommand.pending=false;ReleaseSRWLockExclusive(&blueprintCommandLock);
 if(!cmd.pending)return;
 HeldBlueprintState s{};
 if(GetTickCount64()-cmd.queued>15000||!ResolveHeldBlueprint(s)||s.item!=cmd.item||s.base!=cmd.base||s.owner!=cmd.owner){SetStatus("CANCELLED - HELD WEAPON CHANGED");return;}
 uintptr_t table=0;int count=0;unsigned char init=0;
 if(!Read(gameBase+0x132F648,init)||!(init&1)||!Read(gameBase+0x13279C8,table)||!Read(gameBase+0x13279D0,count)||!table||count<=0||count>10000){SetStatus("BLUEPRINT CATALOGUE UNAVAILABLE");return;}
 if(cmd.selection>=BlueprintCount||cmd.selection<-1)return;
 int baseId=-1;uintptr_t selected=0;
 for(int i=0;i<count;i++){
  uintptr_t desc=0;if(!Read(table+(uintptr_t)i*8,desc))break;
  if(desc==s.base)baseId=i;
  if(cmd.selection>=0&&desc){char name[96]{};if(BlueprintName(desc,name)&&!strcmp(name,BlueprintIds[cmd.selection])){if(selected&&selected!=desc){SetStatus("AMBIGUOUS BLUEPRINT - CANCELLED");return;}selected=desc;}}
 }
 if(baseId<0||(cmd.selection>=0&&!selected)){SetStatus("BLUEPRINT NOT LOADED");return;}
 if(selected==s.blueprint){SetStatus(selected?"BLUEPRINT ALREADY ATTACHED":"NO BLUEPRINT TO REMOVE");return;}
 using Rebuild=void(*)(void*,int*,unsigned,void*,unsigned,unsigned);
 using Condition=void(*)(void*,float);
 auto rebuild=(Rebuild)(gameBase+0x361C20);auto condition=(Condition)(gameBase+0x361E30);
 rebuild((void*)(s.item+0x50),&baseId,s.attributes,(void*)selected,0,0);
 float maximum=0;bool limitReadable=BlueprintConditionLimit(s.item+0x60,maximum);
 float expected=limitReadable?BlueprintClampedCondition(s.condition,maximum):s.condition;
 if(limitReadable)condition((void*)(s.item+0x50),expected);
 HeldBlueprintState after{};bool ok=ResolveHeldBlueprint(after)&&limitReadable&&BlueprintStateMatches(s,after,selected,expected);
 bool revertOk=true;
 if(!ok){
  rebuild((void*)(s.item+0x50),&baseId,s.attributes,(void*)s.blueprint,0,0);
  float revertMaximum=0;bool revertLimitReadable=BlueprintConditionLimit(s.item+0x60,revertMaximum);
  float revertExpected=revertLimitReadable?BlueprintClampedCondition(s.condition,revertMaximum):s.condition;
  if(revertLimitReadable)condition((void*)(s.item+0x50),revertExpected);
                                                                                                   
                                                                                                 
                                                                                               
                                                                                                
  HeldBlueprintState reverted{};
  revertOk=ResolveHeldBlueprint(reverted)&&revertLimitReadable&&BlueprintStateMatches(s,reverted,s.blueprint,revertExpected);
  if(!revertOk){blueprintMenuReady=false;SetStatus("VERIFY FAILED - REVERT ALSO FAILED - EDITING DISABLED");}
  else {unsigned char dirty=1;Write(s.item+0xB0,dirty);SetStatus("VERIFY FAILED - REVERTED SAFELY, EDITING STILL AVAILABLE");}
 }
 else{unsigned char dirty=1;Write(s.item+0xB0,dirty);SetStatus(selected?"BLUEPRINT APPLIED - RE-EQUIP IF NEEDED":"BLUEPRINT REMOVED - RE-EQUIP IF NEEDED");}
 std::ofstream f(LogPath(),std::ios::app);f<<GetTickCount64()<<",BLUEPRINT_EDIT,ok="<<ok<<",revert_ok="<<revertOk<<",item="<<(void*)s.item<<",base_id="<<baseId<<",old="<<(void*)s.blueprint<<",new="<<(void*)selected
  <<",selected="<<(cmd.selection>=0?BlueprintIds[cmd.selection]:"NONE")
  <<",attributes="<<s.attributes<<",quantity="<<s.quantity<<",condition="<<s.condition
  <<",maximum_condition="<<maximum<<",expected_condition="<<expected<<",limit_readable="<<limitReadable
  <<",after_attributes="<<after.attributes<<",after_quantity="<<after.quantity<<",after_condition="<<after.condition<<",after_blueprint="<<(void*)after.blueprint<<'\n';
}
void PumpBlueprintChange(void*who){if(LocalMove(who))ConsumeBlueprintChange();}
void InstallBlueprintMenu(){
 const unsigned char rebuild[16]={0x48,0x89,0x5C,0x24,0x10,0x48,0x89,0x6C,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x41};
 const unsigned char condition[16]={0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x30,0x48,0x8B,0x41,0x10,0x48,0x8B};
 unsigned char a[16]{},b[16]{};
 blueprintMenuReady=TrueGodBuildMatches()&&ReadBytes(gameBase+0x361C20,a,16)&&!memcmp(a,rebuild,16)&&ReadBytes(gameBase+0x361E30,b,16)&&!memcmp(b,condition,16);
 Log(blueprintMenuReady?"BLUEPRINT_MENU_READY_IDLE":"BLUEPRINT_MENU_GUARD_FAILED");
}

                                                                
void QueueHeldRepair(){
 HeldBlueprintState s{};if(!blueprintMenuReady||!ResolveHeldBlueprint(s)){SetStatus("EQUIP A SUPPORTED WEAPON");return;}
 AcquireSRWLockExclusive(&blueprintCommandLock);
 if(blueprintCommand.pending||heldRepairCommand.pending){ReleaseSRWLockExclusive(&blueprintCommandLock);SetStatus("WEAPON CHANGE ALREADY QUEUED");return;}
 heldRepairCommand={true,s,GetTickCount64()};ReleaseSRWLockExclusive(&blueprintCommandLock);menuOpen=false;SetStatus("WEAPON REPAIR REQUESTED");
}
void ConsumeHeldRepair(){
 AcquireSRWLockExclusive(&blueprintCommandLock);auto cmd=heldRepairCommand;heldRepairCommand.pending=false;ReleaseSRWLockExclusive(&blueprintCommandLock);if(!cmd.pending)return;
 HeldBlueprintState now{};auto&s=cmd.before;float maximum=0;
 if(!blueprintMenuReady||GetTickCount64()-cmd.queued>15000||!ResolveHeldBlueprint(now)||!BlueprintStateMatches(s,now,s.blueprint,s.condition)){SetStatus("REPAIR CANCELLED - WEAPON CHANGED");return;}
 if(!BlueprintConditionLimit(s.item+0x60,maximum)||maximum<=0){SetStatus("WEAPON HAS NO REPAIRABLE CONDITION");return;}
 using Condition=void(*)(void*,float);auto condition=(Condition)(gameBase+0x361E30);condition((void*)(s.item+0x50),maximum);
 HeldBlueprintState after{};bool ok=ResolveHeldBlueprint(after)&&BlueprintStateMatches(s,after,s.blueprint,maximum);
 bool reverted=false;if(!ok){condition((void*)(s.item+0x50),s.condition);HeldBlueprintState back{};reverted=ResolveHeldBlueprint(back)&&BlueprintStateMatches(s,back,s.blueprint,s.condition);if(!reverted)blueprintMenuReady=false;}
 if(ok||reverted){unsigned char dirty=1,check=0;if(!Write(s.item+0xB0,dirty)||!Read(s.item+0xB0,check)||check!=dirty){
  blueprintMenuReady=false;
  SetStatus(ok?"WEAPON REPAIRED - REFRESH FAILED - EDITING DISABLED":"REPAIR REVERTED - REFRESH FAILED - EDITING DISABLED");
  Log(ok?"HELD_REPAIR_APPLIED_REFRESH_FAILED":"HELD_REPAIR_REVERTED_REFRESH_FAILED");return;
 }}
 SetStatus(ok?"HELD WEAPON REPAIRED":reverted?"REPAIR FAILED - REVERTED":"REPAIR REVERT FAILED - EDITING DISABLED");Log(ok?"HELD_REPAIR_OK":reverted?"HELD_REPAIR_REVERTED":"HELD_REPAIR_REVERT_FAILED");
}
