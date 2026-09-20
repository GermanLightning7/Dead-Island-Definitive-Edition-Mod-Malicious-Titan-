#pragma once
#include <windows.h>
#include <wincrypt.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

                                                                    
                                                                
namespace RoutineDiagnostics {
constexpr size_t QueueCapacity=256, TextCapacity=1024;
constexpr unsigned long long FileLimit=2*1024*1024;
constexpr size_t RetainedFiles=12;
struct Entry { unsigned long long tick; DWORD thread; char text[TextCapacity]; };
inline std::array<Entry,QueueCapacity> queue{};
inline SRWLOCK lock=SRWLOCK_INIT;
inline size_t head=0,count=0;
inline std::atomic<unsigned long long> dropped{0},written{0},errors{0};
inline std::atomic_bool ready{false};
inline std::filesystem::path folder,file;
inline std::string channel,session;
inline unsigned part=0;
inline unsigned long long bytes=0,nextPump=0;
inline bool started=false;

inline void Emit(const char* text) noexcept {
 if(!ready.load(std::memory_order_acquire))return;
 if(!TryAcquireSRWLockExclusive(&lock)){++dropped;return;}
 if(count==QueueCapacity){++dropped;ReleaseSRWLockExclusive(&lock);return;}
 auto& e=queue[(head+count)%QueueCapacity];e.tick=GetTickCount64();e.thread=GetCurrentThreadId();
 size_t i=0;for(;text&&text[i]&&i<TextCapacity-1;++i)e.text[i]=(text[i]=='\r'||text[i]=='\n')?' ':text[i];
 e.text[i]=0;++count;ReleaseSRWLockExclusive(&lock);
}
inline void Prune() noexcept {
 try {
  std::vector<std::filesystem::directory_entry> files;
  std::error_code ec;
  for(auto& f:std::filesystem::directory_iterator(folder,ec)){
   auto name=f.path().filename().string();
   if(f.is_regular_file(ec)&&name.rfind("dide-"+channel+"-",0)==0&&f.path().extension()==".log")files.push_back(f);
  }
  std::sort(files.begin(),files.end(),[](const auto&a,const auto&b){return a.path().filename()<b.path().filename();});
  while(files.size()>RetainedFiles){
   auto i=std::find_if(files.begin(),files.end(),[](const auto& f){return f.path()!=file;});
   if(i==files.end())break;
                                                                                         
   std::filesystem::remove(i->path(),ec);if(ec){++errors;break;}files.erase(i);
  }
 }catch(...){++errors;}
}
inline void NewPart(){
 char suffix[40]{};sprintf_s(suffix,"-%06u.log",part++);
 file=folder/("dide-"+channel+"-"+session+suffix);bytes=0;
}
inline void WriteLine(const std::string& line) {
 if(bytes+line.size()>FileLimit)NewPart();
 std::ofstream out(file,std::ios::app|std::ios::binary);
 if(!out){++errors;return;}
 out.write(line.data(),static_cast<std::streamsize>(line.size()));out.flush();
 if(!out){++errors;return;}bytes+=line.size();++written;
}
inline void Pump(bool force=false) noexcept {
 if(!started)return;
 auto now=GetTickCount64();if(!force&&now<nextPump)return;nextPump=now+1000;
 try {
                                                                             
  std::array<Entry,QueueCapacity> batch{};size_t n=0;
  AcquireSRWLockExclusive(&lock);
  while(count&&n<QueueCapacity){batch[n++]=queue[head];head=(head+1)%QueueCapacity;--count;}
  ReleaseSRWLockExclusive(&lock);
  for(size_t i=0;i<n;++i){char prefix[96]{};sprintf_s(prefix,"tick=%llu thread=%lu ",batch[i].tick,batch[i].thread);WriteLine(std::string(prefix)+batch[i].text+'\n');}
  static unsigned long long previousDropped=0,previousErrors=0;
  if(dropped.load()!=previousDropped||errors.load()!=previousErrors){
   previousDropped=dropped.load();previousErrors=errors.load();
   WriteLine("LOGGER_HEALTH dropped_total="+std::to_string(previousDropped)+" io_errors_total="+std::to_string(previousErrors)+"\n");
  }
  Prune();
 }catch(...){++errors;}
}
inline std::string HashFile(const wchar_t* name) {
 HCRYPTPROV provider=0;HCRYPTHASH hash=0;std::string result="unavailable";
 if(!CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT))return result;
 if(CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){
  std::ifstream in(std::filesystem::path(name),std::ios::binary);char data[65536];bool ok=!!in;
  while(in&&ok){in.read(data,sizeof(data));auto n=in.gcount();if(n>0)ok=!!CryptHashData(hash,reinterpret_cast<BYTE*>(data),static_cast<DWORD>(n),0);}
  BYTE value[32];DWORD len=sizeof(value);
  if(ok&&in.eof()&&CryptGetHashParam(hash,HP_HASHVAL,value,&len,0)){result.clear();char hex[3];for(BYTE b:value){sprintf_s(hex,"%02x",b);result+=hex;}}
  CryptDestroyHash(hash);
 }
 CryptReleaseContext(provider,0);return result;
}
inline void Module(const wchar_t* name,const char* label) noexcept {
 try {auto mod=GetModuleHandleW(name);wchar_t path[32768]{};
  if(!mod||!GetModuleFileNameW(mod,path,32768)){Emit("MODULE_IDENTITY unavailable");return;}
  auto hash=HashFile(path);std::string message=std::string("MODULE_IDENTITY name=")+label+" sha256="+hash;Emit(message.c_str());
 }catch(...){++errors;}
}
inline bool Start(const char* module,const std::filesystem::path& overrideFolder={}) noexcept {
 if(started)return ready.load();
 try {
  channel=module;
  if(channel!="menu"&&channel!="melee"&&channel!="test")return false;
  if(overrideFolder.empty()){
   wchar_t local[32768]{};auto len=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
   if(!len||len>=32768)return false;
   folder=std::filesystem::path(local)/L"JentaSpecialEdition"/L"MenuDiagnostics";
  }else folder=overrideFolder;
  std::filesystem::create_directories(folder);folder=std::filesystem::weakly_canonical(folder);
  SYSTEMTIME utc{};GetSystemTime(&utc);char id[100]{};
  sprintf_s(id,"%04u%02u%02uT%02u%02u%02u%03u-p%lu",utc.wYear,utc.wMonth,utc.wDay,utc.wHour,utc.wMinute,utc.wSecond,utc.wMilliseconds,GetCurrentProcessId());session=id;
  NewPart();started=true;ready.store(true,std::memory_order_release);
  Emit("SESSION_START schema=1 build=DIDE-RoutineLogger-20260914-v1 base=pre-spawn-1D33 offline_validation_only");
  Emit("LIMITS file_bytes=2097152 retained_files_per_channel=12 queue=256 flush_ms=1000 status_ms=10000");
  Pump(true);return errors.load()==0;
 }catch(...){ready=false;++errors;return false;}
}
inline void Stop() noexcept {Emit("SESSION_END worker_return");Pump(true);ready=false;}
struct WorkerScope { ~WorkerScope(){Stop();} };
}
