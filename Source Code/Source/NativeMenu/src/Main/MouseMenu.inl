                                                                            
constexpr int MenuTabCount=15;
constexpr int MenuTabOrder[MenuTabCount]={0,1,17,3,5,13,4,10,9,11,12,6,7,2,15};
struct MenuLayout {float x,y,scale;};
float MenuHeight(){return 540.f;}
MenuLayout MenuLayoutFor(float w,float h){float scale=std::min(2.f,std::min((w-24)/720.f,(h-24)/MenuHeight()));scale=std::max(.1f,scale);return {(w-720*scale)*.5f,(h-MenuHeight()*scale)*.5f,scale};}
int MenuNextTab(int page,int direction){for(int i=0;i<MenuTabCount;i++)if(MenuTabOrder[i]==page)return MenuTabOrder[(i+direction+MenuTabCount)%MenuTabCount];return 0;}
bool MenuAdjustable(int page,int row){return (page==5&&row>=3&&row<=5)||(page==14&&row==6)||(page==11&&(row==0||row==4))||(page==12&&(row==0||row==1))||(page==1&&row==1)||(page==3&&((row>=1&&row<=3)||row==6))||(page==4&&(row==2||row==3||row==8||row==10||row==11))||(page==6&&row>=4&&row<=6)||(page==7)||(page==9&&(row==3||row==4||row==7));}float MenuRowHeight(int page){return page==4?29.f:32.f;}
std::atomic<HWND> menuWindow{nullptr};WNDPROC menuPreviousProc=nullptr;
std::atomic<unsigned> menuLegacyButtonEvents{0};
std::atomic_int menuMouseClick{0},menuMouseWheel{0};std::atomic_bool menuLeftHeld{false};
                                                                                                
                                                                                                
                                                                                                
                                                                                                 
                                                                                                  
                                                                                                  
                                                                                                
                                                                                                  
                                                                                
std::atomic<unsigned> windowClickEvents{0},windowWheelEvents{0},windowMouseMoveEvents{0};
std::atomic<unsigned> cursorPosHookCalls{0},clipCursorHookCalls{0};
using CursorPosFn=BOOL(WINAPI*)(int,int);CursorPosFn menuOriginalCursorPos=nullptr;
using ClipFn=BOOL(WINAPI*)(const RECT*);ClipFn menuOriginalClip=nullptr;
using KeyFn=SHORT(WINAPI*)(int);KeyFn menuOriginalAsync=nullptr,menuOriginalKey=nullptr;
using PollFn=int(*)(void*);PollFn menuOriginalPoll=nullptr;
bool MenuCapturesMouse(){return menuOpen.load()&&ForegroundGame();}
#include "MouseButtons.inl"
std::atomic<float> diMenuX{255},diMenuY{270};std::atomic_bool diCursorActive{false};
std::atomic<unsigned> diStateReads{0},diDataReads{0},diCaptured{0};
bool MenuMousePosition(float w,float h,float&x,float&y){if(MenuCapturesMouse()&&diCursorActive){x=diMenuX.load()-210;y=diMenuY.load();return true;}POINT pt{};RECT rc{};HWND wnd=menuWindow.load();if(w<=0||h<=0||!wnd||!GetCursorPos(&pt)||!ScreenToClient(wnd,&pt)||!GetClientRect(wnd,&rc)||rc.right<=0||rc.bottom<=0)return false;auto a=MenuLayoutFor(w,h);x=(pt.x*w/rc.right-a.x)/a.scale-210;y=(pt.y*h/rc.bottom-a.y)/a.scale;return true;}
BOOL WINAPI MenuCursorPos(int x,int y){bool capturing=MenuCapturesMouse();if(capturing)++cursorPosHookCalls;return capturing?TRUE:menuOriginalCursorPos(x,y);}
BOOL WINAPI MenuClip(const RECT*r){bool capturing=MenuCapturesMouse();if(capturing)++clipCursorHookCalls;return menuOriginalClip(capturing?nullptr:r);}
bool MenuGameCaller(uintptr_t p){return InGame(p)||(p>=engineBase&&p<engineBase+engineSize);}
SHORT WINAPI MenuAsync(int key){return MenuCapturesMouse()&&MenuGameCaller((uintptr_t)_ReturnAddress())?0:menuOriginalAsync(key);}
SHORT WINAPI MenuKey(int key){return MenuCapturesMouse()&&MenuGameCaller((uintptr_t)_ReturnAddress())?0:menuOriginalKey(key);}
int MenuPoll(void*event){if(!event||!MenuCapturesMouse())return menuOriginalPoll(event);for(int i=0;i<64;i++){int result=menuOriginalPoll(event);if(!result)return result;unsigned type=*(unsigned*)event;if(type<0x300||type>0x403)return result;}return 0;}
LRESULT CALLBACK MenuWindowProc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp){
 if(MenuCapturesMouse()){
  if(msg==WM_LBUTTONDOWN){++windowClickEvents;menuLeftHeld=true;++menuLegacyButtonEvents;return 0;}if(msg==WM_LBUTTONUP){menuLeftHeld=false;return 0;}if(msg==WM_RBUTTONDOWN){++windowClickEvents;++menuLegacyButtonEvents;return 0;}
  if(msg==WM_MOUSEWHEEL){++windowWheelEvents;menuMouseWheel+=(short)HIWORD(wp)>0?1:-1;return 0;}
  if(msg==WM_INPUT)return DefWindowProcW(wnd,msg,wp,lp);
  if(msg==WM_MOUSEMOVE)++windowMouseMoveEvents;
  if((msg>=WM_MOUSEFIRST&&msg<=WM_MOUSELAST)||msg==WM_KEYDOWN||msg==WM_KEYUP||msg==WM_CHAR)return 0;
 }
 return CallWindowProcW(menuPreviousProc,wnd,msg,wp,lp);
}
void EnsureMenuWindow(HWND wnd){
 if(!wnd)return;
 if(!menuWindow.load()){
  menuPreviousProc=(WNDPROC)GetWindowLongPtrW(wnd,GWLP_WNDPROC);
  SetLastError(0);auto old=(WNDPROC)SetWindowLongPtrW(wnd,GWLP_WNDPROC,(LONG_PTR)MenuWindowProc);
  if(old){menuPreviousProc=old;menuWindow=wnd;Log("MOUSE_MENU_WINDOW_CONNECTED");}
 }
 if(MenuCapturesMouse()&&menuOriginalClip)menuOriginalClip(nullptr);
}
void SyncMenuCursor(){static bool captured=false;static RECT saved{};static bool savedValid=false;bool now=MenuCapturesMouse();if(now&&!captured){savedValid=GetClipCursor(&saved)!=FALSE;if(menuOriginalClip)menuOriginalClip(nullptr);}else if(!now&&captured&&menuOriginalClip&&ForegroundGame()){menuOriginalClip(savedValid?&saved:nullptr);}captured=now;}
void RestoreMenuWindow(){HWND wnd=menuWindow.exchange(nullptr);if(wnd&&menuPreviousProc&&(WNDPROC)GetWindowLongPtrW(wnd,GWLP_WNDPROC)==MenuWindowProc)SetWindowLongPtrW(wnd,GWLP_WNDPROC,(LONG_PTR)menuPreviousProc);}
void InstallMenuMouseHooks(){
 auto init=MH_Initialize();if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED){Log("MOUSE_MENU_INIT_FAILED");return;}
 struct Hook {const wchar_t*module;const char*name;void*handler;void**original;};
 Hook hooks[]={{L"user32.dll","SetCursorPos",(void*)MenuCursorPos,(void**)&menuOriginalCursorPos},{L"user32.dll","ClipCursor",(void*)MenuClip,(void**)&menuOriginalClip},{L"user32.dll","GetAsyncKeyState",(void*)MenuAsync,(void**)&menuOriginalAsync},{L"user32.dll","GetKeyState",(void*)MenuKey,(void**)&menuOriginalKey},{L"SDL2.dll","SDL_PollEvent",(void*)MenuPoll,(void**)&menuOriginalPoll}};
 unsigned ready=0;
 for(auto&hook:hooks){
  HMODULE mod=GetModuleHandleW(hook.module);void*fn=mod?(void*)GetProcAddress(mod,hook.name):nullptr;
  if(!fn){Log((std::string("MOUSE_MENU_HOOK_MISSING_")+hook.name).c_str());continue;}
  auto created=MH_CreateHook(fn,hook.handler,hook.original);
  if(created!=MH_OK){Log((std::string("MOUSE_MENU_HOOK_CREATE_FAILED_")+hook.name+"_"+std::to_string(created)).c_str());continue;}
                                                                             
                                                                        
  auto enabled=MH_EnableHook(fn);
  if(enabled!=MH_OK){Log((std::string("MOUSE_MENU_HOOK_ENABLE_FAILED_")+hook.name+"_"+std::to_string(enabled)).c_str());continue;}
  ++ready;
 }
 Log(ready==5?"MOUSE_MENU_INPUT_HOOKS_READY":"MOUSE_MENU_INPUT_HOOKS_PARTIAL_TRAMPOLINES_RETAINED");
}


#include "NativeInputCapture.inl"
                                                                                                 
                                                                                              
                                                         
void LogMenuInput(){
 static ULONGLONG next=0;auto now=GetTickCount64();if(now<next)return;next=now+1000;
 POINT pt{};bool gotCursor=GetCursorPos(&pt)!=FALSE;
 std::ofstream f(LogPath(),std::ios::app);
 f<<now<<",MOUSE_MENU_DIAG,menu_open="<<menuOpen.load()<<",window_clicks="<<windowClickEvents.load()<<",window_wheel="<<windowWheelEvents.load()<<",window_mousemove="<<windowMouseMoveEvents.load()<<",cursorpos_hook_calls="<<cursorPosHookCalls.load()<<",clipcursor_hook_calls="<<clipCursorHookCalls.load()<<",di_state_reads="<<diStateReads.load()<<",di_data_reads="<<diDataReads.load()<<",di_captured="<<diCaptured.load()<<",native_keyboard_reads="<<nativeKeyboardReads.load()<<",native_releases="<<nativeReleaseEvents.load()<<",button_poll_samples="<<menuButtonPollSamples.load()<<",button_click_edges="<<menuPolledClickEdges.load()<<",legacy_button_events="<<menuLegacyButtonEvents.load()<<",button_left_held="<<menuLeftHeld.load()<<",menu_mouse_click="<<menuMouseClick.load()<<",di_menu_x="<<diMenuX.load()<<",di_menu_y="<<diMenuY.load()<<",di_cursor_active="<<diCursorActive.load()<<",screen_cursor_x="<<(gotCursor?pt.x:-1)<<",screen_cursor_y="<<(gotCursor?pt.y:-1)<<'\n';
}


