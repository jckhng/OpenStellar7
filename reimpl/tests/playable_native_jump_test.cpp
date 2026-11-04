#include "playable_startup_resources.hpp"
#include "playable_campaign_comparison.hpp"
#include "stellar7/audio_direct_work.hpp"
#include "stellar7/input_movie.hpp"

#include <algorithm>
#include <iostream>

namespace {
using namespace stellar7;
using test::requireCampaignComparison;

std::unique_ptr<RecoveredFrontend> begin(const GameplayAssets& assets, const NativeStartupAssets& startup,
    const FrameCopyProfile copy, const std::size_t budget, std::vector<std::int16_t>& pcm) {
    auto frontend = std::make_unique<RecoveredFrontend>(assets,startup,copy,budget,
        std::shared_ptr<HighscoreStore>{},CampaignExecution::playable);
    unsigned polls = 0;
    for (unsigned call = 0; !frontend->session() && call < 10000; ++call) {
        NativeMenuInput input{};
        if (frontend->startup().phase() == NativeStartupPhase::awaitingMenuDecision) {
            input.confirm = polls++ == 1;
        }
        const auto before = frontend->elapsedSamples();
        const auto delivery = frontend->advanceStartup(input);
        requireCampaignComparison(delivery.firstSample == before
            && before + delivery.samples.size() == frontend->elapsedSamples(),"Jump startup lost PCM prefix");
        pcm.insert(pcm.end(),delivery.samples.begin(),delivery.samples.end());
    }
    requireCampaignComparison(frontend->session() != nullptr,"Jump startup did not reach actual Begin");
    return frontend;
}

template<class Call> void rejects(Call call) {
    bool rejected = false;
    try { call(); } catch (const std::invalid_argument&) { rejected = true; }
    requireCampaignComparison(rejected,"unsupported system action was admitted");
}

void run(const std::filesystem::path& root) {
    const auto assets = naturalRoute::assets(root);
    const auto startup = decodeNativeStartupAssets(naturalRoute::read(root / "STELART.RES"));
    const auto drivers = decodeTaggedPayloads(naturalRoute::read(root / "STELLAR.RES"),{'V','G','A',':'});
    requireCampaignComparison(drivers.size() == 1,"missing actual VGA driver");
    const auto copy = decodeFrameCopyProfile(drivers.front());
    test::CampaignBudgetComparison comparison;
    auto left = begin(assets,startup,copy,10000,comparison.leftPcm);
    auto right = begin(assets,startup,copy,257,comparison.rightPcm);
    requireCampaignComparison(comparison.leftPcm == comparison.rightPcm,"Jump startup changed whole PCM");
    const auto advance = [&](const GameplayInput& input, const test::CampaignComparisonStage stage) {
        const auto before = left->elapsedSamples();
        const auto a = left->advanceGameplay(input);
        const auto b = right->advanceGameplay(input);
        comparison.compare(*left->session(),*right->session(),a,b,stage);
        requireCampaignComparison(a.firstSample == before && before + a.samples.size() == left->elapsedSamples(),
            "Jump replaced its retained sample epoch");
        return a;
    };
    advance({},test::CampaignComparisonStage::boundary); //Normal first common-pool adoption.
    const auto* owner = left->session();
    for (int charge = 3; charge > 0; --charge) {
        GameplayInput input{};
        input.fire = true;
        input.systemAction = charge == 2 ? GameplaySystemAction::activateSelected : GameplaySystemAction::jump;
        const auto activation = advance(input,test::CampaignComparisonStage::boundary);
        const auto fire = std::find_if(activation.events.begin(),activation.events.end(),[](const auto& event) {
            return event.eventId == 14;
        });
        const auto jump = std::find_if(activation.events.begin(),activation.events.end(),[](const auto& event) {
            return event.eventId == gameplaySystemActivateEvent;
        });
        requireCampaignComparison(fire != activation.events.end() && jump != activation.events.end() && fire < jump,
            "simultaneous Space/J changed source fire/action order");
        requireCampaignComparison(owner->model().level1State().systemTimers()[2] == 220
            && owner->model().level1State().systemStatusLevels()[2] == -(charge - 1)
            && owner->model().level2Player().position.z == 0,"Jump activation lost canonical charge/velocity storage");
        for (int step = 1; step <= 45; ++step) {
            input = {};
            input.fire = step <= 9;
            input.systemAction = charge == 3 && step == 1 ? GameplaySystemAction::jump : GameplaySystemAction::none;
            const auto delivery = advance(input,step == 45 ? test::CampaignComparisonStage::boundary
                : test::CampaignComparisonStage::frame);
            const auto& model = owner->model();
            const auto expectedHeight = step == 45 ? 0 : 220 * step - 5 * step * (step - 1);
            requireCampaignComparison(model.level2Player().position.z == expectedHeight
                && model.level2Player().zVelocity == 0,"Jump diverged from signed 918E altitude");
            requireCampaignComparison(model.level1State().systemStatusLevels()[2]
                == (step == 45 ? charge - 1 : -(charge - 1)),"Jump consumed or refilled an extra charge");
            for (const auto role : {NativePageRole::back,NativePageRole::front}) {
                const auto page = owner->retainedPagePixels(role);
                const auto panelColor = step < 45 ? 176 : charge > 1 ? 137 : 146;
                const auto borderColor = step == 45 && charge == 1 ? 148 : 234;
                requireCampaignComparison(page && (*page)[153 * 320 + 37] == panelColor
                    && (*page)[151 * 320 + 35] == borderColor,
                    "native main/cockpit redraw lost the retained Jump panel or selection");
            }
            if (step == 9) {
                requireCampaignComparison(std::any_of(model.level2Entities().begin(),model.level2Entities().end(),
                    [](const auto& actor) { return actor.active() && actor.ownerIsPlayer
                        && actor.type == 23 && actor.position.z == 1620; }),"ordinary primary shot lost Jump altitude");
            }
            if (step == 45 && charge == 1) {
                requireCampaignComparison(model.level1State().selectedSystem() == 3
                    && std::any_of(delivery.events.begin(),delivery.events.end(),[](const auto& event) {
                        return event.eventId == gameplaySystemCycleEvent;
                    }),"last landing lost passive selection/cycle audio");
            }
        }
    }
    GameplayInput other{};
    other.systemAction = static_cast<GameplaySystemAction>(255);
    const auto samples = left->elapsedSamples(), ticks = owner->model().ticks();
    const auto before = std::make_unique<GameplayModel>(owner->model());
    rejects([&] { left->advanceGameplay(other); });
    requireCampaignComparison(left->session() == owner && left->elapsedSamples() == samples
        && owner->model().ticks() == ticks && owner->model().level1State().systemStatusLevels()
            == before->level1State().systemStatusLevels(),"unknown ability activation changed retained state");
    test::compareCampaignRng(owner->model().level2Rng(),before->level2Rng(),test::CampaignComparisonStage::boundary);
    rejects([&] { soundBlasterDirectAdvance(owner->synth().callbackWorkState(),gameplaySystemActivateDirectRecord,
        SoundBlasterDirectCaller::gameplaySystemActivate); });
    auto bad = gameplaySystemActivateDirectRecord;
    ++bad[1];
    rejects([&] { soundBlasterPlayableDirectAdvance(owner->synth().callbackWorkState(),bad,
        SoundBlasterDirectCaller::gameplaySystemActivate); });
    GameplayInputRecorder recorder;
    rejects([&] { recorder.observe(1,other); });
    requireCampaignComparison(recorder.events().empty(),"v1 movie silently dropped queued system input");
    std::cout << "Actual startup -> three native Jump flights: canonical charges/altitude, primary XYZ, ordered audio, pages and PCM twins PASS\n";
}
}

int main(int argc,char** argv) {
    if (argc != 2) { return 2; }
    try { run(argv[1]); return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
