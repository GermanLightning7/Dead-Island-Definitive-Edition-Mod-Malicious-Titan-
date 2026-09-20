using StaminaConsumeFn=void(*)(void*,float,char);
StaminaConsumeFn originalStaminaConsume=nullptr;
std::atomic_bool staminaOn{false},staminaReady{false};std::atomic<unsigned> staminaBlocked{0},staminaPassed{0};
std::atomic<float> staminaLastBefore{0},staminaLastAfter{0},staminaLastCost{0};
void StaminaConsume(void*who,float cost,char flag){
 uintptr_t player=0;float before=0,after=0;
 bool valid=TrueGodPlayer(player)&&(uintptr_t)who==player&&Read(player+0xDBC,before)&&std::isfinite(before)&&before>=0&&std::isfinite(cost)&&cost>0;
 bool blocked=valid&&staminaOn.load();if(!blocked)originalStaminaConsume(who,cost,flag);
 if(valid&&Read(player+0xDBC,after)){staminaLastBefore=before;staminaLastAfter=after;staminaLastCost=cost;if(blocked)++staminaBlocked;else ++staminaPassed;}
}
void InstallStaminaFeature(){
 if(!TrueGodBuildMatches()){Log("STAMINA_BUILD_MISMATCH");return;}
 unsigned char expected[16]={0x48,0x89,0x5C,0x24,0x10,0x56,0x48,0x83,0xEC,0x50,0x48,0x8B,0xD9,0x48,0x83,0xC1},actual[16]{};void*target=(void*)(gameBase+0x65BCE0);
 if(!ReadBytes((uintptr_t)target,actual,16)||memcmp(expected,actual,16)){Log("STAMINA_HOOK_CONFLICT");return;}
 auto init=MH_Initialize();if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED)return;
 if(MH_CreateHook(target,(void*)StaminaConsume,(void**)&originalStaminaConsume)!=MH_OK||MH_EnableHook(target)!=MH_OK){Log("STAMINA_HOOK_FAILED");return;}
 staminaReady=true;Log("STAMINA_READY_DISABLED");
}
void LogStaminaFeature(){static ULONGLONG next=0;auto now=GetTickCount64();if(now<next)return;next=now+1000;std::ofstream f(LogPath(),std::ios::app);f<<now<<",STAMINA_DIAG,on="<<staminaOn.load()<<",blocked="<<staminaBlocked.load()<<",passed="<<staminaPassed.load()<<",before="<<staminaLastBefore.load()<<",after="<<staminaLastAfter.load()<<",cost="<<staminaLastCost.load()<<",di_state="<<diStateReads.load()<<",di_data="<<diDataReads.load()<<",di_captured="<<diCaptured.load()<<'\n';}
