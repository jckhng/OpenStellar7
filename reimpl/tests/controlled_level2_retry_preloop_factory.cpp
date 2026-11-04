#include "stellar7/native_session.hpp"
#include "stellar7/level1_loader_work.hpp"
#include "stellar7/level2_music_work.hpp"
#include "stellar7/success_audio_work.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace stellar7 {
struct Level1ReloadTestAccess {
    static void successReturn(Level1State& state,std::uint16_t loop) {
        state.levelIndex_=0;state.loopTick_=loop;state.player_.position={100,-250,0};state.player_.yawVelocity=20;state.player_.forwardVelocity=30;
        state.playerArmor_=600;state.score_=3400;state.systemTimers_={0,0,0,17,0,0,0};state.selectedSystem_=-1;
        state.entities_[0].stateBytes4dTo52[1]=0;state.entities_[1].stateBytes4dTo52[1]=0;
        Level1Entity guardian{};guardian.allocationIndex=state.entities_.size();guardian.type=44;guardian.collisionRadius=2792;
        guardian.stateBytes4dTo52={255,0,2,0,5,0};guardian.renderProfile=WorldActorRenderProfile::heapSelectorNoCallback;guardian.animatedModelPresenterEnabled=false;
        state.postWaveObjectAllocationIndex_=guardian.allocationIndex;state.entities_.push_back(guardian);
        Level1Entity reward{};reward.allocationIndex=state.entities_.size();reward.type=25;reward.collisionRadius=897;reward.position={12000,4000,0};
        reward.stateBytes4dTo52={254,100,2,0,0,1};reward.renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;reward.animatedModelPresenterEnabled=false;
        state.entities_.push_back(reward);state.completionObjectAllocationIndex_=static_cast<std::size_t>(-1);
        state.activeEnemyCount_=0;state.spawnedEnemyCount_=12;state.missionActive_=false;state.successState_=true;
        state.failureState_=false;state.failurePending_=false;state.exitState_=false;state.successContactPending_=false;
        state.ordinaryTailPending_=false;state.ordinaryTailConsumed_=true;state.hasAdvanced_=true;state.playerCollisionMomentum_=0;
        state.playerWeaponCooldown_=0;state.playerProjectileTimers_={};state.projectionListOrigin_={13,29,0};
        state.allocationList_.clear();state.objectListOrder_.clear();state.activeListOrder_.clear();state.semanticActiveListOrder_.clear();
        for(const std::size_t index:{2,3,4,5,7}) {
            state.allocationList_.push_back({Level1State::ObjectKind::entity,index});state.objectListOrder_.push_back({Level1State::ObjectKind::entity,index});
            state.activeListOrder_.push_back(index);state.semanticActiveListOrder_.push_back(index);
        }
        std::array<std::uint16_t,56> table{};for(unsigned i=0;i<table.size();++i) {table[i]=static_cast<std::uint16_t>(i*1193+83);}
        state.rng_=StellarRng(table,48,110,0,100);state.lastDestroyedType_=44;state.consecutiveDestroyedTypeCount_=1;
    }
};

struct GameplayModelTestAccess {
    static void returned(GameplayModel& model,std::uint16_t loop) {
        Level1ReloadTestAccess::successReturn(model.level1_,loop);
        model.phase_=GameplayPhase::successPresentation;model.level2PickupIndex_=7;
        model.recoveredSuccessPause_.reset();model.recoveredSuccessReturned_=false;model.ticks_=loop;model.recoveredLevel1TickBase_=0;
        model.remainingContinues_=1;
    }
};
struct NativeSessionTestAccess {
    static void controlledReturn(NativeGameplaySession& session,std::uint16_t loop) {
        GameplayModelTestAccess::returned(session.model_,loop);
        while(session.recoveredController_->expectedLoopTick()!=loop) {
            session.recoveredController_->consumeEmptyTail(session.recoveredController_->expectedLoopTick());
        }
        session.level1Cursor_=session.model_.level1State().audioEvents().size();
        NativeSuccessState state{};state.control.stage=SuccessControlStage::finished;
        state.control.channel=SuccessPaletteChannel::green;
        session.recoveredSuccessState_=state;
        NativeSuccessRun run{};run.finished=true;run.state=state;session.recoveredSuccess_=std::move(run);
    }
    static SoundBlasterSynth& synth(NativeGameplaySession& session) {return session.synth_;}
    static PresentationClock& clock(NativeGameplaySession& session) {return session.clock_;}
    static const RadarState* radar(const NativeGameplaySession& session) {return &*session.radar_;}
    static const IndexedPages* pixels(const NativeGameplaySession& session) {return &*session.pixels_;}
    static std::uint16_t expected(const NativeGameplaySession& session) {return session.recoveredController_->expectedLoopTick();}
    static void publishPhase(NativeGameplaySession& session,const SoundBlasterClockPhase& phase) {
        //Test-only receipt adoption after the same live clock rendered each child.
        //No callback/model bytes are imported from an original-state snapshot.
        *const_cast<SoundBlasterClockPhase*>(session.recoveredClockPhase())=phase;
    }
};
}
using namespace stellar7;
namespace {
std::vector<std::uint8_t> read(const std::filesystem::path& path) {
    std::ifstream stream(path,std::ios::binary);
    if(!stream) {throw std::runtime_error("missing immutable resource");}
    return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
GameplayAssets assets(const std::filesystem::path& root) {
    const auto cockpit=read(root/"COCKPIT.RES"),main=read(root/"STELLAR.RES"),art=read(root/"STELART.RES");
    GameplayAssets result{};
    result.artGroups=decodeBitmapGroups(art);const auto palettes=decodePalettes(art);
    result.uiPalette=palettes.back();
    for(const auto color:palettes.at(2)) {
        for(const auto component:{color.red,color.green,color.blue}) {result.cockpitDacPalette.push_back(component>>2);}
    }
    result.cockpit=applyVgaDacPalette(decodeFirstIndexedScreenResource(cockpit),result.cockpitDacPalette);
    const auto fonts=decodeFontResources(main);result.titleFont=fonts.at(0);result.scoreFont=fonts.at(1);
    result.soundSequences=decodeTaggedPayloads(cockpit,{'S','N','G',':'});
    result.soundBlasterInstrumentResource=decodeTaggedPayloads(main,{'0','0','3',':'}).at(0);
    const auto songs=decodeTaggedPayloads(main,{'S','N','G',':'});
    result.logoMusicSequence=songs.at(0);result.presentationMusicSequence=songs.at(1);
    result.soundBlasterVolumeTable=decodeSoundBlasterVolumeTable(decodeTaggedPayloads(main,{'S','B','L',':'}).at(0));
    result.models=decodeFirstModelTableResource(cockpit);
    for(unsigned level=1;level<=2;++level) {
        const auto name="LEVEL"+std::to_string(level)+".RES";const auto bytes=read(root/name);
        result.campaignLevels.push_back({name,decodeBitmapGroups(bytes),decodeTaggedPayloads(bytes,{'S','N','G',':'}).at(0),decodeFirstModelTableResource(bytes)});
    }
    const auto& own=result.campaignLevels[0].models;result.models.insert(result.models.end(),own.begin(),own.end());
    result.sharedModelRegistry={256,256}; //Actual loaded44 must be derived from owned banks.
    result.modelColorTables=decodeByteArrays(main);
    const CockpitSettings settings{82,{1,0,1,1,0,0}};result.cockpitSettings=settings;
    //Controlled rank-miss input BEFORE startup; no post-death table/score writes.
    HighscoreTable::Entries entries{};
    for(auto& entry:entries) {entry.score=10000;entry.name="TEST";}
    HighscoreTable scores({},entries);scores.setConfiguration(settings);result.highscoreTable=scores;
    return result;
}
std::unique_ptr<NativeGameplaySession> session(GameplayAssets& game,const std::filesystem::path& root,std::uint16_t loop) {
    const auto resources=decodeNativeStartupAssets(read(root/"STELART.RES"));
    auto startup=std::make_unique<NativeStartupSequence>(game,resources);
    startup->start(NativeStartupExecution::recoveredContext);
    startup->initializeSettings(StartupCapabilityProfile::noMouseOrJoystick);
    startup->initializeResources(NativeStartupDeviceProfile::portableOplNoDspProbe);
    startup->initializePresentation();startup->initializeFonts();
    while(startup->phase()==NativeStartupPhase::logoCountdownWork) {startup->pollLogoCountdown(10000);}
    startup->playLogoEvent();startup->animateLogo();startup->finishLogoSound();startup->releaseLogo();
    startup->skipIntro(NativeIntroKey::escape);startup->beginMenuSong();startup->initializeMenuResources();
    startup->skipCredits(NativeIntroKey::escape);startup->enterMenu(NativeMenuEntryProfile::keyboardOnly);
    startup->beginGame(NativeMenuBeginProfile::enterPressRelease);
    auto result=std::make_unique<NativeGameplaySession>(game,startup->releaseGameplay());
    result->initializeRecoveredLevel1();
    auto phase=bindSuccessAudioResource(*result->recoveredClockPhase(),game.soundSequences.at(0));
    auto& clock=NativeSessionTestAccess::clock(*result);
    for(const auto child:{SuccessAudioChild::entryStop,SuccessAudioChild::activation,SuccessAudioChild::countdownStore,SuccessAudioChild::returnStop}) {
        phase=renderSuccessAudio(clock,phase,child).timing.phase;
    }
    NativeSessionTestAccess::publishPhase(*result,phase);
    NativeSessionTestAccess::controlledReturn(*result,loop);
    return result;
}
template<class Action>void rejects(NativeGameplaySession& session,Action action) {
    const auto sample=session.elapsedSamples(),ticks=session.model().ticks();
    const auto writes=session.synth().registerWrites().size();const auto page=session.pageObservation();
    bool rejected=false;try {action();}catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected&&session.elapsedSamples()==sample&&session.model().ticks()==ticks);
    assert(session.synth().registerWrites().size()==writes);
    assert(session.pageObservation().front==page.front&&session.pageObservation().back==page.back);
    assert(!session.recoveredLevel2Transition());
}
}

namespace stellar7::test {
std::unique_ptr<NativeGameplaySession> makeRecoveredLevel2PreloopSession(const std::filesystem::path& root) {
    //Controlled completed-success fixture, not the actual317 ancestry route.
    static auto game=assets(root);
    auto current=session(game,root,17);
    current->advanceRecoveredLevel2Transition(10000);
    return current;
}
}
