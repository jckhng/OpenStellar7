#include "stellar7/retained_level2.hpp"

#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::uint8_t solidRasterStyle = 1;
constexpr std::size_t thirdWeaponSlot = 2;

bool released(const GameplayInput& input) {
    return !input.left && !input.right && !input.forward && !input.reverse &&
        !input.fire && !input.exit && !input.confirm;
}
}

RetainedLevel2Renderer::RetainedLevel2Renderer(RetainedLevel2Owners owners,
    const FrameCopyProfile profile, RetainedLevel2Observer& observer)
    : owners_(owners), profile_(profile), observer_(observer) {
    if (profile_ != FrameCopyProfile::modeXNoop ||
        owners_.model.phase() != GameplayPhase::level2 ||
        owners_.model.level2LoopTick() != owners_.controller.expectedLoopTick()) {
        throw std::invalid_argument("retained Level2 renderer requires an owned applied tail and Mode-X profile");
    }
}

void RetainedLevel2Renderer::require(const Hook hook) const {
    if (state_ != State::advancing || hook_ != hook) {
        throw std::invalid_argument("retained Level2 renderer hook out of order");
    }
}

void RetainedLevel2Renderer::advance(const GameplayInput& input) {
    if (state_ != State::ready) {
        throw std::invalid_argument("retained Level2 renderer is unavailable");
    }
    if (!released(input) || owners_.model.phase() != GameplayPhase::level2 ||
        owners_.model.level2LoopTick() != owners_.controller.expectedLoopTick() ||
        owners_.model.level2Player().weaponSlotTimers[thirdWeaponSlot] != 0) {
        throw std::invalid_argument("retained Level2 renderer requires released gameplay input and owned controller");
    }
    state_ = State::advancing;
    try {
        owners_.model.tickWithRenderer(input, *this);
        if (hook_ != Hook::prefix || owners_.model.phase() != GameplayPhase::level2 ||
            owners_.model.level2LoopTick() != owners_.controller.expectedLoopTick()) {
            throw std::invalid_argument("retained Level2 renderer did not complete its owned tail");
        }
        state_ = State::ready;
    } catch (...) {
        // Earlier stages may already have advanced canonical owners or observers.
        state_ = State::rejected;
        throw;
    }
}

std::optional<StationaryLevel2PrefixWork> RetainedLevel2Renderer::prepare(
    const StationaryLevel2PrefixInput& input) {
    require(Hook::prefix);
    observer_.prefixEntry(input);
    auto result = owners_.executor.prepare(input);
    observer_.prefix(result);
    owners_.pages.beginFrame();
    owners_.rasterStyle = solidRasterStyle;
    hook_ = Hook::render;
    return std::move(result.state);
}

void RetainedLevel2Renderer::render(Level2RenderState frame) {
    require(Hook::render);
    const auto plan = planLevel2Frame(owners_.assets, frame, owners_.model.level2LoopTick(),
        owners_.model.playerArmor(), owners_.horizon, owners_.settings, owners_.pages,
        owners_.radar, owners_.pixels);
    if (plan.radar.pageCopy) {
        throw std::invalid_argument("retained quiet Level2 renderer excludes front armor copy");
    }
    observer_.renderEntry(plan, frame);
    const auto body = owners_.executor.render(plan);
    observer_.body(plan, body);
    const auto display = owners_.executor.display(plan.pages);
    observer_.display(plan.pages, plan.pixels, display);
    const auto postcopy = owners_.executor.postcopy(profile_);
    observer_.postcopy(plan.pages, plan.pixels, postcopy);
    commitLevel2Frame(plan, frame, owners_.pages, owners_.radar, owners_.pixels);
    owners_.pages.flip();
    // Radar marker mode at1fdb does not overwrite world line style at1fdc.
    owners_.enemyFlag = plan.radar.enemyFlag;
    observer_.renderComplete(plan);
    hook_ = Hook::tail;
}

std::optional<FrameTailState> RetainedLevel2Renderer::finish(
    const FrameTailState& state, const FrameTailGuards& guards) {
    require(Hook::tail);
    if (state.loopTick != owners_.controller.expectedLoopTick()) {
        throw std::invalid_argument("retained Level2 tail/controller sequence mismatch");
    }
    auto ownedGuards = owners_.controller.augmentGuards(guards);
    if (ownedGuards.periodic) {
        ownedGuards.periodic->enemyFlag = owners_.enemyFlag;
    }
    observer_.tailEntry(state, ownedGuards);
    auto result = owners_.executor.finish(state, ownedGuards);
    observer_.tail(result);
    if (ownedGuards.sourceKeyboardEffects) {
        owners_.controller.completeTail(state.loopTick, result.state);
    } else {
        owners_.controller.consumeEmptyTail(state.loopTick);
    }
    hook_ = Hook::prefix;
    return std::move(result.state.state);
}

} // namespace stellar7
