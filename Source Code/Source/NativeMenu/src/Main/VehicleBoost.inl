                                                                              
                                                                                          
using VehicleTickFn=void(*)(void*,float);
using CarPositionGetFn=V*(*)(void*,V*);
using CarPositionSetFn=void(*)(void*,const V*);
VehicleTickFn nativeVehicleTick=nullptr;
std::atomic_bool vehicleBoostReady{false};

bool ResolveOccupiedDrivenCar(void* who,uintptr_t&car,uintptr_t&get,uintptr_t&set){
 uintptr_t player=0,controller=0,owner=0,vt=0,seatView=0,model=0,self=0,link=0,component=0;
 unsigned char active=0,input=0;int type=0;
 car=0;get=0;set=0;
 if(!TrueGodPlayer(player)||!Read(player+0x830,controller)||controller!=(uintptr_t)who||
  !Read(controller,vt)||vt!=gameBase+0xEDE7F8||!Read(controller+0x58,owner)||owner!=player||
  !Read(controller+0x50,active)||!active||!Read(controller+0x28,input)||!input||
  !Read(controller+0xB8,seatView)||!seatView||!Read(controller+0xC0,car)||!car||seatView!=car+0x60)return false;
                                                                                
                                                                                    
 if(!Read(car,vt)||!Read(car+0x60,self))return false;
 const bool knownCar=vt==gameBase+0xF6BA38&&self==gameBase+0xF6CC48;
 const bool knownTruck=vt==gameBase+0xF71768&&self==gameBase+0xF72978;
 if((!knownCar&&!knownTruck)||!Read(vt+0x1E8,get)||get!=gameBase+0x76620||
  !Read(vt+0x1D8,set)||set!=gameBase+0x5F9BF0)return false;
 return Read(car+0x20,model)&&model&&Read(model,vt)&&vt==physicalEngine+0x84A458&&
  Read(model+0x250,self)&&self==model&&Read(model+0x258,link)&&link&&Read(link+0x28,component)&&component&&
  Read(component,vt)&&vt==physicalEngine+0x8819A8&&Read(component+0x38,type)&&type==5&&
  Read(component+0x6F0,vt)&&vt==physicalEngine+0x881B38;
}
bool VehicleBoostFinite(V v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&fabsf(v.x)<100000&&fabsf(v.y)<100000&&fabsf(v.z)<100000;}
using VehicleBodyCommandFn=void(*)(void*,const V*);
VehicleBodyCommandFn vehicleStopConnected=nullptr;
bool ResolveVehicleBoostBody(uintptr_t car,uintptr_t& body,unsigned& count){
 uintptr_t model=0,link=0,component=0,vt=0,owner=0,table=0;int n=0;body=0;count=0;
 if(!Read(car+0x20,model)||!Read(model+0x258,link)||!Read(link+0x28,component)||
  !Read(component,vt)||vt!=physicalEngine+0x8819A8||!Read(component+0x7C0,body)||!body||
  !Read(body+0x18,owner)||owner!=component||!Read(component+0x1E8,table)||!table||
  !Read(component+0x1F0,n)||n<1||n>32)return false;
 V root{};if(!Read(body+0xBC,root)||!VehicleBoostFinite(root))return false;
 bool found=false;
 for(int i=0;i<n;++i){
  uintptr_t member=0;V position{};
  if(!Read(table+i*8,member)||!member||!Read(member+0x18,owner)||owner!=component||
   !Read(member+0xBC,position)||!VehicleBoostFinite(position)||Distance(root,position)>20.f)return false;
  if(member==body)found=true;
 }
 count=(unsigned)n;return found;
}
void VehicleBoostTick(void*who,float dt){nativeVehicleTick(who,dt);PumpVehicleBoost(who,dt);}
void InstallVehicleBoost(){
 if(!physicalEngine)return;
                                                                                  
                                                                          

 vehicleStopConnected=(VehicleBodyCommandFn)(physicalEngine+0x3010A0);
 const unsigned char bytes[16]={0x48,0x89,0x5C,0x24,0x18,0x55,0x56,0x57,0x48,0x8D,0x6C,0x24,0xD0,0x48,0x81,0xEC};
 vehicleBoostReady=InstallFeatureHook(0x7823C0,(void*)VehicleBoostTick,(void**)&nativeVehicleTick,bytes);
 Log(vehicleBoostReady?"VEHICLE_BOOST_READY":"VEHICLE_BOOST_HOOK_REJECTED");
}
