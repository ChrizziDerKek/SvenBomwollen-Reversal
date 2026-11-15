#pragma once
#include "GameData.h"
#include <windows.h>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace sven2 {
class FsrPresenter;
enum class ScalingFilter;
// Only trivial ABI containers cross the VC6/modern-MSVC boundary.
// Engine constructors allocate strings; consuming-by-value methods destroy them.
struct LegacyString { std::uint32_t words[4]{}; };
static_assert(sizeof(LegacyString)==16);
static_assert(sizeof(void*)==4, "Original Sven2 DLLs require Win32/x86.");

struct Sprite {
    std::vector<std::uint32_t> pixels;
    int width, frameWidth, frameHeight, rows, columns;
    bool keyed;
};

struct SpriteDraw {
    const Sprite* sprite;
    int x;
    int y;
    int frame;
    bool mirror=false;
    int clipBottom=0; // Exclusive screen Y; zero uses the full drawing surface.
};

class Engine {
public:
    Engine(const std::filesystem::path& directory, const std::filesystem::path& dataDirectory);
    ~Engine();
    Engine(const Engine&)=delete;
    Engine& operator=(const Engine&)=delete;
    void construct();
    void initializePlugins();
    void verifyData();
    const GameData& data() const { return data_; }
    std::vector<unsigned char> readAsset(const std::string& path);
    const Sprite* loadSprite(const std::string& path, short rows, short columns, bool opaque=false);
    void startVideo(bool input=true);
    void startScaledVideo(int width,int height,ScalingFilter filter,float sharpness);
    bool scaledVideo() const { return presenter_!=nullptr; }
    void captureNextFrame(const std::filesystem::path& path) { captureNext_=path; }
    bool startAudio();
    bool playSample(const std::string& path);
    void stopUfoSound();
    bool ufoSoundPlaying() const;
    bool playMusic(const std::string& path);
    void stopMusic();
    void tick();
    HWND windowHandle() const;
    bool exitRequested();
    void drawFrame(const std::vector<SpriteDraw>& scene, const char* capture=nullptr);
    void shutdown();
    void* video() const;
    unsigned frames=0;
private:
    HMODULE core_=nullptr, engine_=nullptr;
    std::unique_ptr<FsrPresenter> presenter_;
    std::vector<std::uint32_t> scaledFrame_;
    std::filesystem::path captureNext_;
    std::filesystem::path directory_;
    GameData data_;
    std::deque<Sprite> sprites_;
    std::map<std::tuple<std::string,short,short,bool>, const Sprite*> spriteCache_;
    void* soundSystem_=nullptr;
    void* ufoChannel_=nullptr;
    void* musicStream_=nullptr;
    std::map<std::string,void*> samples_;
    alignas(4) unsigned char application_[0xd0]{};
    bool constructed_=false, prepared_=false, videoStarted_=false;
    LegacyString string(const char* text, bool url=false);
    void destroyString(LegacyString& text);
    bool initialize(int device, const char* name);
    template<class T> T symbol(HMODULE module,const char* name) const;
};
void log(const std::string& message);
}
