                                                                                   
std::atomic_bool grappleReady{false};
using GrappleInputFn=void(*)(void*,int,float,char);
GrappleInputFn nativeGrappleInput=nullptr;
bool HeldGrapple(uintptr_t& item){
 HeldBlueprintState held{};char name[96]{};item=0;
 if(!grappleReady.load()||!ResolveHeldBlueprint(held)||!held.blueprint||
  !BlueprintName(held.blueprint,name)||strcmp(name,"Craftplan_GrapplingHook"))return false;
 item=held.item;return true;
}
bool GrapplePullValid(uintptr_t item){uintptr_t held=0;return HeldGrapple(held)&&held==item;}
bool GrappleWeaponAction(int action){
                                                                                
 switch(action){case 9:case 10:case 18:case 61:case 68:case 69:
  case 159:case 160:case 161:case 170:case 182:case 183:case 197:return true;default:return false;}
}
void GrappleInput(void* who,int action,float amount,char blocked){
 uintptr_t player=0,item=0;
 if(GrappleWeaponAction(action)&&TrueGodPlayer(player)&&player==(uintptr_t)who&&HeldGrapple(item))return;
 nativeGrappleInput(who,action,amount,blocked);
}
void PollGrappleInput(){
 static bool previousLeft=false;
 bool left=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0,right=(GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0;
 bool click=left&&!previousLeft;previousLeft=left;
 if(!click||!right||!grappleReady.load()||!kickLaunchReady.load()||stop.load()||menuOpen.load()||
  !ForegroundGame()||captureHotkey.load()>=0||kickLaunchUntil.load()||kickLaunchPending.load())return;
 uintptr_t item=0;if(!HeldGrapple(item))return;
 grapplePullItem=item;kickLaunchPending=GetTickCount64();Log("GRAPPLE_RMB_LMB_QUEUED");
}
void InstallGrapplingHook(){
 grappleReady=false;
 if(!kickLaunchReady.load()||!blueprintMenuReady.load()||!TrueGodBuildMatches())return;
 const unsigned char expected[16]={0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57};
 grappleReady=InstallFeatureHook(0x663A00,(void*)GrappleInput,(void**)&nativeGrappleInput,expected);
 Log(grappleReady?"GRAPPLE_READY_EQUIPPED_BLUEPRINT_RMB_LMB":"GRAPPLE_INPUT_HOOK_FAILED");
}
