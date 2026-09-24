#pragma once
#include <array>
#include <atomic>
#include <string>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <unordered_set>
namespace FeatureBindings {
constexpr int Capacity=288;
inline std::array<std::atomic<int>,Capacity> keys{};
inline std::atomic_int group{0};
inline std::array<bool,256> previous{};
inline std::atomic_bool armed{false},suppressUntilRelease{false};
inline bool captureReleased=false;
constexpr int groups[]={0,1,17,3,5,13,4,10,9,11,12,14,6,7,2,15,16};
constexpr const char* groupNames[]={"OVERVIEW","PLAYER","ARENA","MOVEMENT","AIM","CAMERA","ENEMY ESP","STYLE","HOTKEYS","FIREARMS","MELEE","CRAFTING","ITEMS","VISUALS","FUN","CONFIGS","MENU / ACTION KEYS","PLAYERS"};
constexpr int rowCounts[]={5,8,2,7,13,7,9,9,0,12,5,5,3,9,10,4,7,2};
constexpr const char* labels[18][16]={
{"RESET DEFAULTS","UNLOAD","MARK ISSUE IN LOG","OPEN LOG FOLDER","TOGGLE MENU"},
{"GOD MODE","CASH AMOUNT","GIVE CUSTOM CASH","GIVE 1000 CASH","GIVE 200000 CASH","RESTORE GOD BYTES","UNLOCK ALL CHAPTERS","INFINITE FLASHLIGHT"},
{"SKIP CURRENT WAVE","CANCEL WAVE SKIP"},
{"INFINITE STAMINA","JUMP HEIGHT","RUN SPEED","SWING SPEED","INFINITE AIR JUMPS","NOCLIP","NOCLIP SPEED"},
{"SILENT AIM","FOV LIMIT","FOV RADIUS","TARGET PRIORITY","TARGET MARKER","TARGET SNAPLINE","CAMERA LOCK-ON KEY","LOCK-ON TARGET SOURCE","SMOOTHING","SPINBOT","SPIN SPEED","AIM BODY PART","CROSSHAIR"},
{"THIRD PERSON","IGNORE MAP BOUNDS","CUSTOM FOV","FIRST PERSON FOV","THIRD PERSON FOV","CAMERA DISTANCE","RESET CAMERA"},
{"ENEMY ESP","SKELETONS","BOXES","ALL HOSTILE SNAPLINES","MAX DISTANCE","REFRESH","RENDER STYLE","HEALTH BAR + HP","DISTANCE LABELS"},
{"BACKGROUND","TEXT","ACCENT","BORDER","HIGHLIGHT","HEALTH BAR","ZOMBIE","FRIENDLY HUMAN","HOSTILE HUMAN"},
{},
{"MAGAZINE AMMO","MAGAZINE CAPACITY","RESERVE AMMO","SHOT TYPE","SHOT/ANIMATION TYPE","FULL AUTO","FIRE INTERVAL","INTERVAL VALUE","RESTORE NATIVE GUN","ALWAYS CRITICAL","EXPLOSIVE SILENT AIM","NO RECOIL"},
{"MELEE SILENT AIM","INFINITE DURABILITY","ONE-SHOT MELEE","CHAIN MELEE","EXPLOSIVE MELEE"},
{"SELECT BLUEPRINT","APPLY TO HELD WEAPON","REMOVE BLUEPRINT","REPAIR HELD WEAPON","BROWSE INVENTORY"},
{"ITEM CATEGORY","SELECT ITEM","GIVE ONE ITEM"},
{"CHROMATIC ABERRATION","MOTION BLUR","DEPTH OF FIELD","FILM GRAIN","VIGNETTE","BLOOM / SUN SHAFTS","SUN GLARE","DISABLE ALL EFFECTS","RESTORE GAME EFFECTS"},
{"HOMERUN KICKS","GROUND SLAM - G","CHAIN EXPLOSIONS","GRAVITY PUNCH","VEHICLE BOOST - B","GRAPPLE UPGRADES","GRAPPLE SPEED","GRAPPLE MOMENTUM","CHAOS HITS","DRIVE-BY GUNS"},
{"DEFAULT - ALL OFF","PRESET 1","PRESET 2","PRESET 3"},
{"TOGGLE MENU","KICK / LAUNCH KEY","GROUND SLAM KEY","VEHICLE BOOST KEY","SAVE PRESET 1","SAVE PRESET 2","SAVE PRESET 3"},
{"PLAYER TRACKER","YOUR POSITION"}};
constexpr int BindingId(int page,int row){if(page==0&&row==4)return 256;if(page==14&&row==1)return 258;if(page==14&&row==4)return 259;return page*16+row;}
inline int HitBar(float x,float within){return within>=17&&within<29&&x>=22&&x<217?(x>=169?2:1):0;}
inline int Page(){return groups[std::clamp(group.load(),0,16)];}
constexpr bool Exists(int id){if(id==256||id==258||id==259)return true;return id>=0&&id<Capacity&&id/16!=8&&id%16<rowCounts[id/16]&&BindingId(id/16,id%16)==id;}
constexpr int ActionCount(){int n=0;for(int i=0;i<Capacity;i++)if(Exists(i))++n;return n;}
inline bool ValidKey(int key){return key==0||(key>=VK_BACK&&key<255&&key!=VK_ESCAPE&&key!=VK_CONTROL&&key!=VK_LCONTROL&&key!=VK_RCONTROL&&key!=VK_SHIFT&&key!=VK_LSHIFT&&key!=VK_RSHIFT&&key!=VK_MENU&&key!=VK_LMENU&&key!=VK_RMENU&&key!=VK_LWIN&&key!=VK_RWIN);}
inline int DefaultKey(int id){return id==256?VK_INSERT:id==258?'G':id==259?'B':0;}
constexpr bool NativeOnly(int id){return false;}
inline bool Assign(int id,int key){if(!Exists(id)||!ValidKey(key)||(id==256&&!key)||(NativeOnly(id)&&key=='F'))return false;for(int i=0;i<Capacity;i++)if(i!=id&&key&&keys[i]==key)return false;armed=false;suppressUntilRelease=true;keys[id]=key;return true;}
inline void Defaults(){armed=false;suppressUntilRelease=true;for(int i=0;i<Capacity;i++)keys[i]=Exists(i)?DefaultKey(i):0;armed=false;}
inline const char* Label(int row){int page=Page();return row==0?"FEATURE GROUP":row>0&&row<=rowCounts[page]?labels[page][row-1]:"";}
inline void KeyName(int key,char* value,size_t size){if(!key){strcpy_s(value,size,"UNBOUND");return;}LONG scan=MapVirtualKeyW(key,MAPVK_VK_TO_VSC)<<16;if(key==VK_INSERT||key==VK_DELETE||key==VK_HOME||key==VK_END||key==VK_PRIOR||key==VK_NEXT||(key>=VK_LEFT&&key<=VK_DOWN))scan|=1<<24;if(!GetKeyNameTextA(scan,value,(int)size))sprintf_s(value,size,"KEY %d",key);}
inline void Value(int row,char*value,size_t size){if(!row){strcpy_s(value,size,groupNames[Page()]);return;}KeyName(keys[Page()*16+row-1],value,size);}
inline int Poll(bool allowed){
 std::array<bool,256> down{};bool any=false;for(int k=1;k<255;k++){down[k]=(GetAsyncKeyState(k)&0x8000)!=0;any|=down[k];}
 if(!allowed||down[VK_CONTROL]||down[VK_MENU]||down[VK_SHIFT]){previous=down;armed=false;return -1;}
 if(!armed){previous=down;if(!any)armed=true;return -1;}
 int result=-1;for(int id=0;id<Capacity;id++){int key=keys[id];if(Exists(id)&&!NativeOnly(id)&&id<256&&key&&down[key]&&!previous[key]){result=id;break;}if(id>=260&&Exists(id)&&key&&down[key]&&!previous[key]){result=id;break;}}
 previous=down;return result;
}
}
