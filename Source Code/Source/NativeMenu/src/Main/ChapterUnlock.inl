                                                                                 
namespace ChapterUnlock {
std::atomic<bool> ready{false},enabled{false};
bool owned[4]={};
constexpr uintptr_t blocks[2]={0x5067a1,0x513e81};
constexpr unsigned char original[19]={0x85,0xff,0x79,0x05,0x41,0x8b,0xff,0xeb,0x0a,0xb8,0x12,0,0,0,0x3b,0xf8,0x0f,0x4f,0xf8};
                                                                                
constexpr int offsets[2]={17,2};
constexpr unsigned char patched[2]={0x45,0xeb};
bool Match(int block){
 unsigned char actual[19]{},expected[19]{};memcpy(expected,original,19);
 for(int j=0;j<2;j++)if(owned[block*2+j])expected[offsets[j]]=patched[j];
 return ReadBytes(gameBase+blocks[block],actual,19)&&!memcmp(actual,expected,19);
}
bool Change(int i,bool on){
 if(!Match(i/2))return false;if(owned[i]==on)return true;
 auto p=(volatile char*)(gameBase+blocks[i/2]+offsets[i%2]);DWORD old=0,ignored=0;
 if(!VirtualProtect((void*)p,1,PAGE_EXECUTE_READWRITE,&old))return false;
 InterlockedExchange8(p,(char)(on?patched[i%2]:original[offsets[i%2]]));owned[i]=on;
 bool flushed=FlushInstructionCache(GetCurrentProcess(),(void*)p,1)!=0;
 bool restored=VirtualProtect((void*)p,1,old,&ignored)!=0;
 return flushed&&restored&&Match(i/2);
}
bool Set(bool on){
 if(on&&!ready){SetStatus("CHAPTER UNLOCK UNAVAILABLE");return false;}
 bool ok=true;
 for(int step=0;step<4;step++){int i=on?step:3-step;if(!Change(i,on)){ok=false;break;}}
 if(!ok){ready=false;for(int i=3;i>=0;i--)if(owned[i])Change(i,false);}
 enabled=owned[0]||owned[1]||owned[2]||owned[3];
 Log(ok?(on?"CHAPTER_ROWS_UNLOCK_ON":"CHAPTER_ROWS_UNLOCK_OFF"):"CHAPTER_ROWS_UNLOCK_FAULT");
 SetStatus(ok?(on?"REOPEN CHAPTER SELECT OR CHANGE CHARACTER":"NORMAL LIMITS ON NEXT CHAPTER MENU"):"CHAPTER UNLOCK FAILED - CHECK LOG");
 return ok;
}
void Install(){
 unsigned char a[3]{},b[3]{};const unsigned char native[3]={0x0f,0x4c,0xc8};
 ready=CameraFeatures::HashModule(gameBase,"d10c7f59ad3bf62e7f2dd69cd7e49eb634d4e8d046e4f4c07463b5d589833545")&&Match(0)&&Match(1)
  &&ReadBytes(gameBase+0x504029,a,3)&&ReadBytes(gameBase+0x5113f6,b,3)&&!memcmp(a,native,3)&&!memcmp(b,native,3);
 Log(ready?"CHAPTER_ROWS_READY_DISABLED":"CHAPTER_ROWS_BUILD_MISMATCH");
}
bool CanStop(){return !enabled.load()||Set(false);}
}
