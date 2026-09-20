#include "NativeMenu/src/Main/PersistentCollisionRedirect.cpp"
#include <cassert>
#include <iostream>
int main(){
 SetEnvironmentVariableW(L"LOCALAPPDATA",L"../Evidence/ConfigAppData");
 Control fake{};control=&fake;FeatureBindings::Defaults();MenuPresets::overrideDirectory=std::filesystem::absolute("../Evidence/Configs");MenuPresets::Init();
 assert(MenuTabCount==15&&FeatureBindings::ActionCount()==104);
 assert(FeatureBindings::rowCounts[3]==5&&FeatureBindings::rowCounts[12]==3);
 assert(MenuPresets::settings.size()==75);
 assert(MenuPresets::Load(0));assert(themeColors[2]==(LONG)0xFFB784F5);
 assert(FeatureBindings::Assign(64,'J'));runScale=2.5f;assert(MenuPresets::Save(1));assert(MenuPresets::Load(0));assert(MenuPresets::Load(1));assert(runScale==2.5f&&FeatureBindings::keys[64]=='J');
 V velocity=MotionVelocity({0,0,0},{100,0,0},.01f,25.f);assert(fabsf(velocity.x-25.f)<.001f);assert(MotionVelocity({},{1,1,1},0,25).x==0);
 auto layout=MenuLayoutFor(1280,720);assert(layout.x>0&&layout.scale>0);
 assert(MenuNextTab(0,1)==1);
 std::cout<<"PASS: 15 sidebar pages, 104 controls, 75 settings, isolated profile roundtrip and purple defaults\n";
}
