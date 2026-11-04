#include "playable_startup_resources.hpp"
#include "stellar7/native_game_rate.hpp"
#include "stellar7/story_scene.hpp"

#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace stellar7 {
struct NativeSessionTestAccess {
    static CockpitWork cockpit(const NativeGameplaySession& session) {
        return cockpitOperations({static_cast<std::int16_t>(session.model_.level2CockpitIndicatorX()),
            static_cast<std::int16_t>(session.model_.level2CockpitIndicatorVelocity()),
            session.renderSettings().viewSize,CockpitPageRelation::differing},session.cockpitBitmaps_);
    }
    static std::array<const void*,8> owners(const NativeGameplaySession& session) {
        return {session.audio_.get(),&session.synth_,&session.clock_,&session.pages_,
            &session.pixels_,&session.model_,&session.model_.level2Rng(),session.recoveredClockPhase()};
    }
};
}

namespace {
using namespace stellar7;
constexpr unsigned adjacentFrames = 80;
constexpr unsigned maximumStartupCalls = 100000;
constexpr std::int64_t workHorizon = 120000;
constexpr std::int64_t flipHorizon = 2 * workHorizon;
void require(const bool value,const char* message) {
    if (!value) { throw std::runtime_error(message); }
}
class MemoryStore final : public HighscoreStore {
public:
    HighscoreSaveResult saveAndReload(const HighscoreTable& table) override {
        return {HighscoreSaveStatus::saved,decodeHighscoreTable(encodeHighscoreTable(table))};
    }
};
struct Frame {
    std::uint16_t loop;
    std::uint64_t sample,cpu;
    IndexedScreen indexed;
    std::array<std::optional<std::vector<std::uint8_t>>,3> pages;
    StellarRng rng;
};
struct Run {
    std::vector<std::int16_t> pcm;
    std::vector<Frame> frames;
    unsigned checked = 0;
    unsigned damageChecked = 0;
};
bool ordinary(const GameplayPhase phase) {
    return phase == GameplayPhase::playable || phase == GameplayPhase::level1;
}
std::vector<std::uint8_t> armorStrip(const IndexedScreen& frame) {
    constexpr int firstColumn=112,lastColumn=196,firstRow=189,lastRow=195;
    std::vector<std::uint8_t> result;
    for(int y=firstRow;y<=lastRow;++y) {
        for(int x=firstColumn;x<=lastColumn;x+=2) {result.push_back(frame.pixels[y*320+x]);}
    }
    return result;
}
Run run(const GameplayAssets& game,const NativeStartupAssets& art,const FrameCopyProfile copy,const std::size_t budget) {
    auto frontend = std::make_unique<RecoveredFrontend>(game,art,copy,budget,std::make_shared<MemoryStore>(),
        CampaignExecution::playable,NativePointerCapability::portablePointer,NativeMenuFeatures::releaseMenus);
    Run result;
    std::uint64_t delivered = 0;
    std::optional<std::array<const void*,8>> owners;
    const auto accept = [&](const NativeSessionDelivery& delivery) {
        require(delivery.firstSample == delivered && delivery.entry.empty(),"cockpit route lost PCM start");
        delivered += delivery.samples.size();
        require(delivered == frontend->elapsedSamples(),"cockpit route lost PCM end");
        result.pcm.insert(result.pcm.end(),delivery.samples.begin(),delivery.samples.end());
        if (const auto* session = frontend->session()) {
            const auto actual = NativeSessionTestAccess::owners(*session);
            if (!owners) { owners = actual; }
            require(*owners == actual,"cockpit route replaced a retained owner");
        }
    };
    bool skippedSpeech = false;
    unsigned menuPoll = 0;
    for (unsigned call = 0; !frontend->session() && call < maximumStartupCalls; ++call) {
        NativeMenuInput input;
        if (!skippedSpeech && frontend->startup().storyVoice().phase == SampledVoicePhase::playing
            && frontend->startup().storyVoice().transferred != 0) {
            input.cancel = true;
            skippedSpeech = true;
        } else if (frontend->startup().phase() == NativeStartupPhase::awaitingMenuDecision) {
            input.confirm = menuPoll++ == 1;
        }
        accept(frontend->advanceStartup(input));
    }
    require(frontend->session() && menuPoll >= 2,"cockpit route missed actual menu Begin");
    NativeFramePacer pacer(defaultNativeGameRate);
    for (unsigned frame = 0; frame < adjacentFrames; ++frame) {
        const auto& session = *frontend->session();
        const auto beforeSample = frontend->elapsedSamples();
        const auto beforeCpu = session.synth().driverState().normalClockCycles;
        const auto beforeLoop = session.model().level2LoopTick();
        const auto beforeArmor=session.model().playerArmor();
        const auto beforeStrip=armorStrip(session.frame());
        auto phase = *session.recoveredClockPhase();
        phase.callback = session.synth().callbackWorkState();
        const auto cockpit = NativeSessionTestAccess::cockpit(session);
        const auto body = normalWorkTiming(phase,cockpit.operations,workHorizon);
        const auto pages = session.pageObservation();
        const auto flip = pageFlipClockTiming(body.phase,{PageFlipSynchronization::enabled,pages.back,pages.front,
            0x3d4,PageFlipLayout::recoveredStartup,PageFlipWaitDomain::retainedProgram},flipHorizon);
        const auto delivery = frontend->advanceGameplay({});
        accept(delivery);
        const auto& after = *frontend->session();
        if(after.model().playerArmor()<beforeArmor) {
            require(armorStrip(after.frame())!=beforeStrip,"natural hostile damage did not update visible armor strip");
            require(std::any_of(delivery.events.begin(),delivery.events.end(),[](const auto& event) {return event.eventId==6;}),
                "natural hostile damage omitted impact audio");
            ++result.damageChecked;
        }
        require(ordinary(after.model().phase()) && after.model().level2LoopTick() == beforeLoop + 1,
            "cockpit route advanced a frame incorrectly");
        require(after.model().level2CockpitIndicatorX() == cockpit.state.indicatorX
            && after.model().level2CockpitIndicatorVelocity() == cockpit.state.indicatorVelocity,
            "cockpit oscillator advanced more or less than once");
        //First adoption additionally executes its retained preparation/fade;
        //new direct-effect prefixes have their own audio ownership.
        if (frame != 0 && delivery.events.empty()) {
            require(after.synth().driverState().normalClockCycles - beforeCpu
                == static_cast<std::uint64_t>(body.total.elapsedCycles + flip.timing.total.elapsedCycles),
                "ordinary frame omitted or duplicated recovered cockpit work");
            ++result.checked;
        }
        accept(frontend->paceGameplay(pacer.deadline(beforeSample)));
        result.frames.push_back({after.model().level2LoopTick(),frontend->elapsedSamples(),
            after.synth().driverState().normalClockCycles,after.frame(),
            {after.retainedPagePixels(NativePageRole::cache),after.retainedPagePixels(NativePageRole::back),
                after.retainedPagePixels(NativePageRole::front)},after.model().level2Rng()});
    }
    require(result.checked >= 60,"cockpit route did not check enough adjacent ordinary frames");
    require(result.damageChecked!=0,"cockpit route did not exercise natural hostile damage");
    return result;
}
void compare(const Run& left,const Run& right) {
    require(left.pcm == right.pcm && left.frames.size() == adjacentFrames && right.frames.size() == adjacentFrames,
        "packet budget changed retained PCM or frame count");
    for (std::size_t index = 0; index < left.frames.size(); ++index) {
        const auto& a = left.frames[index];
        const auto& b = right.frames[index];
        require(a.loop == b.loop && a.sample == b.sample && a.cpu == b.cpu && a.pages == b.pages
            && a.indexed.pixels == b.indexed.pixels && a.indexed.palette.size() == b.indexed.palette.size(),
            "packet budget changed retained page/state/clock output");
        for (std::size_t color = 0; color < a.indexed.palette.size(); ++color) {
            const auto x = a.indexed.palette[color];
            const auto y = b.indexed.palette[color];
            require(x.red == y.red && x.green == y.green && x.blue == y.blue,"packet budget changed palette");
        }
        auto rngA = a.rng;
        auto rngB = b.rng;
        require(rngA.calls() == rngB.calls() && rngA.indexA() == rngB.indexA() && rngA.indexB() == rngB.indexB(),
            "packet budget changed RNG cursor");
        for (std::size_t word = 0; word < StellarRng::wordCount; ++word) {
            require(rngA.next() == rngB.next(),"packet budget changed RNG storage");
        }
    }
}
}

int main(int argc,char** argv) {
    try {
        require(argc == 2,"requires actual resources");
        const std::filesystem::path root = argv[1];
        auto game = naturalRoute::assets(root);
        require(game.cockpitSettings && game.cockpitSettings->viewSize > 70,"requires original full-detail profile");
        game.storyScenes = std::make_shared<const StorySceneAssets>(decodeStorySceneAssets(
            naturalRoute::read(root/"SCENEB1.RES"),naturalRoute::read(root/"SCENEA.RES"),
            {naturalRoute::read(root/"SCENEB2.RES"),naturalRoute::read(root/"SCENEB3.RES"),naturalRoute::read(root/"SCENEB5.RES")},
            naturalRoute::read(root/"SCENEC.RES"),naturalRoute::read(root/"VOICE.RES")));
        game.attractVoiceAvailable = true;
        const auto art = decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
        const auto drivers = decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
        const auto copy = decodeFrameCopyProfile(drivers.at(0));
        compare(run(game,art,copy,10000),run(game,art,copy,257));
        std::cout << "80 adjacent release-profile cockpit frames: budgets10000/257 pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
