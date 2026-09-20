                                                                                            
namespace SmoothFont {
constexpr unsigned Width=1024,Height=512;
struct GlyphInfo{float u0=0,v0=0,u1=0,v1=0;float advance=0;};
GlyphInfo glyphs[95]{};std::vector<unsigned> pixels;bool ready=false;
ID3D11ShaderResourceView* texture=nullptr;ID3D11SamplerState* sampler=nullptr;
bool Build(){
 if(ready)return true;
 HDC dc=CreateCompatibleDC(nullptr);if(!dc)return false;
 BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=Width;info.bmiHeader.biHeight=-(LONG)Height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
 void*bits=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
 HFONT font=CreateFontW(-48,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
 if(!bitmap||!bits||!font){if(font)DeleteObject(font);if(bitmap)DeleteObject(bitmap);DeleteDC(dc);return false;}
 auto oldBitmap=SelectObject(dc,bitmap);auto oldFont=SelectObject(dc,font);memset(bits,0,Width*Height*4);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));TEXTMETRICW metrics{};GetTextMetricsW(dc,&metrics);

#ifdef DIDE_FONT_TEST
 fprintf(stderr,"FONT_METRICS height=%ld ascent=%ld max_width=%ld\n",metrics.tmHeight,metrics.tmAscent,metrics.tmMaxCharWidth);
#endif
 bool ok=metrics.tmHeight>0&&metrics.tmHeight<=72;
 for(unsigned c=32;c<127&&ok;c++){
  unsigned cell=c-32,x=(cell%16)*64+4,y=(cell/16)*80+4;wchar_t ch=(wchar_t)c;SIZE size{};
  ok=GetTextExtentPoint32W(dc,&ch,1,&size)&&size.cx>0&&size.cx<=56&&TextOutW(dc,x,y,&ch,1);
  glyphs[cell]={x/(float)Width,y/(float)Height,(x+size.cx)/(float)Width,(y+metrics.tmHeight)/(float)Height,std::min(6.f,size.cx*14.f/metrics.tmHeight)};
 }
 GdiFlush();if(ok){pixels.resize(Width*Height);auto source=(unsigned*)bits;unsigned partial=0,ink=0;for(unsigned i=0;i<Width*Height;i++){unsigned a=source[i]&255;pixels[i]=0x00FFFFFFu|(a<<24);if(a&&a<255)partial++;if(a)ink++;}
#ifdef DIDE_FONT_TEST
 fprintf(stderr,"FONT_COVERAGE partial=%u ink=%u\n",partial,ink);
#endif
 ok=partial>100&&ink>1000;}
                                                    
 if(ok)for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++)pixels[y*Width+x]=0xFFFFFFFF;
 SelectObject(dc,oldFont);SelectObject(dc,oldBitmap);DeleteObject(font);DeleteObject(bitmap);DeleteDC(dc);ready=ok;return ok;
}
void Release(){if(texture)texture->Release();if(sampler)sampler->Release();texture=nullptr;sampler=nullptr;}
bool Ensure(ID3D11Device*d){
 if(texture&&sampler)return true;if(!Build())return false;Release();
 D3D11_TEXTURE2D_DESC desc{};desc.Width=Width;desc.Height=Height;desc.MipLevels=1;desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 D3D11_SUBRESOURCE_DATA data{pixels.data(),Width*4,0};ID3D11Texture2D*atlas=nullptr;HRESULT hr=d->CreateTexture2D(&desc,&data,&atlas);if(FAILED(hr))return false;hr=d->CreateShaderResourceView(atlas,nullptr,&texture);atlas->Release();if(FAILED(hr)){Release();return false;}
 D3D11_SAMPLER_DESC sample{};sample.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sample.AddressU=sample.AddressV=sample.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sample.MaxLOD=D3D11_FLOAT32_MAX;
 if(FAILED(d->CreateSamplerState(&sample,&sampler))){Release();return false;}return true;
}
float WidthOf(const char*text,float scale){if(!Build())return 0;float width=0;for(;*text;text++){unsigned c=(unsigned char)*text;if(c<32||c>=127)c='?';width+=glyphs[c-32].advance*scale;}return width;}
void Text(std::vector<D3DVertex>&v,float x,float y,const char*text,float scale,float w,float h,float r,float g,float b){
 if(!Build())return;
 for(;*text;text++){unsigned c=(unsigned char)*text;if(c<32||c>=127)c='?';auto gl=glyphs[c-32];float width=gl.advance*scale;if(c!=' '){
  auto add=[&](float px,float py,float u,float vv){v.push_back({px/w*2-1,1-py/h*2,r,g,b,1,u,vv});};
  add(x,y-2*scale,gl.u0,gl.v0);add(x+width,y-2*scale,gl.u1,gl.v0);add(x+width,y+12*scale,gl.u1,gl.v1);add(x,y-2*scale,gl.u0,gl.v0);add(x+width,y+12*scale,gl.u1,gl.v1);add(x,y+12*scale,gl.u0,gl.v1);
 }x+=width;}
}
}
