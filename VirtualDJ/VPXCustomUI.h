#pragma once
// Native Win32 custom editor for VirtualDJ 8. Coordinates based on 1536x1024 reference.
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include <string>

struct VPXEditor {
 HWND hwnd=nullptr;
 float* params[10]={};
 int* bypass=nullptr;
 float* meter=nullptr;
 int selected=-1;
 static constexpr int W=1536,H=1024;
 struct Knob { int x,y,r,p; const char* name; };
 std::vector<Knob> knobs={
 {113,509,52,0,"DRIVE"},{226,499,27,1,"TONE"},{307,499,27,2,"MIX"},
 {445,493,38,3,"LOW"},{581,493,38,4,"LOW MID"},{718,493,38,5,"HIGH MID"},{854,493,38,6,"HIGH"},
 {1422,494,45,7,"CEILING"},{75,822,27,8,"INPUT"},{182,822,27,9,"OUTPUT"}
 };
 static COLORREF col(int r,int g,int b){return RGB(r,g,b);}
 static void rect(HDC dc,int x,int y,int w,int h,COLORREF c) {
  RECT r={x,y,x+w,y+h};HBRUSH b=CreateSolidBrush(c);FillRect(dc,&r,b);DeleteObject(b);
 }
 static void frame(HDC dc,int x,int y,int w,int h,COLORREF c) {
  HPEN p=CreatePen(PS_SOLID,1,c),*old=(HPEN)SelectObject(dc,p);HGDIOBJ ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
  Rectangle(dc,x,y,x+w,y+h);SelectObject(dc,ob);SelectObject(dc,old);DeleteObject(p);
 }
 static void line(HDC dc,int x,int y,int x2,int y2,COLORREF c,int width=1){
  HPEN p=CreatePen(PS_SOLID,width,c),*old=(HPEN)SelectObject(dc,p);MoveToEx(dc,x,y,nullptr);LineTo(dc,x2,y2);SelectObject(dc,old);DeleteObject(p);
 }
 static void label(HDC dc,int x,int y,const char* s,int size=15,COLORREF c=RGB(226,239,251),bool bold=false){
  HFONT f=CreateFontA(-size,0,0,0,bold?FW_BOLD:FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,"Segoe UI");
  HFONT old=(HFONT)SelectObject(dc,f);SetTextColor(dc,c);SetBkMode(dc,TRANSPARENT);TextOutA(dc,x,y,s,(int)strlen(s));SelectObject(dc,old);DeleteObject(f);
 }
 static void panel(HDC dc,int x,int y,int w,int h,const char* name){
  rect(dc,x,y,w,h,col(7,16,27));frame(dc,x,y,w,h,col(39,90,129));rect(dc,x+3,y+3,w-6,36,col(14,29,44));
  label(dc,x+14,y+11,name,16,col(239,247,255),true);
 }
 void knob(HDC dc,int x,int y,int r,float val,COLORREF glow){
  HBRUSH b=CreateSolidBrush(col(29,44,58));HBRUSH old=(HBRUSH)SelectObject(dc,b);
  HPEN pen=CreatePen(PS_SOLID,3,glow),op=(HPEN)SelectObject(dc,pen);
  Ellipse(dc,x-r-6,y-r-6,x+r+6,y+r+6);
  SelectObject(dc,op);DeleteObject(pen);
  HBRUSH face=CreateSolidBrush(col(66,80,95));SelectObject(dc,face);
  Ellipse(dc,x-r,y-r,x+r,y+r);
  SelectObject(dc,old);DeleteObject(face);DeleteObject(b);
  const float a=(val*270-225)*3.14159265f/180;
  line(dc,x,y,x+(int)(std::sin(a)*(r-10)),y-(int)(std::cos(a)*(r-10)),col(235,248,255),3);
 }
 void draw(HDC dc){
  SetBkMode(dc,TRANSPARENT);rect(dc,0,0,W,H,col(5,12,22));
  rect(dc,8,7,1520,76,col(12,26,43));frame(dc,8,7,1520,76,col(49,102,142));
  label(dc,34,8,"VPX",62,col(234,242,255),true);label(dc,211,16,"PRO",45,col(16,163,252),true);
  label(dc,213,61,"MASTERING PROCESSOR",13);label(dc,409,48,"MULTIBAND  /  DYNAMICS  /  LIMITER  /  SATURATION  /  STEREO  /  MASTERING",13,col(160,190,216));
  rect(dc,849,24,300,42,col(8,18,30));label(dc,871,37,"001 - LIVE MASTER (Default)",17);
  rect(dc,1410,22,109,45,*bypass?col(76,23,30):col(9,59,103));label(dc,1422,34,*bypass?"BYPASSED":"BYPASS",18);
  panel(dc,17,91,123,276,"INPUT");panel(dc,148,91,961,276,"SPECTRUM ANALYZER");
  panel(dc,1116,91,267,276,"STEREO FIELD");panel(dc,1391,91,127,276,"OUTPUT");
  for(int i=0;i<2;++i){int x=44+i*38;rect(dc,x,157,20,170,col(17,36,50));float m=meter?std::min(1.f,std::max(0.f,*meter)):0.55f;int h=(int)(170*m);
   for(int j=0;j<h;j+=6)rect(dc,x,327-j,19,5,j>133?col(255,64,49):j>100?col(251,196,20):col(28,222,83));}
  for(int i=0;i<2;++i){int x=1420+i*39;rect(dc,x,157,20,170,col(17,36,50));float m=meter?std::min(1.f,std::max(0.f,*meter)):0.55f;int h=(int)(170*m*.92f);
   for(int j=0;j<h;j+=6)rect(dc,x,327-j,19,5,j>133?col(255,64,49):j>100?col(251,196,20):col(28,222,83));}
  rect(dc,169,142,850,188,col(5,19,33));
  for(int i=0;i<28;++i){int x=169+i*31;line(dc,x,142,x,330,col(17,48,71));}
  for(int i=0;i<9;++i){int y=142+i*23;line(dc,169,y,1019,y,col(17,48,71));}
  for(int i=0;i<180;++i){int x=175+i*4;float v=.24f+.3f*std::sin(i*.2f)*std::sin(i*.71f)+.15f*std::sin(i*.09f);
   float m=meter?std::min(1.f,std::max(0.f,*meter)):0.5f;int hh=(int)(std::max(.03f,v)*m*280);
   rect(dc,x,325-hh,2,hh,col(9,155+(i%4)*18,242));}
  label(dc,185,338,"20        50          100           200          500            1k            2k             5k             10k           20k",13,col(158,193,221));
  for(int i=0;i<9;++i){int x=1144+i*25;line(dc,1245,146,x,226,col(12,112,201));line(dc,1245,302,x,226,col(12,112,201));}
  line(dc,1143,225,1354,225,col(14,185,255),2);line(dc,1245,140,1245,309,col(14,185,255),2);
  panel(dc,10,380,345,337,"HARMONIC DRIVE");panel(dc,363,380,581,337,"MULTIBAND COMPRESSOR");
  panel(dc,953,380,352,337,"EQ PARAMETRIC");panel(dc,1313,380,214,337,"LIMITER / MAXIMIZER");
  panel(dc,10,726,230,206,"GATE / EXPANDER");panel(dc,247,726,207,206,"TRANSIENT DESIGNER");
  panel(dc,463,726,297,206,"STEREO PROCESSING");panel(dc,767,726,190,206,"EXCITER");
  panel(dc,964,726,314,206,"REVERB");panel(dc,1285,726,243,206,"DELAY");
  panel(dc,10,940,300,74,"OVERSAMPLING");panel(dc,319,940,776,74,"PRESETS");
  panel(dc,1103,940,192,74,"OUTPUT METERING");panel(dc,1302,940,226,74,"DITHER");
  const COLORREF colors[]={col(22,161,255),col(27,158,255),col(27,158,255),col(251,75,67),col(244,183,34),col(36,222,90),col(31,153,255),col(255,69,62),col(30,163,252),col(30,163,252)};
  for(const auto& k:knobs){float v=params[k.p]?*params[k.p]:.5f;knob(dc,k.x,k.y,k.r,v,colors[k.p]);
   char buf[50];std::snprintf(buf,sizeof(buf),"%s  %.0f%%",k.name,v*100);label(dc,k.x-k.r,k.y+k.r+17,buf,13);}
  for(int i=0;i<4;++i){char t[30];std::snprintf(t,sizeof(t),"BAND %d",i+1);label(dc,408+i*135,432,t,15);}
  for(int i=0;i<6;++i){int x=973+i*51;line(dc,x,463,x,635,col(28,57,77));}
  for(int i=0;i<6;++i){int x=978+i*53;int y=545+(i%3-1)*24;knob(dc,x,y,11,.5f,col(25,168,255));}
  label(dc,987,659,"20       100        500        2k        8k       20k",13);
  label(dc,49,962,"OFF       2x        4x        8x       16x",15);
  label(dc,410,965,"001 - Live Master (Default)",18);
  label(dc,1118,967,"PEAK    RMS    LUFS",15);
  label(dc,1331,967,"16 bit     24 bit    OFF",15);
  label(dc,40,892,"ATTACK  /  RELEASE",14);label(dc,271,891,"ATTACK  /  SUSTAIN",14);
  label(dc,487,891,"MONO     STEREO     M/S",14);label(dc,793,890,"SOFT     MED     HARD",14);
  label(dc,994,892,"ROOM      HALL      PLATE",14);label(dc,1314,891,"TIME    FEEDBACK   MIX",14);
  label(dc,1390,651,"TRUE PEAK",15);label(dc,32,658,"TUBE     TAPE     SOLID     CLASSIC",15);
 }
 static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){
  auto* self=(VPXEditor*)GetWindowLongPtrA(h,GWLP_USERDATA);
  if(m==WM_NCCREATE){auto* cs=(CREATESTRUCTA*)l;SetWindowLongPtrA(h,GWLP_USERDATA,(LONG_PTR)cs->lpCreateParams);return TRUE;}
  if(!self)return DefWindowProcA(h,m,w,l);
  if(m==WM_ERASEBKGND)return 1;
  if(m==WM_TIMER){InvalidateRect(h,nullptr,FALSE);return 0;}
  if(m==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);
    HDC mem=CreateCompatibleDC(dc);HBITMAP bm=CreateCompatibleBitmap(dc,r.right,r.bottom);HBITMAP old=(HBITMAP)SelectObject(mem,bm);
    SetMapMode(mem,MM_ANISOTROPIC);SetWindowExtEx(mem,W,H,nullptr);SetViewportExtEx(mem,r.right,r.bottom,nullptr);
    self->draw(mem);BitBlt(dc,0,0,r.right,r.bottom,mem,0,0,SRCCOPY);
    SelectObject(mem,old);DeleteObject(bm);DeleteDC(mem);EndPaint(h,&ps);return 0;}
  if(m==WM_LBUTTONDOWN||m==WM_MOUSEMOVE){if(m==WM_MOUSEMOVE&&!(w&MK_LBUTTON))return 0;
   RECT r;GetClientRect(h,&r);int x=LOWORD(l)*W/std::max(1L,r.right),y=HIWORD(l)*H/std::max(1L,r.bottom);
   if(m==WM_LBUTTONDOWN){self->selected=-1;
    if(x>1400&&y<83){*self->bypass=!*self->bypass;InvalidateRect(h,nullptr,FALSE);return 0;}
    for(int i=0;i<(int)self->knobs.size();++i){auto& k=self->knobs[i];if(abs(x-k.x)<k.r+18&&abs(y-k.y)<k.r+18){self->selected=i;SetCapture(h);break;}}
   }
   if(self->selected>=0){auto& k=self->knobs[self->selected];float v=std::clamp((float)(k.y+75-y)/150.f,0.f,1.f);*self->params[k.p]=v;InvalidateRect(h,nullptr,FALSE);}
   return 0;}
  if(m==WM_LBUTTONUP){self->selected=-1;ReleaseCapture();return 0;}
  if(m==WM_DESTROY){KillTimer(h,1);self->hwnd=nullptr;return 0;}
  return DefWindowProcA(h,m,w,l);
 }
 HWND open(HINSTANCE inst){
  if(hwnd&&IsWindow(hwnd))return hwnd;
  static bool registered=false;
  if(!registered){WNDCLASSEXA wc={sizeof(wc)};wc.lpfnWndProc=proc;wc.hInstance=inst;wc.lpszClassName="VPXProMasteringEditorV4";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassExA(&wc);registered=true;}
  int sw=GetSystemMetrics(SM_CXSCREEN),sh=GetSystemMetrics(SM_CYSCREEN);
  float scale=std::min(1.f,std::min((sw-80.f)/W,(sh-100.f)/H));
  int ww=(int)(W*scale),hh=(int)(H*scale);
  hwnd=CreateWindowExA(0,"VPXProMasteringEditorV4","VPX PRO MASTERING PROCESSOR V4",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
   (sw-ww)/2,(sh-hh)/2,ww,hh,nullptr,nullptr,inst,this);
  if(hwnd){SetTimer(hwnd,1,70,nullptr);ShowWindow(hwnd,SW_SHOW);}
  return hwnd;
 }
};
