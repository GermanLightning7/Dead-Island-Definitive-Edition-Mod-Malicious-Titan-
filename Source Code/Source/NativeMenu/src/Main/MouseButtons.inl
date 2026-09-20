                                                                                     
                                                                            
struct MenuButtonEdges {
 bool armed=false,left=false,right=false;
 int Sample(bool capturing,bool nextLeft,bool nextRight){
  if(!capturing){armed=false;left=nextLeft;right=nextRight;return 0;}
  if(!armed){left=nextLeft;right=nextRight;if(!left&&!right)armed=true;return 0;}
  int click=nextLeft&&!left?1:nextRight&&!right?2:0;
  left=nextLeft;right=nextRight;return click;
 }
};
MenuButtonEdges menuButtonEdges;
std::atomic<unsigned> menuButtonPollSamples{0},menuPolledClickEdges{0};
void PollMenuButtons(){
 const bool capturing=MenuCapturesMouse();
 const bool left=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0,right=(GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0;
 const int click=menuButtonEdges.Sample(capturing,left,right);
 menuLeftHeld=capturing&&left;menuMouseClick=click;
 if(capturing)++menuButtonPollSamples;
 if(click)++menuPolledClickEdges;
}
