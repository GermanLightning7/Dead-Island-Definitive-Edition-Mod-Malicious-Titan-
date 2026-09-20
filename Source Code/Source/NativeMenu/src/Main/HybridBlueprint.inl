                                                                                
                                                                                 
using HybridFlagFn=unsigned char(*)(void*);
HybridFlagFn nativeHybridRagdoll=nullptr,nativeHybridElemental=nullptr;
std::atomic<unsigned long long> hybridRagdollCalls{0},hybridElementalCalls{0},hybridMatches{0};
std::atomic<bool> hybridReady{false};
bool HasHybridBlueprint(void* context){
 uintptr_t wrapper=(uintptr_t)context,vt=0,blueprint=0,bvt=0,nameGetter=0,name=0;
 if(!Read(wrapper,vt)||vt!=gameBase+0xD9AEC8||!Read(wrapper+0x18,blueprint)||!blueprint)return false;
 if(!Read(blueprint,bvt)||!Read(bvt+0x68,nameGetter)||nameGetter!=gameBase+0x85B000)return false;
                                                                   
 if(!Read(blueprint+0x10,name)||!name)return false;
 char shortId[5]{};if(!ReadBytes(name,shortId,5))return false;
 if(!memcmp(shortId,"OPFX",5))return true;
 char longId[15]{};return ReadBytes(name,longId,15)&&!memcmp(longId,"Craftplan_Stk2",15);
}
unsigned char HybridRagdoll(void* context){
 ++hybridRagdollCalls;
 if(hybridReady.load()&&HasHybridBlueprint(context)){++hybridMatches;return 1;}
 return nativeHybridRagdoll(context);
}
unsigned char HybridElemental(void* context){
 ++hybridElementalCalls;
                                                                                           
                                                                                             
                                                                                            
                                                                    
 if(hybridReady.load()&&HasHybridBlueprint(context)){++hybridMatches;return 0;}
 return nativeHybridElemental(context);
}
void InstallHybridBlueprint(){
 if(!TrueGodBuildMatches())return;
 const unsigned char r[16]={0x48,0x8B,0x49,0x08,0x48,0x8B,0x01,0x48,0xFF,0xA0,0xB0,0x07,0x00,0x00,0xCC,0xCC};
 const unsigned char e[16]={0x48,0x8B,0x49,0x08,0x48,0x8B,0x01,0x48,0xFF,0xA0,0xB8,0x07,0x00,0x00,0xCC,0xCC};
 bool a=InstallFeatureHook(0x361A20,(void*)HybridRagdoll,(void**)&nativeHybridRagdoll,r);
 bool b=InstallFeatureHook(0x361A30,(void*)HybridElemental,(void**)&nativeHybridElemental,e);
 hybridReady=a&&b;
 Log(hybridReady?"HYBRID_BLUEPRINT_READY_RAGDOLL_FORCED_ELEMENTAL_FORCED_OFF":"HYBRID_BLUEPRINT_HOOK_FAILED");
}
void LogHybridBlueprint(){
 static ULONGLONG next=0;auto now=GetTickCount64();if(now<next)return;next=now+1000;
 std::ofstream f(LogPath(),std::ios::app);f<<now<<",HYBRID_BLUEPRINT,ready="<<hybridReady.load()<<",ragdoll_calls="<<hybridRagdollCalls.load()<<",elemental_calls="<<hybridElementalCalls.load()<<",matched="<<hybridMatches.load()<<'\n';
}
