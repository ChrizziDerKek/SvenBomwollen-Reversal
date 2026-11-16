#pragma once
#include "Types.h"
#include "Engine/BitmapFont.h"
#include "Game/Highscores.h"
#include "Engine/Structures.h"
#include "Scene/ShagAnimation.h"
#include <memory>
#include "Input/WindowInput.h"
#include <array>

enum class UiAction
{
	None,
	Start,
	Resume,
	Abandon,
	Exit,
	NextLevel
};

class GameUi
{
public:
	enum class Page
	{
		Title,
		GetPacks,
		LevelSets,
		Help,
		Highscores,
		Pause,
		Quit,
		Results,
		Name
	};
	GameUi(MudGE& engine, fpath saves);
	UiAction Handle(const WindowInput::Result& input);
	const std::vector<SpriteDraw>& Draws(f32 elapsed, const std::vector<SpriteDraw>* scene = nullptr);
	const LevelPackInfo& GetPack() const;
	const LevelSetInfo& GetLevelSet() const;
	Page GetPage() const
	{
		return CurrentPage;
	}
	bool UsesSceneBackground() const
	{
		return CurrentPage == Page::Pause || CurrentPage == Page::Results ||
		       (CurrentPage == Page::Quit && QuitReturn == Page::Pause);
	}
	void Show(Page page);
	void ShowResults(const RoundResult& result);
	void SetNotice(const str& notice);
	std::vector<str> TakeSounds();

private:
	Highscores& Scores();
	void AppendButtons();
	void AppendHelp();
	void AppendScores();
	std::vector<std::pair<size_t, size_t>> Episodes() const;
	void StepEpisode(i32 step);
	void Centered(const BitmapFont& font, const str& text, i32 center, i32 y);
	i32 ButtonAt(POINT mouse) const;
	MudGE& Engine;
	const GameData& Data;
	fpath Saves;
	BitmapFont Large, Small, Green;
	const Sprite *Clouds, *Backdrop, *Logo, *LogoDetail, *Buttons, *PackPanel, *HelpBackdrop;
	const Sprite *HelpTabs, *ScorePanel, *PackFinish, *ScoreTabs, *Highlight, *PausePanel;
	std::vector<SpriteDraw> DrawCommands;
	std::vector<str> Sounds;
	std::map<str, std::unique_ptr<Highscores>> ScoreTables;
	Page CurrentPage = Page::Title, QuitReturn = Page::Title;
	size_t PackIndex = 0, SetIndex = 0;
	i32 SelectedButton = 0, HelpTab = 0, HelpCharacter = 0;
	RoundResult Result;
	const Sprite *Timer, *Life, *Sheep, *Score;
	const Sprite *Icons, *Artifact, *Arrows, *Cursor;
	const Sprite *CardBlank, *NoAnimation, *LeaveImage, *GameOverImage;
	std::vector<ShagAnimation> CardAnimations;
	bool CardsTab = false, CardPlaying = false;
	i32 CardIndex = 0;
	i32 EpisodeFirst = 0, DialogChoice = 0;
	f32 CardAge = 0;
	std::array<const Sprite*, 4> People{};
	f32 Scroll = 0, UiAge = 0;
	str EnteredName, Notice;
	POINT Mouse{400, 300};
};
