#include "Game/GameApp.h"
#include "Game/GameSession.h"
#include "Game/GameUi.h"
#include "Game/DebugMenu.h"
#include "Input/WindowInput.h"
#include <array>
#include <chrono>
#include <memory>
#include <stdexcept>

void RunGame(MudGE& engine, const fpath& saves, bool audio, u32 seconds, bool directTutorial)
{
	using Page = GameUi::Page;
	const GameData& data = engine.GetData();
	const str menuMusic = data.SoundPath("Tune01", data.GetThemeEdition());
	const str scoreMusic = data.SoundPath("Tune02", data.GetThemeEdition());
	GameUi ui(engine, saves);
	DebugMenu debug(engine);
	WindowInput controls(engine.WindowHandle(), engine.ScaledVideo());
	std::unique_ptr<GameSession> session;
	bool playing = false;
	auto play = [&](const str& path, bool looped = false, i32 frequency = 0) {
		if (audio && engine.PlaySample(path, looped, frequency))
			Log("Played sound sample: " + path);
	};
	auto music = [&](const str& path) {
		if (audio && !path.empty())
			engine.PlayMusic(path);
		else
			engine.StopMusic();
	};
	auto start = [&] {
		auto next = std::make_unique<GameSession>(engine, ui.GetPack(), ui.GetLevelSet(), static_cast<size_t>(ui.GetPlayerCount()), saves);
		engine.StopLoopedSound();
		session = std::move(next);
		playing = true;
		if (session->GetBonusMode() != BonusMode::Sven)
		{
			ui.ShowBonus(session->GetBonusMode());
			playing = false;
		}
		music(session->Level().Music);
		Log("Started " + ui.GetPack().Name + "/" + ui.GetLevelSet().Name);
	};
	if (directTutorial)
		start();
	else
		music(menuMusic);
	const u64 started = GetTickCount64();
	auto previous = std::chrono::steady_clock::now();
	while (!engine.ExitRequested() && (!seconds || GetTickCount64() - started < seconds * 1000ULL))
	{
		engine.Tick();
		if (engine.ExitRequested())
			break;
		auto now = std::chrono::steady_clock::now();
		const f32 elapsed = std::chrono::duration<f32>(now - previous).count();
		previous = now;
		WindowInput::Result input = controls.Poll();
		if (GetForegroundWindow() == engine.WindowHandle())
		{
			if (debug.IsOpen() || input.Debug)
			{
				if (input.Debug)
					debug.Toggle();
				GameSession* target = session && (playing || ui.GetPage() == Page::Pause) ? session.get() : nullptr;
				if (debug.Handle(input, target) && target)
				{
					engine.StopLoopedSound();
					music(session->Level().Music);
					if (session->GetResults())
					{
						playing = false;
						ui.ShowResults(session->GetResult());
					}
				}
			}
			else if (playing)
			{
				if (input.Back)
				{
					playing = false;
					ui.Show(Page::Pause);
				}
				else
				{
					if (session->GetGameplayEdition() == GameEdition::XXX || ui.HasCustomControls())
					{
						std::array<PlayerInput, 2> players{};
						for (size_t number = 0; number < session->GetPlayerCount(); ++number)
							players[number] = controls.BoundPlayerInput(ui.GetControlBindings()[number]);
						session->Update(elapsed, players);
					}
					else
						session->Update(elapsed, controls.DX(), controls.DY(), controls.ActionHeld());
					if (session->GetResults())
					{
						playing = false;
						ui.ShowResults(session->GetResult());
					}
				}
			}
			else
			{
				const Page priorPage = ui.GetPage();
				if (session && (priorPage == Page::Pause || (priorPage == Page::Quit && ui.UsesSceneBackground())))
					session->UpdateAtmosphere(elapsed);
				if (priorPage == Page::Results && session)
				{
					session->Update(elapsed, 0, 0, false);
					if (input.Confirm && !session->Ready())
					{
						session->SkipBonuses();
						input.Confirm = false;
					}
					ui.ShowResults(session->GetResult());
				}
				UiAction action = UiAction::None;
				try
				{
					action = ui.Handle(input);
					switch (action)
					{
					case UiAction::Start:
						start();
						break;
					case UiAction::Resume:
						playing = session != nullptr;
						break;
					case UiAction::Abandon:
						session.reset();
						ui.Show(Page::Title);
						music(menuMusic);
						break;
					case UiAction::NextLevel:
						engine.StopLoopedSound();
						session->Advance();
						playing = !session->GetResults();
						if (playing)
							music(session->Level().Music);
						if (playing && session->GetBonusMode() != BonusMode::Sven)
						{
							ui.ShowBonus(session->GetBonusMode());
							playing = false;
						}
						if (session->GetResults())
							ui.ShowResults(session->GetResult());
						break;
					default:
						break;
					}
				}
				catch (const std::exception& error)
				{
					ui.SetNotice(error.what());
					Log(error.what());
				}
				if (action == UiAction::Exit)
					break;
				if (!playing && ui.GetPage() != priorPage)
				{
					if (ui.GetPage() == Page::Highscores)
						music(scoreMusic);
					else if (priorPage == Page::Highscores && ui.GetPage() != Page::Name)
						music(menuMusic);
				}
			}
		}
		for (const str& sound : ui.TakeSounds())
			play(sound);
		if (session)
		{
			for (const SoundRequest& request : session->TakeAudioEvents())
				play(request.Asset, request.Playback == SoundPlayback::Loop, request.Frequency);
		}
		if (!session || !session->UfoActive() || session->GetResults())
			engine.StopLoopedSound();
		if (playing)
		{
			const std::vector<SpriteDraw>& scene = session->Draws();
			engine.DrawFrame(debug.IsOpen() ? debug.Draws(scene) : scene);
		}
		else
		{
			const bool overlay = session && ui.UsesSceneBackground();
			const std::vector<SpriteDraw>* background = overlay ? &session->Draws(false) : nullptr;
			const std::vector<SpriteDraw>& menu = ui.Draws(elapsed, background);
			engine.DrawFrame(debug.IsOpen() ? debug.Draws(menu) : menu);
		}
		Sleep(10);
	}
	Log("Presented frames: " + std::to_string(engine.GetFrameCount()));
	engine.StopMusic();
	engine.StopLoopedSound();
}
