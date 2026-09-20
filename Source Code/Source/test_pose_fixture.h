#pragma once
uintptr_t PoseAllocate(size_t n){auto p=(uintptr_t)VirtualAlloc(nullptr,n,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);assert(p);return p;}
struct PoseFixture {uintptr_t descriptors,component,locals,worlds,flags,names,map;};
PoseFixture MakePose(uintptr_t entity,uintptr_t complete,V root){
 auto model=PoseAllocate(0x500),pose=PoseAllocate(0x80),owner=PoseAllocate(0x80),skeleton=PoseAllocate(0x80);
 PoseFixture f{PoseAllocate(24*0x30),PoseAllocate(0x80),PoseAllocate(24*0x30),PoseAllocate(24*0x30),PoseAllocate(24),PoseAllocate(16),PoseAllocate(8)};
 Write(complete+8,model);Write(model+0x398,pose);Write(pose+0x20,owner);Write(pose+0x28,(unsigned short)24);Write(pose+0x2c,(unsigned char)1);Write(pose+0x38,skeleton);
 Write(owner+0x38,f.descriptors);Write(skeleton+0x28,f.names);Write(skeleton+0x30,2u);Write(skeleton+0x58,f.map);Write(skeleton+0x60,2u);
 Write(f.names,17);Write(f.names+4,PoseNameHash("bip01 spine3"));Write(f.names+8,7);Write(f.names+12,PoseNameHash("bip01 head"));Write(f.map,17);Write(f.map+4,7);
 Write(f.component+0x20,f.locals);Write(f.component+0x30,f.worlds);Write(f.component+0x40,f.flags);
 for(int i=0;i<24;++i){int key=i*7%24;Write(f.descriptors+i*0x30+0x20,f.component);Write(f.descriptors+i*0x30+0x28,key);
  M34 mat{{1,0,0,root.x,0,1,0,root.y+(i==7?1.55f:1.35f),0,0,1,root.z}};Write(f.worlds+key*0x30,mat);Write(f.locals+key*0x30,mat);
 }return f;
}
void PoseCamera(){
 auto view=PoseAllocate(0x91000),owner=PoseAllocate(0x300),pub=PoseAllocate(0x20);Write(gameBase+CameraViewRva,view);Write(view+0x90390,owner);Write(owner+0x2b0,pub);Write(pub+8,owner);
 Write(owner+0x40,1.f);Write(owner+0x54,1.f);Write(owner+0x68,-1.f);Write(owner+0x5c,1.55f);Write(owner+0x70,1.f);Write(owner+0x84,1.f);presentWidth=1280;presentHeight=720;
}
