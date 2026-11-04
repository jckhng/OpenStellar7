//Private focused test proposal. Uncompiled/unexecuted; argv1 is resource root.
#include "playable_startup_resources.hpp"
#include "playable_campaign_comparison.hpp"

#include <algorithm>
#include <iostream>
#include <tuple>

namespace {
using namespace stellar7;
using test::requireCampaignComparison;
constexpr std::size_t soundsOption = 3;

void configureSounds(GameplayAssets& assets, const std::int16_t value) {
    assets.cockpitSettings->options[soundsOption] = value;
    assets.highscoreTable->setConfiguration(*assets.cockpitSettings);
}

void sourceRejectsDisabled(const GameplayAssets& assets, const NativeStartupAssets& art) {
    NativeStartupSequence startup(assets,art);
    startup.start(NativeStartupExecution::recoveredContext);
    startup.initializeSettings(StartupCapabilityProfile::noMouseOrJoystick);
    startup.initializeResources(NativeStartupDeviceProfile::portableOplNoDspProbe);
    startup.initializePresentation();startup.initializeFonts();
    ClockedPresentationWork last{};
    while (startup.phase() == NativeStartupPhase::logoCountdownWork) {
        last = startup.pollLogoCountdown(10000);
    }
    requireCampaignComparison(startup.phase() == NativeStartupPhase::awaitingLogoFrames,
        "SOUNDS0 did not retain the actual logo countdown boundary");
    const auto sample = startup.elapsedSamples();
    bool rejected = false;
    try { startup.playLogoEvent(); }
    catch (const std::invalid_argument&) { rejected = true; }
    requireCampaignComparison(rejected && startup.elapsedSamples() == sample
        && startup.phase() == NativeStartupPhase::awaitingLogoFrames,
        "default source logo profile admitted disabled sound or mutated state");
    const auto expected = normalWorkTiming(last.timing.phase,{{NormalWorkKind::instructions,15}},120000);
    const auto suppressed = startup.playLogoEvent(StartupLogoEventProfile::playableSoundsDisabled);
    requireCampaignComparison(suppressed.timing.total.elapsedCycles == expected.total.elapsedCycles
        && suppressed.timing.callbacks.size() == expected.callbacks.size()
        && suppressed.timing.soundBlasterEffects.empty()
        && suppressed.timing.phase.sequenceSelection->logoEventPlayed
        && suppressed.timing.phase.sequenceSelection->logoEventSuppressed,
        "suppressed logo event did not execute its15 plain source dispatches");
    requireCampaignComparison(suppressed.timing.phase.callback->position == expected.phase.callback->position
        && suppressed.timing.phase.callback->wait == expected.phase.callback->wait
        && suppressed.timing.phase.callback->voices.directTimers == expected.phase.callback->voices.directTimers,
        "suppression changed direct state instead of retaining ordinary callbacks");
    startup.animateLogo();
    requireCampaignComparison(startup.recoveredLogo()->finalPhase.sequenceSelection->logoEventSuppressed
        && startup.recoveredLogo()->frames.size() == 17,
        "suppression proof did not survive all17 original logo frames");
}

struct StartupEvidence {
    BitmapCache menuCache;
    std::vector<std::uint8_t> menuSong;
    CockpitSettings initialSettings{};
    bool logoEventSuppressed=false;
    bool captured=false;
};

std::unique_ptr<RecoveredFrontend> begin(const GameplayAssets& assets, const NativeStartupAssets& art,
    const FrameCopyProfile copy, const std::size_t budget, std::vector<std::int16_t>& pcm, StartupEvidence& evidence) {
    auto frontend=std::make_unique<RecoveredFrontend>(assets,art,copy,budget,
        std::shared_ptr<HighscoreStore>{},CampaignExecution::playable);
    unsigned polls=0;
    for(unsigned call=0;!frontend->session() && call<10000;++call) {
        NativeMenuInput input{};
        if(frontend->startup().phase()==NativeStartupPhase::awaitingMenuDecision) { input.confirm=polls++==1; }
        if(frontend->startup().phase()==NativeStartupPhase::awaitingGameplayLoader) {
            const auto& startup=frontend->startup();
            const auto* resources=startup.recoveredMenuResources();
            const auto* song=startup.recoveredMenuSong();
            const auto* settings=startup.recoveredSettings();
            const auto* logo=startup.recoveredLogo();
            requireCampaignComparison(resources && song && settings && logo && logo->finalPhase.sequenceSelection,
                "startup lost immutable receipts before owner handoff");
            //Only test observations are copied. The real audio owner still moves
            //once into the session on the immediately following public call.
            evidence.menuCache=resources->cache;
            evidence.menuSong=song->song.payload;
            evidence.initialSettings=settings->settings.configuration;
            evidence.logoEventSuppressed=logo->finalPhase.sequenceSelection->logoEventSuppressed;
            evidence.captured=true;
        }
        const auto sample=frontend->elapsedSamples();
        const auto delivery=frontend->advanceStartup(input);
        requireCampaignComparison(delivery.firstSample==sample
            && sample+delivery.samples.size()==frontend->elapsedSamples(),"SOUNDS0 startup broke PCM ownership");
        pcm.insert(pcm.end(),delivery.samples.begin(),delivery.samples.end());
    }
    requireCampaignComparison(frontend->session()!=nullptr && evidence.captured,"SOUNDS0 cold Begin did not reach its actual loader/evidence boundary");
    return frontend;
}

void compareCache(const BitmapCache& a, const BitmapCache& b) {
    requireCampaignComparison(a.initialCursor==b.initialCursor && a.nextCursor==b.nextCursor
        && a.planes==b.planes && a.frames.size()==b.frames.size(),"event26 suppression changed native cache generation");
    for(std::size_t index=0;index<a.frames.size();++index) {
        const auto& x=a.frames[index];const auto& y=b.frames[index];
        requireCampaignComparison(std::tie(x.header.segment,x.header.offset,x.header.width,x.header.height)
            ==std::tie(y.header.segment,y.header.offset,y.header.width,y.header.height)
            &&x.bitmap.pixels==y.bitmap.pixels,"event26 suppression changed cached source headers/payload");
    }
}

void run(const std::filesystem::path& root) {
    auto disabled=naturalRoute::assets(root);
    configureSounds(disabled,0);
    const auto art=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto drivers=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    const auto copy=decodeFrameCopyProfile(drivers.at(0));
    sourceRejectsDisabled(disabled,art);
    test::CampaignBudgetComparison twins;
    StartupEvidence leftEvidence,rightEvidence,controlEvidence;
    auto left=begin(disabled,art,copy,10000,twins.leftPcm,leftEvidence);
    auto right=begin(disabled,art,copy,257,twins.rightPcm,rightEvidence);
    requireCampaignComparison(twins.leftPcm==twins.rightPcm,"SOUNDS0 startup PCM differs across packet cuts");
    auto enabled=disabled;configureSounds(enabled,1);
    std::vector<std::int16_t> enabledPcm;
    auto control=begin(enabled,art,copy,10000,enabledPcm,controlEvidence);
    compareCache(leftEvidence.menuCache,controlEvidence.menuCache);
    requireCampaignComparison(leftEvidence.menuSong==controlEvidence.menuSong,"suppression regenerated menu song payload");
    requireCampaignComparison(leftEvidence.initialSettings.options[soundsOption]==0
        &&leftEvidence.logoEventSuppressed,
        "cold disabled profile lost immutable initialization/suppression evidence");
    auto advance=[&](const GameplayInput& input) {
        const auto a=left->advanceGameplay(input),b=right->advanceGameplay(input);
        twins.compare(*left->session(),*right->session(),a,b,test::CampaignComparisonStage::boundary);
        return a;
    };
    advance({});
    const auto* owner=left->session();
    const auto* phase=owner->recoveredClockPhase();
    requireCampaignComparison(phase && phase->callback && phase->callback->program
        &&phase->callback->program->enemy20McbByte && phase->callback->program->bomb7CallerRow,
        "disabled source branch failed to retain actual MCB/Bomb owner generations");
    const auto firstPosition=owner->synth().driverState().sequencePosition;
    const auto firstWrites=owner->synth().registerWrites().size();
    unsigned fired=0;
    for(unsigned frame=0;frame<40;++frame) {
        GameplayInput input{};input.fire=true;
        if(frame==1) {input.systemAction=GameplaySystemAction::bomb;}
        const auto delivery=advance(input);
        fired+=static_cast<unsigned>(std::count_if(delivery.events.begin(),delivery.events.end(),
            [](const auto& event) {return event.eventId==14;}));
        const auto state=owner->synth().driverState();
        requireCampaignComparison(state.sequenceSelected && state.sequenceAllocated
            &&std::all_of(state.directTimers.begin(),state.directTimers.end(),[](const auto value) {return value==0;}),
            "disabled effects started a direct timer or stopped live music");
    }
    requireCampaignComparison(fired>0 && owner->synth().driverState().sequencePosition!=firstPosition
        &&owner->synth().registerWrites().size()>firstWrites,"fire semantic events or real MIDI/OPL callbacks did not continue");
    std::cout<<"SOUNDS0 source rejection, actual cold Begin/fire/Bomb and budget twins PASS\n";
}
} // namespace

int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {run(argv[1]);return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
