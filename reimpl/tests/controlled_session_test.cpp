#include "stellar7/controlled_session.hpp"

#include <cassert>
#include <functional>
#include <stdexcept>
#include <string>
#include <type_traits>

using namespace stellar7;

static_assert(!std::is_copy_constructible_v<ControlledLevel2Session>);
static_assert(!std::is_copy_assignable_v<ControlledLevel2Session>);
static_assert(std::is_nothrow_move_constructible_v<ControlledLevel2Session>);
static_assert(std::is_nothrow_move_assignable_v<ControlledLevel2Session>);

namespace {
struct AdmissionObserver final : ControlledSessionObserver {
    unsigned startupCalls = 0;
    void startup(const ControlledSessionView&) override { ++startupCalls; }
};

ControlledBootstrapRequest requestShape() {
    ControlledBootstrapRequest request{};
    request.delivery = ControlledInputDelivery::beforeTick;
    request.events = {{1, true, {ControlledInputKey::forward}},
        {2, false, {ControlledInputKey::forward}}};
    request.horizon = {24, 145, 0, 71, 256, 16,
        HorizonBackground::solid, HorizonOverlay::disabled};
    request.copyProfile = FrameCopyProfile::modeXNoop;
    request.horizonCycles = 1000000;
    // Synthetic addressing shape only; never execute it as an admitted phase.
    auto& phase = request.returnedRaw4Phase;
    phase.bios.emplace();
    phase.devices.emplace();
    phase.devices->frame.emplace();
    phase.linesTotal = QuietScanout::height;
    phase.frameStart = AudioClockFrameStart::activeUnlockedVgaSurface;
    phase.vgaInterrupt = AudioClockVgaInterrupt::fixedVgaNoDemand;
    phase.devices->memoryMask = 0x1fffff;
    return request;
}

void rejectedRequest(
    const std::function<void(ControlledBootstrapRequest&)>& mutate,
    const std::string& requiredAdmissionText
) {
    AdmissionObserver observer;
    auto request = requestShape();
    mutate(request);
    bool rejected = false;
    try {
        ControlledLevel2Session session(GameplayAssets{}, std::move(request), &observer);
    } catch (const std::invalid_argument& error) {
        rejected = std::string(error.what()).find(requiredAdmissionText) != std::string::npos;
    }
    assert(rejected);
    assert(observer.startupCalls == 0);
}
}

// Synthetic admission controls; no source clock or bootstrap execution.
void testControlledSessionAdmission() {
    rejectedRequest([](auto& request) {
        request.arrivalInput = ControlledArrivalInput::hostForward;
        request.returnedRaw4Phase.keyboard.emplace();
    }, "keyboard source");
    rejectedRequest([](auto& request) { request.events.clear(); }, "movie");
    rejectedRequest([](auto& request) {
        request.arrivalInput = static_cast<ControlledArrivalInput>(-1);
    }, "resident input");
    rejectedRequest([](auto& request) { request.events[0].tick = 0; }, "movie");
    rejectedRequest([](auto& request) {
        request.events[0].tick = 2;
        request.events[1].tick = 1;
    }, "ordered nonempty movie events");
    rejectedRequest([](auto& request) {
        request.events[0].keys.push_back(ControlledInputKey::forward);
    }, "key state is inconsistent");
    rejectedRequest([](auto& request) {
        request.events[0].keys.push_back(ControlledInputKey::reverse);
    }, "opposing directions");
    rejectedRequest([](auto& request) { request.events.pop_back(); }, "release all keys");
    rejectedRequest([](auto& request) { request.events[0].keys.clear(); }, "movie");
    rejectedRequest([](auto& request) {
        request.events[0].keys = {static_cast<ControlledInputKey>(-1)};
    }, "key");
    rejectedRequest([](auto& request) {
        request.delivery = static_cast<ControlledInputDelivery>(-1);
    }, "before-tick movie");
    rejectedRequest([](auto& request) { request.copyProfile = FrameCopyProfile::unknown; }, "profile");
    rejectedRequest([](auto& request) { request.horizon.cameraYaw = 1; }, "horizon");
}

void testControlledPaletteAdmission() {
    for (const std::size_t length : {767U, 769U, 768U}) {
        GameplayAssets assets{};
        assets.cockpitDacPalette.assign(length, 0);
        if (length == 768) {
            assets.cockpitDacPalette[0] = 64;
        }
        AdmissionObserver observer;
        bool rejected = false;
        try {
            ControlledLevel2Session session(std::move(assets), requestShape(), &observer);
        } catch (const std::invalid_argument& error) {
            rejected = std::string(error.what()).find("palette") != std::string::npos;
        }
        assert(rejected);
        assert(observer.startupCalls == 0);
    }
}

int main() {
    testControlledSessionAdmission();
    testControlledPaletteAdmission();
}
