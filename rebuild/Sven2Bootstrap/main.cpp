#include "EngineAbi.h"
#include "GameApp.h"
#include "graphics/FsrPresenter.h"
#include <algorithm>
#include <cstdio>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <objbase.h>

static std::ofstream logfile;
namespace sven2 { void log(const std::string& message) {std::cout<<message<<std::endl; if(logfile) {logfile<<message<<'\n';logfile.flush();}} }
static LONG CALLBACK observeException(EXCEPTION_POINTERS* e) {
    DWORD code=e->ExceptionRecord->ExceptionCode;
    if (code==EXCEPTION_ACCESS_VIOLATION || code==EXCEPTION_ILLEGAL_INSTRUCTION) {
        char text[256];std::snprintf(text,sizeof(text),"First-chance native exception 0x%08lX at %p",code,e->ExceptionRecord->ExceptionAddress);sven2::log(text);
        if(code==EXCEPTION_ACCESS_VIOLATION) {
            MEMORY_BASIC_INFORMATION memory{};
            if(VirtualQuery(e->ExceptionRecord->ExceptionAddress,&memory,sizeof(memory))) {
                char module[MAX_PATH]{};
                GetModuleFileNameA(static_cast<HMODULE>(memory.AllocationBase),module,MAX_PATH);
                std::snprintf(text,sizeof(text),"  Fault module %s base %p access %lu address %p",module,memory.AllocationBase,
                              static_cast<unsigned long>(e->ExceptionRecord->ExceptionInformation[0]),
                              reinterpret_cast<void*>(e->ExceptionRecord->ExceptionInformation[1]));
                sven2::log(text);
            }
            void* frames[24]{};USHORT count=CaptureStackBackTrace(0,24,frames,nullptr);
            for(USHORT i=0;i<count;++i) {
                HMODULE module=nullptr;char path[MAX_PATH]{};
                GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<const char*>(frames[i]),&module);
                GetModuleFileNameA(module,path,MAX_PATH);
                std::snprintf(text,sizeof(text),"  frame %u: %p RVA %08lX",i,frames[i],static_cast<DWORD>(reinterpret_cast<uintptr_t>(frames[i])-reinterpret_cast<uintptr_t>(module)));
                sven2::log(std::string(text)+" "+path);
            }
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static int bootstrap(int argc,char** argv) {
    wchar_t executable[32768]{};
    GetModuleFileNameW(nullptr,executable,32768);
    auto root=std::filesystem::path(executable).parent_path().parent_path().parent_path().parent_path().parent_path();
    auto game=root/L"games"/L"sven2";
    auto dataDirectory=root/L"assets";
    bool probe=false,audio=true,nativeInput=false,directTutorial=false;
    unsigned seconds=0;
    bool scaled=true;
    int outputWidth=1280,outputHeight=960;
    float sharpness=0.5f;
    auto filter=sven2::ScalingFilter::fsr1;
    std::filesystem::path capture;
    for(int i=1;i<argc;++i) {
        const std::string option=argv[i];
        if(option=="--probe") probe=true;
        else if(option=="--upscaler" && i+1<argc) {
            const std::string mode=argv[++i];scaled=mode!="native";
            if(mode=="fsr1") filter=sven2::ScalingFilter::fsr1;
            else if(mode=="nearest") filter=sven2::ScalingFilter::nearest;
            else if(mode=="bilinear") filter=sven2::ScalingFilter::bilinear;
            else if(mode!="native") throw std::runtime_error("Upscaler must be native, nearest, bilinear or fsr1.");
        }
        else if(option=="--output-width" && i+1<argc) outputWidth=std::stoi(argv[++i]);
        else if(option=="--output-height" && i+1<argc) outputHeight=std::stoi(argv[++i]);
        else if(option=="--sharpness" && i+1<argc) sharpness=std::stof(argv[++i]);
        else if(option=="--capture" && i+1<argc) capture=std::filesystem::absolute(argv[++i]);
        else if(option=="--tutorial") directTutorial=true;
        else if(option=="--no-audio") audio=false;
        else if(option=="--no-input") nativeInput=false;
        else if(option=="--native-input") nativeInput=true;
        else if(option=="--seconds" && i+1<argc) seconds=std::stoul(argv[++i]);
        else if(option=="--data-dir" && i+1<argc) dataDirectory=std::filesystem::absolute(argv[++i]);
        else if(option=="--game-dir" && i+1<argc) game=std::filesystem::absolute(argv[++i]);
        else if(option=="--help") {
            std::cout<<"Sven2Bootstrap [--tutorial] [--data-dir PATH] [--game-dir PATH] [--no-audio]\n"
                     <<"Graphics: --upscaler native|fsr1|nearest|bilinear (default: fsr1) --output-width N --output-height N --sharpness 0..2\n"
                     <<"Scaled mode: F6 filter; F7 softer; F8 sharper; F11 borderless fullscreen. --capture FILE writes the first scaled frame.\n"
                     <<"Diagnostics: --probe, --seconds N, --native-input\n";
            return 0;
        } else {
            throw std::runtime_error("Unknown or incomplete option: "+option);
        }
    }
    const auto runtime=root/L"rebuild"/L"runtime"/L"sven2";
    std::filesystem::create_directories(runtime);
    std::filesystem::current_path(runtime);
    logfile.open(runtime/(probe?"probe.log":"game.log"),std::ios::trunc);
    AddVectoredExceptionHandler(1,observeException);
    sven2::log("Sven2 rebuild / Win32");
    HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(com)) throw std::runtime_error("Cannot initialize COM for game devices.");
    struct ComLifetime { ~ComLifetime() { CoUninitialize(); } } apartment;
    sven2::Engine engine(game,dataDirectory);
    engine.construct();
    engine.initializePlugins();
    engine.verifyData();
    if(!probe) {
        if(scaled) SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        engine.startVideo(nativeInput);
        if(scaled) engine.startScaledVideo(outputWidth,outputHeight,filter,sharpness);
        if(!capture.empty()) {
            if(!scaled) throw std::runtime_error("--capture requires a scaled presentation mode.");
            engine.captureNextFrame(capture);
        }
        const bool audioReady=audio && engine.startAudio();
        if(audio && !audioReady) sven2::log("Audio device unavailable.");
        sven2::runGame(engine,runtime/L"saves",audioReady,seconds,directTutorial);
    }
    engine.shutdown();
    sven2::log("PASS");
    return 0;
}
static LONG crash(EXCEPTION_POINTERS* e) {
    if(e->ExceptionRecord->ExceptionCode==0xe06d7363) return EXCEPTION_CONTINUE_SEARCH;
    char text[256];std::snprintf(text,sizeof(text),"Native failure: code=0x%08lX address=%p",e->ExceptionRecord->ExceptionCode,e->ExceptionRecord->ExceptionAddress);
    sven2::log(text);return EXCEPTION_EXECUTE_HANDLER;
}
static int guarded(int argc,char**argv) {
    __try {return bootstrap(argc,argv);} __except(crash(GetExceptionInformation())) {return 2;}
}
int main(int argc,char**argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    try{return guarded(argc,argv);}catch(const std::exception& e){sven2::log(std::string("ERROR: ")+e.what());return 1;}
    catch(...){sven2::log("ERROR: legacy engine exception; see the engine HTML log in rebuild/runtime/sven2.");return 2;}
}
