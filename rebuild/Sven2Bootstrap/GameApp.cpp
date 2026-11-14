#include "GameApp.h"
#include "GameSession.h"
#include "GameUi.h"
#include "platform/WindowInput.h"
#include <array>
#include <chrono>
#include <memory>
#include <stdexcept>

namespace sven2 {

void runGame(Engine& engine,const std::filesystem::path& saves,bool audio,unsigned seconds,bool directTutorial) {
    using Page=GameUi::Page;
    GameUi ui(engine,saves);
    WindowInput controls(engine.windowHandle());
    std::unique_ptr<GameSession> session;
    bool playing=false;
    auto play=[&](const std::string& path) {
        if(audio && engine.playSample(path)) log("Played sound sample: "+path);
    };
    auto music=[&](const std::string& path) {
        if(audio && !path.empty()) engine.playMusic(path);
        else engine.stopMusic();
    };
    auto start=[&] {
        auto next=std::make_unique<GameSession>(engine,ui.pack(),ui.levelset());
        session=std::move(next);
        playing=true;
        music(session->level().music);
        log("Started "+ui.pack().name+"/"+ui.levelset().name);
    };
    if(directTutorial) start();
    else music("sven2/sfx/Sven2_music.ogg");
    const auto started=GetTickCount64();
    auto previous=std::chrono::steady_clock::now();
    while(!engine.exitRequested() && (!seconds || GetTickCount64()-started<seconds*1000ULL)) {
        engine.tick();
        if(engine.exitRequested()) break;
        auto now=std::chrono::steady_clock::now();
        const float elapsed=std::chrono::duration<float>(now-previous).count();
        previous=now;
        auto input=controls.poll();
        if(GetForegroundWindow()==engine.windowHandle()) {
            if(playing) {
                if(input.back) {
                    playing=false;
                    ui.show(Page::pause);
                } else {
                    session->update(elapsed,controls.dx(),controls.dy(),controls.actionHeld());
                    if(session->results()) {
                        playing=false;
                        ui.showResults(session->result());
                    }
                }
            } else {
                const auto priorPage=ui.page();
                if(priorPage==Page::results && session) {
                    session->update(elapsed,0,0,false);
                    if(input.confirm && !session->ready()) {
                        session->skipBonuses();
                        input.confirm=false;
                    }
                    ui.showResults(session->result());
                }
                UiAction action=UiAction::none;
                try {
                    action=ui.handle(input);
                    switch(action) {
                    case UiAction::start: start();break;
                    case UiAction::resume: playing=session!=nullptr;break;
                    case UiAction::abandon:
                        session.reset();
                        ui.show(Page::title);
                        music("sven2/sfx/Sven2_music.ogg");
                        break;
                    case UiAction::nextLevel:
                        session->advance();
                        playing=!session->results();
                        if(playing) music(session->level().music);
                        else ui.showResults(session->result());
                        break;
                    default:break;
                    }
                } catch(const std::exception& error) {
                    ui.setNotice(error.what());
                    log(error.what());
                }
                if(action==UiAction::exit) break;
                if(!playing && ui.page()!=priorPage) {
                    if(ui.page()==Page::highscores) music("sven2/sfx/Sven2Highscore.ogg");
                    else if(priorPage==Page::highscores && ui.page()!=Page::name) music("sven2/sfx/Sven2_music.ogg");
                }
            }
        }
        for(const auto& sound:ui.takeSounds()) play(sound);
        if(session) {
            for(const auto& sound:session->takeSounds()) play(sound);
        }
        if(playing) {
            const auto& scene=session->draws();
            engine.drawFrame(scene);
        } else {
            const bool overlay=session && (ui.page()==Page::pause || ui.page()==Page::quit || ui.page()==Page::results);
            const auto* background=overlay?&session->draws():nullptr;
            engine.drawFrame(ui.draws(elapsed,background));
        }
        Sleep(10);
    }
    log("Presented frames: "+std::to_string(engine.frames));
    engine.stopMusic();
}
}
