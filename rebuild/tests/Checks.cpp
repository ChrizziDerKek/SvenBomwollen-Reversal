#include "../Sven2Bootstrap/EngineAbi.h"
#include "../Sven2Bootstrap/LevelScene.h"
#include "../Sven2Bootstrap/GameUi.h"
#include "../Sven2Bootstrap/GameSession.h"
#include "graphics/FsrPresenter.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <set>
#include <array>
#include <queue>
#include <cstring>
#include <functional>

namespace sven2 { void log(const std::string& text) { std::cout<<text<<std::endl; } }
namespace {
void require(bool value,const char* message) {
    if(!value) throw std::runtime_error(message);
}
void checkScalingCoordinates() {
    using sven2::FsrPresenter;
    auto r=FsrPresenter::viewport(1920,1080);
    require(r.left==240 && r.right==1680 && r.top==0 && r.bottom==1080,"Widescreen scaling must pillarbox to 4:3.");
    auto p=FsrPresenter::logicalMouse({960,540},1920,1080);
    require(p.x==400 && p.y==300,"Scaled mouse center must map to the logical game center.");
    p=FsrPresenter::logicalMouse({239,100},1920,1080);
    require(p.x<0 && p.y<0,"Black bars must not activate game UI.");
    p=FsrPresenter::logicalMouse({1679,1079},1920,1080);
    require(p.x==799 && p.y==599,"Bottom-right scaled pixel must stay inside logical bounds.");
    r=FsrPresenter::viewport(800,1000);
    require(r.top==200 && r.bottom==800,"Tall windows must letterbox without distorting the game.");
    p=FsrPresenter::logicalMouse({0,0},0,0);
    require(p.x<0 && p.y<0,"Minimized windows must not divide mouse coordinates by zero.");
}
std::vector<std::uint32_t> readScaledCapture(const char* path,int width,int height) {
    std::ifstream file(path,std::ios::binary);
    BITMAPFILEHEADER header{};BITMAPINFOHEADER info{};
    file.read(reinterpret_cast<char*>(&header),sizeof(header));file.read(reinterpret_cast<char*>(&info),sizeof(info));
    require(file && info.biWidth==width && info.biHeight==-height && info.biBitCount==32,"Scaled capture dimensions/format are incorrect.");
    file.seekg(header.bfOffBits);
    std::vector<std::uint32_t> pixels(width*height);
    file.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);
    require(static_cast<bool>(file),"Truncated GPU capture.");
    for(auto& pixel:pixels) pixel&=0xffffff;
    return pixels;
}
void checkUpscalerGpu() {
    using sven2::ScalingFilter;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    sven2::FsrPresenter presenter(1280,720,ScalingFilter::fsr1,0.5f);
    std::vector<std::uint32_t> source(800*600,0x336699);
    presenter.present(source.data(),"fsr-flat.bmp");
    auto flat=readScaledCapture("fsr-flat.bmp",1280,720);
    require(flat[360*1280]==0 && flat[360*1280+1279]==0,"FSR black bars must be black.");
    for(int x:{160,640,1119}) {
        const auto c=flat[360*1280+x];
        for(int shift:{0,8,16}) require(std::abs(static_cast<int>((c>>shift)&255)-static_cast<int>((0x336699>>shift)&255))<=1,
            "FSR must preserve constant colors, channels and edge clamping.");
    }
    for(int y=0;y<600;++y) for(int x=0;x<800;++x)
        source[y*800+x]=x<400?0x202020:(y>x-400?0xf0c080:0x4070c0);
    presenter.setFilter(ScalingFilter::nearest);presenter.present(source.data(),"nearest.bmp");
    auto nearest=readScaledCapture("nearest.bmp",1280,720);
    presenter.setFilter(ScalingFilter::bilinear);presenter.present(source.data(),"bilinear.bmp");
    auto bilinear=readScaledCapture("bilinear.bmp",1280,720);
    presenter.setFilter(ScalingFilter::fsr1);presenter.present(source.data(),"fsr-edges.bmp");
    auto fsr=readScaledCapture("fsr-edges.bmp",1280,720);
    require(nearest!=bilinear && fsr!=bilinear && fsr!=nearest,"FSR, bilinear and nearest must execute distinct filters.");
    presenter.setSharpness(2.0f);presenter.present(source.data(),"fsr-soft.bmp");
    require(fsr!=readScaledCapture("fsr-soft.bmp",1280,720),"RCAS sharpening adjustment has no effect.");
    RECT rect{0,0,800,600};AdjustWindowRect(&rect,WS_OVERLAPPEDWINDOW,FALSE);
    SetWindowPos(presenter.window(),nullptr,0,0,rect.right-rect.left,rect.bottom-rect.top,SWP_NOMOVE|SWP_NOZORDER);
    presenter.present(source.data(),"native-sized.bmp");
    require(readScaledCapture("native-sized.bmp",800,600)==source,"Native-size fallback must not sharpen or resample the source.");
    SendMessageW(presenter.window(),WM_KEYDOWN,VK_F11,0);
    presenter.present(source.data());
    SendMessageW(presenter.window(),WM_KEYDOWN,VK_F11,0);
    presenter.present(source.data(),"restored.bmp");
    require(readScaledCapture("restored.bmp",800,600)==source,"Fullscreen restoration must recreate the original-sized targets.");
    SendMessageW(presenter.window(),WM_CLOSE,0,0);
    require(presenter.closed(),"Closing the scaled window must request game exit.");
    sven2::log("PASS: native FSR1 shaders, RCAS, comparison filters, color/edge preservation, letterboxing, resize/fullscreen and close.");
}
void checkCapturePixel(const char* path,int x,int y,std::uint32_t expected) {
    std::ifstream file(path,std::ios::binary);
    BITMAPFILEHEADER header{};BITMAPINFOHEADER info{};
    file.read(reinterpret_cast<char*>(&header),sizeof(header));
    file.read(reinterpret_cast<char*>(&info),sizeof(info));
    require(file && header.bfType==0x4d42 && info.biBitCount==32 && info.biHeight<0,
            "Unexpected native capture format.");
    file.seekg(header.bfOffBits+(static_cast<std::streamoff>(y)*info.biWidth+x)*4);
    std::uint32_t observed=0;
    file.read(reinterpret_cast<char*>(&observed),4);
    require(file && (observed&0xffffff)==(expected&0xffffff),"Native tab pixels covered the paper body.");
}
void step(sven2::LevelScene& scene,int dx,int dy) {
    scene.move(0.1f,dx,dy);
    scene.move(0.1f,dx,dy);
    scene.move(0.1f,0,0);
}

// Use an isolated asset tree so authored regression levels never touch game data.
void linkFixtureData(const std::filesystem::path& source,const std::filesystem::path& target) {
    std::filesystem::create_directories(target);
    for(const auto& entry:std::filesystem::recursive_directory_iterator(source)) {
        const auto destination=target/std::filesystem::relative(entry.path(),source);
        if(entry.is_directory()) std::filesystem::create_directories(destination);
        else if(entry.is_regular_file()) {
            std::error_code error;
            std::filesystem::create_hard_link(entry.path(),destination,error);
            if(error) std::filesystem::copy_file(entry.path(),destination);
        }
    }
}

struct FixtureActor {
    int kind,x,y,facing,anger=6,temperament=0;
    int state=0;
    float pulse=1000;
    int angerTicks=0;
};
sven2::LevelInfo fixtureLevel(sven2::Engine& engine,const std::string& name,
                              std::initializer_list<FixtureActor> actors,int pickup=0) {
    const auto& packs=engine.data().packs();
    const auto xs=std::find_if(packs.begin(),packs.end(),[](const auto& pack) { return pack.name=="XS"; });
    auto level=xs->levelsets.front().levels.front();
    // v6 layout is documented in docs/VERIFIED_FORMAT.md and LevelScene's parser.
    std::vector<unsigned char> data(16+9048+25600);
    auto integer=[&](size_t offset,int value) { std::memcpy(data.data()+offset,&value,4); };
    auto real=[&](size_t offset,float value) { std::memcpy(data.data()+offset,&value,4); };
    integer(0,6);
    integer(8,0); // Render requested UFOs and their shadows, as in visible-UFO levels.
    for(int tile=0;tile<22*17;++tile) {
        const auto offset=16+tile*24;
        integer(offset,tile%22==0 || tile%22==21 || tile/22==0 || tile/22==16?1:0);
        integer(offset+16,-1);
    }
    real(16+9044,180);
    for(int slot=0;slot<256;++slot) integer(16+9048+slot*100+44,-1);
    int slot=0;
    for(const auto& actor:actors) {
        const int tile=((actor.y+60)/40+1)*22+(actor.x+60)/40+1;
        const auto offset=16+9048+slot*100;
        // Inputs locate a 120px actor canvas. Preserve that logical center for
        // the farmer's 160px canvas and the distractions' 80px canvases.
        const int size=actor.kind==5?160:actor.kind>=7 && actor.kind<=10?80:120;
        const int x=actor.x+60-size/2,y=actor.y+60-size/2;
        real(offset,static_cast<float>(x));real(offset+4,static_cast<float>(y));
        real(offset+8,static_cast<float>(x));real(offset+12,static_cast<float>(y));
        integer(offset+20,size);integer(offset+24,size);
        integer(offset+92,actor.kind==5?40:actor.kind==0?30:actor.kind>=7 && actor.kind<=10?60:0);
        integer(offset+44,actor.kind);
        real(offset+52,1000);real(offset+56,15);
        data[offset+65]=static_cast<unsigned char>(actor.temperament);
        data[offset+64]=static_cast<unsigned char>(actor.angerTicks);
        integer(offset+72,actor.state);
        integer(offset+76,actor.facing);integer(offset+80,tile);integer(offset+84,actor.anger);
        real(offset+88,actor.pulse);
        if(actor.kind>=7 && actor.kind<=10) {
            constexpr int itemCodes[]={130,131,129,128};
            integer(offset+68,1); // Permanent distraction fixture.
            data[16+tile*24+12]=static_cast<unsigned char>(itemCodes[actor.kind-7]);
            integer(16+tile*24+16,slot);
        } else integer(16+tile*24,actor.kind==0?100:actor.kind>=11?actor.kind-4:actor.kind);
        if(slot==0 && pickup) data[16+tile*24+12]=static_cast<unsigned char>(pickup);
        ++slot;
    }
    level.map="XS/levels/check-"+name+".lvl";
    std::ofstream output(engine.data().root()/std::filesystem::u8path(level.map),std::ios::binary);
    output.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()));
    if(!output) throw std::runtime_error("Cannot write isolated regression level.");
    return level;
}

void checkDepthOrdering(sven2::Engine& engine,bool capture=false) {
    const auto& packs=engine.data().packs();
    const auto xs=std::find_if(packs.begin(),packs.end(),[](const auto& p) {return p.name=="XS";});
    auto addScenery=[&](sven2::LevelInfo level,int y,int texture=0,int height=80,int adjustment=0,int x=240) {
        auto bytes=engine.readAsset(level.map);
        const size_t record=16+9048+2*100;
        auto integer=[&](size_t offset,int value) {std::memcpy(bytes.data()+record+offset,&value,4);};
        auto real=[&](size_t offset,float value) {std::memcpy(bytes.data()+record+offset,&value,4);};
        real(0,static_cast<float>(x));real(4,static_cast<float>(y));real(8,static_cast<float>(x));real(12,static_cast<float>(y));
        integer(16,0);integer(20,80);integer(24,height);integer(44,1);integer(48,0);integer(68,texture);integer(92,adjustment);
        std::ofstream file(engine.data().root()/std::filesystem::u8path(level.map),std::ios::binary|std::ios::trunc);
        file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
        require(static_cast<bool>(file),"Cannot write depth regression scenery.");
        return level;
    };
    auto index=[](const std::vector<sven2::SpriteDraw>& draws,const sven2::Sprite* sprite,int x,int y) {
        const auto found=std::find_if(draws.begin(),draws.end(),[&](const auto& d) {return d.sprite==sprite && d.x==x && d.y==y;});
        require(found!=draws.end(),"Depth regression sprite is missing.");
        return found-draws.begin();
    };
    const char* files[]={"Chicken","Goat","Pig"};
    for(int kind=11;kind<=13;++kind) {
        const auto* animalSprite=engine.loadSprite(std::string("Sven2/gfx/Animals/")+files[kind-11]+"_Stand.bmp",4,1);
        const auto north=fixtureLevel(engine,"depth-north-"+std::to_string(kind),{{0,240,160,4},{kind,240,200,1}});
        sven2::LevelScene approach(engine,*xs,north,1);
        const auto& front=approach.draws();
        require(index(front,animalSprite,240,200)>index(front,approach.player().sprite,240,160),
                "Sven approaching from above painted his legs over the animal.");
        if(capture) engine.drawFrame(front,("animal-north-"+std::to_string(kind)+".bmp").c_str());
        const auto south=addScenery(fixtureLevel(engine,"depth-interaction-"+std::to_string(kind),{{0,240,240,0},{kind,240,200,0}}),230);
        const auto& texture=engine.data().tileset(*xs,south).tiles.front();
        const auto* scenery=engine.loadSprite(texture.file,texture.frames,1);
        sven2::LevelScene interacting(engine,*xs,south,1);
        const auto& idle=interacting.draws();
        require(index(idle,animalSprite,240,200)<index(idle,scenery,240,230),"Occlusion fixture animal is not behind the scenery.");
        if(capture) engine.drawFrame(idle,("animal-behind-idle-"+std::to_string(kind)+".bmp").c_str());
        interacting.update(0.01f,0,0,true);
        require(interacting.player().sprite!=animalSprite,"Animal interaction did not start.");
        for(int frame=0;frame<4;++frame) {
            const auto& active=interacting.draws();
            require(index(active,interacting.player().sprite,240,200)<index(active,scenery,240,230),
                    "Animal interaction jumped in front of intervening scenery.");
            require(std::none_of(active.begin(),active.end(),[&](const auto& d) {return d.sprite==animalSprite;}),
                    "Animal stand sprite was drawn again over its combined interaction.");
            if(capture && frame==0) engine.drawFrame(active,("animal-behind-interaction-"+std::to_string(kind)+".bmp").c_str());
            interacting.update(0.01f,0,0,false);
        }
    }
    const auto moving=addScenery(fixtureLevel(engine,"depth-mid-step",{{0,240,160,4},{11,480,400,0}}),175);
    const auto& texture=engine.data().tileset(*xs,moving).tiles.front();
    const auto* scenery=engine.loadSprite(texture.file,texture.frames,1);
    sven2::LevelScene walk(engine,*xs,moving,1);
    const auto& before=walk.draws();
    require(index(before,walk.player().sprite,240,160)<index(before,scenery,240,175),"Mid-step depth fixture has the wrong initial order.");
    walk.move(0.1f,0,1);
    require(walk.player().y>160 && walk.player().y<200,"Depth fixture did not stop between grid cells.");
    const auto& after=walk.draws();
    require(index(after,walk.player().sprite,walk.player().x,walk.player().y)>index(after,scenery,240,175),
            "Moving Sven kept the old tile's depth until committing his step.");
    const auto xxl=std::find_if(packs.begin(),packs.end(),[](const auto& p) {return p.name=="XXL";});
    auto heapLevel=fixtureLevel(engine,"depth-forest-heap",{{0,240,280,4},{11,480,400,0}});
    heapLevel.tileset=xxl->levelsets.front().levels.front().tileset;
    heapLevel=addScenery(heapLevel,379,2,19,20,284);
    const auto& heapImage=engine.data().tileset(*xxl,heapLevel).tiles.at(2);
    const auto* heap=engine.loadSprite(heapImage.file,heapImage.frames,1);
    sven2::LevelScene crossing(engine,*xxl,heapLevel,1);
    crossing.move(0.1f,0,1);
    const auto& crossingDraws=crossing.draws();
    require(crossing.player().y>280 && crossing.player().y<320 &&
            index(crossingDraws,crossing.player().sprite,crossing.player().x,crossing.player().y)>index(crossingDraws,heap,284,379),
            "Forest dirt heap covered Sven after his rendered depth crossed it mid-step.");
    if(capture) engine.drawFrame(crossingDraws,"forest-heap-mid-step.bmp");
    sven2::log("PASS: interpolated depth, north animal approaches and occluded chicken/goat/pig interactions.");
}

void checkSerializedPowerups(sven2::Engine& engine) {
    const auto& packs=engine.data().packs();
    const auto xs=std::find_if(packs.begin(),packs.end(),[](const auto& p) {return p.name=="XS";});
    const auto* icons=engine.loadSprite("Sven2/gfx/icons.bmp",5,1);
    const char* durations[]={"FireworkTime","GiftTime","ImpotentTime","SmellTime","FlowerTime"};
    for(int kind=14;kind<=18;++kind) {
        auto level=fixtureLevel(engine,"serialized-powerup-"+std::to_string(kind),{{0,240,200,2},{7,280,200,0},{2,640,400,0}});
        auto bytes=engine.readAsset(level.map);
        const size_t record=16+9048+100;
        auto put=[&](size_t off,int value) {std::memcpy(bytes.data()+off,&value,4);};
        int tile=0;std::memcpy(&tile,bytes.data()+record+80,4);
        put(record+44,kind);put(record+48,kind-14);
        bytes[16+tile*24+12]=static_cast<unsigned char>(kind+118);
        std::ofstream file(engine.data().root()/std::filesystem::u8path(level.map),std::ios::binary|std::ios::trunc);
        file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));file.close();
        require(static_cast<bool>(file),"Cannot write serialized powerup fixture.");
        sven2::LevelScene pickup(engine,*xs,level,1),expiry(engine,*xs,level,1);
        const auto& initial=pickup.draws();
        require(std::any_of(initial.begin(),initial.end(),[&](const auto& d) {return d.sprite==icons && d.frame==kind-14;}),
                "Serialized powerup used the wrong icon frame.");
        step(pickup,1,0);
        const auto& collected=pickup.draws();
        require(std::none_of(collected.begin(),collected.end(),[&](const auto& d) {return d.sprite==icons;}),
                "Serialized powerup did not disappear on collection.");
        require(!pickup.takeSounds().empty(),"Serialized powerup did not activate on collection.");
        const float duration=static_cast<float>(engine.data().parameter(durations[kind-14],10));
        expiry.update(duration+0.1f,0,0,false);
        const auto& expired=expiry.draws();
        require(std::none_of(expired.begin(),expired.end(),[&](const auto& d) {return d.sprite==icons;}),
                "Serialized powerup did not expire at its native Zwo duration.");
    }
    sven2::log("PASS: all five serialized Zwo powerup kinds render, collect and expire.");
}

void driveTutorial(sven2::Engine& engine,
                   const std::function<void(float,int,int,bool)>& update,
                   const std::function<const std::vector<sven2::SpriteDraw>&()>& draws,
                   const std::function<int()>& remaining) {
    auto bytes=engine.readAsset("XS/levels/Tutorial01v01.lvl");
    std::uint32_t nameLength=0;
    std::memcpy(&nameLength,bytes.data()+12,4);
    std::array<bool,300> floor{};
    for(int cell=0;cell<300;++cell) {
        int code=0;
        const int raw=23+(cell/20)*22+cell%20;
        std::memcpy(&code,bytes.data()+16+nameLength+raw*24,4);
        floor[cell]=code==0 || code==2;
    }
    const auto* player=engine.loadSprite("Sven2/gfx/Sven/sven_stand.bmp",8,1);
    const auto* sheep=engine.loadSprite("Sven2/gfx/Sheep/Sheep_Stand.bmp",4,1);
    constexpr int directions[][2]={{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}};
    auto cellAt=[](const sven2::SpriteDraw& draw) {
        const int x=(draw.x+60)/40,y=(draw.y+60)/40;
        return x>=0 && x<20 && y>=0 && y<15?y*20+x:-1;
    };
    float elapsed=0;
    auto tick=[&](float seconds,int dx,int dy,bool action) {
        update(seconds,dx,dy,action);
        elapsed+=seconds;
    };
    while(remaining()>0 && elapsed<150) {
        const auto& frame=draws();
        auto hero=std::find_if(frame.begin(),frame.end(),[&](const auto& draw) { return draw.sprite==player; });
        if(hero==frame.end()) { tick(0.1f,0,0,false);continue; }
        const int start=cellAt(*hero);
        require(start>=0,"Player left the tutorial grid.");
        auto passable=floor;
        std::vector<int> targets;
        for(const auto& draw:frame) {
            if(draw.sprite!=sheep) continue;
            const int cell=cellAt(draw);
            if(cell>=0) { targets.push_back(cell);passable[cell]=false; }
        }
        std::array<int,300> previous;
        previous.fill(-1);
        previous[start]=start;
        std::queue<int> queue;
        queue.push(start);
        while(!queue.empty()) {
            const int current=queue.front();
            queue.pop();
            for(const auto& direction:directions) {
                const int x=current%20+direction[0],y=current/20+direction[1];
                if(x<0 || x>=20 || y<0 || y>=15) continue;
                const int next=y*20+x;
                if(!passable[next] || previous[next]>=0) continue;
                previous[next]=current;
                queue.push(next);
            }
        }
        int destination=-1,facing=-1,bestDistance=301;
        for(int target:targets) {
            for(int direction=0;direction<8;++direction) {
                const int x=target%20-directions[direction][0],y=target/20-directions[direction][1];
                if(x<0 || x>=20 || y<0 || y>=15) continue;
                const int candidate=y*20+x;
                if(previous[candidate]<0) continue;
                int distance=0;
                for(int cell=candidate;cell!=start;cell=previous[cell]) ++distance;
                if(distance<bestDistance) {
                    bestDistance=distance;
                    destination=candidate;
                    facing=direction;
                }
            }
        }
        if(destination<0) { tick(0.1f,0,0,false);continue; }
        if(destination==start) {
            tick(0.01f,directions[facing][0],directions[facing][1],false);
            const int before=remaining();
            for(int i=0;i<2400 && remaining()==before && elapsed<150;++i)
                tick(1.0f/60,0,0,i%8==0);
        } else {
            int next=destination;
            while(previous[next]!=start) next=previous[next];
            const int dx=next%20-start%20,dy=next/20-start/20;
            tick(0.1f,dx,dy,false);
            tick(0.1f,dx,dy,false);
            tick(0.2f,0,0,false);
        }
    }
    require(remaining()==0,"Public-input tutorial driver did not complete the round.");
}
void checkMenus(sven2::Engine& engine,const std::filesystem::path& saves) {
    using Page=sven2::GameUi::Page;
    sven2::GameUi ui(engine,saves);
    sven2::UiInput input;
    input.confirm=true;
    require(ui.handle(input)==sven2::UiAction::none && ui.page()==Page::packs,"Start did not open episodes.");
    require(ui.handle(input)==sven2::UiAction::start,"Flat episode selection did not start a session.");
    require(!ui.pack().name.empty() && !ui.levelset().levels.empty(),"Catalog selection is empty.");
    ui.show(Page::help);
    for(int i=0;i<4;++i) {
        require(ui.draws(0).size()>40,"Original help text is absent.");
        sven2::UiInput next;
        next.right=true;
        ui.handle(next);
    }
    for(int i=0;i<3;++i) { sven2::UiInput next;next.right=true;ui.handle(next); }
    const auto* smallFont=engine.loadSprite("Sven2/gfx/font2.bmp",88,1);
    const auto& credits=ui.draws(0);
    require(std::any_of(credits.begin(),credits.end(),[&](const auto& draw) {
        return draw.sprite==smallFont && draw.x==120 && draw.y==220 && draw.frame==2;
    }),"Credits shifted the Coloration row into subsequent text.");
    require(std::any_of(credits.begin(),credits.end(),[&](const auto& draw) {
        return draw.sprite==smallFont && draw.x==120 && draw.y==380 && draw.frame==21;
    }),"Credits did not retain the original two-column thanks row.");
    ui.show(Page::highscores);
    require(ui.draws(0).size()>100,"Original highscore defaults are absent.");
    const auto firstEpisode=ui.pack().name+"/"+ui.levelset().name;
    sven2::UiInput nextEpisode;
    nextEpisode.click=true; nextEpisode.mouse={275,100};
    ui.handle(nextEpisode);
    require(ui.pack().name+"/"+ui.levelset().name!=firstEpisode,"Highscore mouse arrow did not change episodes.");
    sven2::UiInput previousEpisode;
    previousEpisode.left=true;
    ui.handle(previousEpisode);
    require(ui.pack().name+"/"+ui.levelset().name==firstEpisode,"Highscore keyboard traversal did not restore the episode.");
    sven2::UiInput cards;
    cards.click=true;cards.mouse={510,40};
    ui.handle(cards);
    const auto& cardDraws=ui.draws(0);
    require(std::any_of(cardDraws.begin(),cardDraws.end(),[](const auto& draw) {
        return draw.x==310 && draw.y==150;
    }),"Card omitted its title.");
    require(std::any_of(cardDraws.begin(),cardDraws.end(),[](const auto& draw) {
        return draw.x==310 && draw.y==200;
    }),"Card omitted its description.");
    const auto variants=sven2::loadInteractionAnimations(engine);
    for(int card=0;card<33;++card) {
        const auto& draws=ui.draws(0);
        require(std::any_of(draws.begin(),draws.end(),[&](const auto& draw) {
            return draw.sprite==variants[static_cast<size_t>(card)*2].sprite;
        }),"An available original card was incorrectly locked.");
        sven2::UiInput nextCard;nextCard.right=true;
        ui.handle(nextCard);
    }
    sven2::UiInput playCard;playCard.confirm=true;
    ui.handle(playCard);
    const auto cardSounds=ui.takeSounds();
    require(cardSounds.size()==1 && cardSounds.front()==variants.front().sound,
            "Card playback omitted its original sound.");
    ui.handle(playCard);
    require(ui.takeSounds().empty(),"Card input restarted an animation that was already playing.");
    const auto& playback=ui.draws(0.11f);
    require(std::any_of(playback.begin(),playback.end(),[&](const auto& draw) {
        return draw.sprite==variants.front().sprite && draw.frame==variants.front().frames[1];
    }),"Card browser did not use the original 15 fps animation clock.");
    const auto* arrows=engine.loadSprite("Sven2/gfx/pijlen.bmp",8,1);
    sven2::UiInput arrowHover;arrowHover.mouseMoved=true;arrowHover.mouse={245,360};
    ui.handle(arrowHover);
    const auto& hoveredArrow=ui.draws(0);
    require(std::any_of(hoveredArrow.begin(),hoveredArrow.end(),[&](const auto& draw) {
        return draw.sprite==arrows && draw.x==238 && draw.y==350 && draw.frame==3;
    }),"Card arrow did not change color on hover.");
    const auto* paper=engine.loadSprite("Sven2/gfx/Help_BG.jpg",1,1);
    const auto* scoreTabs=engine.loadSprite("Sven2/gfx/Highsc_file_cards.bmp",10,1);
    require(std::all_of(hoveredArrow.begin(),hoveredArrow.end(),[&](const auto& draw) {
        return draw.sprite!=scoreTabs || draw.clipBottom==60;
    }),"Highscore tabs extend over the paper body.");
    require(std::any_of(hoveredArrow.begin(),hoveredArrow.end(),[&](const auto& draw) {
        return draw.sprite==paper;
    }),"Highscore paper backdrop is absent.");
    ui.show(Page::pause);
    sven2::UiInput escape; escape.back=true;
    require(ui.handle(escape)==sven2::UiAction::resume,"Pause escape did not resume.");
    sven2::UiInput chooseNo; chooseNo.right=true;
    ui.handle(chooseNo);
    require(ui.handle(input)==sven2::UiAction::resume,"Pause No did not resume.");
    ui.showResults({1250,1,180,3,3,0,0,0,true,false});
    ui.handle(input);
    require(ui.page()==Page::name,"A qualifying round did not open name entry.");
    sven2::UiInput typing;
    typing.text=u8"J\u00f6rg";
    ui.handle(typing);
    sven2::BitmapFont nameFont(engine);
    const auto* largeFont=engine.loadSprite("Sven2/gfx/font1.bmp",88,1);
    const auto& nameDraws=ui.draws(0);
    require(std::any_of(nameDraws.begin(),nameDraws.end(),[&](const auto& draw) {
        return draw.sprite==largeFont && draw.x==150+nameFont.width(typing.text) && draw.y==130 && draw.frame==8;
    }),"Highscore name caret did not use the original I glyph in the ranked row.");
    ui.handle(input);
    require(ui.page()==Page::highscores,"Name entry did not return to the score screen.");
    sven2::Highscores saved(saves/L"XS-0.tsv");
    require(saved.entries().front().score==1250,"Results did not persist the submitted highscore.");
    sven2::GameUi hover(engine,saves/L"hover");
    hover.show(Page::packs);
    sven2::UiInput mouse;
    mouse.mouseMoved=true; mouse.mouse={200,170};
    hover.handle(mouse);
    const auto& hoverDraws=hover.draws(0);
    const auto* green=engine.loadSprite("Sven2/gfx/fontgreen1.bmp",88,1);
    require(std::any_of(hoverDraws.begin(),hoverDraws.end(),[&](const auto& draw) {
        return draw.sprite==green && draw.y==158;
    }),"Hover did not make the episode text green.");
    mouse.click=true;
    require(hover.handle(mouse)==sven2::UiAction::start,"Clicking a hovered episode did not start it.");
    sven2::GameUi exitMenu(engine,saves/L"exit-menu");
    sven2::UiInput exitClick;exitClick.click=true;exitClick.mouse={650,555};
    exitMenu.handle(exitClick);
    require(exitMenu.page()==Page::quit,"Exit button did not open confirmation.");
    const auto* logo=engine.loadSprite("Sven2/gfx/SvenLogo.bmp",1,1);
    const auto* logoDetail=engine.loadSprite("Sven2/gfx/SvenLogo_O.bmp",4,1);
    // Model a retained completed session being supplied after returning to title.
    const std::vector<sven2::SpriteDraw> staleLevel={{logo,-777,-777,0}};
    const auto& exitDraws=exitMenu.draws(0,&staleLevel);
    require(!exitMenu.usesSceneBackground() && std::none_of(exitDraws.begin(),exitDraws.end(),[](const auto& draw) {
        return draw.x==-777;
    }),"Title exit confirmation drew the retained level behind the menu.");
    require(std::any_of(exitDraws.begin(),exitDraws.end(),[&](const auto& draw) {
        return draw.sprite==logo && draw.x==234 && draw.y==180;
    }) && std::any_of(exitDraws.begin(),exitDraws.end(),[&](const auto& draw) {
        return draw.sprite==logoDetail && draw.x==429 && draw.y==316;
    }),"Exit confirmation removed the title artwork behind it.");
    require(std::count_if(exitDraws.begin(),exitDraws.end(),[](const auto& draw) {
        return draw.y==545;
    })==4,"Exit confirmation removed the four menu buttons.");
    exitMenu.handle(chooseNo);
    require(exitMenu.handle(input)==sven2::UiAction::none && exitMenu.page()==Page::title,
            "Declining exit did not return to the title.");
    exitMenu.show(Page::pause);
    const auto& pauseDraws=exitMenu.draws(0,&staleLevel);
    require(exitMenu.usesSceneBackground() && pauseDraws.front().x==-777,"Pause dialog lost its active level background.");
    sven2::RoundResult completed{1250,1,180,3,3,0,0,0,false,false,3,2};
    ui.showResults(completed);
    const auto firstDrawCount=ui.draws(0.1f).size();
    ui.showResults(completed);
    require(ui.draws(0).size()==firstDrawCount,"Updating bonus values restarted the blinking prompt every frame.");
    const auto* scoreSprite=engine.loadSprite("Sven2/gfx/score.bmp",4,1);
    const auto& bonusDraws=ui.draws(0);
    require(std::any_of(bonusDraws.begin(),bonusDraws.end(),[&](const auto& draw) {
        return draw.sprite==scoreSprite && draw.x==395 && draw.y==380 && draw.frame==2;
    }),"Bonus screen lost the session's score-icon animation frame.");
    const auto* sheepSprite=engine.loadSprite("Sven2/gfx/Sheep/Sheep_Stand.bmp",4,1);
    require(std::any_of(bonusDraws.begin(),bonusDraws.end(),[&](const auto& draw) {
        return draw.sprite==sheepSprite && draw.x==310-sheepSprite->frameWidth/2 && draw.y==240-sheepSprite->frameHeight/2;
    }),"Bonus sheep sprite was not centered at the original coordinates.");
    completed.episodeComplete=true;
    completed.totalSheep=17;
    ui.showResults(completed);
    const auto* packImage=engine.loadSprite("Sven2/gfx/ScoreScreen_BG_levelpack.bmp",1,1);
    const auto& finishDraws=ui.draws(0.1f);
    require(std::any_of(finishDraws.begin(),finishDraws.end(),[&](const auto& draw) {
        return draw.sprite==packImage && draw.x==315 && draw.y==294;
    }),"Pack completion screen omitted its original illustration.");
    sven2::log("PASS: flat episode selection, hover, highscore traversal, pause and four help tabs.");
}
void checkScores(const std::filesystem::path& saves) {
    const auto file=saves/L"persistence.tsv";
    sven2::Highscores scores(file,{{"A",1000,0},{"B",900,0}});
    scores.submit(u8"J\u00f6rg",1050,2,3);
    const auto& rows=scores.entries();
    require(rows.front().score==1050 && rows.front().level==2,"Score was not ranked.");
    sven2::Highscores reload(file);
    require(reload.entries().front().name==u8"J\u00f6rg","UTF-8 score name did not survive reload.");
    require(reload.entries().front().sheep==3,"Highscore animal count did not survive reload.");
    const auto legacy=saves/L"legacy.tsv";
    { std::ofstream output(legacy); output<<"800\tOld\t2\n"; }
    sven2::Highscores old(legacy);
    require(old.entries().front().score==800 && old.entries().front().sheep==0,"Earlier score tables stopped loading.");
    bool rejected=false;
    try { reload.submit("",0); } catch(const std::invalid_argument&) { rejected=true; }
    require(rejected,"Empty score name was accepted.");
    sven2::log("PASS: highscore ranking, Unicode, atomic persistence and validation.");
}
void checkGlyphs(sven2::Engine& engine) {
    const auto* placeholder=engine.loadSprite("Sven2/gfx/noanim.bmp",1,1);
    require((placeholder->pixels.front()&0xffffff)==0xff00ff,
            "Original RGB565-equivalent chroma key was not normalized.");
    const int expected[]={74,61,9,33,42,32,46,37,39,60,59,66,67,73,86,43,85};
    for(bool smallFont:{false,true}) {
        sven2::BitmapFont font(engine,smallFont);
        std::vector<sven2::SpriteDraw> glyphs;
        font.append(glyphs,u8"1.Jendrik?!,';\u00dfo\u00f8",0,0);
        require(glyphs.size()==std::size(expected),"Font dropped punctuation or Unicode glyphs.");
        for(size_t i=0;i<glyphs.size();++i)
            require(glyphs[i].frame==expected[i],"Font disagrees with the original character map.");
    }
    sven2::log("PASS: original font punctuation, normal o, crossed o and sharp s mapping.");
}
void checkMovement(sven2::Engine& engine) {
    sven2::LevelScene wall(engine);
    require(wall.initialTime()==180 && wall.remainingTargets()==3,"Original tutorial state was not loaded.");
    step(wall,1,0);step(wall,1,0);
    for(int i=0;i<25;++i) wall.move(0.1f,0,-1);
    require(wall.player().x==440 && wall.player().y==40,"Authored wall did not stop Sven.");
    sven2::LevelScene bush(engine);
    step(bush,1,0);
    for(int i=0;i<25;++i) bush.move(0.1f,0,-1);
    step(bush,1,0);
    for(int i=0;i<5;++i) bush.move(0.1f,0,1);
    require(bush.player().x==440 && bush.player().y==-40,"North bush collision regressed.");
    sven2::LevelScene puddle(engine);
    step(puddle,-1,0);step(puddle,-1,0);
    for(int i=0;i<20 && !puddle.takeSplashSound();++i) puddle.move(0.1f,0,-1);
    const auto landing=puddle.player();
    require((landing.x==80 && landing.y==80)||(landing.x==0 && landing.y==440),"Puddle landed outside its authored ring.");
    for(int i=0;i<5;++i) puddle.move(0.1f,0,1);
    require(puddle.player().x==landing.x && puddle.player().y==landing.y,"Sven moved during the splash state.");
    sven2::log("PASS: original tutorial timer, wall, bush and splash lock.");
}

void checkInteraction(sven2::Engine& engine) {
    sven2::LevelScene scene(engine);
    for(int i=0;i<3;++i) step(scene,-1,0);
    for(int i=0;i<3;++i) step(scene,0,-1);
    scene.move(0.01f,0,-1);
    require(scene.player().x==240 && scene.player().y==160,"Sheep approach route was blocked.");
    int score=0;
    float bonusTime=0;
    for(int i=0;i<2400 && scene.remainingTargets()==3;++i) {
        scene.update(1.0f/60.0f,0,0,i%8==0);
        score+=scene.takeScoreDelta();
        bonusTime+=scene.takeTimeDelta();
    }
    require(scene.remainingTargets()==2 && scene.completedSheep()==1,"Repeated action did not resolve the approached sheep.");
    require(score>0 && (bonusTime==5 || bonusTime==10),"Sheep score and time bonus did not stay separate.");
    require(!scene.takeSounds().empty(),"Interaction did not emit its original sound events.");
    driveTutorial(engine,[&](float dt,int dx,int dy,bool action) { scene.update(dt,dx,dy,action); },
                  [&]() -> const auto& { return scene.draws(); },[&] { return scene.remainingTargets(); });
    require(scene.completed() && scene.completedSheep()==3,"Tutorial did not reach completion after resolving its three targets.");
    sven2::log("PASS: normal-entry sheep interaction, repeated input, target resolution, score and time rewards.");
}

void checkReportedGameplay(sven2::Engine& engine) {
    const auto& packs=engine.data().packs();
    const auto xs=std::find_if(packs.begin(),packs.end(),[](const auto& pack) { return pack.name=="XS"; });
    const auto walking=fixtureLevel(engine,"walking",{{0,240,200,0},{2,640,400,0}});
    sven2::LevelScene east(engine,*xs,walking,1),west(engine,*xs,walking,1);
    east.update(0.05f,1,0,false);west.update(0.05f,-1,0,false);
    require(east.player().mirror && !west.player().mirror,"Sven's side-running sprite faces away from movement.");

    const auto angry=fixtureLevel(engine,"angry",{{0,240,200,0},{2,240,160,0,10},{2,640,400,0}});
    sven2::LevelScene kicked(engine,*xs,angry,1);
    const auto variants=sven2::loadInteractionAnimations(engine);
    std::set<const sven2::Sprite*> usedVariants;
    int kickSounds=0;
    for(int frame=0;frame<60*60 && kicked.remainingTargets()==2;++frame) {
        kicked.update(1.0f/60,0,0,frame%8==0);
        if(std::any_of(variants.begin(),variants.end(),[&](const auto& variant) {
            return variant.sprite==kicked.player().sprite;
        })) usedVariants.insert(kicked.player().sprite);
        for(const auto& sound:kicked.takeSounds()) if(sound=="Sven2/sfx/Sheep_kickSven.ogg") ++kickSounds;
    }
    require(kickSounds==1,"Angry sheep repeatedly kicked during one stationary approach.");
    require(kicked.remainingTargets()==1,"Sheep could not be interacted with after its kick.");
    require(usedVariants.size()==1,"Repeated action switched animation during one sheep interaction.");

    const auto smelly=fixtureLevel(engine,"smelly",{{0,200,240,0},{2,240,160,0,6},{2,640,400,0}},135);
    sven2::LevelScene stink(engine,*xs,smelly,1);
    stink.update(0.01f,0,0,false);
    step(stink,0,-1);
    const auto sounds=stink.takeSounds();
    require(std::find(sounds.begin(),sounds.end(),"Sven2/sfx/item_stinky.ogg")!=sounds.end(),"Stinky mushroom was not picked up.");
    require(std::any_of(sounds.begin(),sounds.end(),[](const auto& sound) {
        return sound=="Sven2/sfx/Sheep_beamup.ogg" || sound=="Sven2/sfx/Sheep_angry2.ogg";
    }),"Smell did not emit the sheep's anger reaction.");
    const auto* mood=engine.loadSprite("Sven2/gfx/symbols.bmp",12,1);
    const auto& stinkDraws=stink.draws();
    require(std::any_of(stinkDraws.begin(),stinkDraws.end(),[&](const auto& draw) {
        return draw.sprite==mood && draw.y==160 && draw.frame>=10;
    }),"Smell did not anger the neighboring sheep.");

    const auto cold=fixtureLevel(engine,"frigida",{{0,240,200,0},{2,240,160,0,6}});
    sven2::LevelScene frigida(engine,*xs,cold,1);
    for(int frame=0;frame<90;++frame) frigida.update(1.0f/60,0,0,frame%8==0);
    const auto* kettle=engine.loadSprite("Sven2/gfx/symbol_kettle.bmp",12,1);
    const auto& coldDraws=frigida.draws();
    require(std::any_of(coldDraws.begin(),coldDraws.end(),[&](const auto& draw) { return draw.sprite==kettle && draw.frame>0; }),
            "Frigida interaction hid its kettle indicator.");
    const auto kettlePosition=std::find_if(coldDraws.begin(),coldDraws.end(),[&](const auto& draw) { return draw.sprite==kettle; });
    const auto interactionPosition=std::find_if(coldDraws.begin(),coldDraws.end(),[&](const auto& draw) { return draw.sprite==frigida.player().sprite; });
    require(kettlePosition>interactionPosition,"Frigida interaction painted over its kettle indicator.");

    const auto enemy=fixtureLevel(engine,"fight",{{0,240,200,0},{4,280,200,6},{2,640,400,0}});
    sven2::LevelScene fight(engine,*xs,enemy,1);
    const auto* cloud=engine.loadSprite("Sven2/gfx/anim_cloud.bmp",10,1);
    bool fought=false;
    for(int frame=0;frame<600 && !fought;++frame) {
        fight.update(1.0f/60,0,0,false);
        const auto& draws=fight.draws();
        fought=std::any_of(draws.begin(),draws.end(),[&](const auto& draw) { return draw.sprite==cloud; });
    }
    require(fought,"Enemy battle did not render the fight cloud.");
    const auto animals=fixtureLevel(engine,"animal-meters",{{0,240,200,0},{12,240,160,0},{11,400,160,0},{13,560,160,0}});
    sven2::LevelScene otherAnimals(engine,*xs,animals,1);
    const auto& animalDraws=otherAnimals.draws();
    require(std::count_if(animalDraws.begin(),animalDraws.end(),[&](const auto& draw) {
        return draw.sprite==mood;
    })==3,"Chicken, goat and pig did not all display their impatience meters.");
    otherAnimals.update(0.1f,0,0,true);
    const auto& goatDraws=otherAnimals.draws();
    const auto goatMeter=std::find_if(goatDraws.begin(),goatDraws.end(),[&](const auto& draw) {
        return draw.sprite==mood && draw.y==150 && draw.x<320;
    });
    const auto goatInteraction=std::find_if(goatDraws.begin(),goatDraws.end(),[&](const auto& draw) {
        return draw.sprite==otherAnimals.player().sprite;
    });
    require(goatMeter!=goatDraws.end() && goatMeter>goatInteraction,
            "Goat interaction covered or omitted its impatience meter.");
    sven2::log("PASS: run direction, single kick recovery, stable interaction, smell, Frigida kettle and fight animation.");
}

void checkEnemyAcquisition(sven2::Engine& engine) {
    const auto& packs=engine.data().packs();
    const auto xs=std::find_if(packs.begin(),packs.end(),[](const auto& pack) { return pack.name=="XS"; });
    auto hasSound=[&](const std::vector<std::string>& sounds,const char* name) {
        const auto path=engine.data().soundPath(name);
        return !path.empty() && std::find(sounds.begin(),sounds.end(),path)!=sounds.end();
    };
    const auto outside=fixtureLevel(engine,"dog-boundary",{{0,240,200,0},{4,360,200,6},{2,640,400,0}});
    const auto reservation=fixtureLevel(engine,"player-reservation",{{0,240,200,0},{5,240,240,0,6,0,0,0.15f},{2,640,400,0}});
    sven2::LevelScene moving(engine,*xs,reservation,1);
    moving.update(0.2f,1,0,false);
    require(moving.player().x>260 && moving.player().x<280,"Reservation fixture did not cross the half-step point.");
    const auto* farmerNorth=engine.loadSprite("Sven2/gfx/Guy/anim_guy_walk_back.bmp",16,1);
    const auto* farmerDiagonal=engine.loadSprite("Sven2/gfx/Guy/anim_guy_walk_diagonal_back.bmp",16,1);
    const auto& movingDraws=moving.draws();
    require(std::none_of(movingDraws.begin(),movingDraws.end(),[&](const auto& draw) {
        return draw.sprite==farmerNorth || (draw.sprite==farmerDiagonal && draw.mirror);
    }),"Enemy entered Sven's committed source or reserved destination during a half-finished step.");
    sven2::LevelScene distant(engine,*xs,outside,1);
    distant.update(0.1f,0,0,false);
    require(!hasSound(distant.takeSounds(),"SfxDogFoundSven"),"Dog acquired Sven at the excluded D=3 boundary.");
    const auto inside=fixtureLevel(engine,"dog-notice",{{0,240,200,0},{4,320,200,6},{2,640,400,0}});
    sven2::LevelScene noticed(engine,*xs,inside,1);
    noticed.update(0.01f,0,0,false);
    require(hasSound(noticed.takeSounds(),"SfxDogFoundSven"),"Dog did not bark on acquiring Sven.");
    noticed.update(0.2f,0,0,false);
    require(!hasSound(noticed.takeSounds(),"SfxDogFoundSven"),"Dog repeated its acquisition bark while walking the same tile.");

    const auto boxed=fixtureLevel(engine,"dog-blocked",{{0,240,200,0},{4,320,200,6},{2,640,400,0}});
    {
        std::fstream file(engine.data().root()/std::filesystem::u8path(boxed.map),std::ios::in|std::ios::out|std::ios::binary);
        const int dogTile=((200+60)/40+1)*22+(320+60)/40+1;
        for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
            if(!dx && !dy) continue;
            const int tile=dogTile+dy*22+dx;
            const int wall=1;
            file.seekp(16+tile*24);
            file.write(reinterpret_cast<const char*>(&wall),4);
        }
        require(static_cast<bool>(file),"Cannot author blocked-enemy fixture.");
    }
    sven2::LevelScene blocked(engine,*xs,boxed,1);
    blocked.update(0.01f,0,0,false);
    require(hasSound(blocked.takeSounds(),"SfxDogFoundSven"),"Wall incorrectly suppressed enemy detection.");
    blocked.update(2.0f,0,0,false);
    require(!hasSound(blocked.takeSounds(),"SfxDogFoundSven"),"Blocked enemy ignored its retry delay and repeated notice.");

    const auto bone=fixtureLevel(engine,"dog-bone",{{0,240,200,0},{4,320,200,6},{9,320,240,0},{2,640,400,0}});
    sven2::LevelScene distracted(engine,*xs,bone,1);
    distracted.update(0.01f,0,0,false);
    const auto notice=distracted.takeSounds();
    require(hasSound(notice,"SfxDogFoundBone") && !hasSound(notice,"SfxDogFoundSven"),
            "Dog did not prioritize a nearby bone over Sven.");
    distracted.update(1.0f,0,0,false);
    require(hasSound(distracted.takeSounds(),"SfxDogDig"),"Dog failed to consume its bone and start digging.");

    const auto beer=fixtureLevel(engine,"farmer-beer",{{0,640,400,0},{5,240,200,2},{7,280,200,0},{2,640,120,0}});
    sven2::LevelScene drinking(engine,*xs,beer,1);
    bool foundBeer=false,drank=false;
    for(int frame=0;frame<60*15 && !drank;++frame) {
        drinking.update(1.0f/60,0,0,false);
        const auto sounds=drinking.takeSounds();
        foundBeer=foundBeer || hasSound(sounds,"SfxGuyFoundBeer");
        drank=drank || hasSound(sounds,"SfxLarsDrink");
    }
    require(foundBeer && drank,"Farmer did not notice and drink the bottle with its original sounds.");
    drinking.update(12.0f,0,0,false);
    const auto sleepSounds=drinking.takeSounds();
    require(hasSound(sleepSounds,"SfxLarsFall") && hasSound(sleepSounds,"SfxLarsSleep"),
            "Farmer's drinking-to-sleep transition omitted its fall or snore sound.");

    const auto idle=fixtureLevel(engine,"farmer-idle",{{0,240,200,0},{5,320,200,6},{2,640,400,0}});
    sven2::LevelScene farmer(engine,*xs,idle,1);
    farmer.update(0.1f,0,0,false);
    require(!hasSound(farmer.takeSounds(),"SfxGuyFoundSven"),"Awake farmer used a dog-style proximity acquisition.");
    const auto active=fixtureLevel(engine,"farmer-activity",{{0,240,200,0},{2,240,160,0},{2,640,400,0},{5,640,40,4}});
    sven2::LevelScene activity(engine,*xs,active,1);
    activity.update(0.01f,0,0,true);
    activity.update(0.02f,0,0,false);
    require(hasSound(activity.takeSounds(),"SfxGuyFoundSven"),"Farmer ignored Sven's interaction outside proximity range.");
    for(int kind:{3,6}) {
        const char* sound=kind==3?"SfxAlienFoundSven":"SfxBroemseFoundSven";
        const auto nearby=fixtureLevel(engine,"notice-"+std::to_string(kind),{{0,240,200,0},{kind,360,200,6},{2,640,400,0}});
        sven2::LevelScene acquisition(engine,*xs,nearby,1);
        acquisition.update(0.01f,0,0,false);
        require(hasSound(acquisition.takeSounds(),sound),"Alien or girl did not notice Sven inside its original square range.");
        const auto excluded=fixtureLevel(engine,"boundary-"+std::to_string(kind),{{0,240,200,0},{kind,400,200,6},{2,640,400,0}});
        sven2::LevelScene boundary(engine,*xs,excluded,1);
        boundary.update(0.01f,0,0,false);
        require(!hasSound(boundary.takeSounds(),sound),"Alien or girl noticed Sven at the excluded D=4 boundary.");
    }
    sven2::log("PASS: strict dog detection range, one-shot notice, bone priority/digging, farmer drink and activity detection.");
}

void checkRound(sven2::Engine& engine) {
    const auto& packs=engine.data().packs();
    auto pack=std::find_if(packs.begin(),packs.end(),[](const auto& item) { return item.name=="XS"; });
    require(pack!=packs.end(),"XS package missing.");
    sven2::GameSession game(engine,*pack,pack->levelsets.front());
    driveTutorial(engine,[&](float dt,int dx,int dy,bool action) { game.update(dt,dx,dy,action); },
                  [&]() -> const auto& { return game.draws(); },[&] { return game.remainingTargets(); });
    require(!game.results(),"Level results appeared before the original completion delay.");
    game.update(1.51f,0,0,false);
    require(game.results() && !game.ready() && !game.gameOver(),"Completed tutorial did not enter bonus counting.");
    const int expected=game.score()+game.timeBonus()+30+75;
    game.skipBonuses();
    require(game.ready() && game.score()==expected,"Time, sheep and lives bonuses were not converted correctly.");
    require(!game.takeSounds().empty(),"Round did not emit audio events.");
    const int episodeScore=game.score();
    game.advance();
    require(!game.results() && game.levelNumber()==2 && game.score()==episodeScore,"Level transition reset the episode score.");
    sven2::log("PASS: complete tutorial round, result transition and exact time/sheep/lives bonus totals.");
    sven2::LevelSetInfo finalSet{"Single tutorial",{pack->levelsets.front().levels.front()}};
    sven2::GameSession finalLevel(engine,*pack,finalSet);
    driveTutorial(engine,[&](float dt,int dx,int dy,bool action) { finalLevel.update(dt,dx,dy,action); },
                  [&]() -> const auto& { return finalLevel.draws(); },[&] { return finalLevel.remainingTargets(); });
    finalLevel.update(1.51f,0,0,false);
    require(finalLevel.results() && !finalLevel.result().episodeComplete,"Final level skipped its bonus screen.");
    finalLevel.skipBonuses();
    require(finalLevel.ready() && !finalLevel.finished(),"Final bonus completion prematurely opened the pack screen.");
    finalLevel.takeSounds();
    finalLevel.advance();
    const auto sounds=finalLevel.takeSounds();
    require(finalLevel.finished() && finalLevel.result().episodeComplete &&
            std::find(sounds.begin(),sounds.end(),engine.data().soundPath("SfxFinishPack"))!=sounds.end(),
            "Acknowledging the final bonus did not open pack completion with its sound.");
    sven2::log("PASS: final level bonus, acknowledgement and separate pack-completion transition.");
}

void checkLifeChanges(sven2::Engine& engine,bool capture=false) {
    const auto& packs=engine.data().packs();
    const auto xs=std::find_if(packs.begin(),packs.end(),[](const auto& pack) { return pack.name=="XS"; });
    const auto arena=fixtureLevel(engine,"life-alien",{{0,240,200,0},{3,280,200,6},{2,640,400,0}});
    const auto* icon=engine.loadSprite("Sven2/gfx/Life_icon.bmp",1,1);
    const auto newLife=engine.data().soundPath("SfxNewLife");
    const auto loseLife=engine.data().soundPath("SfxLoseLife");
    sven2::LevelSetInfo freshSet{"Zero score",{arena}};
    sven2::GameSession fresh(engine,*xs,freshSet);
    bool refunded=false;
    for(int frame=0;frame<60*60 && !refunded;++frame) {
        fresh.update(1.0f/60,0,0,false);
        const auto sounds=fresh.takeSounds();
        refunded=std::find(sounds.begin(),sounds.end(),newLife)!=sounds.end();
    }
    require(refunded && fresh.lives()==3,"Original zero-score first-hit refund was not preserved.");
    fresh.update(0.2f,0,0,false);
    const auto& graceDraws=fresh.draws();
    require(std::count_if(graceDraws.begin(),graceDraws.end(),[&](const auto& draw) {
        return draw.sprite==icon && draw.y==565;
    })==2,"Refund did not temporarily reduce the displayed life count.");
    require(std::any_of(graceDraws.begin(),graceDraws.end(),[&](const auto& draw) {
        return draw.sprite==icon && draw.y<500;
    }),"Refund did not spawn the incoming life icon.");
    if(capture) engine.drawFrame(graceDraws,"life-refund.bmp");

    auto setup=xs->levelsets.front().levels.front();
    auto bytes=engine.readAsset(setup.map);
    std::uint32_t nameLength=0;
    std::memcpy(&nameLength,bytes.data()+12,4);
    const float timer=800;
    std::memcpy(bytes.data()+16+nameLength+9044,&timer,4);
    setup.map="XS/levels/check-life-score.lvl";
    {
        std::ofstream file(engine.data().root()/std::filesystem::u8path(setup.map),std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
        require(static_cast<bool>(file),"Cannot create life-score setup.");
    }
    sven2::LevelSetInfo campaign{"Lives",{setup,arena}};
    sven2::GameSession game(engine,*xs,campaign);
    driveTutorial(engine,[&](float dt,int dx,int dy,bool action) { game.update(dt,dx,dy,action); },
                  [&]() -> const auto& { return game.draws(); },[&] { return game.remainingTargets(); });
    require(game.remainingTargets()==0,"Life setup tutorial did not finish.");
    game.update(1.51f,0,0,false);
    game.skipBonuses();
    require(game.ready() && game.score()>750,"Life setup did not cross the original score threshold.");
    game.advance();
    game.update(1.0f/60,0,0,false);
    require(game.lives()==4,"Campaign score did not grant the next level's extra life.");
    const auto& gainDraws=game.draws();
    require(std::count_if(gainDraws.begin(),gainDraws.end(),[&](const auto& draw) {
        return draw.sprite==icon && draw.y==565;
    })==3,"Extra life appeared in the HUD before its animation arrived.");
    require(std::any_of(gainDraws.begin(),gainDraws.end(),[&](const auto& draw) {
        return draw.sprite==icon && draw.x==96 && draw.y<0;
    }),"Extra life did not start falling from above the screen.");
    game.update(0.35f,0,0,false);
    if(capture) engine.drawFrame(game.draws(),"life-gain.bmp");
    game.update(2.0f,0,0,false);
    const auto& landed=game.draws();
    require(std::count_if(landed.begin(),landed.end(),[&](const auto& draw) {
        return draw.sprite==icon && draw.y==565;
    })==4,"Incoming life did not join the HUD on landing.");
    game.takeSounds();
    bool lost=false;
    for(int frame=0;frame<60*60 && !lost;++frame) {
        game.update(1.0f/60,0,0,false);
        const auto sounds=game.takeSounds();
        lost=std::find(sounds.begin(),sounds.end(),loseLife)!=sounds.end();
    }
    require(lost && game.lives()==3,"Alien attack failed to take a logical life after campaign score carried over.");
    game.update(0.2f,0,0,false);
    const auto& lossDraws=game.draws();
    require(std::count_if(lossDraws.begin(),lossDraws.end(),[&](const auto& draw) {
        return draw.sprite==icon && draw.y==565;
    })==3,"Alien life loss did not reduce the stationary HUD icons.");
    if(capture) engine.drawFrame(lossDraws,"life-loss.bmp");
    sven2::log("PASS: zero-score refund animation, campaign extra-life drop/landing, alien logical loss and outgoing icon.");
}

void checkPlayerItems(sven2::Engine& engine,bool capture=false) {
    const auto& packs=engine.data().packs();
    const auto xs=std::find_if(packs.begin(),packs.end(),[](const auto& pack) { return pack.name=="XS"; });
    struct Powerup { int code; const char* effect; int frames; const char* sound; const char* duration; int distance; };
    const Powerup items[]={
        {132,"symbol_power.bmp",13,"SfxFirework","FireworkDuration",22},
        {134,"symbol_bubbles_good.bmp",6,"SfxImpotence","ImpotentDuration",9},
        {135,"symbol_smell.bmp",14,"SfxSmell","SmellDuration",14},
        {136,"symbol_flowers.bmp",8,"SfxRose","FlowerDuration",14},
    };
    for(const auto& item:items) {
        const auto level=fixtureLevel(engine,"pickup-"+std::to_string(item.code),{{0,240,200,0},{2,640,400,0}},item.code);
        sven2::LevelScene scene(engine,*xs,level,1);
        scene.update(0.01f,0,0,false);
        const auto sounds=scene.takeSounds();
        require(std::any_of(sounds.begin(),sounds.end(),[&](const auto& sound) {
            return _stricmp(sound.c_str(),engine.data().soundPath(item.sound).c_str())==0;
        }),
                "Player pickup omitted its original sound.");
        const auto* effect=engine.loadSprite(std::string("Sven2/gfx/")+item.effect,static_cast<short>(item.frames),1);
        const auto& draws=scene.draws();
        require(std::any_of(draws.begin(),draws.end(),[&](const auto& draw) { return draw.sprite==effect; }),
                "Player pickup omitted its visual effect.");
        if(capture && item.code==134) engine.drawFrame(draws,"slow-motion.bmp");
        scene.update(0.1f,1,0,false);
        require(scene.player().x>=240+item.distance && scene.player().x<=241+item.distance,
                "Player pickup did not apply the original movement-speed modifier.");
        scene.update(static_cast<float>(engine.data().parameter(item.duration,20))+1,0,0,false);
        const auto& expired=scene.draws();
        require(std::none_of(expired.begin(),expired.end(),[&](const auto& draw) { return draw.sprite==effect; }),
                "Player pickup effect outlived its configured duration.");
    }
    const auto gift=fixtureLevel(engine,"gift-pickup",{{0,240,200,0},{2,640,400,0}},133);
    for(std::uint32_t seed=1;seed<=64;++seed) {
        sven2::LevelScene scene(engine,*xs,gift,seed);
        scene.update(0.01f,0,0,false);
        const int lives=scene.takeLivesGained();
        const float time=scene.takeTimeDelta();
        require((lives==1 && time==0) || (lives==0 && (time==5 || time==10 || time==15 || time==25)),
                "Gift pickup awarded a reward outside the original nested random branches.");
        require(scene.takeScoreDelta()==0,"Gift pickup invented a score award.");
        scene.update(0.1f,0,0,false);
        require(scene.takeLivesGained()==0 && scene.takeTimeDelta()==0,"Gift was consumed more than once.");
    }
    sven2::log("PASS: all four timed player powerups, speed/effect expiry, and 64 gift reward/consume scenarios.");
}

void checkAttackReservations(sven2::Engine& engine) {
    const auto& packs=engine.data().packs();
    const auto xs=std::find_if(packs.begin(),packs.end(),[](const auto& pack) { return pack.name=="XS"; });
    const auto level=fixtureLevel(engine,"attack-reservation",{{0,240,120,4},{4,240,40,4},{2,640,400,0}},134);
    bool exercised=false;
    for(std::uint32_t seed=1;seed<=32 && !exercised;++seed) {
        sven2::LevelScene scene(engine,*xs,level,seed);
        bool interrupted=false;
        for(int frame=0;frame<600;++frame) {
            const auto previous=scene.player();
            scene.update(1.0f/60,1,0,false);
            for(const auto& sound:scene.takeSounds())
                if(sound.find("fightdog.ogg")!=std::string::npos && previous.x%40!=0 && previous.x<640)
                    interrupted=true;
            if(scene.takeLifeHits()>0) {
                if(interrupted) {
                    const auto before=scene.player();
                    scene.update(0.5f,1,0,false);
                    require(scene.player().x>before.x+20,
                            "An interrupted step left an impassable player reservation after the attack.");
                    exercised=true;
                }
                break;
            }
        }
    }
    require(exercised,"Attack reservation scenario did not exercise a mid-step capture.");
    sven2::log("PASS: mid-step enemy capture releases the cancelled player destination after recovery.");
}

void checkEffectAndAnimationStates(sven2::Engine& engine,bool capture=false) {
    const auto& packs=engine.data().packs();
    const auto xs=std::find_if(packs.begin(),packs.end(),[](const auto& pack) { return pack.name=="XS"; });
    auto contains=[](const std::vector<sven2::SpriteDraw>& draws,const sven2::Sprite* sprite) {
        return std::any_of(draws.begin(),draws.end(),[&](const auto& draw) { return draw.sprite==sprite; });
    };
    const auto cold=fixtureLevel(engine,"frigida-continuation",{{0,240,200,0},{2,240,160,0}});
    sven2::LevelScene frigida(engine,*xs,cold,1);
    const auto* combined=engine.loadSprite("Sven2/gfx/Frigida/anim_frigida_final_normal_back.bmp",3,1);
    frigida.update(0,0,0,true);
    require(frigida.player().sprite==combined,"Frigida's initial frame exposed standing Sven.");
    for(int frame=0;frame<240 && !frigida.completed();++frame) {
        frigida.update(1.0f/60,0,0,frame%2==1);
        if(!frigida.completed()) require(frigida.player().sprite==combined,"Queued Frigida continuation flickered to standing Sven.");
    }

    const auto artifact=fixtureLevel(engine,"artifact-transform",{{0,240,200,0},{2,640,400,0}},128);
    sven2::LevelScene alienPower(engine,*xs,artifact,1);
    require(!alienPower.ufoActive(),"Artifact UFO was active before pickup.");
    const auto* alienIcon=engine.loadSprite("Sven2/gfx/icon_alien.bmp",13,1);
    bool ascended=false,hidden=false,landed=false;
    for(int frame=0;frame<60*60 && !landed;++frame) {
        alienPower.update(1.0f/60,0,0,false);
        if(frame==0) require(alienPower.ufoActive(),"Artifact pickup did not begin the UFO audio lifecycle.");
        const auto hero=alienPower.player();
        ascended=ascended || (hero.sprite && hero.y<200);
        hidden=hidden || !hero.sprite;
        const auto& draws=alienPower.draws();
        require(std::all_of(draws.begin(),draws.end(),[](const auto& draw) { return draw.sprite!=nullptr; }),
                "Hidden artifact phase published a null drawing sprite.");
        if(contains(draws,alienIcon)) {
            require(hero.x==240 && hero.y==200,"Artifact beam moved Sven's ground position to another tile.");
            require(std::any_of(draws.begin(),draws.end(),[&](const auto& draw) {
                return draw.sprite==alienIcon && draw.x==268 && draw.y==196;
            }),"Alien powerup used the wrong icon geometry or following position.");
            landed=true;
            if(capture) engine.drawFrame(draws,"artifact-landed.bmp");
        }
    }
    require(ascended && hidden && landed && alienPower.takeLifeHits()==0,"Artifact ascent/wait/landing sequence was incomplete.");
    require(alienPower.ufoActive(),"UFO lifecycle ended before departure.");
    alienPower.update(16.0f,0,0,false);
    require(!alienPower.ufoActive(),"UFO lifecycle stayed active after departure.");
    require(!contains(alienPower.draws(),alienIcon),"Alien powerup did not expire.");
    const auto movingArtifact=fixtureLevel(engine,"artifact-walking",{{0,240,240,0},{2,640,400,0}});
    {
        std::fstream file(engine.data().root()/std::filesystem::u8path(movingArtifact.map),std::ios::in|std::ios::out|std::ios::binary);
        const int tile=((200+60)/40+1)*22+(240+60)/40+1;
        const char artifactCode=static_cast<char>(128);
        file.seekp(16+tile*24+12);file.write(&artifactCode,1);
        require(static_cast<bool>(file),"Cannot author moving artifact fixture.");
    }
    sven2::LevelScene movingPower(engine,*xs,movingArtifact,1);
    step(movingPower,0,-1);
    require(movingPower.player().y==200,"Moving artifact fixture did not reach the pickup tile.");
    bool movingLanded=false;
    for(int frame=0;frame<60*60 && !movingLanded;++frame) {
        movingPower.update(1.0f/60,0,0,false);
        movingLanded=contains(movingPower.draws(),alienIcon);
    }
    require(movingLanded && movingPower.player().x==240 && movingPower.player().y==200,
            "Artifact collected while walking returned Sven to the wrong committed tile.");

    const auto bone=fixtureLevel(engine,"dig-particles",{{0,640,400,0},{4,240,200,2},{9,280,200,0},{2,640,120,0}});
    sven2::LevelScene dog(engine,*xs,bone,1);
    const auto* dirt=engine.loadSprite("Sven2/gfx/Digging.bmp",18,1);
    bool dug=false;
    for(int frame=0;frame<600 && !dug;++frame) {
        dog.update(1.0f/60,0,0,false);
        dug=contains(dog.draws(),dirt);
    }
    require(dug,"Bone consumption omitted its digging particles.");
    if(capture) engine.drawFrame(dog.draws(),"dog-digging.bmp");
    dog.update(10.0f,0,0,false);
    require(contains(dog.draws(),dirt),"Dig effect did not preserve its original hold/replay lifetime.");
    const auto* dogRest=engine.loadSprite("Sven2/gfx/Dog/anim_dog_sleep_front.bmp",4,1);
    require(contains(dog.draws(),dogRest),"Dog stopped digging before the configured 15 seconds.");
    dog.update(4.5f,0,0,false);
    require(!contains(dog.draws(),dirt),"Dig effect failed to finish its second pass.");
    dog.update(0.7f,0,0,false);
    require(!contains(dog.draws(),dogRest),"Dog remained in the digging state beyond its configured timer.");

    const auto sleeping=fixtureLevel(engine,"farmer-zzz",{{0,640,400,0},{5,240,200,0,6,0,10,20},{2,640,120,0}});
    sven2::LevelScene farmer(engine,*xs,sleeping,1);
    const auto* zzz=engine.loadSprite("Sven2/gfx/Guy/anim_guy_gfx_zzz.bmp",13,1);
    farmer.update(0.1f,0,0,false);
    require(contains(farmer.draws(),zzz),"Sleeping farmer omitted the Zzz overlay.");
    if(capture) engine.drawFrame(farmer.draws(),"farmer-sleeping.bmp");
    const auto sleepingDog=fixtureLevel(engine,"dog-zzz",{{0,640,400,0},{4,240,200,0,6,0,10,20},{2,640,120,0}});
    sven2::LevelScene dogSleep(engine,*xs,sleepingDog,1);
    dogSleep.update(0.1f,0,0,false);
    require(contains(dogSleep.draws(),zzz),"Sleeping dog omitted its Zzz overlay.");
    const auto wakingDog=fixtureLevel(engine,"dog-wake",{{0,240,200,0},{4,320,200,0,6,0,10,20},{2,640,120,0}});
    sven2::LevelScene dogWake(engine,*xs,wakingDog,1);
    dogWake.update(0.1f,0,0,false);
    require(!contains(dogWake.draws(),zzz),"Dog did not wake when Sven entered its original wake range.");

    const auto girlLevel=fixtureLevel(engine,"girl-combined",{{0,240,200,0},{6,240,120,4},{2,640,400,0}});
    sven2::LevelScene girl(engine,*xs,girlLevel,1);
    const auto* girlFront=engine.loadSprite("Sven2/gfx/Broemse/anim_broemse_beating_sven_front.bmp",35,1);
    const auto* svenStand=engine.loadSprite("Sven2/gfx/Sven/sven_stand.bmp",8,1);
    bool played=false;int maximumFrame=0;float time=0;
    for(int frame=0;frame<900;++frame) {
        girl.update(1.0f/60,0,0,false);
        time+=girl.takeTimeDelta();
        const auto& draws=girl.draws();
        if(contains(draws,girlFront)) {
            if(!played && capture) engine.drawFrame(draws,"girl-playing.bmp");
            played=true;
            require(!contains(draws,svenStand),"Girl interaction showed standing Sven alongside the combined animation.");
            for(const auto& draw:draws) if(draw.sprite==girlFront) {
                require(draw.sprite->frameHeight==120,"Girl front animation was sliced into partial rows.");
                maximumFrame=std::max(maximumFrame,draw.frame);
            }
        } else if(played) break;
    }
    require(played && maximumFrame==34 && time==-20 && girl.takeLifeHits()==0,
            "Girl interaction failed its complete 35-frame front sheet or time-only penalty.");
    sven2::log("PASS: Frigida continuity, artifact ground/icon recovery, dig particles, sleep Zzz and complete girl interaction.");
}

void checkEnemyHeadings(sven2::Engine& engine,bool capture=false) {
    const auto& packs=engine.data().packs();
    const auto xs=std::find_if(packs.begin(),packs.end(),[](const auto& pack) { return pack.name=="XS"; });
    constexpr int directions[][2]={{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}};
    constexpr bool mirrors[4][8]={{0,1,1,1,0,0,0,0},{0,1,0,0,0,1,1,0},{0,1,0,0,0,1,1,0},{0,1,1,1,0,0,0,0}};
    const char* family[]={"Alien","Dog","Guy","Broemse"};
    const char* lower[]={"alien","dog","guy","broemse"};
    const char* headings[]={"back","diagonal_back","side","diagonal_front","front","diagonal_front","side","diagonal_back"};
    constexpr short rows[]={12,16,16,8};
    sven2::BitmapFont labels(engine,true);
    const char* directionNames[]={"N","NE","E","SE","S","SW","W","NW"};
    for(int kind=3;kind<=6;++kind) {
        std::array<const sven2::Sprite*,8> sprites{};
        for(int d=0;d<8;++d) sprites[d]=engine.loadSprite(std::string("Sven2/gfx/")+family[kind-3]+"/anim_"+lower[kind-3]+"_walk_"+headings[d]+".bmp",rows[kind-3],1);
        std::set<int> covered;
        std::vector<sven2::SpriteDraw> montage;
        for(int facing=0;facing<8;++facing) for(std::uint32_t seed=1;seed<=4;++seed) {
            const auto level=fixtureLevel(engine,"heading-"+std::to_string(kind)+"-"+std::to_string(facing),
                                          {{0,640,400,0},{kind,240,120,facing,6,0,0,0},{2,640,480,0}});
            sven2::LevelScene scene(engine,*xs,level,seed);
            sven2::SpriteDraw before{};bool walking=false;
            for(int frame=0;frame<600 && !walking;++frame) {
                scene.update(1.0f/60,0,0,false);
                for(const auto& draw:scene.draws()) if(std::find(sprites.begin(),sprites.end(),draw.sprite)!=sprites.end()) {
                    before=draw;walking=true;break;
                }
            }
            require(walking,"Heading fixture never entered its first walking tile.");
            scene.update(0.05f,0,0,false);
            const auto& draws=scene.draws();
            const auto after=std::find_if(draws.begin(),draws.end(),[&](const auto& draw) {
                return std::find(sprites.begin(),sprites.end(),draw.sprite)!=sprites.end();
            });
            require(after!=draws.end(),"Heading fixture left walking before its first short step.");
            const int dx=(after->x>before.x)-(after->x<before.x),dy=(after->y>before.y)-(after->y<before.y);
            int direction=0;while(direction<8 && (dx!=directions[direction][0] || dy!=directions[direction][1])) ++direction;
            require(direction<8 && after->sprite==sprites[direction] && after->mirror==mirrors[kind-3][direction],
                    "Enemy walking sprite or mirroring disagreed with its actual movement direction.");
            if(capture && covered.find(direction)==covered.end()) {
                if(montage.empty()) montage.push_back(draws.front());
                auto posed=*after;
                posed.x=(direction%4)*200+40;posed.y=(direction/4)*240+80;
                montage.push_back(posed);
                labels.append(montage,directionNames[direction],(direction%4)*200+60,(direction/4)*240+40);
            }
            covered.insert(direction);
        }
        require(covered.size()==8,"Heading regression did not cover all eight movement directions.");
        if(capture) engine.drawFrame(montage,(std::string("directions-")+lower[kind-3]+".bmp").c_str());
    }
    sven2::log("PASS: all eight authored walk directions and companion mirrors for all four enemy families.");
}

void checkEnemyItems(sven2::Engine& engine,bool capture=false) {
    const auto& packs=engine.data().packs();
    const auto xs=std::find_if(packs.begin(),packs.end(),[](const auto& pack) { return pack.name=="XS"; });
    auto heard=[&](const std::vector<std::string>& sounds,const char* name) {
        const auto path=engine.data().soundPath(name);
        return std::any_of(sounds.begin(),sounds.end(),[&](const auto& sound) { return _stricmp(sound.c_str(),path.c_str())==0; });
    };
    const auto teddy=fixtureLevel(engine,"girl-teddy",{{0,640,400,0},{6,240,200,2},{8,280,200,0},{2,640,120,0}});
    sven2::LevelScene girl(engine,*xs,teddy,1);
    bool played=false;
    for(int frame=0;frame<1200 && !played;++frame) {
        girl.update(1.0f/60,0,0,false);
        played=heard(girl.takeSounds(),"SfxBroemsePlaysTeddy");
    }
    require(played,"Girl did not consume her teddy and play its interaction sound.");
    girl.update(20.0f,0,0,false);
    const auto* teddyFront=engine.loadSprite("Sven2/gfx/Broemse/anim_broemse_beating_teddybear_diagonal_front.bmp",14,1);
    const auto* teddyBack=engine.loadSprite("Sven2/gfx/Broemse/anim_broemse_beating_teddybear_diagonal_back.bmp",9,1);
    const auto& recovered=girl.draws();
    require(std::none_of(recovered.begin(),recovered.end(),[&](const auto& draw) { return draw.sprite==teddyFront || draw.sprite==teddyBack; }),
            "Girl remained in her teddy interaction after the original sequence completed.");
    const auto playDog=fixtureLevel(engine,"girl-dog",{{0,640,400,0},{6,240,120,4},{4,240,200,0},{2,640,480,0}});
    bool dogInteraction=false;
    for(std::uint32_t seed=1;seed<=16 && !dogInteraction;++seed) {
        sven2::LevelScene pair(engine,*xs,playDog,seed);
        for(int frame=0;frame<1200 && !dogInteraction;++frame) {
            pair.update(1.0f/60,0,0,false);
            dogInteraction=heard(pair.takeSounds(),"SfxBroemsePlaysWotan");
            if(dogInteraction) {
                const auto* dogStand=engine.loadSprite("Sven2/gfx/Dog/Dog_Stand.bmp",8,1);
                const auto& draws=pair.draws();
                require(std::none_of(draws.begin(),draws.end(),[&](const auto& draw) { return draw.sprite==dogStand; }),
                        "Girl-dog interaction retained a separate standing dog.");
            }
        }
    }
    require(dogInteraction,"Girl did not enter the dog interaction in the authored encounter.");
    const auto* artifactSprite=engine.loadSprite("Sven2/gfx/Artifact.bmp",1,1);
    for(int kind=3;kind<=6;++kind) {
        const auto level=fixtureLevel(engine,"artifact-enemy-"+std::to_string(kind),
                                      {{0,640,400,0},{kind,240,200,2,6,0,0,0},{10,280,200,0},{2,640,120,0}});
        bool beamed=false;
        for(std::uint32_t seed=1;seed<=16 && !beamed;++seed) {
            sven2::LevelScene scene(engine,*xs,level,seed);
            for(int frame=0;frame<3600 && !beamed;++frame) {
                scene.update(1.0f/60,0,0,false);
                beamed=heard(scene.takeSounds(),"SfxBeam");
                if(beamed) {
                    const auto& draws=scene.draws();
                    require(std::none_of(draws.begin(),draws.end(),[&](const auto& draw) { return draw.sprite==artifactSprite; }),
                            "Enemy artifact pickup left the collectible behind.");
                    for(const auto* path:{"Sven2/gfx/ufo.bmp","Sven2/gfx/ufo_shadow.bmp","Sven2/gfx/anim_beam.bmp"}) {
                        const auto* image=engine.loadSprite(path,std::strstr(path,"anim_beam")?44:13,1);
                        require(std::any_of(draws.begin(),draws.end(),[&](const auto& draw) { return draw.sprite==image; }),
                                "Enemy artifact sequence omitted visible UFO, shadow or beam.");
                    }
                    require(scene.takeLifeHits()==0,"Enemy artifact pickup caused a player life hit.");
                    if(capture) engine.drawFrame(draws,("artifact-ufo-"+std::to_string(kind)+".bmp").c_str());
                }
            }
        }
        require(beamed,"Enemy failed to consume an artifact and enter its UFO interaction.");
    }
    sven2::log("PASS: girl teddy recovery, girl-dog combined interaction and artifact consumption by all four enemy families.");
}

void checkDemonSheep(sven2::Engine& engine,bool capture=false) {
    const auto& packs=engine.data().packs();
    const auto xs=std::find_if(packs.begin(),packs.end(),[](const auto& pack) { return pack.name=="XS"; });
    const auto* back=engine.loadSprite("Sven2/gfx/Sheep/anim_sheep_demon_back.bmp",23,1);
    const auto* front=engine.loadSprite("Sven2/gfx/Sheep/anim_sheep_demon_front.bmp",23,1);
    const auto* run=engine.loadSprite("Sven2/gfx/Sheep/anim_sheep_demon_run_front.bmp",8,1);
    const auto* flash=engine.loadSprite("Sven2/gfx/flash.bmp",2,1);
    const auto* mood=engine.loadSprite("Sven2/gfx/symbols.bmp",12,1);
    const auto* stand=engine.loadSprite("Sven2/gfx/Sheep/Sheep_Stand.bmp",4,1);
    const auto* power=engine.loadSprite("Sven2/gfx/symbol_power.bmp",13,1);
    const auto devil=engine.data().soundPath("SfxDevil");
    const auto electro=engine.data().soundPath("SfxDevilElectro");
    auto contains=[](const auto& draws,const auto* sprite) {
        return std::any_of(draws.begin(),draws.end(),[&](const auto& draw) { return draw.sprite==sprite; });
    };
    const auto natural=fixtureLevel(engine,"demon-natural",{{0,80,240,0},{2,480,160,0,10},{2,480,320,0,10}});
    bool triggered=false;
    for(std::uint32_t seed=1;seed<=4 && !triggered;++seed) {
        sven2::LevelScene scene(engine,*xs,natural,seed);
        for(int frame=0;frame<60*120 && !triggered;++frame) {
            scene.update(1.0f/60,0,0,false);
            const auto sounds=scene.takeSounds();
            triggered=std::find(sounds.begin(),sounds.end(),devil)!=sounds.end();
            if(triggered) {
                require(frame>60*17,"Demon triggered before the 41-pulse impatience interval.");
                require(contains(scene.draws(),back) || contains(scene.draws(),front),"Natural demon trigger omitted transformation artwork.");
                if(capture) { scene.update(0.8f,0,0,false);engine.drawFrame(scene.draws(),"demon-transform.bmp"); }
            }
        }
    }
    require(triggered,"Maximum impatience never reached the original random demon branch.");
    const auto chance=fixtureLevel(engine,"demon-chance",{{0,80,240,0},{2,480,160,0,10,0,0,0,40},{2,480,320,0,10,0,0,0,40}});
    bool succeeded=false,failed=false;
    for(std::uint32_t seed=1;seed<=20;++seed) {
        sven2::LevelScene scene(engine,*xs,chance,seed);
        scene.update(0.01f,0,0,false);
        const auto sounds=scene.takeSounds();
        const bool devilSound=std::find(sounds.begin(),sounds.end(),devil)!=sounds.end();
        succeeded=succeeded || devilSound;failed=failed || !devilSound;
        if(!devilSound) {
            const auto& draws=scene.draws();
            require(std::all_of(draws.begin(),draws.end(),[&](const auto& draw) {
                return draw.sprite!=mood || draw.frame==11;
            }),"Failed demon attempt did not cap impatience back to 10.");
        }
    }
    require(succeeded && failed,"Demon chance did not exercise both original random outcomes.");
    const auto protectedLevel=fixtureLevel(engine,"demon-frigida-protected",{{0,80,240,0},{2,480,160,0,10,2,0,0,40},{2,480,320,0,0}});
    sven2::LevelScene protectedSheep(engine,*xs,protectedLevel,1);
    protectedSheep.update(30,0,0,false);
    const auto protectedSounds=protectedSheep.takeSounds();
    require(std::find(protectedSounds.begin(),protectedSounds.end(),devil)==protectedSounds.end(),"Frigida incorrectly entered the ordinary demon branch.");
    for(bool afterStep:{false,true}) {
        const auto level=fixtureLevel(engine,afterStep?"demon-moving-shock":"demon-direct-shock",
            {{0,240,200,0},{2,afterStep?160:200,afterStep?160:200,afterStep?1:0,12,0,11},{2,640,400,0}},132);
        sven2::LevelScene scene(engine,*xs,level,1);
        bool shocked=false,sawRun=false;
        for(int frame=0;frame<600 && !shocked;++frame) {
            scene.update(1.0f/60,0,0,false);
            sawRun=sawRun || contains(scene.draws(),run);
            const auto sounds=scene.takeSounds();
            shocked=std::find(sounds.begin(),sounds.end(),electro)!=sounds.end();
        }
        require(shocked,"Demon did not acquire Sven in its four-way facing arc.");
        require(scene.takeTimeDelta()==(afterStep?-15:10),"Demon time change disagreed with its original direct/moving branch.");
        require(scene.takeLifeHits()==0,"Demon shock invented a life penalty.");
        require(!contains(scene.draws(),power) && contains(scene.draws(),flash),"Demon shock failed to clear the powerup or emit the flash.");
        if(afterStep) require(sawRun,"Demon did not use the run animation before moving contact.");
        if(capture && afterStep) engine.drawFrame(scene.draws(),"demon-shock.bmp");
        const auto stopped=scene.player();
        scene.update(0.2f,1,0,true);
        require(scene.player().x==stopped.x && scene.player().y==stopped.y,"Sven moved during electric stun.");
        scene.update(3.0f,0,0,false);
        const auto& recovered=scene.draws();
        require(!contains(recovered,front) && !contains(recovered,back) && !contains(recovered,flash),"Demon did not finish its shock and reverse transformation.");
        require(contains(recovered,stand) && contains(recovered,mood),"Recovered sheep lost its ordinary body/meter.");
        step(scene,1,0);
        require(scene.player().x>stopped.x,"Sven did not regain movement after electric stun.");
    }
    const auto walk=fixtureLevel(engine,"demon-three-steps",{{0,80,240,0},{2,480,160,2,12,0,11},{2,640,400,0}});
    sven2::LevelScene walker(engine,*xs,walk,1);
    bool moved=false,recovered=false;
    for(int frame=0;frame<900 && !recovered;++frame) {
        walker.update(1.0f/60,0,0,false);
        const auto& draws=walker.draws();
        moved=moved || contains(draws,run);
        recovered=moved && !contains(draws,back) && !contains(draws,front) && !contains(draws,run);
    }
    require(moved && recovered,"Demon failed to move and return after its three-step budget.");
    const auto blocked=fixtureLevel(engine,"demon-blocked",{{0,80,240,0},{2,480,200,0,12,0,11},
        {1,520,160,0},{1,520,240,0},{1,440,240,0},{1,440,160,0}});
    sven2::LevelScene trapped(engine,*xs,blocked,1);
    trapped.update(4,0,0,false);
    const auto& trappedDraws=trapped.draws();
    require(std::any_of(trappedDraws.begin(),trappedDraws.end(),[&](const auto& draw) {
        return draw.sprite==stand && draw.x==480 && draw.y==200;
    }),"Blocked demon failed to recover at its original position.");
    const auto timeout=fixtureLevel(engine,"demon-timeout",{{0,80,240,0},{2,480,200,0,12,0,11}});
    sven2::LevelScene expired(engine,*xs,timeout,1);
    expired.timeExpired();
    for(int frame=0;frame<60*120 && !expired.completed();++frame) expired.update(1.0f/60,0,0,false);
    require(expired.completed() && expired.completedSheep()==0 && expired.takeLifeHits()==0,
            "Timeout UFO failed to collect a sheep after its busy demon sequence.");
    sven2::log("PASS: natural demon trigger, transform/run/recovery, direct and moving shock branches, flash, powerup cancellation and stun release.");
}

void checkFarmerOccupancy(sven2::Engine& engine) {
    const auto& packs=engine.data().packs();
    const auto xs=std::find_if(packs.begin(),packs.end(),[](const auto& pack) { return pack.name=="XS"; });
    constexpr int directions[8][2]={{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}};
    for(int state:{0,10}) for(const auto& direction:directions) {
        const int x=240-40*direction[0],y=200-40*direction[1];
        const auto level=fixtureLevel(engine,"farmer-solid-"+std::to_string(state)+"-"+std::to_string(x)+"-"+std::to_string(y),
                                     {{0,x,y,0},{5,240,200,0,6,0,state,1000},{2,640,400,0}});
        sven2::LevelScene scene(engine,*xs,level,1);
        scene.update(0.3f,direction[0],direction[1],false);
        require(scene.player().x==x && scene.player().y==y,"Player entered the farmer's occupied cell.");
    }
    sven2::log("PASS: correctly centered 160px farmer blocks his logical cell from eight directions, awake and sleeping.");
}

void checkGirlBeamReturn(sven2::Engine& engine,bool capture=false) {
    const auto& packs=engine.data().packs();
    const auto xs=std::find_if(packs.begin(),packs.end(),[](const auto& pack) { return pack.name=="XS"; });
    const auto* stand=engine.loadSprite("Sven2/gfx/Broemse/Broemse_stand.bmp",8,1);
    const auto* back=engine.loadSprite("Sven2/gfx/Broemse/anim_broemse_beam_up_diagonal_back.bmp",5,1);
    const auto* front=engine.loadSprite("Sven2/gfx/Broemse/anim_broemse_beam_up_diagonal_front.bmp",5,1);
    const auto fall=engine.data().soundPath("SfxSvenFall");
    for(int quadrant=0;quadrant<4;++quadrant) {
        const auto level=fixtureLevel(engine,"girl-beam-return-"+std::to_string(quadrant),
            {{0,640,400,0},{6,240,200,quadrant*2,6,0,17},{2,640,120,0}});
        sven2::LevelScene scene(engine,*xs,level,1);
        const auto* beam=quadrant==0 || quadrant==3?back:front;
        scene.update(0.5f,0,0,false);
        const auto& rising=scene.draws();
        require(std::any_of(rising.begin(),rising.end(),[&](const auto& draw) {
            return draw.sprite==beam && draw.mirror==(quadrant<2) && draw.x==240 && draw.y==190;
        }),"Girl beam-up body used the wrong anchor or mirrored direction.");
        if(capture && quadrant==0) engine.drawFrame(rising,"girl-beam-up.bmp");
        scene.update(1.0f,0,0,false);
        const auto& hidden=scene.draws();
        require(std::none_of(hidden.begin(),hidden.end(),[&](const auto& draw) {
            return draw.sprite==stand || draw.sprite==back || draw.sprite==front;
        }),"Girl remained visible during the artifact's hidden wait.");
        scene.update(4.5f,0,0,false);
        const auto& falling=scene.draws();
        require(std::any_of(falling.begin(),falling.end(),[&](const auto& draw) {
            return draw.sprite==stand && draw.x==240 && draw.y<200;
        }),"Girl did not descend using her original standing body.");
        if(capture && quadrant==0) engine.drawFrame(falling,"girl-beam-return.bmp");
        scene.update(1.5f,0,0,false);
        const auto& landed=scene.draws();
        require(std::any_of(landed.begin(),landed.end(),[&](const auto& draw) {
            return draw.sprite==stand && draw.x==240 && draw.y==200;
        }),"Girl did not return to her unchanged ground position.");
        const auto sounds=scene.takeSounds();
        require(std::find(sounds.begin(),sounds.end(),fall)!=sounds.end(),"Girl landing omitted its fall sound.");
    }
    sven2::log("PASS: all four girl beam directions, hidden wait, standing descent, ground recovery and landing sound.");
}

void checkLabFixtures(sven2::Engine& engine) {
    for(const auto& pack:engine.data().packs()) if(pack.name=="ParityLab") {
        const auto episode=std::find_if(pack.levelsets.begin(),pack.levelsets.end(),[](const auto& set) {
            return set.name=="Items and teleports";
        });
        require(episode!=pack.levelsets.end(),"ParityLab items/teleports episode is missing.");
        const auto& level=episode->levels.front();
        sven2::LevelScene puddles(engine,pack,level,1);
        for(int cell=0;cell<3;++cell) step(puddles,1,0);
        step(puddles,0,1);step(puddles,0,1);
        require(puddles.takeSplashSound() && puddles.player().x==560 && puddles.player().y==280,
                "Authored lab puddle did not teleport Sven to the other puddle's north exit.");
        sven2::LevelScene water(engine,pack,level,1);
        for(int cell=0;cell<7;++cell) step(water,1,0);
        for(int cell=0;cell<5;++cell) step(water,0,1);
        require(water.takeSplashSound() && (water.player().x==200 || water.player().x==560) && water.player().y==280,
                "Authored lab water did not send Sven to a valid puddle exit.");
        sven2::LevelScene artifact(engine,pack,level,1);
        for(int cell=0;cell<12;++cell) step(artifact,1,0);
        for(int cell=0;cell<3;++cell) step(artifact,0,-1);
        const auto* sprite=engine.loadSprite("Sven2/gfx/Artifact.bmp",1,1);
        const auto& consumed=artifact.draws();
        require(std::none_of(consumed.begin(),consumed.end(),[&](const auto& draw) { return draw.sprite==sprite; }),
                "Authored lab artifact failed to disappear when Sven entered its cell.");
        const auto* icon=engine.loadSprite("Sven2/gfx/icon_alien.bmp",13,1);
        const auto* ship=engine.loadSprite("Sven2/gfx/ufo.bmp",13,1);
        const auto* shadow=engine.loadSprite("Sven2/gfx/ufo_shadow.bmp",13,1);
        bool powered=false,visibleUfo=false;
        for(int frame=0;frame<3600 && !powered;++frame) {
            artifact.update(1.0f/60,0,0,false);
            const auto& draws=artifact.draws();
            visibleUfo=visibleUfo || (std::any_of(draws.begin(),draws.end(),[&](const auto& draw) { return draw.sprite==ship; }) &&
                                    std::any_of(draws.begin(),draws.end(),[&](const auto& draw) { return draw.sprite==shadow; }));
            powered=std::any_of(draws.begin(),draws.end(),[&](const auto& draw) { return draw.sprite==icon; });
        }
        require(powered,"Authored lab artifact never completed Sven's UFO powerup sequence.");
        require(visibleUfo,"Authored lab artifact sequence hid the UFO and its shadow.");
        sven2::log("PASS: actual ParityLab puddle destination, water entry and artifact pickup/beam/powerup.");
    }
}

void checkTimeout(sven2::Engine& engine) {
    sven2::LevelScene scene(engine);
    scene.timeExpired();
    for(int frame=0;frame<60*120 && !scene.completed();++frame)
        scene.update(1.0f/60,0,0,false);
    require(scene.completed() && scene.completedSheep()==0,"Timeout UFO did not collect the remaining animals.");
    require(scene.takeScoreDelta()==0 && scene.takeTimeDelta()==0 && scene.takeLifeHits()==0,
            "Timeout collection incorrectly awarded interaction rewards or took a life.");
    const auto sounds=scene.takeSounds();
    require(std::find(sounds.begin(),sounds.end(),"Sven2/sfx/beam.ogg")!=sounds.end(),"UFO pickup did not emit its beam sound.");
    sven2::log("PASS: timeout UFO collection, beam events, no interaction reward and no invented life penalty.");
}

void writeBehaviorTrace(sven2::Engine& engine,const std::filesystem::path& path) {
    std::ofstream output(path,std::ios::binary);
    require(static_cast<bool>(output),"Cannot create behavior trace.");
    std::map<const sven2::Sprite*,std::uint64_t> spriteHashes;
    for(const auto& pack:engine.data().packs()) for(const auto& set:pack.levelsets) for(const auto& level:set.levels) {
        for(std::uint32_t seed:{1u,73u}) {
            sven2::LevelScene scene(engine,pack,level,seed);
            output<<level.map<<' '<<seed<<'\n';
            for(int frame=0;frame<480;++frame) {
                std::uint64_t hash=14695981039346656037ull;
                auto mix=[&](const auto& value) {
                    const auto* bytes=reinterpret_cast<const unsigned char*>(&value);
                    for(size_t i=0;i<sizeof(value);++i) hash=(hash^bytes[i])*1099511628211ull;
                };
                constexpr int direction[8][2]={{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}};
                constexpr float delta[]={1.0f/60,0.03f,0.1f,0.25f};
                const int heading=(frame/30)%8;
                scene.update(delta[frame%4],direction[heading][0],direction[heading][1],frame%11==0);
                if(frame==360) scene.timeExpired();
                for(const auto& draw:scene.draws()) {
                    auto entry=spriteHashes.find(draw.sprite);
                    if(entry==spriteHashes.end()) {
                        std::uint64_t pixels=14695981039346656037ull;
                        for(auto pixel:draw.sprite->pixels) for(int byte=0;byte<4;++byte)
                            pixels=(pixels^((pixel>>(8*byte))&255))*1099511628211ull;
                        entry=spriteHashes.emplace(draw.sprite,pixels).first;
                    }
                    mix(entry->second);mix(draw.sprite->width);mix(draw.sprite->rows);mix(draw.sprite->columns);
                    mix(draw.x);mix(draw.y);mix(draw.frame);mix(draw.mirror);mix(draw.clipBottom);
                }
                mix(scene.takeScoreDelta());mix(scene.takeTimeDelta());mix(scene.takeLifeHits());mix(scene.takeLivesGained());
                mix(scene.remainingTargets());mix(scene.completedSheep());
                for(const auto& sound:scene.takeSounds()) for(char byte:sound) mix(byte);
                output<<frame<<' '<<hash<<'\n';
            }
        }
    }
    require(static_cast<bool>(output),"Cannot finish behavior trace.");
    sven2::log("PASS: deterministic behavior trace written.");
}

void checkCatalog(sven2::Engine& engine,size_t minimum=105) {
    size_t levels=0;
    std::set<std::string> soundAssets;
    for(const auto& pack:engine.data().packs()) {
        for(const auto& set:pack.levelsets) {
            for(const auto& level:set.levels) {
                try {
                    sven2::LevelScene scene(engine,pack,level);
                    for(int frame=0;frame<1200;++frame) scene.update(1.0f/60,0,0,false);
                    scene.timeExpired();
                    for(int frame=0;frame<600;++frame) scene.update(1.0f/60,0,0,false);
                    for(const auto& draw:scene.draws()) {
                        require(draw.sprite!=nullptr,"Null actor sprite.");
                        require(draw.frame>=0 && draw.frame<draw.sprite->rows*draw.sprite->columns,"Actor frame exceeds its sheet.");
                    }
                    for(const auto& sound:scene.takeSounds())
                        if(soundAssets.insert(sound).second) engine.data().assetPath(sound);
                    ++levels;
                } catch(const std::exception& error) {
                    throw std::runtime_error(level.map+": "+error.what());
                }
            }
        }
    }
    require(levels>=minimum,"Catalog fixture count is below the required minimum.");
    sven2::log("PASS: "+std::to_string(levels)+" folder levels load and simulate with valid sprite frames and sound references.");
}
}

int main(int argc,char** argv) {
    try {
        const bool preview=argc==3 && std::string(argv[1])=="--catalog-preview";
        const bool catalogAudio=argc==3 && std::string(argv[1])=="--catalog-audio";
        const auto trace=argc==3 && std::string(argv[1])=="--trace"?
            std::filesystem::absolute(std::filesystem::u8path(argv[2])):std::filesystem::path{};
        const auto catalog=argc==3 && (std::string(argv[1])=="--catalog" || preview || catalogAudio)?
            std::filesystem::absolute(std::filesystem::u8path(argv[2])):std::filesystem::path{};
        wchar_t executable[32768]{};
        GetModuleFileNameW(nullptr,executable,32768);
        const auto root=std::filesystem::path(executable).parent_path().parent_path().parent_path().parent_path().parent_path();
        const auto saves=root/L"rebuild"/L"runtime"/L"checks"/
            (std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
        std::filesystem::create_directories(saves);
        std::filesystem::current_path(saves);
        checkScalingCoordinates();
        if(argc==2 && std::string(argv[1])=="--upscaler") {
            checkUpscalerGpu();
            sven2::log("GPU captures: "+saves.string());
            return 0;
        }
        if(FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED))) throw std::runtime_error("Cannot initialize COM.");
        struct ComLifetime { ~ComLifetime() { CoUninitialize(); } } apartment;
        const auto fixtureData=saves/L"data";
        if(catalog.empty()) linkFixtureData(root/L"assets",fixtureData);
        sven2::Engine engine(root/L"games"/L"sven2",catalog.empty()?fixtureData:catalog);
        engine.construct();
        engine.initializePlugins();
        engine.verifyData();
        if(!trace.empty()) {
            writeBehaviorTrace(engine,trace);
            engine.shutdown();
            return 0;
        }
        if(!catalog.empty()) {
            if(catalogAudio) {
                engine.startVideo(false);
                require(engine.startAudio(),"Catalog audio backend failed to initialize.");
                require(engine.playMusic("Sven2/sfx/Sven2_music.ogg"),"Menu music failed.");
                for(int pass=0;pass<3;++pass) for(const auto& pack:engine.data().packs()) for(const auto& set:pack.levelsets) for(const auto& level:set.levels) {
                    sven2::LevelScene scene(engine,pack,level,1);
                    sven2::log("Testing ambient stream: "+level.music);
                    require(engine.playMusic(level.music),"Level ambient stream failed.");
                    engine.drawFrame(scene.draws());
                    const auto started=GetTickCount64();
                    while(GetTickCount64()-started<1200) {engine.tick();scene.update(1.0f/60,0,0,false);Sleep(10);}
                    engine.drawFrame(scene.draws());
                }
                engine.stopMusic();
                engine.stopMusic(); // Repeated cleanup must be harmless.
                require(engine.playMusic("Sven2/sfx/Sven2Highscore.ogg"),"Highscore music failed after campaign streams.");
                require(engine.playMusic("Sven2/sfx/Sven2_music.ogg"),"Returning to menu music failed.");
                engine.stopMusic();
                sven2::log("PASS: sequential catalog ambient streams.");
            }
            checkLabFixtures(engine);
            checkCatalog(engine,1);
            if(preview) {
                engine.startVideo(false);
                size_t index=0;
                for(const auto& pack:engine.data().packs()) for(const auto& set:pack.levelsets) for(const auto& level:set.levels) {
                    sven2::LevelScene scene(engine,pack,level,1);
                    engine.drawFrame(scene.draws(),("pack-level-"+std::to_string(++index)+".bmp").c_str());
                }
                sven2::log("PASS: package previews in "+saves.string());
            }
            engine.shutdown();
            sven2::log("PASS: package catalog checks.");
            return 0;
        }
        require(engine.data().packs().size()>=5,"Original package folders were not discovered.");
        require(engine.data().text("SelectPacks")==u8"W\u00c4HLE LEVELPACK","Legacy XML encoding was not decoded.");
        checkScores(saves);
        checkGlyphs(engine);
        checkMenus(engine,saves);
        checkDepthOrdering(engine);
        checkSerializedPowerups(engine);
        checkMovement(engine);
        checkInteraction(engine);
        checkReportedGameplay(engine);
        checkEnemyAcquisition(engine);
        checkRound(engine);
        checkLifeChanges(engine);
        checkPlayerItems(engine);
        checkAttackReservations(engine);
        checkEffectAndAnimationStates(engine);
        checkEnemyHeadings(engine);
        checkEnemyItems(engine);
        checkDemonSheep(engine);
        checkFarmerOccupancy(engine);
        checkGirlBeamReturn(engine);
        checkTimeout(engine);
        checkCatalog(engine);
        if(argc==2 && std::string(argv[1])=="--visual") {
            engine.startVideo(false);
            checkDepthOrdering(engine,true);
            checkEnemyItems(engine,true);
            checkDemonSheep(engine,true);
            checkGirlBeamReturn(engine,true);
            checkPlayerItems(engine,true);
            checkLifeChanges(engine,true);
            checkEffectAndAnimationStates(engine,true);
            checkEnemyHeadings(engine,true);
            sven2::GameUi ui(engine,saves/L"visual-scores");
            using Page=sven2::GameUi::Page;
            engine.tick();
            engine.drawFrame(ui.draws(0),"menu.bmp");
            sven2::UiInput exitClick;exitClick.click=true;exitClick.mouse={650,555};
            ui.handle(exitClick);
            engine.drawFrame(ui.draws(0),"menu-exit.bmp");
            ui.show(Page::packs);
            engine.tick();
            engine.drawFrame(ui.draws(0),"packs.bmp");
            ui.show(Page::help);
            for(int tab=0;tab<4;++tab) {
                const auto name="help-"+std::to_string(tab)+".bmp";
                engine.drawFrame(ui.draws(0),name.c_str());
                sven2::UiInput next;
                next.right=true;
                ui.handle(next);
            }
            ui.show(Page::highscores);
            engine.drawFrame(ui.draws(0),"highscores.bmp");
            const auto* paper=engine.loadSprite("Sven2/gfx/Help_BG.jpg",1,1);
            checkCapturePixel("highscores.bmp",110,65,paper->pixels[65*paper->width+110]);
            sven2::UiInput collectionTab;
            collectionTab.click=true;
            collectionTab.mouse={510,40};
            ui.handle(collectionTab);
            engine.drawFrame(ui.draws(0),"cards-first.bmp");
            sven2::UiInput previousCard;previousCard.left=true;
            ui.handle(previousCard);
            engine.drawFrame(ui.draws(0),"cards-last.bmp");
            checkCapturePixel("cards-last.bmp",510,65,paper->pixels[65*paper->width+510]);
            ui.showResults({1250,1,180,3,3,0,0,0,false,false});
            engine.drawFrame(ui.draws(0),"results.bmp");
            ui.showResults({1250,1,180,3,3,0,0,0,true,false,17,3});
            engine.drawFrame(ui.draws(0.1f),"pack-complete.bmp");
            sven2::UiInput continueResult;continueResult.confirm=true;
            ui.handle(continueResult);
            sven2::UiInput enterName;enterName.text="Tester";
            ui.handle(enterName);
            engine.drawFrame(ui.draws(0),"name-entry.bmp");
            const auto& packs=engine.data().packs();
            const auto xs=std::find_if(packs.begin(),packs.end(),[](const auto& pack) { return pack.name=="XS"; });
            sven2::GameSession session(engine,*xs,xs->levelsets.front());
            engine.drawFrame(session.draws(),"level.bmp");
            ui.show(Page::pause);
            engine.drawFrame(ui.draws(0,&session.draws()),"pause.bmp");
            ui.show(Page::title);
            ui.handle(exitClick);
            engine.drawFrame(ui.draws(0,&session.draws()),"menu-exit-after-level.bmp");
            const auto walking=fixtureLevel(engine,"visual-walk",{{0,240,200,0},{2,640,400,0}});
            sven2::LevelScene east(engine,*xs,walking,1),west(engine,*xs,walking,1);
            east.update(0.1f,1,0,false);west.update(0.1f,-1,0,false);
            engine.drawFrame(east.draws(),"running-east.bmp");
            engine.drawFrame(west.draws(),"running-west.bmp");
            const auto cold=fixtureLevel(engine,"visual-frigida",{{0,240,200,0},{2,240,160,0}});
            sven2::LevelScene frigida(engine,*xs,cold,1);
            for(int frame=0;frame<90;++frame) frigida.update(1.0f/60,0,0,frame%8==0);
            engine.drawFrame(frigida.draws(),"frigida.bmp");
            const auto enemy=fixtureLevel(engine,"visual-fight",{{0,240,200,0},{4,280,200,6},{2,640,400,0}});
            sven2::LevelScene fight(engine,*xs,enemy,1);
            const auto* cloud=engine.loadSprite("Sven2/gfx/anim_cloud.bmp",10,1);
            for(int frame=0;frame<600;++frame) {
                fight.update(1.0f/60,0,0,false);
                const auto& draws=fight.draws();
                if(std::any_of(draws.begin(),draws.end(),[&](const auto& draw) { return draw.sprite==cloud; })) {
                    engine.drawFrame(draws,"fight.bmp");break;
                }
            }
            const auto animals=fixtureLevel(engine,"visual-meters",{{0,240,200,0},{12,240,160,0},{11,400,160,0},{13,560,160,0}});
            sven2::LevelScene meters(engine,*xs,animals,1);
            engine.drawFrame(meters.draws(),"animal-meters.bmp");
            meters.update(0.2f,0,0,true);
            engine.drawFrame(meters.draws(),"goat-interaction.bmp");
            require(engine.startAudio(),"Native regression audio backend failed to initialize.");
            for(const char* name:{"SfxDogFoundSven","SfxGuyFoundBeer","SfxLarsDrink","SfxLarsSleep"})
                require(engine.playSample(engine.data().soundPath(name)),"Native enemy sample playback request failed.");
            sven2::log("PASS: native enemy notice, drink and sleep sample playback requests.");
            require(engine.playSample(engine.data().soundPath("SfxUFO")),"Native UFO loop failed to start.");
            const auto loopStarted=GetTickCount64();
            while(GetTickCount64()-loopStarted<6200) { engine.tick();Sleep(10); }
            require(engine.ufoSoundPlaying(),"UFO sound stopped after the original five-second recording instead of looping.");
            engine.stopUfoSound();
            require(!engine.ufoSoundPlaying(),"UFO channel did not stop on lifecycle cleanup.");
            require(engine.playSample(engine.data().soundPath("SfxUFO")),"Stopped UFO loop failed to restart.");
            engine.stopUfoSound();
            sven2::log("PASS: native UFO sample loops beyond 6 seconds, stops and restarts.");
            sven2::log("PASS: native UI captures in "+saves.string());
        }
        engine.shutdown();
        sven2::log("PASS: all remake checks.");
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL: "<<error.what()<<std::endl;
        return 1;
    }
}
