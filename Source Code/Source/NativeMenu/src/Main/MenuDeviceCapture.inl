                                                                                 
                                                                                   
struct MenuDeviceCapture {
 bool delivered[256]{};
 int Index(bool mouse,DWORD offset)const {
  if(!mouse)return offset<256?(int)offset:-1;
  return offset>=DIMOFS_BUTTON0&&offset<DIMOFS_BUTTON0+8?(int)(offset-DIMOFS_BUTTON0):-1;
 }
 void State(bool mouse,void*data,DWORD size,bool capture){
  auto bytes=(BYTE*)data;
  if(capture){memset(data,0,size);return;}
  const unsigned count=mouse?size-12:256;
  for(unsigned i=0;i<count;i++)if(bytes[(mouse?12:0)+i]&0x80)delivered[i]=true;
 }
 DWORD Events(bool mouse,DIDEVICEOBJECTDATA*data,DWORD count,DWORD capacity,bool capture,bool peek){
  if(!capture){
   if(!peek)for(DWORD i=0;i<count;i++){int key=Index(mouse,data[i].dwOfs);if(key>=0)delivered[key]=(data[i].dwData&0x80)!=0;}
   return count;
  }
  DWORD output=0;
                                                                            
                                                                         
  for(unsigned i=0;i<(mouse?8u:256u)&&output<capacity;i++)if(delivered[i]){
   DIDEVICEOBJECTDATA release{};release.dwOfs=mouse?DIMOFS_BUTTON0+i:i;
   data[output++]=release;if(!peek)delivered[i]=false;
  }
  return output;
 }
};
