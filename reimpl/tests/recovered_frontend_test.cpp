#include "stellar7/recovered_frontend.hpp"

#include <cassert>
#include <memory>
#include <stdexcept>

namespace {
template<class Error, class Function>
void rejects(Function function) {
    bool rejected = false;
    try { function(); }
    catch (const Error&) { rejected = true; }
    assert(rejected);
}
}

int main() {
    using namespace stellar7;
    GameplayAssets assets;
    NativeStartupAssets resources;
    for (const auto profile : {FrameCopyProfile::unknown, static_cast<FrameCopyProfile>(255)}) {
        rejects<std::invalid_argument>([&]() { RecoveredFrontend frontend(assets, resources, profile); });
    }
    for (const std::size_t budget : {0U, 10001U}) {
        rejects<std::invalid_argument>([&]() {
            RecoveredFrontend frontend(assets, resources, FrameCopyProfile::modeXNoop, budget);
        });
    }
    assets.uiPalette.resize(256);
    resources.logoScreen = {std::vector<std::uint8_t>(320 * 200), std::vector<Rgb>(256)};
    resources.menuScreen = resources.logoScreen;
    resources.logoFrames.resize(8, {1, 1, {0}});
    auto frontend = std::make_unique<RecoveredFrontend>(assets, resources, FrameCopyProfile::modeXNoop);
    assert(frontend->elapsedSamples() == 0 && frontend->session() == nullptr);
    assert(frontend->execution() == CampaignExecution::sourceWork);
    rejects<std::invalid_argument>([&]() {
        RecoveredFrontend invalid(assets, resources, FrameCopyProfile::modeXNoop, 10000, {},
            static_cast<CampaignExecution>(255));
    });
    auto playable = std::make_unique<RecoveredFrontend>(assets, resources, FrameCopyProfile::modeXNoop, 10000,
        std::shared_ptr<HighscoreStore>{}, CampaignExecution::playable);
    assert(playable->execution() == CampaignExecution::playable);
    rejects<std::logic_error>([&]() { playable->advanceGameplay({}); });
    assert(frontend->startup().phase() == NativeStartupPhase::idle);
    rejects<std::logic_error>([&]() { frontend->advanceGameplay({}); });
    rejects<std::invalid_argument>([&]() { frontend->advanceStartup({false, false, true}); });
    assert(frontend->elapsedSamples() == 0 && frontend->startup().recoveredContext() == nullptr);
    assert(frontend->startup().phase() == NativeStartupPhase::idle);
}
