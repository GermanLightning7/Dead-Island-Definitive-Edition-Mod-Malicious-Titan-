#include <windows.h>
ULONGLONG previewNow=1000;HWND previewWindow=nullptr;ULONGLONG PreviewNow(){return previewNow;}HWND PreviewForeground(){return previewWindow;}
#define GetTickCount64 PreviewNow
#define GetForegroundWindow PreviewForeground
#include "NativeMenu/src/Main/PersistentCollisionRedirect.cpp"
#include <cassert>
#include <gdiplus.h>
using namespace Gdiplus;
ID3D11Device*previewDevice=nullptr;ID3D11DeviceContext*previewContext=nullptr;IDXGISwapChain*previewSwap=nullptr;
void InitPreview(){
 WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"DideOfflinePreview";RegisterClassW(&wc);previewWindow=CreateWindowW(wc.lpszClassName,L"Offline test",WS_OVERLAPPEDWINDOW,0,0,1280,720,nullptr,nullptr,wc.hInstance,nullptr);assert(previewWindow);
 DXGI_SWAP_CHAIN_DESC sd{};sd.BufferDesc.Width=1280;sd.BufferDesc.Height=720;sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.SampleDesc.Count=1;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.BufferCount=1;sd.OutputWindow=previewWindow;sd.Windowed=TRUE;sd.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
 assert(SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&sd,&previewSwap,&previewDevice,nullptr,&previewContext)));assert(EnsureD3D(previewDevice));
}
void Preview(const std::vector<D3DVertex>&v,const wchar_t*path,int width,int height){
 auto ctx=previewContext;D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_RENDER_TARGET;ID3D11Texture2D*target=nullptr;assert(SUCCEEDED(previewDevice->CreateTexture2D(&desc,nullptr,&target)));ID3D11RenderTargetView*rtv=nullptr;assert(SUCCEEDED(previewDevice->CreateRenderTargetView(target,nullptr,&rtv)));
 float clear[4]={.106f,.114f,.122f,1};ctx->ClearRenderTargetView(rtv,clear);D3D11_MAPPED_SUBRESOURCE map{};assert(SUCCEEDED(ctx->Map(d3dVertices,0,D3D11_MAP_WRITE_DISCARD,0,&map)));memcpy(map.pData,v.data(),v.size()*sizeof(D3DVertex));ctx->Unmap(d3dVertices,0);
 ctx->OMSetRenderTargets(1,&rtv,nullptr);ctx->OMSetBlendState(d3dBlend,nullptr,0xffffffff);ctx->OMSetDepthStencilState(d3dDepth,0);ctx->RSSetState(d3dRaster);D3D11_VIEWPORT vp{0,0,(float)width,(float)height,0,1};ctx->RSSetViewports(1,&vp);ctx->IASetInputLayout(d3dLayout);ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);UINT stride=sizeof(D3DVertex),offset=0;ctx->IASetVertexBuffers(0,1,&d3dVertices,&stride,&offset);ctx->VSSetShader(d3dVs,nullptr,0);ctx->PSSetShader(d3dPs,nullptr,0);ctx->GSSetShader(nullptr,nullptr,0);ctx->HSSetShader(nullptr,nullptr,0);ctx->DSSetShader(nullptr,nullptr,0);ctx->PSSetShaderResources(0,1,&SmoothFont::texture);ctx->PSSetSamplers(0,1,&SmoothFont::sampler);ctx->Draw((UINT)v.size(),0);
 desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ID3D11Texture2D*staging=nullptr;assert(SUCCEEDED(previewDevice->CreateTexture2D(&desc,nullptr,&staging)));ctx->CopyResource(staging,target);assert(SUCCEEDED(ctx->Map(staging,0,D3D11_MAP_READ,0,&map)));std::vector<unsigned>pixels(width*height);for(int y=0;y<height;y++){auto row=(unsigned*)((char*)map.pData+y*map.RowPitch);for(int x=0;x<width;x++){auto c=row[x];pixels[y*width+x]=0xff000000u|((c&255)<<16)|(c&0xff00)|((c>>16)&255);}}ctx->Unmap(staging,0);
 Bitmap bitmap(width,height,width*4,PixelFormat32bppARGB,(BYTE*)pixels.data());CLSID png={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};assert(bitmap.Save(path,&png,nullptr)==Ok);staging->Release();rtv->Release();target->Release();
}
void TestPipelineRestore(){
 menuOpen=true;espEnabled=false;menuPage=6;auto ctx=previewContext;
                                                                              
 for(bool populated:{true,false}){
  ID3D11ShaderResourceView*before=populated?SmoothFont::texture:nullptr;ID3D11SamplerState*sampler=populated?SmoothFont::sampler:nullptr;ctx->PSSetShaderResources(0,1,&before);ctx->PSSetSamplers(0,1,&sampler);auto draws=presentDraws.load();DrawNativeEsp(previewSwap);assert(presentDraws.load()>draws);
  ID3D11ShaderResourceView*after=nullptr;ID3D11SamplerState*afterSampler=nullptr;ctx->PSGetShaderResources(0,1,&after);ctx->PSGetSamplers(0,1,&afterSampler);assert(before==after&&sampler==afterSampler);if(after)after->Release();if(afterSampler)afterSampler->Release();
 }
 menuOpen=false;RestoreMenuWindow();puts("PASS: actual DrawNativeEsp restores font texture/sampler slots, populated and null");
}

int main(){ FeatureBindings::Defaults();FeatureBindings::Assign(64,'F'); SetEnvironmentVariableW(L"LOCALAPPDATA", L"../Evidence/PreviewAppData");
 GdiplusStartupInput startup;ULONG_PTR token=0;assert(GdiplusStartup(&token,&startup,nullptr)==Ok);
 InitPreview();
 MenuAnimation::State motion;auto start=motion.Sample(1000,6,0);assert(start.opacity<.3f);auto settled=motion.Sample(1200,6,0);assert(settled.opacity==1&&settled.content==1);auto moved=motion.Sample(1216,6,8);assert(moved.row>0&&moved.row<8);auto tabChange=motion.Sample(1220,9,2);assert(tabChange.row==2&&tabChange.content==.4f);motion.Close();assert(motion.Sample(1221,9,2).opacity<.3f);assert(motion.Sample(1200,9,2).opacity<.3f);
 assert(MenuTabCount==15);int page=0;for(int i=0;i<15;i++){assert(page!=8);page=MenuNextTab(page,1);}assert(page==0);
 AimTargeting::priority=1;AimTargeting::bodyPart=1;NoRecoil::enabled=true;
 unsigned long long maximum=0;FILE*report=nullptr;fopen_s(&report,"../Evidence/menu-preview-report.txt","w");assert(report);
                                                                                                         
 for(auto dimensions:{std::pair<int,int>{1280,720},{1920,1080},{3440,1440}}){
  for(int tab:MenuTabOrder){MenuAnimation::state.Close();MenuAnimation::state.Sample(previewNow,tab,tab==4?10:tab==9?11:tab==6?7:tab==12?2:tab==14?9:0);previewNow+=250;menuPage=tab;menuIndex=tab==4?10:tab==9?11:tab==6?7:tab==12?2:tab==14?9:0;std::vector<D3DVertex>v;AddMenu(v,(float)dimensions.first,(float)dimensions.second);
   assert(!v.empty()&&v.size()%3==0&&v.size()<131072);maximum=std::max(maximum,(unsigned long long)v.size());
   for(auto a:v)assert(std::isfinite(a.x)&&std::isfinite(a.y)&&a.x>=-1&&a.x<=1&&a.y>=-1&&a.y<=1&&a.r>=0&&a.r<=1&&a.g>=0&&a.g<=1&&a.b>=0&&a.b<=1);
   fprintf(report,"page=%d size=%dx%d vertices=%zu PASS\n",tab,dimensions.first,dimensions.second,v.size());
   if(dimensions.first==1280){wchar_t path[128]{};swprintf_s(path,L"../Evidence/menu-page-%02d.png",tab);Preview(v,path,1280,720);}
  }
 }
 MenuAnimation::state.Close();MenuAnimation::state.Sample(previewNow,6,7);previewNow+=250;menuPage=6;espEnabled=true;menuIndex=7;std::vector<D3DVertex>combined;AddMenu(combined,1280,720);EspEntity e{};e.state=100;e.complete=200;e.generation=1;EspVitals::Fraction(e,100,1000);AppendEspVitals(combined,e,42,12.3f,1100,1030,290,1120,530,1280,720);Preview(combined,L"../Evidence/menu-and-esp.png",1280,720);
 for(int frame=0;frame<36;frame++){previewNow=20000+frame*50;if(frame==0)MenuAnimation::state.Close();menuPage=6;menuIndex=frame<14?7:8;std::vector<D3DVertex>v;AddMenu(v,1280,720);wchar_t path[128]{};swprintf_s(path,L"../Evidence/animation-%02d.png",frame);Preview(v,path,1280,720);}
 for(auto dimensions:{std::pair<int,int>{320,200},{1280,720},{3840,2160}}){for(int mask=1;mask<4;mask++){espHealthEnabled=(mask&1)!=0;espDistanceEnabled=(mask&2)!=0;std::vector<D3DVertex>v;assert(AppendEspVitals(v,e,999999,800,30000,0,0,40,100,(float)dimensions.first,(float)dimensions.second));for(auto a:v)assert(std::isfinite(a.x)&&std::isfinite(a.y)&&a.x>=-1&&a.x<=1&&a.y>=-1&&a.y<=1);}}
 TestPipelineRestore();ReleaseD3D();assert(!SmoothFont::texture&&!SmoothFont::sampler);assert(EnsureD3D(previewDevice));puts("PASS: font atlas has smooth coverage; production shaders render; animation timing and renderer resource recreation");
 fprintf(report,"max_menu_vertices=%llu; buffer_capacity=131072\n",maximum);fclose(report);previewContext->ClearState();ReleaseD3D();previewSwap->Release();previewContext->Release();previewDevice->Release();DestroyWindow(previewWindow);GdiplusShutdown(token);puts("PASS: 45 production-menu layouts, finite vertices, 15-tab navigation; previews saved");
}

