#include "stellar7/success_audio_work.hpp"
#include "stellar7/audio_direct_work.hpp"
#include "stellar7/audio_program_work.hpp"
#include "stellar7/level1_loader_work.hpp"
#include "stellar7/native_session.hpp"
#include "stellar7/native_startup.hpp"
#include "stellar7/timer_install_work.hpp"
#include "stellar7/timer_registration_work.hpp"

#include <algorithm>
#include <cassert>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
#include <stdexcept>
#include <tuple>

namespace stellar7 {
struct NativeSessionTestAccess {
    static SoundBlasterSynth& synth(NativeGameplaySession& session) { return session.synth_; }
    static PresentationClock& clock(NativeGameplaySession& session) { return session.clock_; }
};
}

namespace {
using namespace stellar7;
constexpr NormalWorkOperation combatEvent{NormalWorkKind::gameplayArrivalEvent,1,gameplayArrivalEvent};
constexpr std::int64_t horizon = 120000;
//6F65..6F6C,02E6..030C,0311..0316,0478 and dispatcher CALL17E5.
constexpr std::uint64_t successKernelPrefixDispatches = 4+14+1+4+11+12;
constexpr std::uint16_t arrivalDuration = 360;
constexpr std::uint8_t arrivalProgram = 40;

template<class Function>
void rejects(Function function) {
    bool rejected = false;
    try { function(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

std::vector<std::uint8_t> bank() {
    std::vector<std::uint8_t> result(96 * 28);
    for (std::size_t program = 0; program < 96; ++program) {
        const auto offset = program * 28;
        for (const auto op : {0U, 13U}) {
            result[offset + op + 1] = 1;
            result[offset + op + 3] = 15;
            result[offset + op + 4] = 7;
            result[offset + op + 7] = 7;
        }
        result[offset + 12] = 1;
    }
    return result;
}

SoundBlasterCallbackState fixture() {
    SoundBlasterCallbackState state{};
    state.program.emplace();
    state.program->instrumentBank = bank();
    state.program->volumeTable.resize(16 * 64);
    state.program->accumulator = 0;
    for (std::size_t row = 0; row < 16; ++row) {
        for (std::size_t column = 0; column < 64; ++column) {
            state.program->volumeTable[row * 64 + column] = static_cast<std::uint8_t>((row * 3 + column * 5) % 64);
        }
    }
    state.channelEnabled.fill(1);
    state.voices.channels.fill(0xff);
    state.voices.channels[0] = 1;
    state.sounding[0] = 1;
    for (std::size_t voice = 0; voice < 9; ++voice) {
        state.voices.notes[voice] = static_cast<std::uint8_t>(60 + voice);
        state.velocities[voice] = static_cast<std::uint8_t>(voice < 6 ? 40 + voice : std::array<unsigned,3>{99,104,71}[voice-6]);
        state.program->voices[voice].carrierFallback = static_cast<std::uint16_t>(31 - voice);
        state.program->voices[voice].operatorFallback = static_cast<std::uint16_t>(31 - voice);
    }
    state.sequence.resize(36);
    state.sequence.insert(state.sequence.end(), {0xc1, 0, 0, 0x91, 65, 90, 127, 0xfc});
    state.position = 36;
    return state;
}

std::uint64_t blockHash(const std::vector<SoundBlasterWorkBlock>& blocks) {
    auto hash = 14695981039346656037ULL;
    for (const auto& block : blocks) {
        for (const auto value : {static_cast<std::uint64_t>(block.kind), block.dispatches,
                static_cast<std::uint64_t>(block.registerNumber), static_cast<std::uint64_t>(block.value)}) {
            hash = (hash ^ value) * 1099511628211ULL;
        }
    }
    return hash;
}

auto callbackFields(const SoundBlasterCallbackState& state) {
    return std::make_tuple(state.voices.channels, state.voices.notes, state.voices.tuning,
        state.voices.directTimers, state.velocities, state.sounding, state.channelVoiceCursors,
        state.sequence, state.position, state.wait, state.runningStatus, state.active, state.latch,
        state.program->channelPrograms, state.program->operators,state.program->playerHitVolumeRow);
}

SoundBlasterClockPhase phaseFor(const SoundBlasterCallbackState& state) {
    auto phase = normalWorkTiming(nativeTimerInstallPhase(), timerInstallOperations(), 1000).phase;
    SoundBlasterCallbackState noHandle{};
    noHandle.domain = SoundBlasterCallbackDomain::noHandle;
    noHandle.operation = 0x1e;
    noHandle.active = false;
    phase.callback = noHandle;
    phase.soundBlasterInitialization = AudioClockSoundBlasterInitializationPhase{158, true};
    phase = normalWorkTiming(phase, timerRegistrationOperations(), 1000).phase;
    phase.callback = state;
    phase.sequenceSelection = AudioClockSequenceSelectionPhase{state};
    phase.sequenceSelection->published = true;
    phase.sequenceSelection->songOrdinal = 2; //Third retained handle, Level1 SNG0.
    phase.sequenceSelection->level1DirectResetCompleted = true;
    return phase;
}

const SoundBlasterForegroundEffect& directReceipt(const NormalWorkTiming& work) {
    const auto found = std::find_if(work.soundBlasterEffects.begin(), work.soundBlasterEffects.end(),
        [](const auto& effect) { return effect.kind == SoundBlasterForegroundKind::gameplayArrivalDirectState; });
    assert(found != work.soundBlasterEffects.end() && found->directState);
    return *found;
}

SoundBlasterVolumeRow syntheticRow() {
    SoundBlasterVolumeRow row{gameplayPlayerHitCallerVolume,{}};
    for (std::size_t index = 0; index < row.values.size(); ++index) {
        row.values[index] = static_cast<std::uint8_t>(29*index+1);
    }
    return row;
}

SoundBlasterCallbackState history() {
    auto state = soundBlasterDirectResetState(fixture());
    state = soundBlasterCallbackAdvance(state).state;
    state = soundBlasterDirectAdvance(state,gameplayPrimaryFireDirectRecord,SoundBlasterDirectCaller::gameplayPrimaryFire).state;
    for (unsigned callback = 0; callback < 12; ++callback) { state = soundBlasterCallbackAdvance(state).state; }
    state = soundBlasterDirectAdvance(state,gameplayEnemyType17DirectRecord,SoundBlasterDirectCaller::gameplayEnemyFire).state;
    for (unsigned callback = 0; callback < 10; ++callback) { state = soundBlasterCallbackAdvance(state).state; }
    state = soundBlasterDirectAdvance(state,gameplayPrimaryFireDirectRecord,SoundBlasterDirectCaller::gameplayPrimaryFire).state;
    assert((state.voices.directTimers == std::array<std::uint16_t,3>{50,12,0}));
    return state;
}

GameplayAssets assets() {
    GameplayAssets game;
    game.cockpit = {std::vector<std::uint8_t>(320 * 200), std::vector<Rgb>(256)};
    game.cockpitDacPalette.resize(256 * 3);
    game.uiPalette.assign(256, {12, 20, 28});
    game.horizonFrames = {{256, 16, std::vector<std::uint8_t>(256 * 16)}};
    game.soundBlasterVolumeTable.resize(16 * 64);
    for (std::size_t i = 0; i < game.soundBlasterVolumeTable.size(); ++i) {
        game.soundBlasterVolumeTable[i] = static_cast<std::uint8_t>(i % 64);
    }
    game.artGroups.resize(6, std::vector<IndexedBitmap>(64, {4, 1, {1, 2, 3, 4}}));
    auto& ui = game.artGroups[0];
    ui[0] = {16, 20, std::vector<std::uint8_t>(320, 5)};
    ui[11] = {4, 5, std::vector<std::uint8_t>(20, 9)};
    ui[12] = {40, 40, std::vector<std::uint8_t>(1600, 6)};
    ui[14] = {24, 22, std::vector<std::uint8_t>(528, 5)};
    ui[16] = {120, 43, std::vector<std::uint8_t>(5160, 3)};
    ui[17] = {120, 33, std::vector<std::uint8_t>(3960, 8)};
    ui[18] = {120, 25, std::vector<std::uint8_t>(3000, 8)};
    ui[20] = {264, 2, std::vector<std::uint8_t>(528, 4)};
    ui[21] = ui[22] = {8, 5, std::vector<std::uint8_t>(40, 7)};
    game.titleFont = {32, 6, 0, std::vector<IndexedBitmap>(64, {8, 6, std::vector<std::uint8_t>(48, 1)})};
    game.scoreFont = {32, 8, 0, std::vector<IndexedBitmap>(96, {4, 8, std::vector<std::uint8_t>(32, 1)})};
    game.modelColorTables.resize(2);
    auto song = std::vector<std::uint8_t>(36);
    song[0] = 0x84; song[5] = 1; song[6] = 4;
    game.levelMusicSequence = song;
    // Enable the synthetic song channel through its recovered controller message.
    game.levelMusicSequence.insert(game.levelMusicSequence.end(), {0xb1, 0x4e, 1, 0, 0xc1, 0, 0, 0x91, 65, 90, 127, 0xfc});
    game.logoMusicSequence = song;
    game.logoMusicSequence[5] = 7;
    game.logoMusicSequence.insert(game.logoMusicSequence.end(), {0xc1, 0, 127, 0xfc});
    game.presentationMusicSequence = song;
    for (unsigned group = 0; group < 8; ++group) {
        game.presentationMusicSequence.insert(game.presentationMusicSequence.end(), {0xc1, 0, 127});
    }
    game.presentationMusicSequence.push_back(0xfc);
    game.soundBlasterInstrumentResource.resize(19 + 2 * 0x540 + 2);
    game.soundBlasterInstrumentResource[0] = 0x89;
    game.soundBlasterInstrumentResource[1] = 0x11;
    const auto patches = bank();
    std::copy(patches.begin(), patches.begin() + 0x540, game.soundBlasterInstrumentResource.begin() + 19);
    game.soundBlasterInstrumentResource[19 + 0x540] = 0xab;
    game.soundBlasterInstrumentResource[19 + 0x540 + 1] = 0xcd;
    std::copy(patches.begin() + 0x540, patches.end(), game.soundBlasterInstrumentResource.begin() + 19 + 0x540 + 2);
    const CockpitSettings configuration{81, {1, 0, 1, 1, 0, 255}};
    HighscoreTable scores({}, {});
    scores.setConfiguration(configuration);
    game.cockpitSettings = configuration;
    game.highscoreTable = scores;
    return game;
}


enum class SuccessSequenceProfile { synthetic, ownedHeader };
std::vector<std::uint8_t> successSequence(const SuccessSequenceProfile profile=SuccessSequenceProfile::synthetic) {
    std::vector<std::uint8_t> result(36);
    result[0]=0x84; result[5]=1; result[6]=4;
    if (profile==SuccessSequenceProfile::ownedHeader) {
        //Immutable COCKPIT firstSNG header; callback body below stays synthetic.
        constexpr std::array<std::uint8_t,36> header{0x84,0,0,0,0,0,1,0,1,0,1,2,4,2,4,6,20,1,4,0,0,0,0,0,16,0,0,0,32,0,0,0,0,0,0,0};
        std::copy(header.begin(),header.end(),result.begin());
    }
    result.insert(result.end(),{0xc1,0,0,0x91,65,90,127,0xfc});
    return result;
}

SoundBlasterClockPhase isolatedChildPhase(const SoundBlasterClockPhase& retained) {
    //Independent component hardware fixture. Typed driver history stays continuous.
    //The live replay below separately tests one uninterrupted retained clock.
    auto result=phaseFor(*retained.callback);
    result.successAudio=retained.successAudio;
    result.sequenceSelection=retained.sequenceSelection;
    result.timer->globalCount=retained.timer->globalCount;
    return result;
}

struct FamilyControl { const char* name; std::array<std::uint16_t,3> timers; SuccessSequenceProfile profile=SuccessSequenceProfile::synthetic; };
std::array<FamilyControl,4> controls() {
    return {{{"zero",{0,0,0}},{"live",{4,2,0}},{"expiry",{1,2,1}},{"owned-header",{0,0,0},SuccessSequenceProfile::ownedHeader}}};
}
struct FamilyResult {
    NormalWorkTiming entry;
    SoundBlasterCallbackAdvance noHandle;
    NormalWorkTiming activation;
    NormalWorkTiming countdown;
    NormalWorkTiming returned;
    SoundBlasterClockPhase activationEntry;
};
FamilyResult family(const FamilyControl& control) {
    auto incoming=fixture(); incoming.voices.directTimers=control.timers;
    const auto bound=bindSuccessAudioResource(phaseFor(incoming),successSequence(control.profile));
    auto entry=successAudioTiming(bound,SuccessAudioChild::entryStop);
    assert(entry.callbacks.empty()&&entry.total.dispatches==2656);
    const auto noHandle=soundBlasterCallbackAdvance(*entry.phase.callback);
    auto phase=entry.phase; phase.callback=noHandle.state;
    const auto activationEntry=isolatedChildPhase(phase);
    auto activation=successAudioTiming(activationEntry,SuccessAudioChild::activation);
    assert(activation.callbacks.empty()&&activation.total.dispatches==(control.profile==SuccessSequenceProfile::ownedHeader?633:514));
    auto countdown=successAudioTiming(isolatedChildPhase(activation.phase),SuccessAudioChild::countdownStore);
    assert(countdown.total.dispatches==1&&countdown.phase.timer->globalCount==992);
    auto returned=successAudioTiming(isolatedChildPhase(countdown.phase),SuccessAudioChild::returnStop);
    assert(returned.callbacks.empty()&&returned.total.dispatches==2656);
    return {std::move(entry),noHandle,std::move(activation),std::move(countdown),std::move(returned),activationEntry};
}

void componentControls() {
    for (const auto& control:controls()) {
        const auto result=family(control);
        assert(result.entry.phase.callback->domain==SoundBlasterCallbackDomain::successNoHandle);
        assert(result.entry.phase.callback->voices.directTimers==control.timers);
        assert(result.noHandle.dispatches==(control.timers[0]==0?58:control.timers[0]==4?64:637));
        assert(result.activation.phase.callback->voices.directTimers==result.noHandle.state.voices.directTimers);
        assert(result.activation.phase.callback->program->repeatCount==1);
        assert(result.activation.phase.callback->program->handleStatus==0);
        assert(result.activation.phase.callback->sequence==successSequence(control.profile));
        assert(result.returned.phase.successAudio->stage==SuccessAudioStage::returned);
        const auto& writes=result.activation.soundBlasterEffects;
        const auto source=soundBlasterSequenceOperations(successSequence(control.profile),DriverSequenceAdmission::successInheritedChannels);
        std::size_t stores=0;
        for (const auto& effect:writes) {
            if (effect.kind==SoundBlasterForegroundKind::successActivationState&&effect.value!=0) {
                assert(effect.value==stores+1);
                const auto ordinal=successKernelPrefixDispatches+source.effects[stores].afterDispatch;
                //Finish prefix-1 first: its terminal settlement precedes the store.
                //The store's elapsed receipt is before its own trailing settlement.
                const auto before=normalWorkTiming(result.activationEntry,{{NormalWorkKind::instructions,ordinal-1}},horizon);
                assert(before.callbacks.empty());
                assert(effect.cycleOffset==before.total.elapsedCycles+1);
                ++stores;
            }
        }
        assert(stores==source.effects.size());
    }
    const auto actualHeader=successSequence(SuccessSequenceProfile::ownedHeader);
    const auto activation=soundBlasterSequenceOperations(actualHeader,DriverSequenceAdmission::successInheritedChannels);
    assert(activation.state.channels[0].voiceCount==6&&activation.state.channels[1].voiceCount==7);
    std::vector<std::tuple<DriverSequenceEffectKind,std::uint16_t,std::uint16_t>> aliases;
    for (const auto& effect:activation.effects) {
        if (effect.sourcePc==0x1947&&effect.kind==DriverSequenceEffectKind::channelVoiceCount) {
            aliases.emplace_back(effect.kind,effect.index,effect.value);
        }
    }
    assert((aliases==std::vector<std::tuple<DriverSequenceEffectKind,std::uint16_t,std::uint16_t>>{
        {DriverSequenceEffectKind::channelVoiceCount,0,6},{DriverSequenceEffectKind::channelVoiceCount,1,7}}));
    rejects([&]{soundBlasterSequenceOperations(actualHeader,DriverSequenceAdmission::freshChannels);});
    rejects([&]{soundBlasterSequenceOperations(actualHeader,DriverSequenceAdmission::inheritedChannels);});
    auto tooMany=actualHeader;tooMany[15]=7;
    rejects([&]{soundBlasterSequenceOperations(tooMany,DriverSequenceAdmission::successInheritedChannels);});
    auto state=fixture(); state.voices.directTimers={4,2,0};
    rejects([&]{soundBlasterDeselectState(state);});
    auto bound=bindSuccessAudioResource(phaseFor(state),successSequence());
    rejects([&]{bindSuccessAudioResource(bound,successSequence());});
    rejects([&]{successAudioTiming(bound,SuccessAudioChild::activation);});
    rejects([&]{successAudioTiming(bound,SuccessAudioChild::countdownStore);});
    rejects([&]{successAudioTiming(bound,SuccessAudioChild::returnStop);});
    auto phase=family(controls()[0]).activation.phase;
    rejects([&]{normalWorkTiming(phase,{{NormalWorkKind::successCountdownStore,1,1000}},horizon);});
    //One remaining instruction lets MOV finish before the pending IRQ is serviced.
    phase=isolatedChildPhase(phase);
    constexpr std::uint16_t oldCount=9;
    phase.timer->globalCount=oldCount;
    const auto local=5000-phase.remainingCycles-phase.deferredCycles;
    phase.remainingCycles=1; phase.deferredCycles=5000-local-1;
    phase.devices->irq0Requested=true; phase.irqCheckRequested=true; phase.timer->callbackCount=1;
    const auto preceding=normalWorkTiming(phase,{{NormalWorkKind::instructions,1}},horizon);
    const auto countdown=successAudioTiming(phase,SuccessAudioChild::countdownStore);
    assert(countdown.callbacks.size()==preceding.callbacks.size());
    const auto afterIrqs=static_cast<std::uint16_t>(oldCount-preceding.phase.timer->globalCount);
    assert(afterIrqs==1&&countdown.phase.timer->globalCount==successRawCountdown-afterIrqs);
    //The same explicit predecessor can settle that IRQ before the7072 store.
    assert(preceding.phase.timer->globalCount==oldCount-1&&preceding.callbacks.size()==1);
    const auto before=successAudioTiming(preceding.phase,SuccessAudioChild::countdownStore);
    assert(before.callbacks.empty()&&before.phase.timer->globalCount==successRawCountdown);
    //Deployment op14h is the original driver's2ECE RET, preserving live timers.
    const auto input=history();
    const auto stop=normalWorkTiming(phaseFor(input),{{NormalWorkKind::gameplayArrivalStopEvent,1,17}},horizon);
    assert(stop.total.dispatches==50&&stop.callbacks.empty()&&stop.soundBlasterEffects.empty());
    assert(callbackFields(*stop.phase.callback)==callbackFields(input));
    auto hit=input; hit.program->playerHitVolumeRow=syntheticRow();
    const auto guardian=normalWorkTiming(phaseFor(hit),{{NormalWorkKind::gameplayGuardianFireEvent,1,6}},horizon);
    const auto ordinary=normalWorkTiming(phaseFor(hit),{{NormalWorkKind::gameplayPlayerHitEvent,1,6}},horizon);
    assert(guardian.total.dispatches==ordinary.total.dispatches);
    assert(callbackFields(*guardian.phase.callback)==callbackFields(*ordinary.phase.callback));
    rejects([&]{normalWorkTiming(phaseFor(input),{{NormalWorkKind::gameplayArrivalStopEvent,1,6}},horizon);});
}

void liveReplay() {
    auto game=assets(); game.soundSequences={successSequence()};
    NativeStartupAssets resources;
    resources.logoScreen={std::vector<std::uint8_t>(320*200,9),std::vector<Rgb>(256)};
    resources.menuScreen=resources.logoScreen;
    resources.logoFrames.assign(8,{4,1,{1,0,2,3}});
    auto startup=std::make_unique<NativeStartupSequence>(game,resources);
    startup->start(NativeStartupExecution::recoveredContext);
    startup->initializeSettings(StartupCapabilityProfile::noMouseOrJoystick);
    startup->initializeResources(NativeStartupDeviceProfile::portableOplNoDspProbe);
    startup->initializePresentation(); startup->initializeFonts();
    while(startup->phase()==NativeStartupPhase::logoCountdownWork){startup->pollLogoCountdown(10000);}
    startup->playLogoEvent(); startup->animateLogo(); startup->finishLogoSound(); startup->releaseLogo();
    startup->skipIntro(NativeIntroKey::escape); startup->beginMenuSong(); startup->initializeMenuResources();
    startup->skipCredits(NativeIntroKey::escape); startup->enterMenu(NativeMenuEntryProfile::keyboardOnly);
    startup->beginGame(NativeMenuBeginProfile::enterPressRelease);
    auto session=std::make_unique<NativeGameplaySession>(game,startup->releaseGameplay());
    auto& synth=NativeSessionTestAccess::synth(*session);
    auto& clock=NativeSessionTestAccess::clock(*session);
    const auto music=activateLevel1Music(clock,synth,*session->recoveredClockPhase());
    auto phase=resetLevel1DirectEffects(clock,synth,music.phase).phase;
    const auto row=syntheticRow(); synth.configurePlayerHitVolumeRow(row);
    phase.callback->program->playerHitVolumeRow=row;
    phase=clock.renderWork(phase,{{NormalWorkKind::gameplayArrivalEvent,1,17}},horizon).timing.phase;
    phase=clock.renderWork(phase,{{NormalWorkKind::gameplayGuardianFireEvent,1,6}},horizon).timing.phase;
    phase=clock.renderWork(phase,{{NormalWorkKind::gameplayArrivalStopEvent,1,17}},horizon).timing.phase;
    assert(std::any_of(phase.callback->voices.directTimers.begin(),phase.callback->voices.directTimers.end(),[](const auto timer){return timer>0;}));
    phase=bindSuccessAudioResource(phase,game.soundSequences[0]);
    phase=renderSuccessAudio(clock,phase,SuccessAudioChild::entryStop).timing.phase;
    assert(phase.callback->domain==SoundBlasterCallbackDomain::successNoHandle);
    //Force the actual retained IRQ scheduler into the middle of17E5 partial writes.
    const auto local=5000-phase.remainingCycles-phase.deferredCycles;
    phase.remainingCycles=80; phase.deferredCycles=5000-local-80;
    phase.devices->irq0Requested=true; phase.irqCheckRequested=true; phase.timer->callbackCount=1;
    const auto activation=successAudioTiming(phase,SuccessAudioChild::activation);
    assert(activation.callbacks.size()==1);
    assert(activation.callbacks[0].advance.kind==SoundBlasterCallbackKind::successNoHandle);
    const auto origin=synth.driverState().normalClockCycles;
    const auto before=callbackFields(synth.callbackWorkState());
    const auto writeCount=synth.registerWrites().size();
    auto invalid=activation;
    auto receipt=std::find_if(invalid.soundBlasterEffects.begin(),invalid.soundBlasterEffects.end(),
        [](const auto& effect){return effect.kind==SoundBlasterForegroundKind::successSequencePublish;});
    assert(receipt!=invalid.soundBlasterEffects.end()); receipt->value=2;
    rejects([&]{synth.renderClocked(invalid,origin);});
    assert(callbackFields(synth.callbackWorkState())==before&&synth.driverState().normalClockCycles==origin);
    assert(synth.registerWrites().size()==writeCount&&!synth.driverState().sequenceAllocations[3]);
    //Clock owns timing and replay together; neither preparation nor timers mutate untimed.
    phase=renderSuccessAudio(clock,phase,SuccessAudioChild::activation).timing.phase;
    assert(callbackFields(synth.callbackWorkState())==callbackFields(*phase.callback));
    phase=renderSuccessAudio(clock,phase,SuccessAudioChild::countdownStore).timing.phase;
    assert(phase.timer->globalCount==992);
    phase=clock.renderWork(phase,{{NormalWorkKind::instructions,100000}},horizon).timing.phase;
    assert(callbackFields(synth.callbackWorkState())==callbackFields(*phase.callback));
    phase=renderSuccessAudio(clock,phase,SuccessAudioChild::returnStop).timing.phase;
    assert(phase.successAudio->stage==SuccessAudioStage::returned);
    assert(callbackFields(synth.callbackWorkState())==callbackFields(*phase.callback));
    rejects([&]{renderSuccessAudio(clock,phase,SuccessAudioChild::activation);});
    bool sampleClockRejected=false;
    try { synth.render(1); } catch (const std::logic_error&) { sampleClockRejected=true; }
    assert(sampleClockRejected); //Legacy sample clock remains rejected.
}
} //namespace

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
    componentControls(); liveReplay();
    return 0;
}
