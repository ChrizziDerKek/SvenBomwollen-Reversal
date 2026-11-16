#include "Game/GameUi.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace
{
str Padded(i32 value, i32 width = 5)
{
	std::ostringstream text;
	text << std::setw(width) << value;
	return text.str();
}
i32 Wrap(i32 value, i32 length)
{
	return (value + length) % length;
}
bool Within(POINT point, i32 x, i32 y, i32 width, i32 height)
{
	return point.x >= x && point.y >= y && point.x < x + width && point.y < y + height;
}
str Numbered(cstr prefix, i32 index)
{
	std::ostringstream text;
	text << prefix << std::setfill('0') << std::setw(2) << index;
	return text.str();
}
} // namespace

GameUi::GameUi(MudGE& engine, fpath saves)
    : Engine(engine), Data(engine.GetData()), Saves(std::move(saves)), Large(engine), Small(engine, true), Green(engine, false, true), Clouds(engine.LoadSprite("Sven2/gfx/mainmenu_clouds.jpg", 1, 1, true)), Backdrop(engine.LoadSprite("Sven2/gfx/mainmenu_BG.jpg", 1, 1)), Logo(engine.LoadSprite("Sven2/gfx/SvenLogo.bmp", 1, 1)), LogoDetail(engine.LoadSprite("Sven2/gfx/SvenLogo_O.bmp", 4, 1)), Buttons(engine.LoadSprite("Sven2/gfx/mainmenu_buttons.bmp", 10, 1)), PackPanel(engine.LoadSprite("Sven2/gfx/smallmenu_choose_level.bmp", 1, 1)), HelpBackdrop(engine.LoadSprite("Sven2/gfx/Help_BG.jpg", 1, 1)), HelpTabs(engine.LoadSprite("Sven2/gfx/Help_file_cards.bmp", 8, 1)), ScorePanel(engine.LoadSprite("Sven2/gfx/ScoreScreen_BG.bmp", 1, 1)), PackFinish(engine.LoadSprite("Sven2/gfx/ScoreScreen_BG_levelpack.bmp", 1, 1)), ScoreTabs(engine.LoadSprite("Sven2/gfx/Highsc_file_cards.bmp", 10, 1)), Highlight(engine.LoadSprite("Sven2/gfx/selection_bar.bmp", 1, 1)), PausePanel(engine.LoadSprite("Sven2/gfx/smallmenu_BG.bmp", 1, 1)), Timer(engine.LoadSprite("Sven2/gfx/timer.bmp", 1, 1)), Life(engine.LoadSprite("Sven2/gfx/Life_icon.bmp", 1, 1)), Sheep(engine.LoadSprite("Sven2/gfx/Sheep/Sheep_Stand.bmp", 4, 1)), Score(engine.LoadSprite("Sven2/gfx/score.bmp", 4, 1)), Icons(engine.LoadSprite("Sven2/gfx/icons.bmp", 5, 1)), Artifact(engine.LoadSprite("Sven2/gfx/Artifact.bmp", 1, 1)), Arrows(engine.LoadSprite("Sven2/gfx/pijlen.bmp", 8, 1)), Cursor(engine.LoadSprite("Sven2/gfx/mousecursor.bmp", 5, 1)), CardBlank(engine.LoadSprite("Sven2/gfx/Highsc_file_card_blank.bmp", 1, 1)), NoAnimation(engine.LoadSprite("Sven2/gfx/noanim.bmp", 1, 1)), LeaveImage(engine.LoadSprite("Sven2/gfx/smallmenu_BG_verlassen.bmp", 1, 1)), GameOverImage(engine.LoadSprite("Sven2/gfx/smallmenu_BG_gameover.bmp", 1, 1))
{
	People = { engine.LoadSprite("Sven2/gfx/Dog/Dog_Stand.bmp", 8, 1), engine.LoadSprite("Sven2/gfx/Guy/Guy_Stand.bmp", 8, 1), engine.LoadSprite("Sven2/gfx/Broemse/Broemse_stand.bmp", 8, 1), engine.LoadSprite("Sven2/gfx/Alien/Alien_Stand.bmp", 8, 1) };
	for (size_t i = 0; i < Data.GetPacks().size(); ++i)
		if (Data.GetPacks()[i].Name == "XS")
			PackIndex = i;
	DrawCommands.reserve(1200);
}

const LevelPackInfo& GameUi::GetPack() const
{
	return Data.GetPacks().at(PackIndex);
}
const LevelSetInfo& GameUi::GetLevelSet() const
{
	return GetPack().LevelSets.at(SetIndex);
}
void GameUi::Show(Page page)
{
	CurrentPage = page;
	Notice.clear();
	if (CurrentPage == Page::Name || CurrentPage == Page::Results)
		UiAge = 0;
	if (CurrentPage == Page::Pause || CurrentPage == Page::Quit)
		DialogChoice = 0;
	if (CurrentPage == Page::GetPacks || CurrentPage == Page::LevelSets)
	{
		const auto entries = Episodes();
		const auto selected = std::find(entries.begin(), entries.end(), std::make_pair(PackIndex, SetIndex));
		EpisodeFirst = selected == entries.end() ? 0 : std::max(0, static_cast<i32>(selected - entries.begin()) - 6);
	}
}
void GameUi::SetNotice(const str& message)
{
	this->Notice = message;
}
std::vector<str> GameUi::TakeSounds()
{
	std::vector<str> events;
	events.swap(Sounds);
	return events;
}

// Original episode list and highscore traversal: 0x428180 / 0x416C05.
std::vector<std::pair<size_t, size_t>> GameUi::Episodes() const
{
	std::vector<std::pair<size_t, size_t>> entries;
	for (size_t p = 0; p < Data.GetPacks().size(); ++p)
		for (size_t e = 0; e < Data.GetPacks()[p].LevelSets.size(); ++e)
			entries.emplace_back(p, e);
	return entries;
}

void GameUi::StepEpisode(i32 step)
{
	const auto entries = Episodes();
	const auto found = std::find(entries.begin(), entries.end(), std::make_pair(PackIndex, SetIndex));
	if (found == entries.end())
		return;
	const i32 next = std::clamp(static_cast<i32>(found - entries.begin()) + step, 0, static_cast<i32>(entries.size()) - 1);
	PackIndex = entries[next].first;
	SetIndex = entries[next].second;
}

Highscores& GameUi::Scores()
{
	const str key = GetPack().Name + "-" + std::to_string(SetIndex);
	auto& table = ScoreTables[key];
	if (!table)
	{
		std::vector<HighscoreEntry> defaults;
		std::istringstream names(Data.StringParam("DefaultHighscoreNames", "A/B/C/D/E/F/G/H/I/J"));
		str name;
		while (defaults.size() < 10 && std::getline(names, name, '/'))
			defaults.push_back({ name, 1000 - static_cast<i32>(defaults.size()) * 100, 0 });
		table = std::make_unique<Highscores>(Saves / std::filesystem::u8path(key + ".tsv"), std::move(defaults));
	}
	return *table;
}

i32 GameUi::ButtonAt(POINT point) const
{
	constexpr i32 positions[] = { 40, 210, 380, 630 }, widths[] = { 115, 115, 200, 115 };
	for (i32 i = 0; i < 4; ++i)
		if (Within(point, positions[i], 545, widths[i], 30))
			return i;
	return -1;
}

void GameUi::ShowResults(const RoundResult& summary)
{
	this->Result = summary;
	CardsTab = false;
	if (CurrentPage != Page::Results)
		Show(Page::Results);
}

UiAction GameUi::Handle(const WindowInput::Result& input)
{
	Mouse = input.Mouse;
	if (CurrentPage == Page::Results)
	{
		if (!input.Confirm)
			return UiAction::None;
		if (!Result.EpisodeComplete && !Result.GameOver)
			return UiAction::NextLevel;
		if (Scores().Qualifies(Result.Score))
		{
			EnteredName.clear();
			Show(Page::Name);
		}
		else
		{
			Show(Page::Highscores);
		}
		return UiAction::None;
	}
	if (CurrentPage == Page::Name)
	{
		if (input.Erase && !EnteredName.empty())
		{
			size_t last = EnteredName.size() - 1;
			while (last && (static_cast<u8>(EnteredName[last]) & 0xc0) == 0x80)
				--last;
			EnteredName.erase(last);
		}
		if (!input.Text.empty())
		{
			for (size_t i = 0; i < input.Text.size();)
			{
				const size_t start = i++;
				while (i < input.Text.size() && (static_cast<u8>(input.Text[i]) & 0xc0) == 0x80)
					++i;
				if (static_cast<u8>(input.Text[start]) < 32)
					continue;
				const str candidate = EnteredName + input.Text.substr(start, i - start);
				const i32 chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, candidate.c_str(), -1, nullptr, 0);
				if (chars > 0 && chars <= 21)
					EnteredName = candidate;
			}
		}
		if (input.Confirm && !EnteredName.empty())
		{
			Scores().Submit(EnteredName, Result.Score, Result.Level, Result.TotalSheep);
			Show(Page::Highscores);
		}
		return UiAction::None;
	}
	if (CurrentPage == Page::Pause || CurrentPage == Page::Quit)
	{
		const bool paused = CurrentPage == Page::Pause || QuitReturn == Page::Pause;
		if (input.Back)
		{
			if (CurrentPage == Page::Pause)
				return UiAction::Resume;
			Show(QuitReturn);
			return UiAction::None;
		}
		if (input.Left || input.Right || input.Tab)
			DialogChoice = 1 - DialogChoice;
		const i32 hovered = Within(input.Mouse, 310, 211, 40, 32) ? 0 : (Within(input.Mouse, 430, 211, 80, 32) ? 1 : -1);
		if (input.MouseMoved && hovered >= 0)
			DialogChoice = hovered;
		if (input.Confirm || (input.Click && hovered >= 0))
		{
			const i32 choice = input.Click ? hovered : DialogChoice;
			if (choice == 0)
				return paused ? UiAction::Abandon : UiAction::Exit;
			if (paused)
				return UiAction::Resume;
			Show(QuitReturn);
		}
		return UiAction::None;
	}
	if (input.Back)
	{
		Show(CurrentPage == Page::LevelSets ? Page::GetPacks : Page::Title);
		return UiAction::None;
	}
	if (CurrentPage == Page::Help)
	{
		if (input.Left)
			HelpTab = Wrap(HelpTab - 1, 4);
		if (input.Right || input.Tab)
			HelpTab = Wrap(HelpTab + 1, 4);
		if (input.Up)
			HelpCharacter = std::max(0, HelpCharacter - 1);
		if (input.Down)
			HelpCharacter = std::min(3, HelpCharacter + 1);
		if (input.Click && HelpTab == 1)
		{
			if (Within(input.Mouse, 100, 80, 32, 32))
				HelpCharacter = std::max(0, HelpCharacter - 1);
			if (Within(input.Mouse, 308, 80, 32, 32))
				HelpCharacter = std::min(3, HelpCharacter + 1);
		}
		if (input.Click && input.Mouse.y >= 21 && input.Mouse.y < 73 && input.Mouse.x >= 100 && input.Mouse.x < 532)
		{
			HelpTab = std::clamp(static_cast<i32>((input.Mouse.x - 100) / 100), 0, 3);
			return UiAction::None;
		}
	}
	else if (CurrentPage == Page::Highscores)
	{
		if (input.Click && Within(input.Mouse, 100, 21, 132, 52))
			CardsTab = false;
		if (input.Click && Within(input.Mouse, 500, 21, 132, 52))
			CardsTab = true;
		if (CardsTab)
		{
			i32 change = (input.Right || (input.Click && Within(input.Mouse, 238, 350, 32, 32))) - (input.Left || (input.Click && Within(input.Mouse, 160, 350, 32, 32)));
			if (change)
			{
				CardIndex = Wrap(CardIndex + change, 33);
				CardPlaying = false;
				CardAge = 0;
			}
			if (!CardPlaying && CardIndex < Data.NumericParam("ShagAnims", 33) && (input.Confirm || (input.Click && Within(input.Mouse, 150, 130, 132, 206))))
			{
				if (CardAnimations.empty())
					CardAnimations = ShagAnimation::Load(Engine);
				CardPlaying = true;
				CardAge = 0;
				Sounds.push_back(CardAnimations.at(static_cast<size_t>(CardIndex) * 2).Sound);
			}
		}
		else
		{
			const bool previous = input.Left || input.Up || (input.Click && Within(input.Mouse, 100, 90, 32, 32));
			const bool next = input.Right || input.Down || (input.Click && Within(input.Mouse, 268, 90, 32, 32));
			if (previous || next)
				StepEpisode(previous ? -1 : 1);
		}
	}
	else if (CurrentPage == Page::GetPacks || CurrentPage == Page::LevelSets)
	{
		const auto entries = Episodes();
		if (entries.empty())
			return UiAction::None;
		auto found = std::find(entries.begin(), entries.end(), std::make_pair(PackIndex, SetIndex));
		i32 selected = found == entries.end() ? 0 : static_cast<i32>(found - entries.begin());
		if (input.Up || input.Down)
			selected = std::clamp(selected + (input.Up ? -1 : 1), 0, static_cast<i32>(entries.size()) - 1);
		if (input.Up || input.Down)
		{
			if (selected < EpisodeFirst)
				EpisodeFirst = selected;
			if (selected >= EpisodeFirst + 7)
				EpisodeFirst = selected - 6;
		}
		if (input.Click && Within(input.Mouse, 510, 158, 32, 32))
			EpisodeFirst = std::max(0, EpisodeFirst - 1);
		if (input.Click && Within(input.Mouse, 510, 330, 32, 32))
			EpisodeFirst = std::min(std::max(0, static_cast<i32>(entries.size()) - 7), EpisodeFirst + 1);
		bool activate = input.Confirm;
		if ((input.MouseMoved || input.Click) && Within(input.Mouse, 154, 158, 350, 189))
		{
			const i32 row = (input.Mouse.y - 158) / 27 + EpisodeFirst;
			if (row < static_cast<i32>(entries.size()))
			{
				selected = row;
				activate = input.Click;
			}
		}
		PackIndex = entries[selected].first;
		SetIndex = entries[selected].second;
		if (activate)
			return UiAction::Start;
	}
	if (input.Tab || (CurrentPage == Page::Title && (input.Left || input.Right)))
	{
		SelectedButton = Wrap(SelectedButton + (input.Left ? -1 : 1), 4);
	}
	const i32 hovered = ButtonAt(input.Mouse);
	if (input.MouseMoved && hovered >= 0)
		SelectedButton = hovered;
	const i32 activated = input.Click ? hovered : (CurrentPage == Page::Title && input.Confirm ? SelectedButton : -1);
	switch (activated)
	{
	case 0:
		Show(CurrentPage == Page::GetPacks ? Page::Title : Page::GetPacks);
		break;
	case 1:
		Show(Page::Help);
		break;
	case 2:
		Show(Page::Highscores);
		break;
	case 3:
		QuitReturn = Page::Title;
		Show(Page::Quit);
		break;
	default:
		break;
	}
	return UiAction::None;
}

void GameUi::Centered(const BitmapFont& font, const str& text, i32 center, i32 y)
{
	font.Append(DrawCommands, text, center - font.Width(text) / 2, y);
}

void GameUi::AppendButtons()
{
	constexpr i32 x[] = { 40, 210, 380, 630 };
	for (i32 i = 0; i < 4; ++i)
		DrawCommands.push_back({ Buttons, x[i], 545, 2 * i + (SelectedButton != i) });
}

void GameUi::AppendHelp()
{
	DrawCommands.push_back({ HelpBackdrop, 0, 0, 0 });
	constexpr i32 frames[] = { 2, 4, 0, 6 };
	// Original 0x4146E0: inactive tabs first, active last. The paper starts at y=60.
	for (i32 i = 0; i < 4; ++i)
		if (i != HelpTab)
			DrawCommands.push_back({ HelpTabs, 100 + i * 100, 34, frames[i] + 1, false, 60 });
	DrawCommands.push_back({ HelpTabs, 100 + HelpTab * 100, 21, frames[HelpTab], false, 60 });
	if (HelpTab == 0)
	{
		constexpr i32 positions[] = { 80, 140, 220, 320, 420 };
		for (i32 i = 0; i < 5; ++i)
			Small.AppendWrapped(DrawCommands, Data.Text(Numbered("spiel", i + 1)), 120, positions[i], 535, 20);
	}
	else if (HelpTab == 1)
	{
		constexpr cstr families[] = { "Dog", "Guy", "Broemse", "Alien" };
		const str person = families[HelpCharacter];
		const Sprite* actor = this->People[HelpCharacter];
		DrawCommands.push_back({ actor, 160 - actor->FrameWidth / 2, 180 - actor->FrameHeight / 2, 3 });
		if (HelpCharacter > 0)
			DrawCommands.push_back({ Arrows, 100, 80, Within(Mouse, 100, 80, 32, 32) ? 1 : 0 });
		if (HelpCharacter < 3)
			DrawCommands.push_back({ Arrows, 308, 80, Within(Mouse, 308, 80, 32, 32) ? 3 : 2 });
		Centered(Large, Data.Text(person + "00"), 220, 80);
		Small.AppendWrapped(DrawCommands, Data.Text(person + "01"), 230, 140, 460, 20);
		Small.AppendWrapped(DrawCommands, Data.Text(person + "02"), 120, 280, 410, 20);
	}
	else if (HelpTab == 2)
	{
		Small.AppendWrapped(DrawCommands, Data.Text("extra01"), 120, 80, 535, 20);
		constexpr i32 iconFrames[] = { 0, 2, 3, 4, 1 };
		for (i32 i = 2; i <= 7; ++i)
		{
			const i32 y = 140 + (i - 2) * 60;
			const Sprite* image = i == 7 ? Artifact : Icons;
			const i32 center = i == 2 ? 175 : y + 20;
			DrawCommands.push_back({ image, 150 - image->FrameWidth / 2, center - image->FrameHeight / 2, i == 7 ? 0 : iconFrames[i - 2] });
			Small.Append(DrawCommands, Data.Text(Numbered("extra", i)), 200, y);
			Small.AppendWrapped(DrawCommands, Data.Text(Numbered("extra_", i)), 200, y + 20, 455, 20);
		}
	}
	else
	{
		constexpr i32 y[] = { 100, 120, 140, 160, 180, 200, 220, 260, 280, 300, 340, 360, 380 };
		for (i32 i = 0; i < 13; ++i)
		{
			Small.AppendWrapped(DrawCommands, Data.Text(Numbered("credits", i * 2 + 1)), 120, y[i], 220, 20);
			Small.AppendWrapped(DrawCommands, Data.Text(Numbered("credits", i * 2 + 2)), 340, y[i], 350, 20);
		}
		Small.Append(DrawCommands, "v1.02", 670, 475);
	}
}

void GameUi::AppendScores()
{
	DrawCommands.push_back({ HelpBackdrop, 0, 0, 0 });
	// Original 0x4171A0 draws the inactive tab before the active tab.
	DrawCommands.push_back({ ScoreTabs, CardsTab ? 100 : 500, 34, CardsTab ? 1 : 9, false, 60 });
	DrawCommands.push_back({ ScoreTabs, CardsTab ? 500 : 100, 21, CardsTab ? 8 : 0, false, 60 });
	if (CardsTab)
	{
		DrawCommands.push_back({ CardBlank, 150, 130, 0 });
		if (CardIndex < Data.NumericParam("ShagAnims", 33))
		{
			if (CardAnimations.empty())
				CardAnimations = ShagAnimation::Load(Engine);
			const ShagAnimation& card = CardAnimations.at(static_cast<size_t>(CardIndex) * 2);
			i32 frame = 0;
			if (CardPlaying)
			{
				const size_t step = static_cast<size_t>(CardAge * 15); // Original browser reset at 0x416A04.
				if (step < card.Frames.size())
					frame = card.Frames[step];
				else
				{
					CardPlaying = false;
					CardAge = 0;
				}
			}
			DrawCommands.push_back({ card.Sprite, 150, 140, frame, card.Mirror });
		}
		else
		{
			DrawCommands.push_back({ NoAnimation, 150, 140, 0 });
		}
		Large.AppendWrapped(DrawCommands, Data.Text(Numbered("Card_", CardIndex + 1)), 310, 150, 380, 25);
		Small.AppendWrapped(DrawCommands, Data.Text(Numbered("Card", CardIndex + 1)), 310, 200, 380, 20);
		Centered(Large, std::to_string(CardIndex + 1), 215, 350);
		DrawCommands.push_back({ Arrows, 160, 350, Within(Mouse, 160, 350, 32, 32) ? 1 : 0 });
		DrawCommands.push_back({ Arrows, 238, 350, Within(Mouse, 238, 350, 32, 32) ? 3 : 2 });
		return;
	}
	Centered(Small, GetPack().Name + "/" + GetLevelSet().Name, 200, 100);
	const auto entries = Episodes();
	if (!entries.empty() && entries.front() != std::make_pair(PackIndex, SetIndex))
		DrawCommands.push_back({ Arrows, 100, 90, Within(Mouse, 100, 90, 32, 32) ? 1 : 0 });
	if (!entries.empty() && entries.back() != std::make_pair(PackIndex, SetIndex))
		DrawCommands.push_back({ Arrows, 268, 90, Within(Mouse, 268, 90, 32, 32) ? 3 : 2 });
	DrawCommands.push_back({ Score, 375, 70, 0 });
	Large.Append(DrawCommands, Data.Text("HighscoreLevel"), 540, 80);
	DrawCommands.push_back({ Sheep, 595, 35, 1 });
	auto entriesToDraw = Scores().GetEntries();
	i32 editingRow = -1;
	if (CurrentPage == Page::Name)
	{
		auto position = std::find_if(entriesToDraw.begin(), entriesToDraw.end(), [&](const HighscoreEntry& entry)
		                             { return entry.Score < Result.Score; });
		editingRow = static_cast<i32>(position - entriesToDraw.begin());
		entriesToDraw.insert(position, { EnteredName, Result.Score, Result.Level, Result.TotalSheep });
		if (entriesToDraw.size() > 10)
			entriesToDraw.resize(10);
	}
	i32 row = 0;
	for (const auto& entry : entriesToDraw)
	{
		const i32 y = 130 + row * 28;
		Large.Append(DrawCommands, Padded(++row, 2) + ".", 105, y);
		// Original 0x417697 appends literal I (frame 8), on even 15 Hz ticks.
		const bool caret = row - 1 == editingRow && static_cast<i32>(UiAge * 15) % 2 == 0;
		Large.Append(DrawCommands, entry.Name + (caret ? "I" : ""), 150, y);
		Large.Append(DrawCommands, Padded(entry.Score), 430, y);
		Large.Append(DrawCommands, Padded(entry.Level, 2), 555, y);
		Large.Append(DrawCommands, Padded(entry.Sheep, 4), 620, y);
	}
}

const std::vector<SpriteDraw>& GameUi::Draws(f32 elapsed, const std::vector<SpriteDraw>* scene)
{
	UiAge += elapsed;
	const bool promptVisible = static_cast<i32>(UiAge * 15) % 5 != 0;
	Scroll = std::fmod(Scroll + elapsed * 10, 800.0f);
	if (CardPlaying)
		CardAge += elapsed;
	DrawCommands.clear();
	if (scene && UsesSceneBackground())
		DrawCommands = *scene;
	else
	{
		const i32 x = -static_cast<i32>(Scroll);
		DrawCommands = { { Clouds, x, 65, 0 }, { Clouds, x + 800, 65, 0 }, { Backdrop, 0, 0, 0 } };
	}
	switch (CurrentPage)
	{
	case Page::Title:
		DrawCommands.push_back({ Logo, 234, 180, 0 });
		DrawCommands.push_back({ LogoDetail, 429, 316, static_cast<i32>(Scroll) % 4 });
		AppendButtons();
		break;
	case Page::GetPacks:
	case Page::LevelSets:
	{
		DrawCommands.push_back({ HelpBackdrop, 0, 0, 0 });
		DrawCommands.push_back({ PackPanel, 150, 155, 0 });
		Large.Append(DrawCommands, Data.Text("SelectPacks"), 150, 120);
		const auto entries = Episodes();
		const i32 first = std::clamp(EpisodeFirst, 0, std::max(0, static_cast<i32>(entries.size()) - 7));
		for (i32 i = first; i < std::min(static_cast<i32>(entries.size()), first + 7); ++i)
		{
			const i32 y = 158 + (i - first) * 27;
			const auto& entry = entries[i];
			const bool selected = entry == std::make_pair(PackIndex, SetIndex);
			if (selected)
				DrawCommands.push_back({ Highlight, 154, y, 0 });
			(selected ? Green : Large).Append(DrawCommands, Data.GetPacks()[entry.first].Name + "/" + Data.GetPacks()[entry.first].LevelSets[entry.second].Name, 182, y);
		}
		if (first > 0)
			DrawCommands.push_back({ Arrows, 510, 158, Within(Mouse, 510, 158, 32, 32) ? 5 : 4 });
		if (first + 7 < static_cast<i32>(entries.size()))
			DrawCommands.push_back({ Arrows, 510, 330, Within(Mouse, 510, 330, 32, 32) ? 7 : 6 });
		Small.Append(DrawCommands, Data.Text("MorePacks"), 150, 360);
		AppendButtons();
		break;
	}
	case Page::Help:
		AppendHelp();
		AppendButtons();
		break;
	case Page::Highscores:
		AppendScores();
		AppendButtons();
		break;
	case Page::Pause:
	case Page::Quit:
		if (CurrentPage == Page::Quit && QuitReturn == Page::Title)
		{
			DrawCommands.push_back({ Logo, 234, 180, 0 });
			DrawCommands.push_back({ LogoDetail, 429, 316, static_cast<i32>(Scroll) % 4 });
			AppendButtons();
		}
		// Original leave-game prompt, 0x4123C0.
		DrawCommands.push_back({ PausePanel, 200, 100, 0 });
		Centered(Large, Data.Text("Quit1"), 400, 110);
		Centered(Large, Data.Text("Quit2"), 400, 125);
		Centered(Large, Data.Text("Quit3"), 400, 145);
		DrawCommands.push_back({ LeaveImage, 360, 193, 0 });
		Centered(DialogChoice == 0 ? Green : Large, Data.Text("Yes"), 330, 211);
		Centered(DialogChoice == 1 ? Green : Large, Data.Text("No"), 470, 211);
		break;
	case Page::Results:
		if (Result.GameOver)
		{
			DrawCommands.push_back({ PausePanel, 200, 100, 0 });
			DrawCommands.push_back({ GameOverImage, 250, 130, 0 });
			Centered(Large, Data.Text("GameOver"), 485, 150);
			if (promptVisible)
				Centered(Small, Data.Text("PressSpace"), 485, 200);
		}
		else if (Result.EpisodeComplete)
		{
			DrawCommands.push_back({ ScorePanel, 78, 0, 0 });
			DrawCommands.push_back({ PackFinish, 315, 294, 0 });
			Centered(Large, Data.Text("Congratulations"), 400, 80);
			Centered(Small, Data.Text("LevelPackMadeIt1") + GetLevelSet().Name + Data.Text("LevelPackMadeIt2"), 400, 140);
			DrawCommands.push_back({ Score, 321, 167, Result.ScoreFrame });
			Small.Append(DrawCommands, Padded(Result.Score), 375, 182);
			DrawCommands.push_back({ Sheep, 400 - Sheep->FrameWidth / 2, 255 - Sheep->FrameHeight / 2, 0 });
			Small.Append(DrawCommands, Padded(Result.TotalSheep), 360, 247);
			if (promptVisible && (Result.EpisodeComplete || (Result.TimeRemaining <= 0 && Result.SheepRemaining <= 0 && Result.LivesRemaining <= 0)))
				Centered(Small, Data.Text("PressSpace"), 400, 450);
		}
		else
		{
			// Sven2_Results_DrawLevelCompletion, 0x411B20.
			DrawCommands.push_back({ ScorePanel, 78, 0, 0 });
			Large.Append(DrawCommands, Data.Text("LevelBonus"), 320, 60);
			Small.Append(DrawCommands, Data.Text("LevelPack"), 320, 100);
			Small.Append(DrawCommands, GetLevelSet().Name, 400, 100);
			Small.Append(DrawCommands, Data.Text("BonusLevel"), 320, 120);
			Small.Append(DrawCommands, (Result.Level > 0 && static_cast<size_t>(Result.Level) <= GetLevelSet().Levels.size() ? GetLevelSet().Levels[Result.Level - 1].Name : std::to_string(Result.Level)), 400, 120);
			DrawCommands.push_back({ Timer, 307 - Timer->FrameWidth / 2, 140, 0 });
			const i32 remaining = std::max(0, Result.TimeRemaining);
			Small.Append(DrawCommands, Padded(remaining / 60, 2) + ":" + (remaining % 60 < 10 ? "0" : "") + std::to_string(remaining % 60), 282, 172);
			DrawCommands.push_back({ Sheep, 310 - Sheep->FrameWidth / 2, 240 - Sheep->FrameHeight / 2, 0 });
			Small.Append(DrawCommands, Padded(Result.SheepRemaining), 270, 232);
			DrawCommands.push_back({ Life, 300 - Life->FrameWidth / 2, 300 - Life->FrameHeight / 2, 0 });
			Small.Append(DrawCommands, Padded(Result.LivesRemaining), 290, 292);
			constexpr i32 rows[] = { 172, 232, 292 };
			constexpr cstr multipliers[] = { "x 1", "x 10", "x 25" };
			const i32 points[] = { Result.Time - Result.TimeRemaining, (Result.Sheep - Result.SheepRemaining) * 10, (Result.Lives - Result.LivesRemaining) * 25 };
			for (i32 i = 0; i < 3; ++i)
			{
				Small.Append(DrawCommands, multipliers[i], 360, rows[i]);
				Small.Append(DrawCommands, "=", 410, rows[i]);
				Small.Append(DrawCommands, Padded(points[i]), 450, rows[i]);
			}
			Small.Append(DrawCommands, Data.Text("BonusTotal"), 365, 340);
			Small.Append(DrawCommands, Padded(points[0] + points[1] + points[2]), 450, 340);
			DrawCommands.push_back({ Score, 395, 380, Result.ScoreFrame });
			Small.Append(DrawCommands, Padded(Result.Score), 450, 395);
			if (promptVisible && (Result.EpisodeComplete || (Result.TimeRemaining <= 0 && Result.SheepRemaining <= 0 && Result.LivesRemaining <= 0)))
				Centered(Small, Data.Text("PressSpace"), 400, 450);
		}
		break;
	case Page::Name:
		AppendScores();
		break;
	}
	if (!Notice.empty())
		Small.AppendWrapped(DrawCommands, Notice, 120, 460, 560, 20);
	if (Mouse.x >= 0 && Mouse.y >= 0)
		DrawCommands.push_back({ Cursor, Mouse.x - 9, Mouse.y - 9, ButtonAt(Mouse) >= 0 ? 1 : 0 });
	return DrawCommands;
}
