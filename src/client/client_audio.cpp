#include "client_audio.hpp"
#include "client_config.hpp"
#include "client_textures.hpp"

#include <SFML/Audio.hpp>
#include <fmt/core.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <fstream>
#include <list>
#include <map>
#include <random>
#include <thread>

namespace bayou::client
{
namespace
{
UiSoundHandler uiHandler=nullptr;
UiHoverTracker hoverTracker;
AudioSystem* dispatcher=nullptr;
void dispatchUi(UiSound sound)
{
    if(!dispatcher)return;
    switch(sound) {
    case UiSound::Hover: dispatcher->submit({{"ui_tap",.55f,4.0f,0,false,false,audio::Bus::Hover,0}});break;
    case UiSound::Select: dispatcher->submit({{"ui_tap",.65f,1.5f,0,false,false,audio::Bus::UI,0}});break;
    case UiSound::Open: dispatcher->submit({{"card_paper",.6f,1.0f,0,false,false,audio::Bus::UI,0}});break;
    case UiSound::Close: dispatcher->submit({{"card_paper",.5f,-1.0f,0,false,true,audio::Bus::UI,0}});break;
    case UiSound::Drag: dispatcher->submit({{"card_paper",.5f,2.0f,0,false,false,audio::Bus::UI,0}});break;
    case UiSound::Drop: dispatcher->play("ui_tap",.65f);break;
    case UiSound::Slider: dispatcher->submit({{"ui_tap",.4f,2.0f,0,false,false,audio::Bus::UI,0}});break;
    case UiSound::Tap: dispatcher->play("ui_tap",.55f);break;
    case UiSound::Paper: dispatcher->play("card_paper",.45f);break;
    case UiSound::Confirm: dispatcher->play("ui_confirm",.55f);break;
    case UiSound::Reject: dispatcher->play("ui_reject",.55f,audio::Bus::Alert);break;
    case UiSound::Coins: dispatcher->play("resource_coins",.5f);break;
    case UiSound::Reveal: dispatcher->play("card_paper",.5f);dispatcher->play("crystal",.4f);break;
    case UiSound::Alert: dispatcher->play("alert_bell",.6f,audio::Bus::Alert);break;
    case UiSound::Power: dispatcher->play("power",.45f);break;
    case UiSound::Water: dispatcher->play("water",.5f);break;
    case UiSound::Fire: dispatcher->play("fire",.35f);break;
    case UiSound::Creak: dispatcher->play("creak",.5f);break;
    }
}
float busLevel(audio::Bus bus)
{
    switch(bus) {
    case audio::Bus::UI:return 32.0f;
    case audio::Bus::Hover:return 18.0f;
    case audio::Bus::Combat:return 40.0f;
    case audio::Bus::Accent:return 24.0f;
    case audio::Bus::Ambient:return 10.0f;
    case audio::Bus::Alert:return 46.0f;
    }
    return 20.0f;
}
}
void setUiSoundHandler(UiSoundHandler handler) {uiHandler=handler;}
void beginUiSoundFrame(std::uint64_t surface,bool enabled) {hoverTracker.begin(surface,enabled);}
void observeUiHover(std::uint64_t target,bool hovered) {hoverTracker.offer(target,hovered);}
void observeUiHoverRect(float x,float y,float width,float height,bool hovered)
{
    if(!hovered)return;
    std::uint64_t key=1469598103934665603ull;
    for(float v:{x,y,width,height}) {key^=std::bit_cast<std::uint32_t>(v);key*=1099511628211ull;}
    hoverTracker.offer(key?key:1,true);
}
void clearUiHoverTargets(){hoverTracker.clear();}
void finishUiSoundFrame(){if(hoverTracker.finish()&&uiHandler)uiHandler(UiSound::Hover);}
void emitUiSound(UiSound sound)
{
    if(sound!=UiSound::Hover)hoverTracker.activated();
    if(uiHandler)uiHandler(sound);
}
void emitUiStatus(std::string_view message,bool success)
{
    if(message.empty())return;
    // Progress/instruction messages aren't confirmations. Callers can explicitly
    // emit Confirm for success that has no message.
    if(!success)emitUiSound(UiSound::Reject);
    else if(message.find("saved")!=std::string_view::npos || message.find("Saved")!=std::string_view::npos ||
        message.find("complete")!=std::string_view::npos || message.find("claimed")!=std::string_view::npos ||
        message.find("Deleted")!=std::string_view::npos || message.find("Copied")!=std::string_view::npos)
        emitUiSound(UiSound::Confirm);
}

struct AudioSystem::Impl
{
    struct Voice {
        sf::Sound sound;
        float level;
        audio::Bus bus;
        Voice(const sf::SoundBuffer& buffer,float value,audio::Bus category):sound(buffer),level(value),bus(category){}
    };
    struct Pending {audio::Request request;double due;};
    std::map<std::string,sf::SoundBuffer> buffers;
    std::map<std::string,std::vector<std::string>> slices;
    std::map<std::string,std::size_t> variants;
    std::map<std::string,double> lastPlayed;
    std::list<Voice> voices;
    std::vector<Pending> pending;
    std::vector<std::string> missing;
    std::unique_ptr<sf::Music> music;
    std::unique_ptr<sf::Sound> ambience;
    std::chrono::steady_clock::time_point epoch=std::chrono::steady_clock::now();
    std::mt19937 rng{0x53465831};
    float allVolume=1, musicVolume=1, effectsVolume=1;
    bool allMuted=false,musicMuted=false,effectsMuted=false,enabled=true,ambientWanted=false;
    audio::Intent intent;
    double intentAt=0;
    std::size_t played=0;
    double now()const {return std::chrono::duration<double>(std::chrono::steady_clock::now()-epoch).count();}
    bool audible()const{return enabled&&!allMuted&&!effectsMuted&&allVolume>0&&effectsVolume>0;}
    void stop()
    {
        pending.clear();voices.clear();
        if(ambience)ambience->stop();
    }
    void levels()
    {
        for(auto& v:voices)v.sound.setVolume(audible()?v.level*allVolume*effectsVolume:0.0f);
        if(music)music->setVolume(enabled&&!allMuted&&!musicMuted?22*allVolume*musicVolume:0);
        if(ambience) {
            ambience->setVolume(audible()&&ambientWanted?10*allVolume*effectsVolume:0);
            if(audible()&&ambientWanted) {
                if(ambience->getStatus()!=sf::SoundSource::Status::Playing)ambience->play();
            } else ambience->stop();
        }
    }
    void load(std::string_view key,std::string_view file)
    {
        const auto location=resolveAssetPath(std::string(file));
        if(!location||!buffers[std::string(key)].loadFromFile(*location))missing.emplace_back(file);
    }
    void start(const audio::Request& request)
    {
        if(!audible())return;
        std::string key=request.id;
        if(request.slice && !slices[key].empty()) {
            auto& options=slices[key];key=options[variants[request.id]++%options.size()];
        }
        auto buffer=buffers.find(key);
        if(buffer==buffers.end()||buffer->second.getSampleCount()==0)return;
        if(request.reverse) {
            const std::string reverseKey=key+"/reverse";
            if(!buffers.contains(reverseKey)) {
                const auto& original=buffer->second;
                std::vector<std::int16_t> samples(original.getSamples(),original.getSamples()+original.getSampleCount());
                std::reverse(samples.begin(),samples.end());
                sf::SoundBuffer reversed;
                if(!reversed.loadFromSamples(samples.data(),samples.size(),original.getChannelCount(),
                    original.getSampleRate(),original.getChannelMap()))return;
                buffers.emplace(reverseKey,std::move(reversed));
            }
            buffer=buffers.find(reverseKey);
        }
        // Shared cues are aggregated in a small window, including duplicate UI
        // activation paths. Alerts evict a low-priority voice at the voice cap.
        const auto stamp=now();
        // Hover must never consume the click sound when the player clicks
        // immediately after entering a control.
        const std::string coalesce=request.id+"/"+std::to_string(request.actorId)+"/"+
            std::to_string(static_cast<int>(request.bus));
        if(lastPlayed.contains(coalesce)&&stamp-lastPlayed[coalesce]<.075)return;
        if(voices.size()>=24) {
            const auto disposable=std::find_if(voices.begin(),voices.end(),[](const auto& v){return v.bus!=audio::Bus::Alert;});
            if(request.bus!=audio::Bus::Alert||disposable==voices.end())return;
            voices.erase(disposable);
        }
        lastPlayed[coalesce]=stamp;
        float semitones=request.semitones,gain=request.gain;
        if(request.bus==audio::Bus::Combat||request.bus==audio::Bus::Accent) {
            semitones+=std::uniform_real_distribution<float>(-.25f,.25f)(rng);
            gain*=std::pow(10.0f,std::uniform_real_distribution<float>(-1.2f,1.2f)(rng)/20);
        }
        auto& voice=voices.emplace_back(buffer->second,busLevel(request.bus)*std::clamp(gain,0.0f,1.0f),request.bus);
        voice.sound.setPitch(std::pow(2.0f,semitones/12));
        voice.sound.setVolume(voice.level*allVolume*effectsVolume);
        voice.sound.play();
        if(voice.sound.getStatus()==sf::SoundSource::Status::Playing)++played;
    }
};

AudioSystem::AudioSystem(bool startMusic):impl(std::make_unique<Impl>())
{
    for(const auto& definition:audio::definitions()) {
        impl->load(definition.id,definition.file);
        for(std::size_t i=0;i<definition.slices.size();++i) {
            const auto key=std::string(definition.id)+"/"+std::to_string(i);
            impl->load(key,definition.slices[i]);impl->slices[std::string(definition.id)].push_back(key);
        }
    }
    if(const auto found=impl->buffers.find("bayou");found!=impl->buffers.end()&&found->second.getSampleCount()) {
        impl->ambience=std::make_unique<sf::Sound>(found->second);impl->ambience->setLooping(true);
    }
    if(startMusic)if(const auto path=resolveAssetPath("audio/GT soundtrack 006.mp3")) {
        auto music=std::make_unique<sf::Music>();
        if(music->openFromFile(*path)){music->setLooping(true);impl->music=std::move(music);impl->levels();impl->music->play();}
    }
    dispatcher=this;setUiSoundHandler(dispatchUi);
    for(const auto& path:impl->missing)fmt::println(stderr,"[Audio] Missing or invalid asset: {}",path);
}
AudioSystem::~AudioSystem(){if(dispatcher==this){dispatcher=nullptr;setUiSoundHandler(nullptr);}}
void AudioSystem::play(std::string_view id,float gain,audio::Bus bus)
{
    submit({{std::string(id),gain,0,0,false,false,bus,0}});
}
void AudioSystem::play(AudioCue cue,float gain)
{
    switch(cue) {
    case AudioCue::ButtonClick:play("ui_tap",.55f*gain);break;
    case AudioCue::PiecePlace:play("teleport",gain,audio::Bus::Combat);break;
    case AudioCue::UnitMove:play("foot_soft",gain,audio::Bus::Combat);break;
    case AudioCue::UnitAttack:play("blade",gain,audio::Bus::Combat);break;
    case AudioCue::UnitDeath:play("death",gain,audio::Bus::Combat);break;
    case AudioCue::Dematerialize:play("teleport",gain,audio::Bus::Combat);break;
    case AudioCue::ClockWarning:play("alert_bell",gain,audio::Bus::Alert);break;
    case AudioCue::Victory:impl->stop();play("victory",gain,audio::Bus::Alert);break;
    case AudioCue::Defeat:impl->stop();play("defeat",gain,audio::Bus::Alert);break;
    }
}
void AudioSystem::submit(const std::vector<audio::Request>& requests)
{
    if(!impl->audible())return;
    for(const auto& request:requests) {
        if(impl->pending.size()>=64) {
            if(request.bus!=audio::Bus::Alert)continue;
            const auto discard=std::find_if(impl->pending.begin(),impl->pending.end(),[](const auto& p){return p.request.bus!=audio::Bus::Alert;});
            if(discard==impl->pending.end())continue;
            impl->pending.erase(discard);
        }
        impl->pending.push_back({request,impl->now()+std::max(0.0f,request.delay)});
    }
}
void AudioSystem::expect(audio::Intent intent){impl->intent=intent;impl->intentAt=impl->now();}
void AudioSystem::resetMatch(bool preserveInterface)
{
    impl->intent={};
    if(preserveInterface) {
        std::erase_if(impl->pending,[](const auto& p){return p.request.bus==audio::Bus::Combat||p.request.bus==audio::Bus::Accent;});
        std::erase_if(impl->voices,[](const auto& v){return v.bus==audio::Bus::Combat||v.bus==audio::Bus::Accent;});
    } else impl->stop();
    impl->lastPlayed.clear();
}
void AudioSystem::transition(const game_data::Snapshot& before,const game_data::Snapshot& after,bool initialized)
{
    if(!initialized){impl->intent={};return;}
    if(impl->now()-impl->intentAt>5.0)impl->intent={};
    if(!audio::gameplayChanged(before,after)) {
        if(impl->intent.kind!=audio::Intent::Kind::None && before.status!=after.status) {
            play("ui_reject",.5f,audio::Bus::Alert);impl->intent={};
        }
        return;
    }
    submit(audio::planTransition(before,after,impl->intent));impl->intent={};
}
void AudioSystem::update()
{
    std::erase_if(impl->voices,[](const auto& v){return v.sound.getStatus()==sf::SoundSource::Status::Stopped;});
    const auto now=impl->now();
    std::stable_sort(impl->pending.begin(),impl->pending.end(),[](const auto& a,const auto& b){
        if(a.due!=b.due)return a.due<b.due;return a.request.bus==audio::Bus::Alert&&b.request.bus!=audio::Bus::Alert;});
    while(!impl->pending.empty()&&impl->pending.front().due<=now) {
        auto request=impl->pending.front().request;impl->pending.erase(impl->pending.begin());impl->start(request);
    }
    impl->levels();
}
void AudioSystem::setEnabled(bool v){impl->enabled=v;if(!v)impl->stop();impl->levels();}
void AudioSystem::setAmbient(bool v){impl->ambientWanted=v;impl->levels();}
void AudioSystem::setAllVolume(float v){impl->allVolume=std::clamp(v,0.0f,1.0f);if(v<=0)impl->stop();impl->levels();}
void AudioSystem::setMusicVolume(float v){impl->musicVolume=std::clamp(v,0.0f,1.0f);impl->levels();}
void AudioSystem::setSoundEffectsVolume(float v){impl->effectsVolume=std::clamp(v,0.0f,1.0f);if(v<=0)impl->stop();impl->levels();}
float AudioSystem::getAllVolume()const{return impl->allVolume;}
float AudioSystem::getMusicVolume()const{return impl->musicVolume;}
float AudioSystem::getSoundEffectsVolume()const{return impl->effectsVolume;}
void AudioSystem::setAllMuted(bool v){impl->allMuted=v;if(v)impl->stop();impl->levels();}
void AudioSystem::setMusicMuted(bool v){impl->musicMuted=v;impl->levels();}
void AudioSystem::setSoundEffectsMuted(bool v){impl->effectsMuted=v;if(v)impl->stop();impl->levels();}
bool AudioSystem::isAllMuted()const{return impl->allMuted;}
bool AudioSystem::isMusicMuted()const{return impl->musicMuted;}
bool AudioSystem::isSoundEffectsMuted()const{return impl->effectsMuted;}
account_data::AudioSettings AudioSystem::getSettings()const
{
    auto percent=[](float x){return static_cast<std::uint8_t>(std::lround(x*100));};
    return {percent(impl->allVolume),percent(impl->musicVolume),percent(impl->effectsVolume),impl->allMuted,impl->musicMuted,impl->effectsMuted};
}
void AudioSystem::applySettings(const account_data::AudioSettings& s)
{
    setAllVolume(s.allVolumePercent/100.0f);setMusicVolume(s.musicVolumePercent/100.0f);
    setSoundEffectsVolume(s.soundEffectsVolumePercent/100.0f);setAllMuted(s.allMuted);
    setMusicMuted(s.musicMuted);setSoundEffectsMuted(s.soundEffectsMuted);
}
bool AudioSystem::smokeTest(const std::filesystem::path& report)
{
    const bool loaded=impl->missing.empty()&&audio::definitions().size()==41;
    setAllVolume(.03f);
    for(const auto& d:audio::definitions()) {play(d.id,.4f,audio::Bus::Combat);update();std::this_thread::sleep_for(std::chrono::milliseconds(90));}
    const auto played=impl->played;
    resetMatch();
    const auto beforeHover=impl->played;
    beginUiSoundFrame(0xFADE);observeUiHover(123,true);finishUiSoundFrame();update();
    beginUiSoundFrame(0xFADE);observeUiHover(123,true);finishUiSoundFrame();update();
    const bool hoverOnce=impl->played==beforeHover+1;
    emitUiSound(UiSound::Tap);update();
    const bool clickAfterHover=impl->played==beforeHover+2;
    resetMatch();
    submit({{"blade",1,0,1,true,false,audio::Bus::Combat,0}});
    setSoundEffectsMuted(true);const bool muted=impl->pending.empty()&&impl->voices.empty();
    setSoundEffectsMuted(false);update();const bool noReplay=impl->pending.empty()&&impl->voices.empty();
    submit({{"blade",1,0,1,true,false,audio::Bus::Combat,0}});
    setAllMuted(true);const bool masterMute=impl->pending.empty()&&impl->voices.empty();
    setAllMuted(false);setMusicMuted(true);play("ui_tap",.4f);update();
    const bool independentBus=!impl->voices.empty();
    setAllVolume(0);const bool zeroClears=impl->pending.empty()&&impl->voices.empty();
    setAllVolume(.03f);
    submit({{"alert_bell",1,0,1,false,false,audio::Bus::Alert,0},
        {"blade",1,0,1,true,false,audio::Bus::Combat,1}});
    resetMatch(true);
    const bool syncPreservesAlert=impl->pending.size()==1&&impl->pending.front().request.id=="alert_bell";
    setAllMuted(true);
    if(report.has_parent_path())std::filesystem::create_directories(report.parent_path());
    std::ofstream file(report);
    const bool passed=loaded&&played==41&&hoverOnce&&clickAfterHover&&muted&&noReplay&&masterMute&&independentBus&&zeroClears&&syncPreservesAlert;
    file<<fmt::format("{{\"passed\":{},\"masters\":41,\"buffers\":{},\"missing\":{},\"played\":{},\"hover_once\":{},\"click_after_hover\":{},\"mute_clears_queue\":{},\"unmute_does_not_replay\":{},\"master_mute_clears_queue\":{},\"music_mute_preserves_fx\":{},\"zero_volume_clears_queue\":{},\"initial_sync_preserves_alert\":{}}}\n",
        passed,impl->buffers.size(),impl->missing.size(),played,hoverOnce,clickAfterHover,muted,noReplay,masterMute,independentBus,zeroClears,syncPreservesAlert);
    return static_cast<bool>(file)&&passed;
}
}
