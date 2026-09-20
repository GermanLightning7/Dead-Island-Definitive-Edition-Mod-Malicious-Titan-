#include "SpawnCatalog.inl"
std::atomic<int> spawnSelection{0};std::atomic<bool> spawnReady{false};
std::atomic<int> spawnCategory{0};
void ChangeSpawnCategory(int delta){int category=(spawnCategory.load()+delta+SpawnCategoryCount)%SpawnCategoryCount;spawnSelection=SpawnCategoryStarts[category];spawnCategory=category;}
void ChangeSpawnSelection(int delta){int category=spawnCategory.load(),start=SpawnCategoryStarts[category],count=SpawnCategoryCounts[category];spawnSelection=start+(spawnSelection.load()-start+delta+count)%count;}
SRWLOCK spawnLock=SRWLOCK_INIT;
struct SpawnCommand{bool pending=false;int selection=0;uintptr_t player=0;ULONGLONG queued=0;};SpawnCommand spawnCommand;
struct SpawnInventory{std::vector<uintptr_t>items;unsigned long long matchingQuantity=0;int matchingItems=0;};
bool ReadSpawnInventory(uintptr_t player,const char*selected,SpawnInventory&out){
 for(uintptr_t offset:{uintptr_t(0x1088),uintptr_t(0x10A0),uintptr_t(0x10B8),uintptr_t(0x10D0),uintptr_t(0x10E8)}){
  uintptr_t array=0;unsigned count=0,capacity=0;
  if(!Read(player+offset,array)||!Read(player+offset+8,count)||!Read(player+offset+12,capacity)||count>4096||capacity<count)return false;
  if(count&&!array)return false;
  for(unsigned i=0;i<count;i++){
   uintptr_t item=0,vt=0,desc=0;int quantity=0;
   if(!Read(array+(uintptr_t)i*8,item))return false;if(!item)continue;
   if(std::find(out.items.begin(),out.items.end(),item)!=out.items.end())continue;
   if(!Read(item,vt)||vt!=gameBase+0xD994D8||!Read(item+0x68,desc)||!Read(item+0x50,quantity)||quantity<0)return false;
   char name[96]{};if(!BlueprintName(desc,name))return false;
   out.items.push_back(item);if(!strcmp(name,selected)){++out.matchingItems;out.matchingQuantity+=(unsigned)quantity;}
  }
 }
 return true;
}
void QueueSpawnItem(){
 if(!spawnReady){SetStatus("ITEM SPAWNER UNAVAILABLE");return;}
 uintptr_t player=0;if(!TrueGodPlayer(player)){SetStatus("LOAD INTO THE GAME FIRST");return;}
 AcquireSRWLockExclusive(&spawnLock);
 if(spawnCommand.pending){ReleaseSRWLockExclusive(&spawnLock);SetStatus("ITEM REQUEST ALREADY QUEUED");return;}
 spawnCommand={true,spawnSelection.load(),player,GetTickCount64()};ReleaseSRWLockExclusive(&spawnLock);
 menuOpen=false;SetStatus("ITEM SPAWN REQUESTED");
}
                                                                                 
struct SpawnNativeItem {
 int quantity=1;float condition=100.f;int unknown=0,pad=0;
 uintptr_t wrapper=0,base=0;unsigned attributes=0;int cached=-1;
 uintptr_t blueprint=0;int a=-1,b=-1;float multiplier=1.f;
 unsigned char flag=0,padding[3]{};int c=-1,d=-1,e=-1,f=-1,g=-1;
};
static_assert(sizeof(SpawnNativeItem)==0x58&&offsetof(SpawnNativeItem,wrapper)==0x10&&offsetof(SpawnNativeItem,base)==0x18&&offsetof(SpawnNativeItem,blueprint)==0x28,"native inventory value layout");
struct SpawnSideEffects {int ammo[12]{};uintptr_t stats=0;int cash=0;bool ammoReadable=false,cashReadable=false;};
SpawnSideEffects ReadSpawnSideEffects(uintptr_t player){
 SpawnSideEffects v{};v.ammoReadable=ReadBytes(player+0x117C,v.ammo,sizeof(v.ammo));
 v.cashReadable=ResolveStats(v.stats)&&Read(v.stats+CashPrimaryOffset,v.cash);return v;
}
bool SpawnSideEffectAdded(const SpawnSideEffects& a,const SpawnSideEffects& b){
 if(a.cashReadable&&b.cashReadable&&a.stats==b.stats&&b.cash>a.cash)return true;
 if(a.ammoReadable&&b.ammoReadable)for(int i=0;i<12;i++)if(b.ammo[i]>a.ammo[i])return true;
 return false;
}
bool GiveSpawnWeapon(uintptr_t player,int baseId,uintptr_t selected,const char*name){
 int quantity=1;
 if(!strncmp(name,"Firearm_",8)||!strncmp(name,"zzz_Firearm_",12)){
  uintptr_t vt=0,quantityFn=0;
  if(!Read(selected,vt)||!InGame(vt)||!Read(vt+0x118,quantityFn)||!InGame(quantityFn))return false;
  quantity=((int(*)(void*))quantityFn)((void*)selected);
  if(quantity<0||quantity>100000)return false;
 }
 SpawnNativeItem value{};value.wrapper=gameBase+0xD9AEC8;
 using Rebuild=void(*)(void*,int*,unsigned,void*,unsigned,unsigned);
 ((Rebuild)(gameBase+0x361C20))(&value,&baseId,0,nullptr,0,0);
 if(value.base!=selected||value.wrapper!=gameBase+0xD9AEC8||!std::isfinite(value.condition))return false;
 value.quantity=quantity;
                                                                         
                                                                           
 using Insert=void(*)(void*,void*,unsigned char,int,unsigned char);
 ((Insert)(gameBase+0x6740B0))((void*)player,&value,1,1,0);
 return true;
}
void ConsumeSpawnItem(){
 AcquireSRWLockExclusive(&spawnLock);auto cmd=spawnCommand;spawnCommand.pending=false;ReleaseSRWLockExclusive(&spawnLock);
 if(!cmd.pending)return;uintptr_t player=0;
 if(!TrueGodPlayer(player)||player!=cmd.player||GetTickCount64()-cmd.queued>15000||cmd.selection<0||cmd.selection>=SpawnCount){SetStatus("SPAWN CANCELLED - PLAYER CHANGED");return;}
 uintptr_t table=0,selected=0;int count=0,baseId=-1;
 if(!Read(gameBase+0x13279C8,table)||!Read(gameBase+0x13279D0,count)||!table||count<=0||count>10000){SetStatus("ITEM CATALOGUE UNAVAILABLE");return;}
 const char*name=SpawnIds[cmd.selection];
 for(int i=0;i<count;i++){uintptr_t desc=0;char candidate[96]{};if(!Read(table+(uintptr_t)i*8,desc))return;if(desc&&BlueprintName(desc,candidate)&&!strcmp(candidate,name)){selected=desc;baseId=i;break;}}
 if(!selected){SetStatus("SELECTED ITEM NOT LOADED");return;}
 SpawnInventory before{};if(!ReadSpawnInventory(player,name,before)){SetStatus("INVENTORY CHECK FAILED - NO SPAWN");return;}
 auto effectsBefore=ReadSpawnSideEffects(player);
 if(SpawnWeaponRoute[cmd.selection]){if(!GiveSpawnWeapon(player,baseId,selected,name)){SetStatus("ITEM CONSTRUCTION REFUSED - NO GRANT");return;}}
 else {using GiveItem=void(*)(void*,const void*,unsigned char);((GiveItem)(gameBase+0x673880))((void*)player,(const void*)(selected+0x10),1);}
 auto effectsAfter=ReadSpawnSideEffects(player);
 SpawnInventory after{};bool readable=ReadSpawnInventory(player,name,after);
 bool inventoryAdded=readable&&(after.matchingItems>before.matchingItems||after.matchingQuantity>before.matchingQuantity);
 bool effectAdded=SpawnSideEffectAdded(effectsBefore,effectsAfter);bool added=inventoryAdded||effectAdded;
 if(!readable){spawnReady=false;SetStatus("SPAWN VERIFY FAILED - DISABLED");}
 else SetStatus(inventoryAdded?"ITEM ADDED - CHECK INVENTORY":effectAdded?"AMMO OR CASH ADDED":"NO CHANGE - FULL, ALREADY OWNED OR UNSUPPORTED");
 std::ofstream f(LogPath(),std::ios::app);f<<GetTickCount64()<<",ITEM_SPAWN,name="<<name<<",route="<<(SpawnWeaponRoute[cmd.selection]?"native_inventory":"native_give")<<",base_id="<<baseId<<",total_items_before="<<before.items.size()<<",total_items_after="<<after.items.size()<<",side_effect_added="<<effectAdded<<",verified="<<added<<",readable="<<readable<<",items_before="<<before.matchingItems<<",items_after="<<after.matchingItems<<",quantity_before="<<before.matchingQuantity<<",quantity_after="<<after.matchingQuantity<<'\n';
}
void PumpSpawnItem(void*who){if(LocalMove(who))ConsumeSpawnItem();}
bool HasPendingInventoryCommands(){
 AcquireSRWLockShared(&spawnLock);bool item=spawnCommand.pending;ReleaseSRWLockShared(&spawnLock);
 AcquireSRWLockShared(&blueprintCommandLock);bool blueprint=blueprintCommand.pending||heldRepairCommand.pending;ReleaseSRWLockShared(&blueprintCommandLock);return item||blueprint;
}
void PumpInventoryCommands(){ConsumeBlueprintChange();ConsumeHeldRepair();ConsumeSpawnItem();}
void InstallItemSpawner(){
 const unsigned char expected[16]={0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x55,0x57,0x41,0x54,0x48,0x8D};unsigned char actual[16]{};
 spawnReady=TrueGodBuildMatches()&&ReadBytes(gameBase+0x673880,actual,16)&&!memcmp(actual,expected,16);
 const unsigned char insert[16]={0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x10,0x48,0x89,0x74,0x24,0x20,0x44};
 const unsigned char rebuild[16]={0x48,0x89,0x5C,0x24,0x10,0x48,0x89,0x6C,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x41};
 spawnReady=spawnReady&&ReadBytes(gameBase+0x6740B0,actual,16)&&!memcmp(actual,insert,16)&&ReadBytes(gameBase+0x361C20,actual,16)&&!memcmp(actual,rebuild,16);
 Log(spawnReady?"ITEM_SPAWNER_READY_IDLE":"ITEM_SPAWNER_GUARD_FAILED");
}

void BrowseInventory(int direction){
 uintptr_t player=0;if(!TrueGodPlayer(player)){SetStatus("INVENTORY UNAVAILABLE - LOAD A SAVE");return;}
 SpawnInventory inventory{};if(!ReadSpawnInventory(player,"",inventory)){SetStatus("INVENTORY SNAPSHOT UNAVAILABLE");return;}
 if(inventory.items.empty()){SetStatus("INVENTORY EMPTY");return;}
 static uintptr_t lastPlayer=0;static int index=-1;if(lastPlayer!=player){lastPlayer=player;index=-1;}
 int count=(int)inventory.items.size();index=(index+direction+count)%count;
 uintptr_t item=inventory.items[index],owner=0,descriptor=0;int quantity=0;float condition=0;char name[96]{},label[160]{};
 if(!Read(item+0xA8,owner)||owner!=player||!Read(item+0x68,descriptor)||!BlueprintName(descriptor,name)||!Read(item+0x50,quantity)||quantity<0||!Read(item+0x54,condition)||!std::isfinite(condition)){SetStatus("INVENTORY ITEM CHANGED - RETRY");return;}
                                                                                
 sprintf_s(label,"%d/%d %.65s | QTY %d | COND %.2f",index+1,count,name,quantity,condition);SetStatus(label);
}
