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
		Name,
		Bonus
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
		return CurrentPage == Page::Pause || CurrentPage == Page::Results || CurrentPage == Page::Bonus || (CurrentPage == Page::Quit && QuitReturn == Page::Pause);
	}
	i32 GetPlayerCount() const;
	void SetPlayerCount(i32 playerCount);
	void SetControlBinding(i32 player, i32 action, i32 key);
	i32 GetControlSelection() const;
	const std::array<std::array<i32, 5>, 2>& GetControlBindings() const;
	bool HasCustomControls() const;
	i32 CardCount() const;
	void Show(Page page);
	void ShowResults(const RoundResult& result);
	void ShowBonus(BonusMode mode);
	void SetNotice(const str& notice);
	std::vector<str> TakeSounds();

private:
	Highscores& Scores();
	const Sprite* LoadTheme(const str& path, i16 rows = 1, i16 columns = 1, bool opaque = false);
	const Sprite* LoadNamedTheme(cstr name);
	str Text(const str& key) const;
	str EpisodeLabel(size_t pack, size_t levelSet) const;
	void AppendTitle();
	void AppendMenuEyes();
	void AppendPlayerHeader();
	void AppendControls();
	void AppendButtons();
	void AppendHelp();
	void AppendScores();
	void LoadCards();
	void LoadControlSettings();
	void SaveControlSettings() const;
	POINT ControlPosition(i32 player, i32 action) const;
	i32 ControlAt(POINT mouse) const;
	std::vector<std::pair<size_t, size_t>> Episodes() const;
	void StepEpisode(i32 step);
	void Centered(const BitmapFont& font, const str& text, i32 center, i32 y);
	i32 ButtonAt(POINT mouse) const;
	MudGE& Engine;
	const GameData& Data;
	fpath Saves;
	GameEdition Theme;
	BitmapFont Large, Small, Green, SmallGreen;
	i32 RequestedPlayerCount = 1;
	i32 ScorePlayers = 1;
	BonusMode IntroMode = BonusMode::Sven;
	std::array<std::array<i32, 5>, 2> ControlBindings{};
	i32 ControlSelection = -1;
	std::array<std::array<bool, 5>, 2> CustomBindings{};
	std::vector<str> KeyNames;
	std::array<ShagAnimation, 5> ControlAnimations;
	std::array<const Sprite*, 19> TitleBlinks{};
	std::array<f32, 19> TitleBlinkTimers{};
	std::array<const Sprite*, 4> TitleActors{};
	u32 TitleRandom = 0x585858;
	u32 NextTitleRandom();
	const Sprite* ControlsPanel = nullptr;
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
	std::array<SpriteDraw, 4> CatchPreviews{};
	std::array<const Sprite*, 4> Distractions{};
	std::array<const Sprite*, 5> MenuEyes{};
	const Sprite* MenuSven = nullptr;
	std::array<const Sprite*, 2> PlayerIcons{};
	f32 Scroll = 0, UiAge = 0;
	str EnteredName, Notice;
	POINT Mouse{ 400, 300 };
	POINT EyeMouse{ 400, 300 };
};
