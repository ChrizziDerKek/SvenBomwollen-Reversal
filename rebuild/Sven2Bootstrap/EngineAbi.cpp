#include "EngineAbi.h"
#include "SupportedBuild.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <fstream>
#include <wincodec.h>
#include <wrl/client.h>
#pragma comment(lib, "windowscodecs.lib")

namespace sven2 {
template<class T> T Engine::symbol(HMODULE module,const char* name) const {
    auto address=GetProcAddress(module,name);
    if (!address) throw std::runtime_error(std::string("Missing engine export: ")+name);
    return reinterpret_cast<T>(address);
}
template<class T> static T method(void* object,unsigned byteOffset) {
    if (!object) throw std::runtime_error("Engine returned a null interface.");
    return reinterpret_cast<T>((*reinterpret_cast<void***>(object))[byteOffset/4]);
}

Engine::Engine(const std::filesystem::path& directory, const std::filesystem::path& dataDirectory)
    : directory_(directory), data_(dataDirectory) {
    requireBuild(directory/L"wtnlib.dll","48faeb2b05d608a08ae54ad1c1fcbad75a1c7bbbeb5cac1498684adc6926ac58");
    requireBuild(directory/L"mudGE.dll","47c34fac01ca7c7fc423c25b944be034931bf01eea404ce841f8b6723fa47a8d");
    requireBuild(directory/L"pluginpack.dll","bd2b5b65d5ab8cd505ffc5502c032dc50cbf750e8064e5b85afc53d03465def1");
    if (!SetDllDirectoryW(directory.c_str())) throw std::runtime_error("SetDllDirectory failed.");
    core_=LoadLibraryW((directory/L"wtnlib.dll").c_str());
    if (!core_) throw std::runtime_error("Cannot load original wtnlib.dll; Win32 error "+std::to_string(GetLastError()));
    engine_=LoadLibraryW((directory/L"mudGE.dll").c_str());
    if (!engine_) throw std::runtime_error("Cannot load original mudGE.dll; Win32 error "+std::to_string(GetLastError()));
    log("Original wtnlib.dll and mudGE.dll loaded.");
}
Engine::~Engine() {
    if(constructed_ || prepared_) {
        try {shutdown();} catch(...) {log("Engine cleanup failed during error unwinding.");}
    }
}
LegacyString Engine::string(const char* text,bool url) {
    LegacyString result;
    using Ctor=void*(__thiscall*)(void*,const char*);
    symbol<Ctor>(core_,url?"??0cUrl@@QAE@PBD@Z":"??0cString@@QAE@PBD@Z")(&result,text);
    return result; // Bit-copy transfers engine-owned payload; no host-side destructor.
}
void Engine::destroyString(LegacyString& text) {
    symbol<void(__thiscall*)(void*)>(core_,"??1cString@@UAE@XZ")(&text);
}
void Engine::construct() {
    using Ctor=void*(__thiscall*)(void*,LegacyString);
    symbol<Ctor>(engine_,"??0cApplicationEx@@QAE@VcString@@@Z")(application_,string("Sven2 Rebuild Bootstrap"));
    constructed_=true;
    log("cApplicationEx constructed (208-byte original layout).");
    symbol<void(__thiscall*)(void*)>(core_,"?PreInit@cApplication@@AAEXXZ")(application_);
    prepared_=true;
    log("PreInit created original plugin and stream managers.");
}
bool Engine::initialize(int device,const char* name) {
    using Init=bool(__thiscall*)(void*,int,LegacyString);
    bool ok=symbol<Init>(engine_,"?Initialize@cApplicationEx@@QAE_NW4EDeviceTypes@1@VcString@@@Z")(application_,device,string(name));
    log(std::string("Initialize ")+name+": "+(ok?"OK":"unavailable"));
    return ok;
}
void Engine::initializePlugins() {
    LegacyString url=string((directory_/L"pluginpack.dll").string().c_str(),true);
    bool linked=symbol<bool(__thiscall*)(void*,const LegacyString&)>(engine_,"?AddPluginPack@cApplicationEx@@QAE_NABVcUrl@@@Z")(application_,url);
    destroyString(url);
    if (!linked) throw std::runtime_error("Original pluginpack.dll registration failed.");
    log("Original pluginpack.dll linked into engine registry.");
}
std::vector<unsigned char> Engine::readAsset(const std::string& path) {
    return data_.readAsset(path);
}

void Engine::verifyData() {
    if(data_.packs().empty()) throw std::runtime_error("No level packages found in "+data_.root().string());
    readAsset("Sven2/Sven2.xml");
    log("Loaded folder game data from "+data_.root().string());
}
const Sprite* Engine::loadSprite(const std::string& path,short rows,short columns,bool opaque) {
    const auto key=std::make_tuple(path,rows,columns,opaque);
    auto cached=spriteCache_.find(key);
    if(cached!=spriteCache_.end()) return cached->second;
    auto bytes=readAsset(path);
    using Microsoft::WRL::ComPtr;
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    UINT width=0,height=0;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))) ||
       FAILED(factory->CreateStream(&stream)) ||
       FAILED(stream->InitializeFromMemory(bytes.data(),static_cast<DWORD>(bytes.size()))) ||
       FAILED(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder)) ||
       FAILED(decoder->GetFrame(0,&frame)) || FAILED(frame->GetSize(&width,&height)) ||
       FAILED(factory->CreateFormatConverter(&converter)) ||
       FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,
                                    nullptr,0,WICBitmapPaletteTypeCustom)))
        throw std::runtime_error("Cannot decode sprite: "+path);
    if(rows<1 || columns<1 || !width || !height || width>8192 || height>8192 ||
       width%columns || height%rows || width*height>16*1024*1024)
        throw std::runtime_error("Invalid sprite sheet dimensions: "+path);
    Sprite sprite{{},static_cast<int>(width),static_cast<int>(width/columns),static_cast<int>(height/rows),
                  rows,columns,!opaque};
    sprite.pixels.resize(width*height);
    if(FAILED(converter->CopyPixels(nullptr,width*4,width*height*4,reinterpret_cast<BYTE*>(sprite.pixels.data()))))
        throw std::runtime_error("Cannot read sprite pixels: "+path);
    // The original 16-bit surface keys RGB565 fuchsia. Some palettes (notably
    // noanim.bmp) use (255,1,255), which is the same key after RGB565 conversion.
    if(!opaque) {
        for(auto& pixel:sprite.pixels)
            if((pixel&0x00f8fcf8)==0x00f800f8) pixel=0x00ff00ff;
    }
    // The menu's keyed background is JPEG, so its magenta varies after decoding.
    if(!opaque && (path=="Sven2/gfx/mainmenu_BG.jpg" || path=="Sven2/gfx/Help_BG.jpg")) {
        for(auto& pixel:sprite.pixels)
            if(((pixel>>16)&255)>185 && ((pixel>>8)&255)<75 && (pixel&255)>185)
                pixel=0xff00ff;
    }
    sprites_.push_back(std::move(sprite));
    const auto* loaded=&sprites_.back();
    spriteCache_.emplace(key,loaded);
    return loaded;
}
void* Engine::video() const { return *reinterpret_cast<void* const*>(application_+0xcc); }
void Engine::startVideo(bool input) {
    if (!initialize(4,"cDirectDraw7")) {
        LegacyString url=string((directory_/L"directX3.dll").string().c_str(),true);
        bool linked=symbol<bool(__thiscall*)(void*,const LegacyString&)>(engine_,"?AddPluginPack@cApplicationEx@@QAE_NABVcUrl@@@Z")(application_,url);
        destroyString(url);
        if (!linked || !initialize(4,"cDirectDraw3")) throw std::runtime_error("Neither original DirectDraw backend could initialize.");
    }
    if (input) {
        requireBuild(directory_/L"directX3.dll","cef253664432719da7d7a6a5cf45a6d25d21bda7882112c42d8f9cbc647d113d");
        LegacyString url=string((directory_/L"directX3.dll").string().c_str(),true);
        bool linked=symbol<bool(__thiscall*)(void*,const LegacyString&)>(engine_,"?AddPluginPack@cApplicationEx@@QAE_NABVcUrl@@@Z")(application_,url);
        destroyString(url);
        if (!linked || !initialize(2,"cDirectInputMouse3") || !initialize(1,"cDirectInputKeyboard3")) throw std::runtime_error("Original DirectInput3 backend is unavailable.");
    }
    // StartVideo also initializes the already-selected input devices against its native window.
    using Start=void(__thiscall*)(void*,unsigned short,unsigned short,unsigned char,bool,int,LegacyString);
    symbol<Start>(engine_,"?StartVideo@cApplicationEx@@QAEXGGE_NHVcString@@@Z")(application_,800,600,32,false,104,string("Sven2 rebuild - original engine bootstrap"));
    videoStarted_=true;
    log("StartVideo returned: 800x600, 32-bit requested, windowed.");
}
bool Engine::startAudio() {
    if(!std::filesystem::exists(directory_/L"DX_SoundSystemPlugin.dll")) {log("Optional sound plugin is missing.");return false;}
    requireBuild(directory_/L"DX_SoundSystemPlugin.dll","17221b94502670c37e426750457baa55e96ae883109f98f62a4d5b6bc2bc5546");
    LegacyString url=string((directory_/L"DX_SoundSystemPlugin.dll").string().c_str(),true);
    bool linked=symbol<bool(__thiscall*)(void*,const LegacyString&)>(engine_,"?AddPluginPack@cApplicationEx@@QAE_NABVcUrl@@@Z")(application_,url);
    destroyString(url);
    if (!linked || !initialize(6,"cDX_SoundSystem")) return false;
    bool result=symbol<bool(__thiscall*)(void*,int,int,bool)>(engine_,"?StartSound@cApplicationEx@@QAE_NHH_N@Z")(application_,44100,32,true);
    if(result) soundSystem_=*reinterpret_cast<void**>(application_+0xc4);
    log(std::string("StartSound: ")+(result?"OK":"unavailable"));
    return result;
}
bool Engine::playSample(const std::string& path) {
    if(!soundSystem_) return false;
    auto [sample,inserted]=samples_.try_emplace(path,nullptr);
    if(inserted) {
        LegacyString url=string(data_.assetPath(path).string().c_str(),true);
        // Sven2_Audio_LoadSoundsFromXml uses the sample loader at slot +0x68.
        try {
            sample->second=method<void*(__thiscall*)(void*,const LegacyString&)>(soundSystem_,0x68)(soundSystem_,url);
        } catch(...) {
            destroyString(url);
            throw;
        }
        destroyString(url);
        if(!sample->second) log("Sound sample unavailable: "+path);
    }
    if(!sample->second) return false;
    return method<void*(__thiscall*)(void*)>(sample->second,0x54)(sample->second)!=nullptr;
}
bool Engine::playMusic(const std::string& path) {
    if(!soundSystem_) return false;
    stopMusic();
    auto found=streams_.find(path);
    if(found==streams_.end()) {
        LegacyString url=string(data_.assetPath(path).string().c_str(),true);
        void* loaded=nullptr;
        try {
            loaded=method<void*(__thiscall*)(void*,const LegacyString&,int)>(soundSystem_,0x70)(soundSystem_,url,1);
        } catch(...) {
            destroyString(url);
            throw;
        }
        destroyString(url);
        if(loaded) found=streams_.emplace(path,loaded).first;
    }
    if(found==streams_.end()) {
        log("Music stream unavailable: "+path);
        return false;
    }
    musicStream_=found->second;
    if(!method<void*(__thiscall*)(void*)>(musicStream_,0x20)(musicStream_)) {
        log("Music playback unavailable: "+path);
        musicStream_=nullptr;
        return false;
    }
    log("Started music stream: "+path);
    return true;
}

void Engine::stopMusic() {
    if(!musicStream_) return;
    method<void(__thiscall*)(void*)>(musicStream_,0x1c)(musicStream_);
    musicStream_=nullptr;
    log("Stopped music stream.");
}
void Engine::tick() { symbol<void(__thiscall*)(void*)>(core_,"?Tick@cApplication@@UAEXXZ")(application_); }
HWND Engine::windowHandle() const {
    void* device=video();
    void* window=method<void*(__thiscall*)(void*)>(device,0x18)(device);
    return symbol<HWND(__thiscall*)(const void*)>(core_,"?GetHandle@cWindow@@QBEPAUHWND__@@XZ")(window);
}
bool Engine::exitRequested() {return symbol<bool(__thiscall*)(void*)>(core_,"?GetExitRequest@cApplication@@QAE_NXZ")(application_);}
void Engine::drawFrame(const std::vector<SpriteDraw>& scene,const char* captureOverride) {
    void* device=video();
    // Original game at 0x410DD0 asks video slot +0x1C for surface 2.
    // Slot +0x50 is pixel-format metadata, not a drawing surface.
    void* surface=method<void*(__thiscall*)(void*,int)>(device,0x1c)(device,2);
    auto get=[&](const char* name){return symbol<int(__thiscall*)(void*)>(engine_,name)(surface);};
    int width=get("?GetWidth@cBitmap@@QBEHXZ"),height=get("?GetHeight@cBitmap@@QBEHXZ");
    int bpp=get("?GetBPP@cBitmap@@QBEHXZ");
    auto* pixels=static_cast<unsigned char*>(symbol<void*(__thiscall*)(void*)>(engine_,"?Lock@cSurface@@QAEPAXXZ")(surface));
    int pitch=get("?GetPitch@cBitmap@@QBEHXZ");
    if (!pixels || width<1 || width>8192 || height<1 || height>8192 || (bpp!=16 && bpp!=32) || pitch<width*(bpp/8))
        throw std::runtime_error("Unexpected native drawing surface layout.");
    if (frames==0) log("Native surface: "+std::to_string(width)+"x"+std::to_string(height)+", bpp="+std::to_string(bpp)+", pitch="+std::to_string(pitch));

    for(const auto& draw:scene) {
        const auto& sprite=*draw.sprite;
        if(draw.frame<0 || draw.frame>=sprite.rows*sprite.columns) continue;
        const int sourceX=draw.frame%sprite.columns*sprite.frameWidth;
        const int sourceY=draw.frame/sprite.columns*sprite.frameHeight;
        const int left=std::max(0,-draw.x), right=std::min(sprite.frameWidth,width-draw.x);
        const int clipBottom=draw.clipBottom>0?std::min(height,draw.clipBottom):height;
        const int top=std::max(0,-draw.y), bottom=std::min(sprite.frameHeight,clipBottom-draw.y);
        if(left>=right || top>=bottom) continue;
        for(int y=top;y<bottom;++y) {
            const auto* source=sprite.pixels.data()+(sourceY+y)*sprite.width+sourceX;
            auto* destination=pixels+(draw.y+y)*pitch;
            if(!sprite.keyed && !draw.mirror && bpp==32) {
                std::memcpy(destination+(draw.x+left)*4,source+left,(right-left)*4);
                continue;
            }
            for(int x=left;x<right;++x) {
                const int sample=draw.mirror?sprite.frameWidth-1-x:x;
                const auto rgb=source[sample]&0xffffff;
                if(sprite.keyed && rgb==0xff00ff) continue;
                if(bpp==32) reinterpret_cast<std::uint32_t*>(destination)[draw.x+x]=rgb;
                else reinterpret_cast<std::uint16_t*>(destination)[draw.x+x]=static_cast<std::uint16_t>(
                    ((rgb>>8)&0xf800)|((rgb>>5)&0x07e0)|((rgb>>3)&0x001f));
            }
        }
    }

    if(captureOverride) {
        BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);header.bfSize=header.bfOffBits+width*height*4;
        BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=width;info.biHeight=-height;info.biPlanes=1;info.biBitCount=32;info.biSizeImage=width*height*4;
        const char* capturePath=captureOverride;
        std::ofstream capture(capturePath,std::ios::binary);capture.write(reinterpret_cast<const char*>(&header),sizeof(header));capture.write(reinterpret_cast<const char*>(&info),sizeof(info));
        for(int y=0;y<height;++y) for(int x=0;x<width;++x) {
            std::uint32_t color;
            if(bpp==32) color=reinterpret_cast<const std::uint32_t*>(pixels+y*pitch)[x];
            else {auto c=reinterpret_cast<const std::uint16_t*>(pixels+y*pitch)[x];color=(((c>>11)&31)*255/31<<16)|(((c>>5)&63)*255/63<<8)|((c&31)*255/31);}
            capture.write(reinterpret_cast<const char*>(&color),4);
        }
        if(!capture) throw std::runtime_error("Cannot write native-surface capture.");
        log(std::string("Captured first native drawing surface to ")+capturePath+".");
    }
    symbol<void(__thiscall*)(void*)>(engine_,"?Unlock@cSurface@@QAEXXZ")(surface);
    method<void(__thiscall*)(void*)>(device,0x34)(device);
    method<void(__thiscall*)(void*,int)>(device,0x38)(device,1);
    ++frames;
}
void Engine::shutdown() {
    stopMusic();
    if (videoStarted_) symbol<void(__thiscall*)(void*)>(engine_,"?Exit@cApplicationEx@@UAEXXZ")(application_);
    if (prepared_) {
        symbol<void(__thiscall*)(void*)>(core_,"?Shutdown@cApplication@@QAEXXZ")(application_);
        symbol<void(__thiscall*)(void*)>(core_,"?PostExit@cApplication@@AAEXXZ")(application_);
        prepared_=false;
    }
    if (constructed_) {symbol<void(__thiscall*)(void*)>(engine_,"??1cApplicationEx@@UAE@XZ")(application_);constructed_=false;}
    log("Engine shutdown completed.");
    // Keep DLLs resident until process exit: registered legacy static destructors refer to them.
}
}
