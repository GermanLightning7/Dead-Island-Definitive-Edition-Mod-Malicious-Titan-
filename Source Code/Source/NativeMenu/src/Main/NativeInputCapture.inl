#include "MenuDeviceCapture.inl"
namespace CameraLockOn { void InjectMouseState(void*data); void InjectBufferedMouseData(void*rawData,DWORD*count,DWORD capacity); }
namespace AutoFire {void State(void*,bool);void Events(void*,DWORD*,DWORD,bool);}
using DIStateFn=HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8A*,DWORD,LPVOID);
using DIDataFn=HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8A*,DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD);
DIStateFn originalDIState=nullptr;DIDataFn originalDIData=nullptr;
std::atomic<uintptr_t> nativeMenuMouse{0},nativeMenuKeyboard{0};
std::atomic<unsigned> nativeKeyboardReads{0},nativeReleaseEvents{0};
std::mutex nativeCaptureMutex;MenuDeviceCapture nativeMouseCapture,nativeKeyboardCapture;
bool NativeMenuCapturing(){return menuOpen.load()&&!stop.load();}
int NativeMenuDevice(IDirectInputDevice8A*dev){auto p=(uintptr_t)dev;return p==nativeMenuMouse.load()?1:p==nativeMenuKeyboard.load()?2:0;}
HRESULT STDMETHODCALLTYPE MenuDIState(IDirectInputDevice8A*dev,DWORD size,LPVOID data){
 HRESULT hr=originalDIState(dev,size,data);if(FAILED(hr)||!data)return hr;
 int type=NativeMenuDevice(dev);bool mouse=type==1;
 if(!type||(mouse?(size!=sizeof(DIMOUSESTATE)&&size!=sizeof(DIMOUSESTATE2)):size!=256))return hr;
 std::lock_guard<std::mutex> lock(nativeCaptureMutex);bool capture=NativeMenuCapturing();
 (mouse?nativeMouseCapture:nativeKeyboardCapture).State(mouse,data,size,capture);
 if(mouse&&!capture)CameraLockOn::InjectMouseState(data);
 if(mouse)AutoFire::State(data,capture);
 if(mouse)++diStateReads;else ++nativeKeyboardReads;if(capture)++diCaptured;
 return hr;
}
HRESULT STDMETHODCALLTYPE MenuDIData(IDirectInputDevice8A*dev,DWORD size,LPDIDEVICEOBJECTDATA data,LPDWORD count,DWORD flags){
 DWORD capacity=count?*count:0;HRESULT hr=originalDIData(dev,size,data,count,flags);
 if(FAILED(hr)||!count||!data||size!=sizeof(DIDEVICEOBJECTDATA)||capacity>4096||*count>capacity)return hr;
 int type=NativeMenuDevice(dev);if(!type)return hr;bool mouse=type==1;
 std::lock_guard<std::mutex> lock(nativeCaptureMutex);bool capture=NativeMenuCapturing();
 *count=(mouse?nativeMouseCapture:nativeKeyboardCapture).Events(mouse,data,*count,capacity,capture,(flags&DIGDD_PEEK)!=0);
 if(mouse&&!capture&&!(flags&DIGDD_PEEK))CameraLockOn::InjectBufferedMouseData(data,count,capacity);
 if(mouse&&!(flags&DIGDD_PEEK))AutoFire::Events(data,count,capacity,capture);
 if(mouse)++diDataReads;else ++nativeKeyboardReads;
 if(capture){++diCaptured;if(!(flags&DIGDD_PEEK))nativeReleaseEvents+=*count;return DI_OK;}
 return hr;
}
bool NativeInputOwnedCode(uintptr_t address,HMODULE module){MEMORY_BASIC_INFORMATION info{};return VirtualQuery((void*)address,&info,sizeof(info))&&info.AllocationBase==module&&(info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));}
void InstallDirectInputMenu(){
 static ULONGLONG next=0;static bool attempted=false,waitingLogged=false;static uintptr_t stateTarget=0,dataTarget=0;
 auto now=GetTickCount64();if(now<next)return;next=now+1000;
 uintptr_t input=0,vt=0,mouse=0,keyboard=0,mvt=0,kvt=0,md=0,kd=0,mdevt=0,kdevt=0,ms=0,ks=0,mb=0,kb=0;
                                                                              
                                                                                
 if(!Pointer(engineBase+0xA4FA68,input)||!Read(input,vt)||vt!=engineBase+0x8719D8||
    !Pointer(input+0x100,mouse)||!Pointer(input+0xF8,keyboard)||
    !Read(mouse,mvt)||mvt!=engineBase+0x871AC8||!Read(keyboard,kvt)||kvt!=engineBase+0x871A60||
    !Pointer(mouse+0x1808,md)||!Pointer(keyboard+0x1808,kd)||
    !Pointer(md,mdevt)||!Pointer(kd,kdevt)||!Read(mdevt+9*8,ms)||!Read(kdevt+9*8,ks)||
    !Read(mdevt+10*8,mb)||!Read(kdevt+10*8,kb)){
  nativeMenuMouse=0;nativeMenuKeyboard=0;
  if(!waitingLogged){Log("NATIVE_MENU_INPUT_WAITING_FOR_DEVICES");waitingLogged=true;}return;
 }
 HMODULE module=GetModuleHandleW(L"dinput8.dll");
 if(ms!=ks||mb!=kb||ms==mb||!module||!NativeInputOwnedCode(ms,module)||!NativeInputOwnedCode(mb,module)){
  nativeMenuMouse=0;nativeMenuKeyboard=0;if(!attempted){Log("NATIVE_MENU_INPUT_TARGET_REFUSED");attempted=true;}return;
 }
 if(attempted&&(ms!=stateTarget||mb!=dataTarget))return;
 {std::lock_guard<std::mutex> lock(nativeCaptureMutex);
  if(nativeMenuMouse.load()!=md){nativeMouseCapture={};nativeMenuMouse=md;}
  if(nativeMenuKeyboard.load()!=kd){nativeKeyboardCapture={};nativeMenuKeyboard=kd;}
 }
 if(attempted)return;attempted=true;stateTarget=ms;dataTarget=mb;
 auto a=MH_CreateHook((void*)ms,(void*)MenuDIState,(void**)&originalDIState);
 auto b=MH_CreateHook((void*)mb,(void*)MenuDIData,(void**)&originalDIData);
 if(a!=MH_OK||b!=MH_OK){if(a==MH_OK)MH_RemoveHook((void*)ms);if(b==MH_OK)MH_RemoveHook((void*)mb);Log("NATIVE_MENU_INPUT_CREATE_FAILED");return;}
                                                                       
 auto ea=MH_EnableHook((void*)ms);auto eb=MH_EnableHook((void*)mb);
 if(ea!=MH_OK||eb!=MH_OK){MH_DisableHook((void*)ms);MH_DisableHook((void*)mb);Log("NATIVE_MENU_INPUT_ENABLE_FAILED");return;}
 std::ofstream f(LogPath(),std::ios::app);f<<now<<",NATIVE_MENU_INPUT_READY,mouse="<<std::hex<<md<<",keyboard="<<kd<<",state="<<ms<<",data="<<mb<<std::dec<<'\n';
}

