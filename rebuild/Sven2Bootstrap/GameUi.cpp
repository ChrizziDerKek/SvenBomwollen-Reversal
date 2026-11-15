#include "GameUi.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace sven2 {
namespace {
std::string padded(int value,int width=5) {
    std::ostringstream text; text<<std::setw(width)<<value; return text.str();
}
int wrap(int value,int length) { return (value+length)%length; }
bool within(POINT point,int x,int y,int width,int height) {
    return point.x>=x && point.y>=y && point.x<x+width && point.y<y+height;
}
std::string numbered(const char* prefix,int index) {
    std::ostringstream text;
    text<<prefix<<std::setfill('0')<<std::setw(2)<<index;
    return text.str();
}
}

GameUi::GameUi(Engine& engine,std::filesystem::path saves)
    : engine_(engine),data_(engine.data()),saves_(std::move(saves)),
      large_(engine),small_(engine,true),green_(engine,false,true),
      clouds_(engine.loadSprite("Sven2/gfx/mainmenu_clouds.jpg",1,1,true)),
      backdrop_(engine.loadSprite("Sven2/gfx/mainmenu_BG.jpg",1,1)),
      logo_(engine.loadSprite("Sven2/gfx/SvenLogo.bmp",1,1)),
      logoDetail_(engine.loadSprite("Sven2/gfx/SvenLogo_O.bmp",4,1)),
      buttons_(engine.loadSprite("Sven2/gfx/mainmenu_buttons.bmp",10,1)),
      packPanel_(engine.loadSprite("Sven2/gfx/smallmenu_choose_level.bmp",1,1)),
      helpBackdrop_(engine.loadSprite("Sven2/gfx/Help_BG.jpg",1,1)),
      helpTabs_(engine.loadSprite("Sven2/gfx/Help_file_cards.bmp",8,1)),
      scorePanel_(engine.loadSprite("Sven2/gfx/ScoreScreen_BG.bmp",1,1)),
      packFinish_(engine.loadSprite("Sven2/gfx/ScoreScreen_BG_levelpack.bmp",1,1)),
      scoreTabs_(engine.loadSprite("Sven2/gfx/Highsc_file_cards.bmp",10,1)),
      highlight_(engine.loadSprite("Sven2/gfx/selection_bar.bmp",1,1)),
      pausePanel_(engine.loadSprite("Sven2/gfx/smallmenu_BG.bmp",1,1)),
      timer_(engine.loadSprite("Sven2/gfx/timer.bmp",1,1)),life_(engine.loadSprite("Sven2/gfx/Life_icon.bmp",1,1)),
      sheep_(engine.loadSprite("Sven2/gfx/Sheep/Sheep_Stand.bmp",4,1)),score_(engine.loadSprite("Sven2/gfx/score.bmp",4,1)),
      icons_(engine.loadSprite("Sven2/gfx/icons.bmp",5,1)),
      artifact_(engine.loadSprite("Sven2/gfx/Artifact.bmp",1,1)),
      arrows_(engine.loadSprite("Sven2/gfx/pijlen.bmp",8,1)),
      cursor_(engine.loadSprite("Sven2/gfx/mousecursor.bmp",5,1)),
      cardBlank_(engine.loadSprite("Sven2/gfx/Highsc_file_card_blank.bmp",1,1)),
      noAnimation_(engine.loadSprite("Sven2/gfx/noanim.bmp",1,1)),
      leaveImage_(engine.loadSprite("Sven2/gfx/smallmenu_BG_verlassen.bmp",1,1)),
      gameOverImage_(engine.loadSprite("Sven2/gfx/smallmenu_BG_gameover.bmp",1,1)) {
    people_={engine.loadSprite("Sven2/gfx/Dog/Dog_Stand.bmp",8,1),
             engine.loadSprite("Sven2/gfx/Guy/Guy_Stand.bmp",8,1),
             engine.loadSprite("Sven2/gfx/Broemse/Broemse_stand.bmp",8,1),
             engine.loadSprite("Sven2/gfx/Alien/Alien_Stand.bmp",8,1)};
    for(size_t i=0;i<data_.packs().size();++i)
        if(data_.packs()[i].name=="XS") packIndex_=i;
    draws_.reserve(1200);
}

const LevelPackInfo& GameUi::pack() const { return data_.packs().at(packIndex_); }
const LevelSetInfo& GameUi::levelset() const { return pack().levelsets.at(setIndex_); }
void GameUi::show(Page page) {
    page_=page;
    notice_.clear();
    if(page==Page::name || page==Page::results) uiAge_=0;
    if(page==Page::pause || page==Page::quit) dialogChoice_=0;
    if(page==Page::packs || page==Page::levelsets) {
        const auto entries=episodes();
        const auto selected=std::find(entries.begin(),entries.end(),std::make_pair(packIndex_,setIndex_));
        episodeFirst_=selected==entries.end()?0:std::max(0,static_cast<int>(selected-entries.begin())-6);
    }
}
void GameUi::setNotice(const std::string& notice) { notice_=notice; }
std::vector<std::string> GameUi::takeSounds() {
    std::vector<std::string> result;
    result.swap(sounds_);
    return result;
}

// Original episode list and highscore traversal: 0x428180 / 0x416C05.
std::vector<std::pair<size_t,size_t>> GameUi::episodes() const {
    std::vector<std::pair<size_t,size_t>> entries;
    for(size_t p=0;p<data_.packs().size();++p)
        for(size_t e=0;e<data_.packs()[p].levelsets.size();++e)
            entries.emplace_back(p,e);
    return entries;
}

void GameUi::stepEpisode(int step) {
    const auto entries=episodes();
    const auto found=std::find(entries.begin(),entries.end(),std::make_pair(packIndex_,setIndex_));
    if(found==entries.end()) return;
    const int next=std::clamp(static_cast<int>(found-entries.begin())+step,0,static_cast<int>(entries.size())-1);
    packIndex_=entries[next].first;
    setIndex_=entries[next].second;
}

Highscores& GameUi::scores() {
    const std::string key=pack().name+"-"+std::to_string(setIndex_);
    auto& table=scoreTables_[key];
    if(!table) {
        std::vector<HighscoreEntry> defaults;
        std::istringstream names(data_.parameterString("DefaultHighscoreNames","A/B/C/D/E/F/G/H/I/J"));
        std::string name;
        while(defaults.size()<10 && std::getline(names,name,'/'))
            defaults.push_back({name,1000-static_cast<int>(defaults.size())*100,0});
        table=std::make_unique<Highscores>(saves_/std::filesystem::u8path(key+".tsv"),std::move(defaults));
    }
    return *table;
}

int GameUi::buttonAt(POINT mouse) const {
    constexpr int positions[]={40,210,380,630}, widths[]={115,115,200,115};
    for(int i=0;i<4;++i)
        if(within(mouse,positions[i],545,widths[i],30)) return i;
    return -1;
}

void GameUi::showResults(const RoundResult& result) {
    result_=result;
    cardsTab_=false;
    if(page_!=Page::results) show(Page::results);
}

UiAction GameUi::handle(const UiInput& input) {
    mouse_=input.mouse;
    if(page_==Page::results) {
        if(!input.confirm) return UiAction::none;
        if(!result_.episodeComplete && !result_.gameOver) return UiAction::nextLevel;
        if(scores().qualifies(result_.score)) {
            enteredName_.clear();
            show(Page::name);
        } else {
            show(Page::highscores);
        }
        return UiAction::none;
    }
    if(page_==Page::name) {
        if(input.erase && !enteredName_.empty()) {
            size_t last=enteredName_.size()-1;
            while(last && (static_cast<unsigned char>(enteredName_[last])&0xc0)==0x80) --last;
            enteredName_.erase(last);
        }
        if(!input.text.empty()) {
            for(size_t i=0;i<input.text.size();) {
                const size_t start=i++;
                while(i<input.text.size() && (static_cast<unsigned char>(input.text[i])&0xc0)==0x80) ++i;
                if(static_cast<unsigned char>(input.text[start])<32) continue;
                const auto candidate=enteredName_+input.text.substr(start,i-start);
                const int chars=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,candidate.c_str(),-1,nullptr,0);
                if(chars>0 && chars<=21) enteredName_=candidate;
            }
        }
        if(input.confirm && !enteredName_.empty()) {
            scores().submit(enteredName_,result_.score,result_.level,result_.totalSheep);
            show(Page::highscores);
        }
        return UiAction::none;
    }
    if(page_==Page::pause || page_==Page::quit) {
        const bool paused=page_==Page::pause || quitReturn_==Page::pause;
        if(input.back) {
            if(page_==Page::pause) return UiAction::resume;
            show(quitReturn_);
            return UiAction::none;
        }
        if(input.left || input.right || input.tab) dialogChoice_=1-dialogChoice_;
        const int hovered=within(input.mouse,310,211,40,32)?0:
                          within(input.mouse,430,211,80,32)?1:-1;
        if(input.mouseMoved && hovered>=0) dialogChoice_=hovered;
        if(input.confirm || (input.click && hovered>=0)) {
            const int choice=input.click?hovered:dialogChoice_;
            if(choice==0) return paused?UiAction::abandon:UiAction::exit;
            if(paused) return UiAction::resume;
            show(quitReturn_);
        }
        return UiAction::none;
    }
    if(input.back) {
        show(page_==Page::levelsets?Page::packs:Page::title);
        return UiAction::none;
    }
    if(page_==Page::help) {
        if(input.left) helpTab_=wrap(helpTab_-1,4);
        if(input.right || input.tab) helpTab_=wrap(helpTab_+1,4);
        if(input.up) helpCharacter_=std::max(0,helpCharacter_-1);
        if(input.down) helpCharacter_=std::min(3,helpCharacter_+1);
        if(input.click && helpTab_==1) {
            if(within(input.mouse,100,80,32,32)) helpCharacter_=std::max(0,helpCharacter_-1);
            if(within(input.mouse,308,80,32,32)) helpCharacter_=std::min(3,helpCharacter_+1);
        }
        if(input.click && input.mouse.y>=21 && input.mouse.y<73 &&
           input.mouse.x>=100 && input.mouse.x<532) {
            helpTab_=std::clamp(static_cast<int>((input.mouse.x-100)/100),0,3);
            return UiAction::none;
        }
    } else if(page_==Page::highscores) {
        if(input.click && within(input.mouse,100,21,132,52)) cardsTab_=false;
        if(input.click && within(input.mouse,500,21,132,52)) cardsTab_=true;
        if(cardsTab_) {
            int change=(input.right || (input.click && within(input.mouse,238,350,32,32)))-
                       (input.left || (input.click && within(input.mouse,160,350,32,32)));
            if(change) {
                cardIndex_=wrap(cardIndex_+change,33);
                cardPlaying_=false;
                cardAge_=0;
            }
            if(!cardPlaying_ && cardIndex_<data_.parameter("ShagAnims",33) &&
               (input.confirm || (input.click && within(input.mouse,150,130,132,206)))) {
                if(cardAnimations_.empty()) cardAnimations_=loadInteractionAnimations(engine_);
                cardPlaying_=true;
                cardAge_=0;
                sounds_.push_back(cardAnimations_.at(static_cast<size_t>(cardIndex_)*2).sound);
            }
        } else {
            const bool previous=input.left || input.up ||
                (input.click && within(input.mouse,100,90,32,32));
            const bool next=input.right || input.down ||
                (input.click && within(input.mouse,268,90,32,32));
            if(previous || next) stepEpisode(previous?-1:1);
        }
    } else if(page_==Page::packs || page_==Page::levelsets) {
        const auto entries=episodes();
        if(entries.empty()) return UiAction::none;
        auto found=std::find(entries.begin(),entries.end(),std::make_pair(packIndex_,setIndex_));
        int selected=found==entries.end()?0:static_cast<int>(found-entries.begin());
        if(input.up || input.down) selected=std::clamp(selected+(input.up?-1:1),0,static_cast<int>(entries.size())-1);
        if(input.up || input.down) {
            if(selected<episodeFirst_) episodeFirst_=selected;
            if(selected>=episodeFirst_+7) episodeFirst_=selected-6;
        }
        if(input.click && within(input.mouse,510,158,32,32)) episodeFirst_=std::max(0,episodeFirst_-1);
        if(input.click && within(input.mouse,510,330,32,32))
            episodeFirst_=std::min(std::max(0,static_cast<int>(entries.size())-7),episodeFirst_+1);
        bool activate=input.confirm;
        if((input.mouseMoved || input.click) && within(input.mouse,154,158,350,189)) {
            const int row=(input.mouse.y-158)/27+episodeFirst_;
            if(row<static_cast<int>(entries.size())) { selected=row;activate=input.click; }
        }
        packIndex_=entries[selected].first;
        setIndex_=entries[selected].second;
        if(activate) return UiAction::start;

    }
    if(input.tab || (page_==Page::title && (input.left || input.right))) {
        selectedButton_=wrap(selectedButton_+(input.left?-1:1),4);
    }
    const int hovered=buttonAt(input.mouse);
    if(input.mouseMoved && hovered>=0) selectedButton_=hovered;
    const int activated=input.click?hovered:(page_==Page::title && input.confirm?selectedButton_:-1);
    switch(activated) {
    case 0: show(page_==Page::packs?Page::title:Page::packs);break;
    case 1: show(Page::help);break;
    case 2: show(Page::highscores);break;
    case 3: quitReturn_=Page::title;show(Page::quit);break;
    default: break;
    }
    return UiAction::none;
}

void GameUi::centered(const BitmapFont& font,const std::string& text,int center,int y) {
    font.append(draws_,text,center-font.width(text)/2,y);
}

void GameUi::appendButtons() {
    constexpr int x[]={40,210,380,630};
    for(int i=0;i<4;++i) draws_.push_back({buttons_,x[i],545,2*i+(selectedButton_!=i)});
}

void GameUi::appendHelp() {
    draws_.push_back({helpBackdrop_,0,0,0});
    constexpr int frames[]={2,4,0,6};
    // Original 0x4146E0: inactive tabs first, active last. The paper starts at y=60.
    for(int i=0;i<4;++i)
        if(i!=helpTab_) draws_.push_back({helpTabs_,100+i*100,34,frames[i]+1,false,60});
    draws_.push_back({helpTabs_,100+helpTab_*100,21,frames[helpTab_],false,60});
    if(helpTab_==0) {
        constexpr int positions[]={80,140,220,320,420};
        for(int i=0;i<5;++i) small_.appendWrapped(draws_,data_.text(numbered("spiel",i+1)),120,positions[i],535,20);
    } else if(helpTab_==1) {
        constexpr const char* people[]={"Dog","Guy","Broemse","Alien"};
        const std::string person=people[helpCharacter_];
        const auto* actor=people_[helpCharacter_];
        draws_.push_back({actor,160-actor->frameWidth/2,180-actor->frameHeight/2,3});
        if(helpCharacter_>0) draws_.push_back({arrows_,100,80,within(mouse_,100,80,32,32)?1:0});
        if(helpCharacter_<3) draws_.push_back({arrows_,308,80,within(mouse_,308,80,32,32)?3:2});
        centered(large_,data_.text(person+"00"),220,80);
        small_.appendWrapped(draws_,data_.text(person+"01"),230,140,460,20);
        small_.appendWrapped(draws_,data_.text(person+"02"),120,280,410,20);
    } else if(helpTab_==2) {
        small_.appendWrapped(draws_,data_.text("extra01"),120,80,535,20);
        constexpr int iconFrames[]={0,2,3,4,1};
        for(int i=2;i<=7;++i) {
            const int y=140+(i-2)*60;
            const auto* image=i==7?artifact_:icons_;
            const int center=i==2?175:y+20;
            draws_.push_back({image,150-image->frameWidth/2,center-image->frameHeight/2,i==7?0:iconFrames[i-2]});
            small_.append(draws_,data_.text(numbered("extra",i)),200,y);
            small_.appendWrapped(draws_,data_.text(numbered("extra_",i)),200,y+20,455,20);
        }
    } else {
        constexpr int y[]={100,120,140,160,180,200,220,260,280,300,340,360,380};
        for(int i=0;i<13;++i) {
            small_.appendWrapped(draws_,data_.text(numbered("credits",i*2+1)),120,y[i],220,20);
            small_.appendWrapped(draws_,data_.text(numbered("credits",i*2+2)),340,y[i],350,20);
        }
        small_.append(draws_,"v1.02",670,475);
    }
}

void GameUi::appendScores() {
    draws_.push_back({helpBackdrop_,0,0,0});
    // Original 0x4171A0 draws the inactive tab before the active tab.
    draws_.push_back({scoreTabs_,cardsTab_?100:500,34,cardsTab_?1:9,false,60});
    draws_.push_back({scoreTabs_,cardsTab_?500:100,21,cardsTab_?8:0,false,60});
    if(cardsTab_) {
        draws_.push_back({cardBlank_,150,130,0});
        if(cardIndex_<data_.parameter("ShagAnims",33)) {
            if(cardAnimations_.empty()) cardAnimations_=loadInteractionAnimations(engine_);
            const auto& card=cardAnimations_.at(static_cast<size_t>(cardIndex_)*2);
            int frame=0;
            if(cardPlaying_) {
                const auto step=static_cast<size_t>(cardAge_*15); // Original browser reset at 0x416A04.
                if(step<card.frames.size()) frame=card.frames[step];
                else { cardPlaying_=false;cardAge_=0; }
            }
            draws_.push_back({card.sprite,150,140,frame,card.mirror});

        } else {
            draws_.push_back({noAnimation_,150,140,0});
        }
        large_.appendWrapped(draws_,data_.text(numbered("Card_",cardIndex_+1)),310,150,380,25);
        small_.appendWrapped(draws_,data_.text(numbered("Card",cardIndex_+1)),310,200,380,20);
        centered(large_,std::to_string(cardIndex_+1),215,350);
        draws_.push_back({arrows_,160,350,within(mouse_,160,350,32,32)?1:0});
        draws_.push_back({arrows_,238,350,within(mouse_,238,350,32,32)?3:2});
        return;
    }
    centered(small_,pack().name+"/"+levelset().name,200,100);
    const auto entries=episodes();
    if(!entries.empty() && entries.front()!=std::make_pair(packIndex_,setIndex_))
        draws_.push_back({arrows_,100,90,within(mouse_,100,90,32,32)?1:0});
    if(!entries.empty() && entries.back()!=std::make_pair(packIndex_,setIndex_))
        draws_.push_back({arrows_,268,90,within(mouse_,268,90,32,32)?3:2});
    draws_.push_back({score_,375,70,0});
    large_.append(draws_,data_.text("HighscoreLevel"),540,80);
    draws_.push_back({sheep_,595,35,1});
    auto entriesToDraw=scores().entries();
    int editingRow=-1;
    if(page_==Page::name) {
        auto position=std::find_if(entriesToDraw.begin(),entriesToDraw.end(),[&](const HighscoreEntry& entry) { return entry.score<result_.score; });
        editingRow=static_cast<int>(position-entriesToDraw.begin());
        entriesToDraw.insert(position,{enteredName_,result_.score,result_.level,result_.totalSheep});
        if(entriesToDraw.size()>10) entriesToDraw.resize(10);
    }
    int row=0;
    for(const auto& entry:entriesToDraw) {
        const int y=130+row*28;
        large_.append(draws_,padded(++row,2)+".",105,y);
        // Original 0x417697 appends literal I (frame 8), on even 15 Hz ticks.
        const bool caret=row-1==editingRow && static_cast<int>(uiAge_*15)%2==0;
        large_.append(draws_,entry.name+(caret?"I":""),150,y);
        large_.append(draws_,padded(entry.score),430,y);
        large_.append(draws_,padded(entry.level,2),555,y);
        large_.append(draws_,padded(entry.sheep,4),620,y);
    }
}

const std::vector<SpriteDraw>& GameUi::draws(float elapsed,const std::vector<SpriteDraw>* scene) {
    uiAge_+=elapsed;
    const bool promptVisible=static_cast<int>(uiAge_*15)%5!=0;
    scroll_=std::fmod(scroll_+elapsed*10,800.0f);
    if(cardPlaying_) cardAge_+=elapsed;
    draws_.clear();
    if(scene && usesSceneBackground()) draws_=*scene;
    else {
        const int x=-static_cast<int>(scroll_);
        draws_={{clouds_,x,65,0},{clouds_,x+800,65,0},{backdrop_,0,0,0}};
    }
    switch(page_) {
    case Page::title:
        draws_.push_back({logo_,234,180,0});
        draws_.push_back({logoDetail_,429,316,static_cast<int>(scroll_)%4});
        appendButtons();
        break;
    case Page::packs:
    case Page::levelsets: {
        draws_.push_back({helpBackdrop_,0,0,0});
        draws_.push_back({packPanel_,150,155,0});
        large_.append(draws_,data_.text("SelectPacks"),150,120);
        const auto entries=episodes();
        const int first=std::clamp(episodeFirst_,0,std::max(0,static_cast<int>(entries.size())-7));
        for(int i=first;i<std::min(static_cast<int>(entries.size()),first+7);++i) {
            const int y=158+(i-first)*27;
            const auto& entry=entries[i];
            const bool selected=entry==std::make_pair(packIndex_,setIndex_);
            if(selected) draws_.push_back({highlight_,154,y,0});
            (selected?green_:large_).append(draws_,data_.packs()[entry.first].name+"/"+data_.packs()[entry.first].levelsets[entry.second].name,182,y);
        }
        if(first>0) draws_.push_back({arrows_,510,158,within(mouse_,510,158,32,32)?5:4});
        if(first+7<static_cast<int>(entries.size())) draws_.push_back({arrows_,510,330,within(mouse_,510,330,32,32)?7:6});
        small_.append(draws_,data_.text("MorePacks"),150,360);
        appendButtons();
        break;
    }
    case Page::help: appendHelp();appendButtons();break;
    case Page::highscores: appendScores();appendButtons();break;
    case Page::pause:
    case Page::quit:
        if(page_==Page::quit && quitReturn_==Page::title) {
            draws_.push_back({logo_,234,180,0});
            draws_.push_back({logoDetail_,429,316,static_cast<int>(scroll_)%4});
            appendButtons();
        }
        // Original leave-game prompt, 0x4123C0.
        draws_.push_back({pausePanel_,200,100,0});
        centered(large_,data_.text("Quit1"),400,110);
        centered(large_,data_.text("Quit2"),400,125);
        centered(large_,data_.text("Quit3"),400,145);
        draws_.push_back({leaveImage_,360,193,0});
        centered(dialogChoice_==0?green_:large_,data_.text("Yes"),330,211);
        centered(dialogChoice_==1?green_:large_,data_.text("No"),470,211);
        break;
    case Page::results:
        if(result_.gameOver) {
            draws_.push_back({pausePanel_,200,100,0});
            draws_.push_back({gameOverImage_,250,130,0});
            centered(large_,data_.text("GameOver"),485,150);
            if(promptVisible) centered(small_,data_.text("PressSpace"),485,200);
        } else if(result_.episodeComplete) {
            draws_.push_back({scorePanel_,78,0,0});
            draws_.push_back({packFinish_,315,294,0});
            centered(large_,data_.text("Congratulations"),400,80);
            centered(small_,data_.text("LevelPackMadeIt1")+levelset().name+data_.text("LevelPackMadeIt2"),400,140);
            draws_.push_back({score_,321,167,result_.scoreFrame});
            small_.append(draws_,padded(result_.score),375,182);
            draws_.push_back({sheep_,400-sheep_->frameWidth/2,255-sheep_->frameHeight/2,0});
            small_.append(draws_,padded(result_.totalSheep),360,247);
            if(promptVisible && (result_.episodeComplete ||
               (result_.timeRemaining<=0 && result_.sheepRemaining<=0 && result_.livesRemaining<=0)))
                centered(small_,data_.text("PressSpace"),400,450);
        } else {
            // Sven2_Results_DrawLevelCompletion, 0x411B20.
            draws_.push_back({scorePanel_,78,0,0});
            large_.append(draws_,data_.text("LevelBonus"),320,60);
            small_.append(draws_,data_.text("LevelPack"),320,100);
            small_.append(draws_,levelset().name,400,100);
            small_.append(draws_,data_.text("BonusLevel"),320,120);
            small_.append(draws_,(result_.level>0 && static_cast<size_t>(result_.level)<=levelset().levels.size()?levelset().levels[result_.level-1].name:std::to_string(result_.level)),400,120);
            draws_.push_back({timer_,307-timer_->frameWidth/2,140,0});
            const int remaining=std::max(0,result_.timeRemaining);
            small_.append(draws_,padded(remaining/60,2)+":"+(remaining%60<10?"0":"")+std::to_string(remaining%60),282,172);
            draws_.push_back({sheep_,310-sheep_->frameWidth/2,240-sheep_->frameHeight/2,0});
            small_.append(draws_,padded(result_.sheepRemaining),270,232);
            draws_.push_back({life_,300-life_->frameWidth/2,300-life_->frameHeight/2,0});
            small_.append(draws_,padded(result_.livesRemaining),290,292);
            constexpr int rows[]={172,232,292};
            constexpr const char* multipliers[]={"x 1","x 10","x 25"};
            const int points[]={result_.time-result_.timeRemaining,
                                (result_.sheep-result_.sheepRemaining)*10,
                                (result_.lives-result_.livesRemaining)*25};
            for(int i=0;i<3;++i) {
                small_.append(draws_,multipliers[i],360,rows[i]);
                small_.append(draws_,"=",410,rows[i]);
                small_.append(draws_,padded(points[i]),450,rows[i]);
            }
            small_.append(draws_,data_.text("BonusTotal"),365,340);
            small_.append(draws_,padded(points[0]+points[1]+points[2]),450,340);
            draws_.push_back({score_,395,380,result_.scoreFrame});
            small_.append(draws_,padded(result_.score),450,395);
            if(promptVisible && (result_.episodeComplete ||
               (result_.timeRemaining<=0 && result_.sheepRemaining<=0 && result_.livesRemaining<=0)))
                centered(small_,data_.text("PressSpace"),400,450);
        }
        break;
    case Page::name:
        appendScores();
        break;
    }
    if(!notice_.empty()) small_.appendWrapped(draws_,notice_,120,460,560,20);
    draws_.push_back({cursor_,mouse_.x-9,mouse_.y-9,buttonAt(mouse_)>=0?1:0});
    return draws_;
}
}
