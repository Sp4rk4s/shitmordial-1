// Original-menu UI integrated after the production payload entry returns.
// Gameplay startup and unrecovered action callbacks remain outside this path.
#include <windows.h>
#include <intrin.h>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <initializer_list>
#include "../../third_party/minhook/include/MinHook.h"
#include "../../shared/mapping.h"
#include "menu_profile.h"
#include "menu_ui.h"
#include "menu_input.h"

extern "C" { __declspec(dllexport) volatile LONG primordial_menu_state=menu_ui::dormant; }

namespace {
constexpr uintptr_t base=0x44340000;
template<class T> T& at(unsigned rva) { return *reinterpret_cast<T*>(base+rva); }
template<class F> F method(void* object,unsigned slot) {
    return reinterpret_cast<F>((*reinterpret_cast<void***>(object))[slot]);
}
using Paint=void(__thiscall*)(void*,unsigned,bool,bool);
Paint original_paint=nullptr;
void *surface=nullptr,*panel_api=nullptr,*engine=nullptr,*client=nullptr,*input=nullptr;
void* materials=nullptr;
HMODULE self_module=nullptr;
HANDLE log_file=INVALID_HANDLE_VALUE;
volatile LONG failed=0,initialized=0;
bool visible=true;
HWND game_window=nullptr;
WNDPROC old_wndproc=nullptr;
volatile LONG wheel_units=0;
unsigned virtual_keys[107]{};
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
    InterlockedExchange(&primordial_menu_state,menu_ui::failed);
    log("MENU STOP exception=%08lX address=%p eax=%08lX ecx=%08lX edx=%08lX esp=%08lX",
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
       h->checksum!=menu_profile::raw_checksum){ log("REFUSED handoff/profile");return false; }
    for(auto& p:menu_profile::payload)
        if(memcmp(reinterpret_cast<void*>(base+p.rva),p.bytes,p.size)){log("REFUSED payload %X",p.rva);return false;}
    if(at<unsigned char>(0x343592)!=0x7F || at<unsigned>(0x804E5C) || at<unsigned>(0x804E60) ||
       at<void*>(0x4DE15C) || at<void*>(0x4DE174)) {log("REFUSED prior menu state or missing selector repair");return false;}
    if(!menu_profile::verify_module_files()){log("REFUSED installed Source identity");return false;}
    for(auto& p:menu_profile::source){
        const char* name=menu_profile::modules[p.module_index].name;
        auto mod=GetModuleHandleA(name);
        auto* object=factory(name,p.version);
        auto* expected=reinterpret_cast<unsigned char*>(mod)+p.rva;
        if(!mod || !object || (*reinterpret_cast<void***>(object))[p.slot]!=expected ||
           !menu_profile::matches(p,expected,p.size,reinterpret_cast<uintptr_t>(mod))){
            log("REFUSED Source profile %s+%X slot=%u",name,p.rva,p.slot);return false;
        }
    }
    for(auto& f:menu_profile::fonts)
        if(at<unsigned>(f.rva)||at<unsigned>(f.rva+4)||at<unsigned>(f.rva+8)){log("REFUSED nonempty font %X",f.rva);return false;}
    if(at<unsigned>(0x91BC4B)!=0x1C85CEC8){log("REFUSED unexpected guard pointer");return false;}
    // Logo texture ID cached by 34CF20; the dump captured the original process's ID 0x132.
    if(at<unsigned>(0x4B3D50)!=0x132){log("REFUSED unexpected logo texture id %X",at<unsigned>(0x4B3D50));return false;}
    surface=factory("vguimatsurface.dll","VGUI_Surface031");
    panel_api=factory("vgui2.dll","VGUI_Panel009");
    engine=factory("engine.dll","VEngineClient014");
    client=factory("client.dll","VClient018");
    input=factory("inputsystem.dll","InputSystemVersion001");
    materials=factory("materialsystem.dll","VMaterialSystem080");
    if(!materials||at<void*>(0x4DE19C)){log("REFUSED native frame material binding");return false;}
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
    for(auto& f:menu_profile::fonts){
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
        // Original one-time configuration directory/vector initialization.
        // Its recovered cdecl path reads the same two scoped CPU-guard fields
        // as the menu constructor. Real imports handle actual filesystem I/O.
        reinterpret_cast<void(__cdecl*)()>(base+0x2CAB40)();
        log("original config initializer returned");
        // Verified original initializer assigns primordial/scripts and creates
        // its include directory. No other image globals change on the retained
        // complete path; no Lua VM or gameplay callback is started here.
        reinterpret_cast<void(__cdecl*)()>(base+0x162FA0)();
        log("original script-directory initializer returned");
        reinterpret_cast<void(__cdecl*)()>(base+0x2E04D0)();
    } __finally { at<unsigned>(0x91BC4B)=saved; }
    log("constructor returned guard restored=%08X windows=%08X..%08X",at<unsigned>(0x91BC4B),at<unsigned>(0x804E5C),at<unsigned>(0x804E60));
}
// Primary window title: MSVC wstring at RVA4B33B4 whose chars draw351AB0 XORs with
// the key byte at +18h. Rewritten in place only if it decodes to the original title.
void rename_title(const wchar_t* title) {
    auto& size=at<unsigned>(0x4B33C4);auto capacity=at<unsigned>(0x4B33C8);auto key=at<unsigned char>(0x4B33CC);
    auto* chars=capacity>=8?at<wchar_t*>(0x4B33B4):&at<wchar_t>(0x4B33B4);
    size_t length=wcslen(title);
    bool original=size==10;
    for(unsigned i=0;original&&i<size;++i)original=(chars[i]^key)==L"primordial"[i];
    if(!original||length>capacity){log("title rename skipped size=%u capacity=%u",size,capacity);return;}
    for(size_t i=0;i<length;++i)chars[i]=static_cast<wchar_t>(title[i]^key);
    chars[length]=0;size=static_cast<unsigned>(length);
    log("title renamed to %ls",title);
}
bool initialize_menu() {
    at<void*>(0x4DE15C)=surface;
    at<void*>(0x4DE174)=engine;
    at<void*>(0x4DE160)=input;
    at<void*>(0x4DE19C)=materials;
    using ButtonName=const char*(__thiscall*)(void*,int);
    const char* insert_name=method<ButtonName>(input,40)(input,72);
    log("verified Source input=%p Insert name=%s",input,insert_name?insert_name:"<null>");
    if(!insert_name||!insert_name[0])return false;
    using VirtualKey=unsigned(__thiscall*)(void*,int);
    auto key=method<VirtualKey>(input,46);
    for(unsigned i=0;i<107;++i){virtual_keys[i]=key(input,i);if(virtual_keys[i]>255)return false;}
    if(virtual_keys[11]!=0x41||virtual_keys[72]!=VK_INSERT)return false;
    log("verified107 Source keyboard mappings; full mouse/wheel input active");
    auto mod=reinterpret_cast<uintptr_t>(GetModuleHandleA("client.dll"));
    at<void*>(0x4DE164)=*reinterpret_cast<void**>(mod+menu_profile::timing_slot_rva);
    if(!at<void*>(0x4DE164)){log("REFUSED timing pointer null");return false;}
    if(!fonts()) return false;
    // -1 makes 34CF20 create the texture and decode the embedded logo PNG (vector 57ECD4)
    // instead of drawing (and re-uploading over) a stale ID that this game owns.
    at<unsigned>(0x4B3D50)=0xFFFFFFFF;
    guarded_constructor();
    auto begin=at<unsigned>(0x804E5C), end=at<unsigned>(0x804E60);
    if(!begin || end-begin!=16){log("FAILED constructed window count");return false;}
    bool primary=false;
    for(unsigned i=0;i<4;++i){auto object=reinterpret_cast<unsigned*>(begin)[i];log("window[%u]=%08X",i,object);primary|=(object==base+0x4B33B0);}
    if(!primary){log("FAILED primary window not in constructed tree");return false;}
    rename_title(L"cyganmordial");
    at<void*>(0x804DF8)=reinterpret_cast<void*>(base+0x4B33B0);
    at<unsigned char>(0x4B3449)=1;at<float>(0x4B3444)=1.0f;
    InterlockedExchange(&initialized,1);
    log("MENU CONSTRUCTED. Initially visible; Insert toggles. Gameplay startup not executed.");
    return true;
}
bool hit(int px,int py,int x,int y,int w,int h){return w>0&&h>0&&px>=x&&py>=y&&px<x+w&&py<y+h;}
LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM wp,LPARAM lp){
    if(initialized&&!failed){
        if(message==WM_MOUSEWHEEL) InterlockedExchangeAdd(&wheel_units,static_cast<short>(HIWORD(wp)));
        if(visible){
            POINT cursor{};GetCursorPos(&cursor);ScreenToClient(window,&cursor);
            bool inside=hit(cursor.x,cursor.y,at<int>(0x4B33EC),at<int>(0x4B33F0),at<int>(0x4B33F4),at<int>(0x4B33F8));
            bool capture=inside||at<unsigned>(0x804DFC)||at<unsigned char>(0x4B341C);
            if(capture&&message>=WM_MOUSEFIRST&&message<=WM_MOUSELAST)return 0;
            // Native text fields use button states, not WM_CHAR. Keep the game's
            // console/menu shortcuts from also receiving captured UI keystrokes.
            if(message>=WM_KEYFIRST&&message<=WM_KEYLAST)return 0;
        }
    }
    return CallWindowProcA(old_wndproc,window,message,wp,lp);
}
void poll_ui_input() {
    bool foreground=GetForegroundWindow()==game_window;
    POINT cursor{-100,-100};
    if(foreground){GetCursorPos(&cursor);ScreenToClient(game_window,&cursor);}
    at<int>(0x807F00)=cursor.x;at<int>(0x807F04)=cursor.y;
    float delta=*reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(at<void*>(0x4DE164))+0x14);
    constexpr unsigned mouse_keys[]={VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2};
    LONG wheel=InterlockedExchange(&wheel_units,0);
    at<float>(0x80869C)=foreground?static_cast<float>(wheel)/WHEEL_DELTA:0.0f;
    for(unsigned i=0;i<114;++i){
        unsigned vk=i<107?virtual_keys[i]:i<112?mouse_keys[i-107]:0;
        bool down=foreground&&(i==112?wheel>0:i==113?wheel<0:vk&&((GetAsyncKeyState(vk)&0x8000)!=0));
        auto next=menu_input::advance({at<unsigned>(0x807F08+4*i),at<float>(0x808460+4*i)},down,delta);
        at<unsigned>(0x807F08+4*i)=next.state;at<float>(0x808460+4*i)=next.held;
    }
    at<unsigned char>(0x80869A)=foreground;
}
void run_ui_frame() {
    // Original panel1550AA performs this store immediately before32BA40.
    at<unsigned>(0x4B33A8)=at<unsigned>(0x540138);
    reinterpret_cast<void(__cdecl*)()>(base+0x32BA40)();
    // Window-message routing observes visibility; all frame decisions, focus,
    // search, deferred callbacks and capture publication remain native.
    visible=at<unsigned char>(0x4B3449)!=0;
}
void draw_menu_frame(unsigned panel) {
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
        reinterpret_cast<void(__cdecl*)()>(base+0x3609A0)();
        reinterpret_cast<void(__cdecl*)()>(base+0x3609E0)();
        run_ui_frame();
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
            InterlockedExchange(&primordial_menu_state,menu_ui::constructing);
            if(!initialize_menu()){InterlockedExchange(&failed,1);InterlockedExchange(&primordial_menu_state,menu_ui::failed);return;}
        }
        poll_ui_input();
        if(!failed){
            draw_menu_frame(panel);
            if(frames&&primordial_menu_state!=menu_ui::ready){
                InterlockedExchange(&primordial_menu_state,menu_ui::ready);
                log("MENU READY: intact original frame32BA40 and native directory initialization active; gameplay startup unresolved.");
                std::printf("[+] Original menu frame ready. Insert toggles; native controls and keyboard input active.\n");
            }
        }
        if(failed)InterlockedExchange(&primordial_menu_state,menu_ui::failed);
    } __except(exception_filter(GetExceptionInformation())) {}
}
DWORD WINAPI worker(void*) {
    char path[MAX_PATH];GetModuleFileNameA(nullptr,path,MAX_PATH);
    char* slash=strrchr(path,'\\');if(slash) strcpy_s(slash+1,MAX_PATH-(slash+1-path),"primordial_menu.log");
    log_file=CreateFileA(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    log("MENU INSTALL pid=%lu tick=%llu owner=%p",GetCurrentProcessId(),GetTickCount64(),self_module);
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
        if(status!=MH_OK){MH_RemoveHook(target);return 1;}
        if(status==MH_OK){
            old_wndproc=reinterpret_cast<WNDPROC>(GetWindowLongPtrA(game_window,GWLP_WNDPROC));
            SetLastError(0);auto previous=SetWindowLongPtrA(game_window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&window_proc));
            if(!previous){InterlockedExchange(&failed,1);log("FAILED window input hook Win32=%lu",GetLastError());return 1;}
        }
    } __except(exception_filter(GetExceptionInformation())) {return 1;}
    return 0;
}
}

namespace menu_ui {
bool install(HMODULE owner) {
    if(InterlockedCompareExchange(&primordial_menu_state,waiting_for_paint,dormant)!=dormant)
        return primordial_menu_state!=failed;
    self_module=owner;
    if(worker(nullptr)!=0){InterlockedExchange(&primordial_menu_state,failed);return false;}
    return primordial_menu_state!=failed;
}
}
