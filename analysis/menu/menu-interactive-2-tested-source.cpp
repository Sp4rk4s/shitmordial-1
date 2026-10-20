// Controlled original-menu preview authorized by the user on 2026-10-20.
// This module is separate from the production loader and gameplay startup.
#include <windows.h>
#include <intrin.h>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include "../../third_party/minhook/include/MinHook.h"
#include "../../shared/mapping.h"
#include "menu_preview_profile.h"

namespace {
constexpr uintptr_t base=0x44340000;
template<class T> T& at(unsigned rva) { return *reinterpret_cast<T*>(base+rva); }
template<class F> F method(void* object,unsigned slot) {
    return reinterpret_cast<F>((*reinterpret_cast<void***>(object))[slot]);
}
using Paint=void(__thiscall*)(void*,unsigned,bool,bool);
Paint original_paint=nullptr;
void *surface=nullptr,*panel_api=nullptr,*engine=nullptr,*client=nullptr,*input=nullptr;
HMODULE self_module=nullptr;
HANDLE log_file=INVALID_HANDLE_VALUE;
volatile LONG failed=0,initialized=0;
bool visible=true,key_down=false;
HWND game_window=nullptr;
WNDPROC old_wndproc=nullptr;
bool left_previous=false,dragging=false;
bool layout_pending=false;
int drag_dx=0,drag_dy=0;
unsigned chosen_panel=0,frames=0;
unsigned char* guard_page=nullptr;
HANDLE embedded_fonts[2]{};

void log(const char* format,...) {
    char line[2048];
    va_list args;va_start(args,format);int n=vsnprintf(line,sizeof(line)-3,format,args);va_end(args);
    if(n<0) return;
    size_t length=strlen(line);line[length++]='\r';line[length++]='\n';
    DWORD written=0;
    if(log_file!=INVALID_HANDLE_VALUE){ WriteFile(log_file,line,static_cast<DWORD>(length),&written,nullptr);FlushFileBuffers(log_file); }
}
int exception_filter(EXCEPTION_POINTERS* e) {
    InterlockedExchange(&failed,1);
    log("PREVIEW STOP exception=%08lX address=%p eax=%08lX ecx=%08lX edx=%08lX esp=%08lX",
        e->ExceptionRecord->ExceptionCode,e->ExceptionRecord->ExceptionAddress,
        e->ContextRecord->Eax,e->ContextRecord->Ecx,e->ContextRecord->Edx,e->ContextRecord->Esp);
    if(e->ExceptionRecord->NumberParameters>=2)
        log("exception operation=%lu operand=%08lX",e->ExceptionRecord->ExceptionInformation[0],e->ExceptionRecord->ExceptionInformation[1]);
    return EXCEPTION_EXECUTE_HANDLER;
}
void* factory(const char* module,const char* name) {
    using Create=void*(__cdecl*)(const char*,int*);
    auto fn=reinterpret_cast<Create>(GetProcAddress(GetModuleHandleA(module),"CreateInterface"));
    return fn?fn(name,nullptr):nullptr;
}
bool preflight() {
    auto* h=reinterpret_cast<const mapping::Handoff*>(0x45751000);
    if(!mapping::valid(*h,GetCurrentProcessId()) || h->state!=mapping::State::initialized ||
       h->checksum!=preview_profile::raw_checksum){ log("REFUSED handoff/profile");return false; }
    for(auto& p:preview_profile::payload)
        if(memcmp(reinterpret_cast<void*>(base+p.rva),p.bytes,p.size)){log("REFUSED payload %X",p.rva);return false;}
    if(at<unsigned char>(0x343592)!=0x7F || at<unsigned>(0x804E5C) || at<unsigned>(0x804E60) ||
       at<void*>(0x4DE15C) || at<void*>(0x4DE174)) {log("REFUSED prior menu state or missing selector repair");return false;}
    for(auto& p:preview_profile::source){
        auto mod=GetModuleHandleA(p.module);
        if(!mod || memcmp(reinterpret_cast<unsigned char*>(mod)+p.rva,p.bytes,p.size)){
            log("REFUSED Source profile %s+%X",p.module,p.rva);return false;
        }
    }
    for(auto& f:preview_profile::fonts)
        if(at<unsigned>(f.rva)||at<unsigned>(f.rva+4)||at<unsigned>(f.rva+8)){log("REFUSED nonempty font %X",f.rva);return false;}
    if(at<unsigned>(0x91BC4B)!=0x1C85CEC8){log("REFUSED unexpected guard pointer");return false;}
    surface=factory("vguimatsurface.dll","VGUI_Surface031");
    panel_api=factory("vgui2.dll","VGUI_Panel009");
    engine=factory("engine.dll","VEngineClient014");
    client=factory("client.dll","VClient018");
    input=factory("inputsystem.dll","InputSystemVersion001");
    if(!surface||!panel_api||!engine||!client||!input){log("REFUSED missing interfaces");return false;}
    log("interfaces surface=%p panel=%p engine=%p client=%p",surface,panel_api,engine,client);
    return true;
}
bool fonts() {
    constexpr unsigned offsets[]={0x4B3D58,0x4B7258}, sizes[]={0x3500,0x23EA4};
    for(unsigned i=0;i<2;++i){
        DWORD count=0;
        if(at<unsigned>(offsets[i])!=0x00000100){log("REFUSED embedded SFNT header");return false;}
        embedded_fonts[i]=AddFontMemResourceEx(reinterpret_cast<void*>(base+offsets[i]),sizes[i],nullptr,&count);
        log("embedded font %u resource=%p count=%lu",i,embedded_fonts[i],count);
        if(!embedded_fonts[i]||!count) return false;
    }
    using Create=unsigned(__thiscall*)(void*);
    using Set=bool(__thiscall*)(void*,unsigned,const char*,int,int,int,int,int,int,int);
    using Size=void(__thiscall*)(void*,unsigned,const wchar_t*,int&,int&);
    auto create=method<Create>(surface,71);auto set=method<Set>(surface,72);auto size=method<Size>(surface,79);
    for(auto& f:preview_profile::fonts){
        unsigned id=create(surface);
        if(!id||!set(surface,id,f.face,f.tall,f.weight,0,0,f.flags,f.lo,f.hi)){
            log("FAILED glyph set %s/%d id=%u",f.face,f.tall,id);return false;
        }
        int width=0,height=0,unused=0;
        size(surface,id,L"A",width,unused);size(surface,id,L"ABCDEF",unused,height);
        if(width<0||width>256||height<=0||height>256){log("FAILED font metrics");return false;}
        at<unsigned>(f.rva)=id;at<int>(f.rva+4)=width;at<int>(f.rva+8)=height;
        log("font %X id=%u %s %d/%d flags=%X metrics=%d,%d",f.rva,id,f.face,f.tall,f.weight,f.flags,width,height);
    }
    return true;
}
void guarded_constructor() {
    int cpu[4]; __cpuid(cpu,1);
    auto eax=static_cast<unsigned>(cpu[0]);
    if((eax&0xFF0)==0xFE0) eax^=0x20;
    auto peb=__readfsdword(0x30);
    auto build=*reinterpret_cast<unsigned short*>(peb+0xAC);
    auto address=reinterpret_cast<uintptr_t>(guard_page);
    *reinterpret_cast<unsigned*>(guard_page+0xB8)=0x59DB1540;
    *reinterpret_cast<unsigned*>(guard_page+0x20)=
        ((eax+(static_cast<unsigned>(cpu[1])&0xFFFFFF)+(static_cast<unsigned>(build)<<7))^address)^0x40DF21BC;
    auto saved=at<unsigned>(0x91BC4B);
    log("constructor begin UI thread=%lu guard=%p CPU=%08X/%08X build=%u",GetCurrentThreadId(),guard_page,cpu[0],cpu[1],build);
    __try {
        at<unsigned>(0x91BC4B)=address;
        reinterpret_cast<void(__cdecl*)()>(base+0x2E04D0)();
    } __finally { at<unsigned>(0x91BC4B)=saved; }
    log("constructor returned guard restored=%08X windows=%08X..%08X",at<unsigned>(0x91BC4B),at<unsigned>(0x804E5C),at<unsigned>(0x804E60));
}
bool initialize_menu() {
    at<void*>(0x4DE15C)=surface;
    at<void*>(0x4DE174)=engine;
    at<void*>(0x4DE160)=input;
    using ButtonName=const char*(__thiscall*)(void*,int);
    const char* insert_name=method<ButtonName>(input,40)(input,72);
    log("verified Source input=%p Insert name=%s",input,insert_name?insert_name:"<null>");
    if(!insert_name||!insert_name[0])return false;
    auto mod=reinterpret_cast<uintptr_t>(GetModuleHandleA("client.dll"));
    at<void*>(0x4DE164)=*reinterpret_cast<void**>(mod+preview_profile::timing_slot_rva);
    if(!at<void*>(0x4DE164)){log("REFUSED timing pointer null");return false;}
    if(!fonts()) return false;
    guarded_constructor();
    auto begin=at<unsigned>(0x804E5C), end=at<unsigned>(0x804E60);
    if(!begin || end-begin!=16){log("FAILED constructed window count");return false;}
    bool primary=false;
    for(unsigned i=0;i<4;++i){auto object=reinterpret_cast<unsigned*>(begin)[i];log("window[%u]=%08X",i,object);primary|=(object==base+0x4B33B0);}
    if(!primary){log("FAILED primary window not in constructed tree");return false;}
    InterlockedExchange(&initialized,1);
    log("MENU CONSTRUCTED. Initially visible; Insert toggles. Gameplay startup not executed.");
    return true;
}
bool layout_menu() {
    auto* window=reinterpret_cast<unsigned char*>(base+0x4B33B0);
    auto get=[window](unsigned offset)->unsigned&{return *reinterpret_cast<unsigned*>(window+offset);};
    auto begin=get(0xD4),end=get(0xD8),selected=get(0xE0);
    if(!begin||end<begin||(end-begin)%20||selected>=(end-begin)/20||(end-begin)/20>32){log("REFUSED tab layout vector");return false;}
    get(0x5C)=get(0x3C);get(0x60)=get(0x40)+get(0xD0);
    get(0x64)=get(0x44);get(0x68)=get(0x48)-get(0xD0)-(begin==end?8:55);
    for(unsigned a=begin,i=0;a<end;a+=20,++i){*reinterpret_cast<float*>(a+4)=i==selected?1.0f:0.0f;*reinterpret_cast<float*>(a+8)=i==selected?1.0f:0.0f;}
    auto* tab=*reinterpret_cast<unsigned char**>(begin+20*selected);
    reinterpret_cast<void(__thiscall*)(void*,const int*)>(base+0x34DB30)(tab,reinterpret_cast<int*>(window+0x5C));
    auto groups=*reinterpret_cast<unsigned*>(tab+0x84),groups_end=*reinterpret_cast<unsigned*>(tab+0x88),group_index=*reinterpret_cast<unsigned*>(tab+0x80);
    if(!groups||groups_end<groups||(groups_end-groups)%20||group_index>=(groups_end-groups)/20){log("REFUSED group layout vector");return false;}
    for(unsigned a=groups,i=0;a<groups_end;a+=20,++i){*reinterpret_cast<float*>(a+4)=i==group_index?1.0f:0.0f;*reinterpret_cast<float*>(a+8)=i==group_index?1.0f:0.0f;}
    auto* group=*reinterpret_cast<unsigned char**>(groups+20*group_index);
    int rectangle[]={static_cast<int>(get(0x5C))+130,static_cast<int>(get(0x60)),static_cast<int>(get(0x64))-130,static_cast<int>(get(0x68))};
    reinterpret_cast<void(__thiscall*)(void*,const int*)>(base+0x34B860)(group,rectangle);
    // Geometry refresh bypasses the partially recovered scrollbar updater.
    group[0x68]=0;
    log("group initial layout begin (input blocked), group=%p input=%p",group,input);
    reinterpret_cast<void(__thiscall*)(void*,bool)>(base+0x34A4B0)(group,true);
    log("group initial layout returned columns=%u",*reinterpret_cast<unsigned*>(group+0x64));
    return true;
}
bool hit(int px,int py,int x,int y,int w,int h){return w>0&&h>0&&px>=x&&py>=y&&px<x+w&&py<y+h;}
LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM wp,LPARAM lp){
    if(visible&&!failed){
        POINT cursor{};GetCursorPos(&cursor);ScreenToClient(window,&cursor);
        bool inside=hit(cursor.x,cursor.y,at<int>(0x4B33EC),at<int>(0x4B33F0),at<int>(0x4B33F4),at<int>(0x4B33F8));
        if((inside||dragging)&&message>=WM_MOUSEFIRST&&message<=WM_MOUSELAST)
            return 0;
    }
    return CallWindowProcA(old_wndproc,window,message,wp,lp);
}
void update_leaf_inputs(unsigned char* object,int depth,int cursor_y,int clip_y,int clip_height) {
    if(depth>8)return;
    auto table=*reinterpret_cast<uintptr_t**>(object);
    auto input_fn=table[1];
    if(input_fn==base+0x337FE0){
        auto begin=*reinterpret_cast<unsigned*>(object+0x124),end=*reinterpret_cast<unsigned*>(object+0x128);
        if(end<begin||(end-begin)%4||end-begin>4096)return;
        for(auto a=begin;a<end;a+=4)update_leaf_inputs(*reinterpret_cast<unsigned char**>(a),depth+1,cursor_y,clip_y,clip_height);
        return;
    }
    // UI values only: empty observer/attachment lists prevent feature/action dispatch.
    if(input_fn!=base+0x32EF80&&input_fn!=base+0x343ED0&&input_fn!=base+0x32E410)return;
    if(*reinterpret_cast<unsigned*>(object+0xCC)!=*reinterpret_cast<unsigned*>(object+0xD0) ||
       *reinterpret_cast<unsigned*>(object+0x124)!=*reinterpret_cast<unsigned*>(object+0x128))return;
    int x=*reinterpret_cast<int*>(object+0xB0),y=*reinterpret_cast<int*>(object+0xB4);
    int w=*reinterpret_cast<int*>(object+0xB8),h=*reinterpret_cast<int*>(object+0xBC);
    if(w<=0||h<=0||x<0||y<clip_y||y+h>clip_y+clip_height)return;
    using Allowed=bool(__thiscall*)(void*);
    if(!reinterpret_cast<Allowed>(table[7])(object))return;
    bool blocked=dragging||cursor_y<clip_y||cursor_y>=clip_y+clip_height;
    unsigned before=input_fn==base+0x32EF80?object[0x138]:*reinterpret_cast<unsigned*>(object+0x130);
    reinterpret_cast<void(__thiscall*)(void*,bool)>(input_fn)(object,blocked);
    unsigned after=input_fn==base+0x32EF80?object[0x138]:*reinterpret_cast<unsigned*>(object+0x130);
    if(before!=after){layout_pending=true;log("UI value changed object=%p inputRVA=%X before=%u after=%u",object,static_cast<unsigned>(input_fn-base),before,after);}
}
void navigation_tick() {
    if(!game_window||GetForegroundWindow()!=game_window)return;
    if(layout_pending){layout_pending=false;if(!layout_menu()){InterlockedExchange(&failed,1);return;}}
    POINT cursor{};GetCursorPos(&cursor);ScreenToClient(game_window,&cursor);
    at<int>(0x807F00)=cursor.x;at<int>(0x807F04)=cursor.y;
    bool left=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
    bool pressed=left&&!left_previous;
    unsigned key_state=left?(left_previous?2:1):(left_previous?3:0);
    at<unsigned>(0x807F08+107*4)=key_state;
    left_previous=left;
    int& x=at<int>(0x4B33EC);int& y=at<int>(0x4B33F0);
    int width=at<int>(0x4B33F4),height=at<int>(0x4B33F8),title=at<int>(0x4B3480);
    bool relayout=false,consumed=false;
    if(pressed&&hit(cursor.x,cursor.y,x,y,width,title)){dragging=true;drag_dx=cursor.x-x;drag_dy=cursor.y-y;consumed=true;log("drag begin x=%d y=%d",x,y);}
    if(dragging){
        consumed=true;
        if(left){
            int sw=0,sh=0;using Screen=void(__thiscall*)(void*,int&,int&);method<Screen>(engine,5)(engine,sw,sh);
            int nx=cursor.x-drag_dx,ny=cursor.y-drag_dy;
            if(nx<0)nx=0;if(ny<0)ny=0;if(nx>sw-width)nx=sw-width;if(ny>sh-height)ny=sh-height;
            relayout=(nx!=x||ny!=y);x=nx;y=ny;
        }else{dragging=false;log("drag end x=%d y=%d",x,y);}
    }
    auto begin=at<unsigned>(0x4B3484),end=at<unsigned>(0x4B3488);
    unsigned count=(end-begin)/20;
    if(pressed&&!consumed&&count&&count<32){
        int tabs_x=x+width/2-static_cast<int>(count)*55/2;
        if(hit(cursor.x,cursor.y,tabs_x,y+height-55,static_cast<int>(count)*55,55)){
            unsigned index=static_cast<unsigned>((cursor.x-tabs_x)/55);
            at<unsigned>(0x4B3490)=index;relayout=true;consumed=true;log("selected tab=%u",index);
        }
    }
    auto* tab=*reinterpret_cast<unsigned char**>(begin+20*at<unsigned>(0x4B3490));
    auto groups=*reinterpret_cast<unsigned*>(tab+0x84),groups_end=*reinterpret_cast<unsigned*>(tab+0x88);
    unsigned group_count=(groups_end-groups)/20;
    if(pressed&&!consumed&&hit(cursor.x,cursor.y,x,y+title+100,130,height-title-155)){
        int row=static_cast<int>(at<int>(0x580F48)*at<float>(0x416DCC))+6;
        unsigned index=static_cast<unsigned>((cursor.y-y-title-100)/row);
        if(index<group_count){*reinterpret_cast<unsigned*>(tab+0x80)=index;relayout=true;consumed=true;log("selected subtab=%u",index);}
    }
    if(relayout&&!layout_menu()){InterlockedExchange(&failed,1);return;}
    if(consumed)at<unsigned>(0x807F08+107*4)=0;
    auto* group=*reinterpret_cast<unsigned char**>(groups+20*(*reinterpret_cast<unsigned*>(tab+0x80)));
    auto controls=*reinterpret_cast<unsigned*>(group+0x308),controls_end=*reinterpret_cast<unsigned*>(group+0x30C);
    if(controls_end<controls||(controls_end-controls)%8||controls_end-controls>8192){InterlockedExchange(&failed,1);log("REFUSED control vector");return;}
    for(auto a=controls;a<controls_end;a+=8)
        update_leaf_inputs(*reinterpret_cast<unsigned char**>(a),0,cursor.y,y+title,height-title-55);
}
void draw_preview(unsigned panel) {
    using Push=void(__thiscall*)(void*,unsigned,bool);
    using Pop=void(__thiscall*)(void*,unsigned);
    using Screen=void(__thiscall*)(void*,int&,int&);
    auto push=method<Push>(surface,12);auto pop=method<Pop>(surface,13);
    auto saved_alpha=*reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(surface)+0x14);
    bool pushed=false;
    __try {
        push(surface,panel,false);pushed=true;
        int w=0,h=0;method<Screen>(engine,5)(engine,w,h);
        if(w<=0||h<=0||w>16384||h>16384){log("FAILED dimensions %d,%d",w,h);InterlockedExchange(&failed,1);return;}
        at<int>(0x8087AC)=w;at<int>(0x8087B0)=h;
        at<float>(0x4B33B0+0x94)=1.0f;
        at<unsigned char>(0x4B33B0+0x99)=1;
        reinterpret_cast<void(__cdecl*)()>(base+0x3609A0)();
        reinterpret_cast<void(__cdecl*)()>(base+0x3609E0)();
        reinterpret_cast<void(__thiscall*)(void*)>(base+0x351AB0)(reinterpret_cast<void*>(base+0x4B33B0));
        ++frames;
        if(frames==1 || frames==300) log("DRAW RETURNED frames=%u panel=%X dimensions=%d,%d",frames,panel,w,h);
    } __finally {
        *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(surface)+0x14)=saved_alpha;
        if(pushed) pop(surface,panel);
    }
}
void __fastcall paint_hook(void* object,void*,unsigned panel,bool force,bool allow) {
    original_paint(object,panel,force,allow);
    if(failed) return;
    __try {
        if(!chosen_panel){
            using Name=const char*(__thiscall*)(void*,unsigned);
            const char* name=method<Name>(panel_api,36)(panel_api,panel);
            if(name && !strcmp(name,"MatSystemTopPanel")){chosen_panel=panel;log("selected panel=%X name=%s UI thread=%lu",panel,name,GetCurrentThreadId());}
        }
        if(panel!=chosen_panel||!chosen_panel) return;
        if(!initialized){
            if(!initialize_menu()||!layout_menu()){InterlockedExchange(&failed,1);return;}
        }
        bool down=(GetAsyncKeyState(VK_INSERT)&0x8000)!=0;
        if(down&&!key_down){visible=!visible;log("visibility=%d",visible);}
        key_down=down;
        if(visible){navigation_tick();if(!failed)draw_preview(panel);}
        else {dragging=false;left_previous=false;at<unsigned>(0x807F08+107*4)=0;}
    } __except(exception_filter(GetExceptionInformation())) {}
}
DWORD WINAPI worker(void*) {
    char path[MAX_PATH];GetModuleFileNameA(self_module,path,MAX_PATH);
    char* slash=strrchr(path,'\\');if(slash) strcpy_s(slash+1,MAX_PATH-(slash+1-path),"primordial_menu_preview.log");
    log_file=CreateFileA(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    log("PREVIEW LOADED pid=%lu tick=%llu",GetCurrentProcessId(),GetTickCount64());
    __try {
        if(!preflight()) return 1;
        EnumWindows([](HWND window,LPARAM)->BOOL{
            DWORD pid=0;GetWindowThreadProcessId(window,&pid);char title[256];GetWindowTextA(window,title,sizeof(title));
            if(pid==GetCurrentProcessId()&&strstr(title,"Counter-Strike: Global Offensive")){game_window=window;return FALSE;}return TRUE;
        },0);
        if(!game_window){log("REFUSED game window absent");return 1;}
        guard_page=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        if(!guard_page){log("FAILED guard allocation");return 1;}
        auto status=MH_Initialize();
        if(status!=MH_OK && status!=MH_ERROR_ALREADY_INITIALIZED){log("FAILED MH init %d",status);return 1;}
        auto target=(*reinterpret_cast<void***>(panel_api))[41];
        status=MH_CreateHook(target,reinterpret_cast<void*>(&paint_hook),reinterpret_cast<void**>(&original_paint));
        if(status!=MH_OK){log("FAILED MH create %d",status);return 1;}
        HMODULE pinned=nullptr;
        if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCSTR>(&paint_hook),&pinned)){
            MH_RemoveHook(target);log("FAILED pin");return 1;
        }
        status=MH_EnableHook(target);
        log("paint hook status=%d target=%p; waiting for MatSystemTopPanel",status,target);
        if(status==MH_OK){
            old_wndproc=reinterpret_cast<WNDPROC>(GetWindowLongPtrA(game_window,GWLP_WNDPROC));
            SetLastError(0);auto previous=SetWindowLongPtrA(game_window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&window_proc));
            if(!previous){InterlockedExchange(&failed,1);log("FAILED window input hook Win32=%lu",GetLastError());}
        }
    } __except(exception_filter(GetExceptionInformation())) {return 1;}
    return 0;
}
}
BOOL WINAPI DllMain(HMODULE module,DWORD reason,void*) {
    if(reason==DLL_PROCESS_ATTACH){self_module=module;DisableThreadLibraryCalls(module);auto thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr);if(thread)CloseHandle(thread);else return FALSE;}
    return TRUE;
}
