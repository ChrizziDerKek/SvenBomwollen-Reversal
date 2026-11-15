#pragma once
#include "BitmapFont.h"
#include "Highscores.h"
#include "RoundResult.h"
#include "InteractionAnimations.h"
#include <memory>
#include <array>

namespace sven2 {

struct UiInput {
    POINT mouse{};
    bool mouseMoved=false, click=false, confirm=false, back=false;
    bool left=false, right=false, up=false, down=false, tab=false;
    bool erase=false;
    std::string text;
};

enum class UiAction { none, start, resume, abandon, exit, nextLevel };

class GameUi {
public:
    enum class Page { title, packs, levelsets, help, highscores, pause, quit, results, name };
    GameUi(Engine& engine, std::filesystem::path saves);
    UiAction handle(const UiInput& input);
    const std::vector<SpriteDraw>& draws(float elapsed, const std::vector<SpriteDraw>* scene=nullptr);
    const LevelPackInfo& pack() const;
    const LevelSetInfo& levelset() const;
    Page page() const { return page_; }
    bool usesSceneBackground() const {
        return page_==Page::pause || page_==Page::results ||
            (page_==Page::quit && quitReturn_==Page::pause);
    }
    void show(Page page);
    void showResults(const RoundResult& result);
    void setNotice(const std::string& notice);
    std::vector<std::string> takeSounds();

private:
    Highscores& scores();
    void appendButtons();
    void appendHelp();
    void appendScores();
    std::vector<std::pair<size_t,size_t>> episodes() const;
    void stepEpisode(int step);
    void centered(const BitmapFont& font,const std::string& text,int center,int y);
    int buttonAt(POINT mouse) const;
    Engine& engine_;
    const GameData& data_;
    std::filesystem::path saves_;
    BitmapFont large_,small_,green_;
    const Sprite *clouds_,*backdrop_,*logo_,*logoDetail_,*buttons_,*packPanel_,*helpBackdrop_;
    const Sprite *helpTabs_,*scorePanel_,*packFinish_,*scoreTabs_,*highlight_,*pausePanel_;
    std::vector<SpriteDraw> draws_;
    std::vector<std::string> sounds_;
    std::map<std::string,std::unique_ptr<Highscores>> scoreTables_;
    Page page_=Page::title, quitReturn_=Page::title;
    size_t packIndex_=0, setIndex_=0;
    int selectedButton_=0, helpTab_=0, helpCharacter_=0;
    RoundResult result_;
    const Sprite *timer_,*life_,*sheep_,*score_;
    const Sprite *icons_,*artifact_,*arrows_,*cursor_;
    const Sprite *cardBlank_,*noAnimation_,*leaveImage_,*gameOverImage_;
    std::vector<InteractionAnimation> cardAnimations_;
    bool cardsTab_=false,cardPlaying_=false;
    int cardIndex_=0;
    int episodeFirst_=0, dialogChoice_=0;
    float cardAge_=0;
    std::array<const Sprite*,4> people_{};
    float scroll_=0,uiAge_=0;
    std::string enteredName_,notice_;
    POINT mouse_{400,300};
};
}
