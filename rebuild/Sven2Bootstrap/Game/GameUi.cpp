#include "Game/GameUi.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <stdexcept>

static str Padded(i32 value, i32 width = 5)
{
	std::ostringstream text;
	text << std::setw(width) << value;
	return text.str();
}

static i32 Wrap(i32 value, i32 length)
{
	return (value + length) % length;
}

static bool Within(POINT point, i32 x, i32 y, i32 width, i32 height)
{
	return point.x >= x && point.y >= y && point.x < x + width && point.y < y + height;
}

static str Numbered(cstr prefix, i32 index)
{
	std::ostringstream text;
	text << prefix << std::setfill('0') << std::setw(2) << index;
	return text.str();
}

GameUi::GameUi(MudGE& engine, fpath saves)
    : Engine(engine), Data(engine.GetData()), Saves(std::move(saves)), Theme(Data.GetThemeEdition()), Large(engine, false, false, Theme), Small(engine, true, false, Theme), Green(engine, false, true, Theme), SmallGreen(engine, true, true, Theme), Clouds(LoadTheme("Sven2/gfx/mainmenu_clouds.jpg", 1, 1, Theme == GameEdition::Zwo)), Backdrop(LoadTheme("Sven2/gfx/mainmenu_BG.jpg", 1, 1)), Logo(Theme == GameEdition::XXX ? nullptr : LoadTheme("Sven2/gfx/SvenLogo.bmp", 1, 1)), LogoDetail(Theme == GameEdition::XXX ? nullptr : LoadTheme("Sven2/gfx/SvenLogo_O.bmp", 4, 1)), Buttons(LoadTheme("Sven2/gfx/mainmenu_buttons.bmp", Theme == GameEdition::XXX ? 12 : 10, 1)), PackPanel(LoadTheme("Sven2/gfx/smallmenu_choose_level.bmp", 1, 1)), HelpBackdrop(LoadTheme("Sven2/gfx/Help_BG.jpg", 1, 1)), HelpTabs(LoadTheme("Sven2/gfx/Help_file_cards.bmp", Theme == GameEdition::XXX ? 10 : 8, 1)), ScorePanel(LoadTheme("Sven2/gfx/ScoreScreen_BG.bmp", 1, 1)), PackFinish(LoadTheme("Sven2/gfx/ScoreScreen_BG_levelpack.bmp", 1, 1)), ScoreTabs(LoadTheme("Sven2/gfx/Highsc_file_cards.bmp", 10, 1)), Highlight(LoadTheme("Sven2/gfx/selection_bar.bmp", 1, 1)), PausePanel(LoadTheme("Sven2/gfx/smallmenu_BG.bmp", 1, 1)), Timer(LoadTheme("Sven2/gfx/timer.bmp", 1, 1)), Life(LoadTheme("Sven2/gfx/Life_icon.bmp", 1, 1)), Sheep(LoadTheme("Sven2/gfx/Sheep/Sheep_Stand.bmp", 4, 1)), Score(LoadTheme("Sven2/gfx/score.bmp", 4, 1)), Icons(LoadTheme("Sven2/gfx/icons.bmp", 5, 1)), Artifact(LoadTheme("Sven2/gfx/Artifact.bmp", 1, 1)), Arrows(LoadTheme("Sven2/gfx/pijlen.bmp", Theme == GameEdition::XXX ? 12 : 8, 1)), Cursor(LoadTheme("Sven2/gfx/mousecursor.bmp", 5, 1)), CardBlank(LoadTheme("Sven2/gfx/Highsc_file_card_blank.bmp", 1, 1)), NoAnimation(LoadTheme(Theme == GameEdition::XXX ? "SvenXXX/gfx/Highsc_file_card_not_included.bmp" : "Sven2/gfx/noanim.bmp", 1, 1)), LeaveImage(LoadTheme("Sven2/gfx/smallmenu_BG_verlassen.bmp", 1, 1)), GameOverImage(LoadTheme("Sven2/gfx/smallmenu_BG_gameover.bmp", 1, 1))
{
	People = { LoadTheme("Sven2/gfx/Dog/Dog_Stand.bmp", 8, 1), LoadTheme("Sven2/gfx/Guy/Guy_Stand.bmp", 8, 1), LoadTheme("Sven2/gfx/Broemse/Broemse_stand.bmp", 8, 1), LoadTheme("Sven2/gfx/Alien/Alien_Stand.bmp", 8, 1) };
	for (size_t i = 0; i < Data.GetPacks().size(); ++i)
		if (Data.GetPacks()[i].Name == "XS")
			PackIndex = i;
	if (Theme == GameEdition::XXX)
	{
		ControlsPanel = LoadTheme("SvenXXX/gfx/steuerung_BG.bmp");
		for (size_t eye = 0; eye < MenuEyes.size(); ++eye)
			MenuEyes[eye] = LoadTheme("SvenXXX/gfx/" + Numbered("Help_BG_eye", static_cast<i32>(eye) + 1) + ".bmp", 6);
		MenuSven = LoadNamedTheme("SvenStand");
		PlayerIcons = { LoadNamedTheme("IconPlayer1"), LoadNamedTheme("IconPlayer2") };
		// Native help bindings at 0x43010C; Broemse uses mirrored slot +95628.
		CatchPreviews = { SpriteDraw{ LoadTheme("Sven2/gfx/anim_cloud.bmp", 10), 0, 0, 4 }, SpriteDraw{ LoadTheme("Sven2/gfx/anim_cloud.bmp", 10), 0, 0, 3 }, SpriteDraw{ LoadNamedTheme("BroemseSvenFront"), 0, 0, 4, true }, SpriteDraw{ LoadNamedTheme("AlienProbe2"), 0, 0, 24 } };
		Distractions = { LoadTheme("Sven2/gfx/Bone.bmp"), LoadTheme("Sven2/gfx/Beer.bmp"), LoadTheme("Sven2/gfx/Teddybear.bmp"), LoadTheme("Sven2/gfx/Artifact.bmp") };
		for (i32 i = 0; i < 19; ++i)
		{
			TitleBlinks[i] = LoadTheme("SvenXXX/gfx/Mainmenu/" + Numbered("mainmenu_blink", i + 1) + ".bmp");
			TitleBlinkTimers[i] = static_cast<f32>(NextTitleRandom() % 1000 + 250);
		}
		TitleActors = { LoadTheme("SvenXXX/gfx/Mainmenu/mainmenu_broemse.bmp", 4), LoadTheme("SvenXXX/gfx/Mainmenu/mainmenu_lars_zzz.bmp", 6), LoadTheme("SvenXXX/gfx/Mainmenu/mainmenu_wotan_zzz.bmp", 6), LoadTheme("SvenXXX/gfx/Mainmenu/mainmenu_sven2_jump.bmp", 5) };
		constexpr cstr actions[] = { "Up", "Down", "Left", "Right", "Action" };
		for (i32 player = 0; player < 2; ++player)
		{
			for (i32 action = 0; action < 5; ++action)
				ControlBindings[player][action] = static_cast<i32>(Data.NumericParam("Player" + std::to_string(player + 1) + "Key" + actions[action], 0, Theme));
		}
		std::istringstream names(Data.StringParam("KeyConvert", "", Theme));
		str name;
		while (std::getline(names, name, '/'))
			KeyNames.push_back(name);
		constexpr cstr previews[] = { "SvenWalkBack", "SvenWalkFront", "SvenWalkSide", "SvenWalkSide", "SvenWhistleFront" };
		for (size_t action = 0; action < ControlAnimations.size(); ++action)
		{
			const GameData::SpriteDefinition& definition = Data.GetSpriteDefinition(previews[action], Theme);
			ShagAnimation& animation = ControlAnimations[action];
			animation.Sprite = Engine.LoadSprite(definition.File, definition.Rows, definition.Columns);
			animation.Mirror = action == 3;
			str sequence = Data.StringParam(action == 4 ? "aSvenWhistle" : "aSvenWalk", "0", Theme);
			std::replace(sequence.begin(), sequence.end(), ',', ' ');
			std::istringstream frames(sequence);
			i32 frame;
			while (frames >> frame)
				animation.Frames.push_back(frame);
		}
		LoadControlSettings();
	}
	DrawCommands.reserve(1200);
}

const Sprite* GameUi::LoadTheme(const str& path, i16 rows, i16 columns, bool opaque)
{
	str asset = path;
	if (Theme == GameEdition::XXX)
	{
		if (path == "Sven2/gfx/mainmenu_clouds.jpg")
			asset = "SvenXXX/gfx/Mainmenu/mainmenu_BG.jpg";
		else if (path == "Sven2/gfx/mainmenu_BG.jpg")
			asset = "SvenXXX/gfx/Mainmenu/mainmenu_BG.bmp";
	}
	return Engine.LoadSprite(Data.EditionAsset(Theme, asset), rows, columns, opaque);
}

const Sprite* GameUi::LoadNamedTheme(cstr name)
{
	const GameData::SpriteDefinition& definition = Data.GetSpriteDefinition(name, Theme);
	return Engine.LoadSprite(definition.File, definition.Rows, definition.Columns);
}

str GameUi::Text(const str& key) const
{
	str text = Data.Text(key, Theme);
	if (Theme == GameEdition::XXX)
		std::replace(text.begin(), text.end(), '#', '\n');
	return text;
}

str GameUi::EpisodeLabel(size_t pack, size_t levelSet) const
{
	const LevelPackInfo& info = Data.GetPacks().at(pack);
	return info.LevelSets.at(levelSet).Name;
}

i32 GameUi::GetPlayerCount() const
{
	return GetPack().Edition == GameEdition::Zwo ? 1 : RequestedPlayerCount;
}

void GameUi::SetPlayerCount(i32 playerCount)
{
	RequestedPlayerCount = std::clamp(playerCount, 1, 2);
}

void GameUi::SetControlBinding(i32 player, i32 action, i32 key)
{
	if (WindowInput::VirtualKeyFromNative(key) < 0)
		throw std::invalid_argument("This key is not supported by the original keyboard mapping.");
	ControlBindings.at(player).at(action) = key;
	CustomBindings.at(player).at(action) = true;
	SaveControlSettings();
	ControlSelection = -1;
}

const std::array<std::array<i32, 5>, 2>& GameUi::GetControlBindings() const
{
	return ControlBindings;
}

bool GameUi::HasCustomControls() const
{
	for (bool customized : CustomBindings.front())
		if (customized)
			return true;
	return false;
}

u32 GameUi::NextTitleRandom()
{
	// UI randomness must not consume the gameplay random stream.
	TitleRandom = TitleRandom * 1664525u + 1013904223u;
	return TitleRandom;
}

void GameUi::AppendTitle()
{
	if (Theme == GameEdition::Zwo)
	{
		DrawCommands.push_back({ Logo, 234, 180, 0 });
		DrawCommands.push_back({ LogoDetail, 429, 316, static_cast<i32>(Scroll) % 4 });
		return;
	}
	// Native XXX title-overlay locations, 0x4532B0 / 0x453300; rendered at 0x43961F.
	constexpr i32 x[] = { 176, 598, 548, 526, 545, 577, 600, 473, 473, 488, 555, 521, 577, 450, 470, 487, 380, 417, 452 };
	constexpr i32 y[] = { 116, 291, 325, 339, 342, 362, 366, 368, 395, 403, 397, 424, 430, 434, 472, 478, 482, 479, 499 };
	for (size_t i = 0; i < TitleBlinks.size(); ++i)
		if (TitleBlinkTimers[i] < 30)
			DrawCommands.push_back({ TitleBlinks[i], x[i], y[i], 0 });
	// Native menu object spawns 0x438E02-0x438E4A; speeds 0x410AFF / 0x410B80.
	constexpr i32 actorX[] = { 231, 538, 397, 145 }, actorY[] = { 360, 62, 423, 411 };
	constexpr f32 speed[] = { 12, 5, 5, 5 };
	for (size_t i = 0; i < TitleActors.size(); ++i)
	{
		const Sprite* actor = TitleActors[i];
		DrawCommands.push_back({ actor, actorX[i] - actor->FrameWidth / 2, actorY[i] - actor->FrameHeight / 2, static_cast<i32>(UiAge * speed[i]) % actor->Rows });
	}
}

void GameUi::AppendMenuEyes()
{
	if (Theme != GameEdition::XXX)
		return;
	// Native XXX 0x423020: instant cursor transforms, not time-based animation.
	const f32 x = static_cast<f32>(EyeMouse.x), y = static_cast<f32>(EyeMouse.y);
	// 0x423052/0x4230FA keep negative values and clear positive values to zero.
	const std::array<f32, 5> eyeX = { std::min(x * 0.33333334f - 130, 0.0f), x * 0.15015015f - 137, x * 0.016666668f + 680, x * 0.22222222f + 640, std::max(x - 600, 0.0f) * 0.66666669f + 680 };
	const std::array<f32, 5> eyeY = { y * 0.016666668f + 100, y * 0.1f + 400, std::min(y * 0.40000001f - 120, 0.0f), y * 0.02f + 150, y * 0.033333335f + 425 };
	for (size_t eye = 0; eye < MenuEyes.size(); ++eye)
	{
		const i32 frame = eye == 2 ? static_cast<i32>(x * 0.0062500001f + 1) : static_cast<i32>(y * 0.0083333338f + 1);
		DrawCommands.push_back({ MenuEyes[eye], static_cast<i32>(eyeX[eye]), static_cast<i32>(eyeY[eye]), 0 });
		DrawCommands.push_back({ MenuEyes[eye], static_cast<i32>(eyeX[eye]), static_cast<i32>(eyeY[eye]), frame });
	}
}

void GameUi::AppendPlayerHeader()
{
	// Requested menu mode remains visible even when a highlighted Zwo pack is single-player.
	Large.Append(DrawCommands, Text(RequestedPlayerCount == 2 ? "Player2_HeadLine" : "Player1_HeadLine"), 150, 80);
	if (RequestedPlayerCount == 2)
	{
		DrawCommands.push_back({ MenuSven, 400, 65, 3 });
		DrawCommands.push_back({ PlayerIcons[0], 390, 10, 0 });
	}
	DrawCommands.push_back({ MenuSven, 460, 65, 5 });
	if (RequestedPlayerCount == 2)
		DrawCommands.push_back({ PlayerIcons[1], 430, 10, 0 });
}

POINT GameUi::ControlPosition(i32 player, i32 action) const
{
	// Native XXX 0x44F8F8/0x44F920, reordered to Up/Down/Left/Right/Action.
	constexpr POINT positions[] = { { 190, 105 }, { 190, 205 }, { 90, 205 }, { 290, 205 }, { 190, 305 } };
	return { positions[action].x + player * 300, positions[action].y };
}

i32 GameUi::ControlAt(POINT mouse) const
{
	for (i32 player = 0; player < 2; ++player)
	{
		for (i32 action = 0; action < 5; ++action)
		{
			const POINT position = ControlPosition(player, action);
			if (Within(mouse, position.x + 10, position.y + 100, 100, 20))
				return player * 5 + action;
		}
	}
	for (i32 player = 0; player < 2; ++player)
	{
		for (i32 action = 0; action < 5; ++action)
		{
			const POINT position = ControlPosition(player, action);
			if (Within(mouse, position.x + 10, position.y + 20, 100, 80))
				return player * 5 + action;
		}
	}
	return -1;
}

void GameUi::LoadControlSettings()
{
	std::ifstream file(Saves / L"controls.tsv");
	i32 player, action, key;
	while (file >> player >> action >> key)
	{
		if (player >= 0 && player < 2 && action >= 0 && action < 5 && WindowInput::VirtualKeyFromNative(key) >= 0)
		{
			ControlBindings[player][action] = key;
			CustomBindings[player][action] = true;
		}
	}
}

void GameUi::SaveControlSettings() const
{
	std::filesystem::create_directories(Saves);
	std::ofstream file(Saves / L"controls.tsv", std::ios::trunc);
	for (size_t player = 0; player < ControlBindings.size(); ++player)
	{
		for (size_t action = 0; action < ControlBindings[player].size(); ++action)
			if (CustomBindings[player][action])
				file << player << '\t' << action << '\t' << ControlBindings[player][action] << '\n';
	}
	if (!file)
		throw std::runtime_error("Cannot save keyboard settings.");
}

void GameUi::AppendControls()
{
	DrawCommands.push_back({ ControlsPanel, 90, 110, 0 });
	DrawCommands.push_back({ ControlsPanel, 390, 110, 0 });
	Centered(Large, "Spieler 1", 250, 75);
	Centered(Large, "Spieler 2", 550, 75);
	for (i32 player = 0; player < 2; ++player)
	{
		for (i32 action = 0; action < 5; ++action)
		{
			const POINT position = ControlPosition(player, action);
			const bool selected = ControlSelection == player * 5 + action;
			const bool active = selected || ControlAt(Mouse) == player * 5 + action;
			const ShagAnimation& animation = ControlAnimations[action];
			const i32 frame = active ? animation.Frames[static_cast<size_t>(UiAge * 15) % animation.Frames.size()] : 0;
			DrawCommands.push_back({ animation.Sprite, position.x, position.y, frame, animation.Mirror });
			const i32 key = ControlBindings[player][action];
			const str name = key >= 0 && static_cast<size_t>(key) < KeyNames.size() ? KeyNames[key] : std::to_string(key);
			Centered(active ? SmallGreen : Small, selected && static_cast<i32>(UiAge * 6) % 2 ? "?" : name, position.x + 60, position.y + 100);
		}
	}
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
	{
		for (size_t e = 0; e < Data.GetPacks()[p].LevelSets.size(); ++e)
			entries.emplace_back(p, e);
	}
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
	const str key = (GetPack().Id.empty() ? GetPack().Name : GetPack().Id) + "-" + std::to_string(SetIndex) + (ScorePlayers == 2 ? "-2player" : "");
	auto& table = ScoreTables[key];
	if (!table)
	{
		std::vector<HighscoreEntry> defaults;
		std::istringstream names(Data.StringParam("DefaultHighscoreNames", "A/B/C/D/E/F/G/H/I/J", GetPack().Edition));
		str name;
		while (defaults.size() < 10 && std::getline(names, name, '/'))
			defaults.push_back({ name, 1000 - static_cast<i32>(defaults.size()) * 100, 0 });
		table = std::make_unique<Highscores>(Saves / std::filesystem::u8path(key + ".tsv"), std::move(defaults));
	}
	return *table;
}

i32 GameUi::ButtonAt(POINT point) const
{
	if (Theme == GameEdition::XXX)
	{
		constexpr i32 positions[] = { 10, 170, 340, 480, 680 }, widths[] = { 140, 140, 120, 200, 120 };
		for (i32 i = 0; i < 5; ++i)
			if (Within(point, positions[i], 545, widths[i], 30))
				return i;
		return -1;
	}
	constexpr i32 positions[] = { 40, 210, 380, 630 }, widths[] = { 115, 115, 200, 115 };
	for (i32 i = 0; i < 4; ++i)
		if (Within(point, positions[i], 545, widths[i], 30))
			return i;
	return -1;
}

void GameUi::ShowResults(const RoundResult& summary)
{
	this->Result = summary;
	ScorePlayers = GetPlayerCount();
	CardsTab = false;
	if (CurrentPage != Page::Results)
		Show(Page::Results);
}

UiAction GameUi::Handle(const WindowInput::Result& input)
{
	Mouse = input.Mouse;
	// Scaled input uses (-1, -1) outside the viewport. Keep hit testing inactive,
	// but retain the last eye pose rather than jumping to the upper-left corner.
	if (Mouse.x >= 0 && Mouse.x < 800 && Mouse.y >= 0 && Mouse.y < 600)
		EyeMouse = Mouse;
	if (Theme == GameEdition::XXX && CurrentPage == Page::Help && HelpTab == 3)
	{
		if (ControlSelection >= 0)
		{
			if (input.Back)
				ControlSelection = -1;
			else if (input.BindingKey >= 0)
				SetControlBinding(ControlSelection / 5, ControlSelection % 5, input.BindingKey);
			else if (input.Click)
				ControlSelection = ControlAt(Mouse);
			return UiAction::None;
		}
		if (input.Click && ControlAt(Mouse) >= 0)
		{
			ControlSelection = ControlAt(Mouse);
			return UiAction::None;
		}
	}
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
			HelpTab = Wrap(HelpTab - 1, Theme == GameEdition::XXX ? 5 : 4);
		if (input.Right || input.Tab)
			HelpTab = Wrap(HelpTab + 1, Theme == GameEdition::XXX ? 5 : 4);
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
		if (input.Click && input.Mouse.y >= 21 && input.Mouse.y < 73 && input.Mouse.x >= 100 && input.Mouse.x < (Theme == GameEdition::XXX ? 600 : 532))
		{
			HelpTab = std::clamp(static_cast<i32>((input.Mouse.x - 100) / 100), 0, Theme == GameEdition::XXX ? 4 : 3);
			return UiAction::None;
		}
	}
	else if (CurrentPage == Page::Highscores)
	{
		if (input.Click && Within(input.Mouse, 100, 21, Theme == GameEdition::XXX ? 100 : 132, 52))
		{
			CardsTab = false;
			ScorePlayers = 1;
		}
		if (Theme == GameEdition::XXX && input.Click && Within(input.Mouse, 200, 21, 120, 52))
		{
			CardsTab = false;
			ScorePlayers = 2;
		}
		if (input.Click && Within(input.Mouse, 500, 21, 132, 52))
			CardsTab = true;
		if (CardsTab)
		{
			i32 change = (input.Right || (input.Click && Within(input.Mouse, 238, 350, 32, 32))) - (input.Left || (input.Click && Within(input.Mouse, 160, 350, 32, 32)));
			if (change)
			{
				CardIndex = Wrap(CardIndex + change, CardCount());
				CardPlaying = false;
				CardAge = 0;
			}
			if (!CardPlaying && CardIndex < CardCount() && (input.Confirm || (input.Click && Within(input.Mouse, 150, 130, 132, 206))))
			{
				if (CardAnimations.empty())
					LoadCards();
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
		SelectedButton = Wrap(SelectedButton + (input.Left ? -1 : 1), Theme == GameEdition::XXX ? 5 : 4);
	const i32 hovered = ButtonAt(input.Mouse);
	if (input.MouseMoved && hovered >= 0)
		SelectedButton = hovered;
	const i32 activated = input.Click ? hovered : (CurrentPage == Page::Title && input.Confirm ? SelectedButton : -1);
	if (Theme == GameEdition::XXX && activated >= 0)
	{
		switch (activated)
		{
		case 0:
		case 1:
		{
			const bool close = (CurrentPage == Page::GetPacks || CurrentPage == Page::LevelSets) && RequestedPlayerCount == activated + 1;
			SetPlayerCount(activated + 1);
			Show(close ? Page::Title : Page::GetPacks);
			break;
		}
		case 2:
			Show(Page::Help);
			break;
		case 3:
			Show(Page::Highscores);
			break;
		case 4:
			QuitReturn = Page::Title;
			Show(Page::Quit);
			break;
		}
		return UiAction::None;
	}
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
	if (Theme == GameEdition::XXX)
	{
		// XXX 0x439689-0x439731: one-player, two-player, help, scores, exit.
		constexpr i32 positions[] = { 10, 170, 340, 480, 680 };
		for (i32 i = 0; i < 5; ++i)
			DrawCommands.push_back({ Buttons, positions[i], 545, i * 2 + (SelectedButton != i) });
		return;
	}
	constexpr i32 x[] = { 40, 210, 380, 630 };
	for (i32 i = 0; i < 4; ++i)
		DrawCommands.push_back({ Buttons, x[i], 545, 2 * i + (SelectedButton != i) });
}

void GameUi::AppendHelp()
{
	DrawCommands.push_back({ HelpBackdrop, 0, 0, 0 });
	const std::vector<i32> frames = Theme == GameEdition::XXX ? std::vector<i32>{ 2, 4, 0, 8, 6 } : std::vector<i32>{ 2, 4, 0, 6 };
	// Original 0x4146E0: inactive tabs first, active last. The paper starts at y=60.
	for (i32 i = 0; i < static_cast<i32>(frames.size()); ++i)
		if (i != HelpTab)
			DrawCommands.push_back({ HelpTabs, 100 + i * 100, 34, frames[i] + 1, false, 60 });
	DrawCommands.push_back({ HelpTabs, 100 + HelpTab * 100, 21, frames[HelpTab], false, 60 });
	if (HelpTab == 0)
	{
		constexpr i32 positions[] = { 80, 140, 220, 320, 420 };
		for (i32 i = 0; i < 5; ++i)
			Small.AppendWrapped(DrawCommands, Text(Numbered("spiel", i + 1)), 120, positions[i], 535, 20);
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
		Centered(Large, Text(person + "00"), 220, 80);
		Small.AppendWrapped(DrawCommands, Text(person + "01"), 230, 140, 460, 20);
		Small.AppendWrapped(DrawCommands, Text(person + "02"), 120, 280, 410, 20);
		if (Theme == GameEdition::XXX)
		{
			// XXX 0x421A20 uses the demonstration frames from 0x44FCF8.
			SpriteDraw caught = CatchPreviews[HelpCharacter];
			caught.X = 630 - caught.Sprite->FrameWidth / 2;
			caught.Y = 350 - caught.Sprite->FrameHeight / 2;
			DrawCommands.push_back(caught);
			const Sprite* item = Distractions[HelpCharacter];
			DrawCommands.push_back({ item, 200 - item->FrameWidth / 2, 220 - item->FrameHeight / 2, 0 });
		}
	}
	else if (HelpTab == 2)
	{
		Small.AppendWrapped(DrawCommands, Text("extra01"), 120, 80, 535, 20);
		constexpr i32 iconFrames[] = { 0, 2, 3, 4, 1 };
		for (i32 i = 2; i <= 7; ++i)
		{
			const i32 y = 140 + (i - 2) * 60;
			const Sprite* image = i == 7 ? Artifact : Icons;
			const i32 center = i == 2 ? 175 : y + 20;
			DrawCommands.push_back({ image, 150 - image->FrameWidth / 2, center - image->FrameHeight / 2, i == 7 ? 0 : iconFrames[i - 2] });
			Small.Append(DrawCommands, Text(Numbered("extra", i)), 200, y);
			Small.AppendWrapped(DrawCommands, Text(Numbered("extra_", i)), 200, y + 20, 455, 20);
		}
	}
	else if (Theme == GameEdition::XXX && HelpTab == 3)
		AppendControls();
	else
	{
		constexpr i32 y[] = { 100, 120, 140, 160, 180, 200, 220, 260, 280, 300, 340, 360, 380 };
		for (i32 i = 0; i < 13; ++i)
		{
			Small.AppendWrapped(DrawCommands, Text(Numbered("credits", i * 2 + 1)), 120, y[i], 220, 20);
			Small.AppendWrapped(DrawCommands, Text(Numbered("credits", i * 2 + 2)), 340, y[i], 350, 20);
		}
		Small.Append(DrawCommands, Theme == GameEdition::XXX ? "v1.0" : "v1.02", 670, 475);
	}
}

i32 GameUi::CardCount() const
{
	return static_cast<i32>(std::clamp(Data.NumericParam("ShagAnims", Theme == GameEdition::XXX ? 39 : 33, Theme), 1.0, Theme == GameEdition::XXX ? 39.0 : 33.0));
}

void GameUi::LoadCards()
{
	if (Theme == GameEdition::Zwo)
	{
		CardAnimations = ShagAnimation::Load(Engine);
		return;
	}
	// Native catalog: thirteen OF entries followed by twenty-six NF entries.
	for (i32 index = 0; index < CardCount(); ++index)
	{
		const bool original = index < OriginalSeriesCards;
		const i32 number = original ? index + 2 : index - OriginalSeriesCards + 1;
		const str name = Numbered(original ? "Shag" : "NF", number);
		const GameData::SpriteDefinition& definition = Data.GetSpriteDefinition(name, Theme);
		ShagAnimation animation;
		animation.Sprite = Engine.LoadSprite(definition.File, definition.Rows, definition.Columns);
		animation.Sound = Data.SoundPath(Numbered(original ? "SfxFuck" : "SfxNewFuck", number), Theme);
		str sequence = Data.StringParam(Numbered(original ? "aShagAnimO" : "aShagAnimN", number), "0", Theme);
		std::replace(sequence.begin(), sequence.end(), ',', ' ');
		std::istringstream frames(sequence);
		i32 frame;
		while (frames >> frame)
			animation.Frames.push_back(frame);
		CardAnimations.push_back(animation);
		animation.Mirror = true;
		CardAnimations.push_back(std::move(animation));
	}
}

i32 GameUi::GetControlSelection() const
{
	return CurrentPage == Page::Help && HelpTab == 3 ? ControlSelection : -1;
}

void GameUi::AppendScores()
{
	DrawCommands.push_back({ HelpBackdrop, 0, 0, 0 });
	// Original 0x4171A0 draws the inactive tab before the active tab.
	if (Theme == GameEdition::XXX)
	{
		const i32 active = CardsTab ? 2 : ScorePlayers - 1;
		constexpr i32 positions[] = { 100, 200, 500 }, frames[] = { 0, 2, 8 };
		for (i32 tab = 0; tab < 3; ++tab)
			if (tab != active)
				DrawCommands.push_back({ ScoreTabs, positions[tab], 34, frames[tab] + 1, false, 60 });
		DrawCommands.push_back({ ScoreTabs, positions[active], 21, frames[active], false, 60 });
	}
	else
	{
		DrawCommands.push_back({ ScoreTabs, CardsTab ? 100 : 500, 34, CardsTab ? 1 : 9, false, 60 });
		DrawCommands.push_back({ ScoreTabs, CardsTab ? 500 : 100, 21, CardsTab ? 8 : 0, false, 60 });
	}
	if (CardsTab)
	{
		DrawCommands.push_back({ CardBlank, 150, 130, 0 });
		if (CardIndex < CardCount())
		{
			if (CardAnimations.empty())
				LoadCards();
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
		Large.AppendWrapped(DrawCommands, Text(Numbered("Card_", CardIndex + 1)), 310, 150, 380, 25);
		Small.AppendWrapped(DrawCommands, Text(Numbered("Card", CardIndex + 1)), 310, 200, 380, 20);
		Centered(Large, std::to_string(CardIndex + 1), 215, 350);
		DrawCommands.push_back({ Arrows, 160, 350, Within(Mouse, 160, 350, 32, 32) ? 1 : 0 });
		DrawCommands.push_back({ Arrows, 238, 350, Within(Mouse, 238, 350, 32, 32) ? 3 : 2 });
		return;
	}
	Centered(Small, EpisodeLabel(PackIndex, SetIndex), 200, 100);
	const auto entries = Episodes();
	if (!entries.empty() && entries.front() != std::make_pair(PackIndex, SetIndex))
		DrawCommands.push_back({ Arrows, 100, 90, Within(Mouse, 100, 90, 32, 32) ? 1 : 0 });
	if (!entries.empty() && entries.back() != std::make_pair(PackIndex, SetIndex))
		DrawCommands.push_back({ Arrows, 268, 90, Within(Mouse, 268, 90, 32, 32) ? 3 : 2 });
	DrawCommands.push_back({ Score, 375, 70, 0 });
	Large.Append(DrawCommands, Text("HighscoreLevel"), 540, 80);
	DrawCommands.push_back({ Sheep, 595, 35, 1 });
	std::vector<HighscoreEntry> entriesToDraw = Scores().GetEntries();
	i32 editingRow = -1;
	if (CurrentPage == Page::Name)
	{
		auto position = std::find_if(entriesToDraw.begin(), entriesToDraw.end(), [&](const HighscoreEntry& entry) { return entry.Score < Result.Score; });
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
	if (Theme == GameEdition::XXX)
	{
		for (f32& timer : TitleBlinkTimers)
		{
			timer -= elapsed * 60;
			if (timer < 0)
				timer = static_cast<f32>(NextTitleRandom() % 1000 + 250);
		}
	}
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
		if (Theme == GameEdition::XXX)
			DrawCommands = { { Clouds, 0, 0, 0 }, { Backdrop, 0, 0, 0 } };
		else
			DrawCommands = { { Clouds, x, 65, 0 }, { Clouds, x + 800, 65, 0 }, { Backdrop, 0, 0, 0 } };
	}
	switch (CurrentPage)
	{
	case Page::Title:
		AppendTitle();
		AppendButtons();
		break;
	case Page::GetPacks:
	case Page::LevelSets:
	{
		DrawCommands.push_back({ HelpBackdrop, 0, 0, 0 });
		DrawCommands.push_back({ PackPanel, 150, 155, 0 });
		if (Theme == GameEdition::XXX)
			AppendPlayerHeader();
		Large.Append(DrawCommands, Text("SelectPacks"), 150, 120);
		const auto entries = Episodes();
		const i32 first = std::clamp(EpisodeFirst, 0, std::max(0, static_cast<i32>(entries.size()) - 7));
		for (i32 i = first; i < std::min(static_cast<i32>(entries.size()), first + 7); ++i)
		{
			const i32 y = 158 + (i - first) * 27;
			const auto& entry = entries[i];
			const bool selected = entry == std::make_pair(PackIndex, SetIndex);
			if (selected)
				DrawCommands.push_back({ Highlight, 154, y, 0 });
			(selected ? Green : Large).Append(DrawCommands, EpisodeLabel(entry.first, entry.second), 182, y);
		}
		if (first > 0)
			DrawCommands.push_back({ Arrows, 510, 158, Within(Mouse, 510, 158, 32, 32) ? 5 : 4 });
		if (first + 7 < static_cast<i32>(entries.size()))
			DrawCommands.push_back({ Arrows, 510, 330, Within(Mouse, 510, 330, 32, 32) ? 7 : 6 });
		Small.Append(DrawCommands, Text("MorePacks"), 150, 360);
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
			AppendTitle();
			AppendButtons();
		}
		// Original leave-game prompt, 0x4123C0.
		DrawCommands.push_back({ PausePanel, 200, 100, 0 });
		Centered(Large, Text("Quit1"), 400, 110);
		Centered(Large, Text("Quit2"), 400, 125);
		Centered(Large, Text("Quit3"), 400, 145);
		DrawCommands.push_back({ LeaveImage, 360, 193, 0 });
		Centered(DialogChoice == 0 ? Green : Large, Text("Yes"), 330, 211);
		Centered(DialogChoice == 1 ? Green : Large, Text("No"), 470, 211);
		break;
	case Page::Results:
		if (Result.GameOver)
		{
			DrawCommands.push_back({ PausePanel, 200, 100, 0 });
			DrawCommands.push_back({ GameOverImage, 250, 130, 0 });
			Centered(Large, Text("GameOver"), 485, 150);
			if (promptVisible)
				Centered(Small, Text("PressSpace"), 485, 200);
		}
		else if (Result.EpisodeComplete)
		{
			DrawCommands.push_back({ ScorePanel, 78, 0, 0 });
			DrawCommands.push_back({ PackFinish, 315, 294, 0 });
			Centered(Large, Text("Congratulations"), 400, 80);
			Centered(Small, Text("LevelPackMadeIt1") + GetLevelSet().Name + Text("LevelPackMadeIt2"), 400, 140);
			DrawCommands.push_back({ Score, 321, 167, Result.ScoreFrame });
			Small.Append(DrawCommands, Padded(Result.Score), 375, 182);
			DrawCommands.push_back({ Sheep, 400 - Sheep->FrameWidth / 2, 255 - Sheep->FrameHeight / 2, 0 });
			Small.Append(DrawCommands, Padded(Result.TotalSheep), 360, 247);
			if (promptVisible && (Result.EpisodeComplete || (Result.TimeRemaining <= 0 && Result.SheepRemaining <= 0 && Result.LivesRemaining <= 0)))
				Centered(Small, Text("PressSpace"), 400, 450);
		}
		else
		{
			// Sven2_Results_DrawLevelCompletion, 0x411B20.
			DrawCommands.push_back({ ScorePanel, 78, 0, 0 });
			Large.Append(DrawCommands, Text("LevelBonus"), 320, 60);
			Small.Append(DrawCommands, Text("LevelPack"), 320, 100);
			Small.Append(DrawCommands, GetLevelSet().Name, 400, 100);
			Small.Append(DrawCommands, Text("BonusLevel"), 320, 120);
			Small.Append(DrawCommands, (Result.Level > 0 && static_cast<size_t>(Result.Level) <= GetLevelSet().Levels.size() ? GetLevelSet().Levels[Result.Level - 1].Name : std::to_string(Result.Level)), 400, 120);
			DrawCommands.push_back({ Timer, 307 - Timer->FrameWidth / 2, 140, 0 });
			const i32 remaining = std::max(0, Result.TimeRemaining);
			Small.Append(DrawCommands, Padded(remaining / 60, 2) + ":" + (remaining % 60 < 10 ? "0" : "") + std::to_string(remaining % 60), Theme == GameEdition::XXX ? 292 : 282, 172);
			DrawCommands.push_back({ Sheep, 310 - Sheep->FrameWidth / 2, (Theme == GameEdition::XXX ? 255 : 240) - Sheep->FrameHeight / 2, 0 });
			if (Theme == GameEdition::XXX)
				Centered(Small, Padded(Result.SheepRemaining), 299, 247);
			else
				Small.Append(DrawCommands, Padded(Result.SheepRemaining), 270, 232);
			DrawCommands.push_back({ Life, 300 - Life->FrameWidth / 2, (Theme == GameEdition::XXX ? 330 : 300) - Life->FrameHeight / 2, 0 });
			if (Theme == GameEdition::XXX)
				Centered(Small, Padded(Result.LivesRemaining), 320, 322);
			else
				Small.Append(DrawCommands, Padded(Result.LivesRemaining), 290, 292);
			const i32 rows[] = { 172, Theme == GameEdition::XXX ? 247 : 232, Theme == GameEdition::XXX ? 322 : 292 };
			const cstr multipliers[] = { GetPack().Edition == GameEdition::XXX ? "x 2" : "x 1", "x 10", GetPack().Edition == GameEdition::XXX ? "x 10" : "x 25" };
			const i32 points[] = { (Result.Time - Result.TimeRemaining) * (GetPack().Edition == GameEdition::XXX ? 2 : 1), (Result.Sheep - Result.SheepRemaining) * 10, (Result.Lives - Result.LivesRemaining) * (GetPack().Edition == GameEdition::XXX ? 10 : 25) };
			for (i32 i = 0; i < 3; ++i)
			{
				Small.Append(DrawCommands, multipliers[i], 360, rows[i]);
				Small.Append(DrawCommands, "=", 410, rows[i]);
				Small.Append(DrawCommands, Padded(points[i]), 450, rows[i]);
			}
			Small.Append(DrawCommands, Text("BonusTotal"), 365, Theme == GameEdition::XXX ? 360 : 340);
			Small.Append(DrawCommands, Padded(points[0] + points[1] + points[2]), 450, Theme == GameEdition::XXX ? 360 : 340);
			DrawCommands.push_back({ Score, 395, Theme == GameEdition::XXX ? 390 : 380, Result.ScoreFrame });
			Small.Append(DrawCommands, Padded(Result.Score), 450, Theme == GameEdition::XXX ? 405 : 395);
			if (promptVisible && (Result.EpisodeComplete || (Result.TimeRemaining <= 0 && Result.SheepRemaining <= 0 && Result.LivesRemaining <= 0)))
				Centered(Small, Text("PressSpace"), 400, 450);
		}
		break;
	case Page::Name:
		AppendScores();
		break;
	}
	if (CurrentPage == Page::GetPacks || CurrentPage == Page::LevelSets || CurrentPage == Page::Help || CurrentPage == Page::Highscores || CurrentPage == Page::Name)
		AppendMenuEyes();
	if (!Notice.empty())
		Small.AppendWrapped(DrawCommands, Notice, 120, 460, 560, 20);
	if (Mouse.x >= 0 && Mouse.y >= 0)
		DrawCommands.push_back({ Cursor, Mouse.x - 9, Mouse.y - 9, ButtonAt(Mouse) >= 0 ? 1 : 0 });
	return DrawCommands;
}
