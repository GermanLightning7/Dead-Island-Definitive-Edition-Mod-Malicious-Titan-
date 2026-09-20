                                                                         
                                                                           
using MeleeDamageDispatchFn=void(*)(void*,void*,void*);
MeleeDamageDispatchFn nativeMeleeDamageDispatch=nullptr;
std::atomic_bool explosiveMeleeOn{false},explosiveMeleeReady{false};
thread_local bool meleeExplosionActive=false;
struct MeleeExplosionStamp{uintptr_t player=0,victim=0;ULONGLONG tick=0;};
thread_local MeleeExplosionStamp meleeExplosionStamps[32]{};
thread_local unsigned meleeExplosionCursor=0;
bool IsMeleeDamageCaller(uintptr_t caller){
 return caller==gameBase+0x7A7066||caller==gameBase+0x7A71FE||caller==gameBase+0x7A740B||caller==gameBase+0x7A77F6||caller==gameBase+0x7A7851;
}
bool PrepareMeleeExplosion(uintptr_t caller,uintptr_t attacker,uintptr_t victim,uintptr_t info,V&position){
 if(!explosiveMeleeOn.load()||!explosiveMeleeReady.load()||meleeExplosionActive||!IsMeleeDamageCaller(caller))return false;
 uintptr_t player=0,vt=0;int offset=-1;float amount=0;
 if(!TrueGodPlayer(player)||attacker!=player||victim==player||!Read(victim,vt)||!TypeContains(vt,"ZombieAI",offset)||offset!=0||
  !Read(info+0x18,amount)||!std::isfinite(amount)||amount<=0||!Read(info+0x20,position)||!Finite(position))return false;
 auto now=GetTickCount64();
 for(const auto&stamp:meleeExplosionStamps)if(stamp.player==player&&stamp.victim==victim&&now>=stamp.tick&&now-stamp.tick<150)return false;
 meleeExplosionStamps[meleeExplosionCursor++%32]={player,victim,now};return true;
}
void ExplosiveMeleeDamage(void*attacker,void*victim,void*info){
 auto caller=(uintptr_t)_ReturnAddress();V position{};
 auto funHit=BeforeFunMelee(caller,(uintptr_t)attacker,(uintptr_t)victim,(uintptr_t)info);
 bool explode=PrepareMeleeExplosion(caller,(uintptr_t)attacker,(uintptr_t)victim,(uintptr_t)info,position);
 nativeMeleeDamageDispatch(attacker,victim,info);
 AfterFunMelee(funHit,(uintptr_t)info);
 if(!explode||!explosiveMeleeOn.load())return;
 uintptr_t player=0;if(!TrueGodPlayer(player)||player!=(uintptr_t)attacker)return;
                                                                              
                                                                             
 meleeExplosionActive=true;
 bool ok=SpawnRedirectedRocket(player+0x28,player+0x18,position,V{0,1,0});
 meleeExplosionActive=false;
 static std::atomic<unsigned> spawned{0},refused{0};auto n=ok?++spawned:++refused;
 if(n<=5||n%25==0)Log(ok?"MELEE_EXPLOSION_AT_HIT":"MELEE_EXPLOSION_REFUSED");
 if(!ok){explosiveMeleeOn=false;SetStatus("MELEE EXPLOSION FAILED - DISABLED");}
}
void InstallExplosiveMelee(){
 if(!damageReady.load())return;
 const unsigned char expected[16]={0x48,0x8B,0xC4,0x56,0x57,0x41,0x56,0x41,0x57,0x48,0x81,0xEC,0x88,0x01,0x00,0x00};
 explosiveMeleeReady=InstallFeatureHook(0x66BB90,(void*)ExplosiveMeleeDamage,(void**)&nativeMeleeDamageDispatch,expected);
 Log(explosiveMeleeReady?"EXPLOSIVE_MELEE_READY_DISABLED":"EXPLOSIVE_MELEE_HOOK_REFUSED");
}
void ToggleExplosiveMelee(){
 if(!explosiveMeleeReady.load()){SetStatus("EXPLOSIVE MELEE UNAVAILABLE");return;}
 explosiveMeleeOn=!explosiveMeleeOn.load();SetStatus(explosiveMeleeOn.load()?"EXPLOSIVE MELEE ON":"EXPLOSIVE MELEE OFF");
}
