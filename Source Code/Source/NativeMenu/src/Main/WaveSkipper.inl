                                                                                 
using ArenaUpdateFn=void(*)(void*);ArenaUpdateFn originalArenaUpdate=nullptr;
std::atomic<bool> waveReady{false};std::atomic<int>waveDisplayed{0},waveWrites{0};
SRWLOCK waveLock=SRWLOCK_INIT;
struct ArenaSeen{uintptr_t address=0;ULONGLONG seen=0;int wave=0;};ArenaSeen arenaSeen[8]{};
struct WaveRequest{bool active=false;uintptr_t arena=0;int start=0,writes=0;ULONGLONG deadline=0,next=0;uintptr_t lastChild=0,lastState=0;};WaveRequest waveRequest;
bool ArenaWave(uintptr_t arena,int&wave){uintptr_t vt=0,array=0;int a=0,b=0,n=0;return Read(arena,vt)&&vt==gameBase+ArenaVtableRva&&Read(arena+0x25C,a)&&Read(arena+0x260,b)&&a==b&&a>0&&a<10000&&Read(arena+0x258,n)&&n>0&&n<=1024&&Pointer(arena+0x2A0,array)&&(wave=a)>0;}
void EndWave(const char*message){waveRequest.active=false;SetStatus(message);}
void CancelWaveNative(){AcquireSRWLockExclusive(&waveLock);EndWave("WAVE SKIP CANCELLED");ReleaseSRWLockExclusive(&waveLock);}
bool SkipWaveNative(){
 AcquireSRWLockExclusive(&waveLock);
 if(!waveReady){SetStatus("WAVE SKIP UNAVAILABLE");ReleaseSRWLockExclusive(&waveLock);return false;}
 if(waveRequest.active){SetStatus("WAVE SKIP ALREADY RUNNING");ReleaseSRWLockExclusive(&waveLock);return false;}
 auto now=GetTickCount64();ArenaSeen selected{};int count=0;
 for(auto&s:arenaSeen)if(s.address&&now-s.seen<1000){selected=s;++count;}
 if(count!=1){SetStatus(count?"WAVE SKIP REFUSED - MULTIPLE ARENAS":"START AN ARENA WAVE AND RESUME GAME");ReleaseSRWLockExclusive(&waveLock);return false;}
 waveRequest={true,selected.address,selected.wave,0,now+90000,0,0,0};waveWrites=0;SetStatus("SKIPPING WAVE - RESUME GAME");ReleaseSRWLockExclusive(&waveLock);return true;
}
bool WaveState(uintptr_t child,uintptr_t&state,unsigned char&done){
 uintptr_t vt=0,query=0,owner=0,marker=0,back=0;unsigned char gate=0;
 return Read(child,vt)&&InGame(vt)&&Read(vt+0xF8,query)&&query==gameBase+CompletionQueryRva&&Pointer(child+0x760,owner)&&Pointer(child+0x768,state)&&Pointer(state+0x50,marker)&&Read(state+0x58,back)&&back==child&&Read(child+0x50,gate)&&gate&&Read(state+0x581,done)&&done<=1;
}
bool CompleteWaveState(uintptr_t state){__try{*(volatile unsigned char*)(state+0x581)=1;return *(volatile unsigned char*)(state+0x581)==1;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
void ArenaUpdateHook(void*who){
 originalArenaUpdate(who);
 const auto now=GetTickCount64();const auto arena=(uintptr_t)who;int wave=0;bool valid=ArenaWave(arena,wave);
 AcquireSRWLockExclusive(&waveLock);
 if(valid){
  ArenaSeen*slot=nullptr;for(auto&s:arenaSeen)if(s.address==arena){slot=&s;break;}
  if(!slot)for(auto&s:arenaSeen)if(!s.address||now-s.seen>1000){slot=&s;break;}
  if(slot)*slot={arena,now,wave};waveDisplayed=wave;
 }else for(auto&s:arenaSeen)if(s.address==arena)s={};
 auto&r=waveRequest;
 if(!r.active||r.arena!=arena){ReleaseSRWLockExclusive(&waveLock);return;}
 if(!valid){EndWave("WAVE SKIP STOPPED - ARENA CHANGED");ReleaseSRWLockExclusive(&waveLock);return;}
 if(wave>r.start){char msg[96]{};sprintf_s(msg,"WAVE SKIP COMPLETE - WAVE %d TO %d",r.start,wave);EndWave(msg);ReleaseSRWLockExclusive(&waveLock);return;}
 if(wave<r.start||now>=r.deadline){EndWave("WAVE SKIP STOPPED - RESET OR TIMEOUT");ReleaseSRWLockExclusive(&waveLock);return;}
 if(now<r.next){ReleaseSRWLockExclusive(&waveLock);return;}r.next=now+250;
                                                                                  
 uintptr_t array=0;int count=0;if(!Pointer(arena+0x2A0,array)||!Read(arena+0x258,count)||count<1||count>1024){EndWave("WAVE SKIP STOPPED - INVALID SLOTS");ReleaseSRWLockExclusive(&waveLock);return;}
 if(r.lastState){
  for(int i=0;i<count;i++){uintptr_t child=0,state=0;unsigned char done=0;if(!Read(array+i*8,child)){EndWave("WAVE SKIP STOPPED - SLOT READ FAILED");break;}if(child==r.lastChild&&(!WaveState(child,state,done)||state!=r.lastState||!done)){EndWave("WAVE SKIP STOPPED - READBACK CHANGED");break;}}
  r.lastState=0;r.lastChild=0;
 }
 if(r.active&&r.writes>=128)EndWave("WAVE SKIP STOPPED - CONTROLLER LIMIT");
 if(r.active)for(int i=0;i<count;i++){
  uintptr_t child=0,state=0;unsigned char done=0;
  if(!Read(array+i*8,child)){EndWave("WAVE SKIP STOPPED - SLOT READ FAILED");break;}
  if(!child||!WaveState(child,state,done)||done)continue;
  int checkWave=0;uintptr_t checkArray=0,checkChild=0,checkState=0;unsigned char checkDone=0;
  if(!ArenaWave(arena,checkWave)||checkWave!=r.start||!Read(arena+0x2A0,checkArray)||checkArray!=array||!Read(array+i*8,checkChild)||checkChild!=child||!WaveState(child,checkState,checkDone)||checkState!=state||checkDone){EndWave("WAVE SKIP STOPPED - IDENTITY CHANGED");break;}
  if(!CompleteWaveState(state)){EndWave("WAVE SKIP STOPPED - WRITE FAILED");break;}
  ++r.writes;waveWrites=r.writes;r.lastState=state;r.lastChild=child;
  char msg[96]{};sprintf_s(msg,"WAVE %d - COMPLETED %d CONTROLLERS",r.start,r.writes);SetStatus(msg);break;
 }
 ReleaseSRWLockExclusive(&waveLock);
}
void WaveMenuTick(){AcquireSRWLockExclusive(&waveLock);auto now=GetTickCount64();if(waveRequest.active&&now>=waveRequest.deadline)EndWave("WAVE SKIP TIMED OUT");bool fresh=false;for(auto&s:arenaSeen)if(s.address&&now-s.seen<1000)fresh=true;if(!fresh)waveDisplayed=0;ReleaseSRWLockExclusive(&waveLock);}
void InstallWaveSkipper(){
 const unsigned char expected[]={0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0x6C};unsigned char actual[16]{};
 const unsigned char queryExpected[]={0x48,0x83,0xB9,0x60,0x07,0,0,0,0x74,0x18,0x48,0x8B,0x81,0x68,0x07,0};unsigned char query[16]{};
 if(!TrueGodBuildMatches()||!ReadBytes(gameBase+0x43120,actual,16)||memcmp(actual,expected,16)||!ReadBytes(gameBase+CompletionQueryRva,query,16)||memcmp(query,queryExpected,16)){Log("WAVE_SKIP_GUARD_FAILED");return;}
 auto init=MH_Initialize();if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED)return;
 void*target=(void*)(gameBase+0x43120);
 if(MH_CreateHook(target,(void*)ArenaUpdateHook,(void**)&originalArenaUpdate)!=MH_OK)return;
 if(MH_EnableHook(target)!=MH_OK){MH_RemoveHook(target);return;}
 waveReady=true;Log("WAVE_SKIP_READY_IDLE");
}
