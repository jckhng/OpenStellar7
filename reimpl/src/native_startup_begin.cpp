#include "stellar7/native_startup.hpp"
#include "stellar7/menu_widget_work.hpp"
#include "stellar7/ui_pointer_work.hpp"
#include "normal_work_packets.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::int64_t beginWorkHorizon = 120000;
constexpr std::int64_t beginBlackoutHorizon = 2 * beginWorkHorizon;
constexpr std::size_t maximumPacketBudget = 10000;
constexpr std::uint64_t maximumWaitPolls = 100000000;
constexpr std::size_t difficultyOption = 4;
constexpr CockpitCopyRegion pointerRectangle{163,69,16,21};
constexpr Viewport display{0,319,0,199};
enum class Execution { qualify, render };

std::vector<std::vector<NormalWorkOperation>> workPackets(
    const std::vector<NormalWorkOperation>& operations, const std::size_t budget) {
    return normalWorkPackets(operations,budget);
}

std::vector<std::uint8_t> pixels(const IndexedPages& pages,const std::uint16_t page) {
    return pages.snapshot(page);
}
}

const NativeStartupBeginRun* NativeStartupSequence::recoveredBegin() const noexcept {
    return audio_ && audio_->begin_ ? &*audio_->begin_ : nullptr;
}

const NativeStartupMenuInputRun* NativeStartupSequence::recoveredMenuInput() const noexcept {
    return audio_ && audio_->menuInput_ ? &*audio_->menuInput_ : nullptr;
}

void NativeStartupSequence::beginGame(const NativeMenuBeginProfile profile,const std::size_t workPacketBudget) {
    if (profile != NativeMenuBeginProfile::enterPressRelease) {
        throw std::invalid_argument("recovered Begin requires the native Enter press/release profile");
    }
    executeRecoveredMenu(std::nullopt,workPacketBudget);
}

NativeMenuAction NativeStartupSequence::pollRecoveredMenu(const NativeMenuInput input,const std::size_t workPacketBudget,
    const NativeDeliverySink& sink) {
    executeRecoveredMenu(input,workPacketBudget,sink);
    return audio_->menuInput_->activated ? NativeMenuAction::begin : NativeMenuAction::none;
}

void NativeStartupSequence::executeRecoveredMenu(const std::optional<NativeMenuInput>& input,
    const std::size_t workPacketBudget, const NativeDeliverySink& sink, const NativeMenuBeginProfile profile,
    const MenuAttractProvider attractProvider) {
    if (phase_ != NativeStartupPhase::awaitingMenuDecision || !audio_ || !audio_->workPhase_
        || !audio_->workPhase_->sequenceSelection || !audio_->menuEntry_ || !audio_->resources_
        || !audio_->fonts_ || !audio_->menuResources_) {
        throw std::logic_error("recovered Begin requires the retained neutral menu return");
    }
    if((attractProvider!=MenuAttractProvider::unsupported&&attractProvider!=MenuAttractProvider::unavailable)
        ||(attractProvider==MenuAttractProvider::unavailable&&(!audio_->playablePreferences_||!input))) {
        throw std::invalid_argument("unavailable attract provider requires an explicit native playable input poll");
    }
    const bool pointerProfile=profile==NativeMenuBeginProfile::portablePointer;
    const bool nativeWidget=profile==NativeMenuBeginProfile::playableWidget;
    const bool widgetProfile=pointerProfile||nativeWidget;
    if(nativeWidget && (!audio_->playablePreferences_
        || !hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::automaticAttract)
        || input || menu_.selection()!=0 || pressedMenu_!=std::optional<std::size_t>{0})) {
        throw std::invalid_argument("native widget Begin requires its actual automatic-profile widget0 release");
    }
    if(pointerProfile&&(!audio_->playablePreferences_
        ||audio_->pointerCapability_!=NativePointerCapability::portablePointer)) {
        throw std::invalid_argument("native pointer Begin requires its actual host capability");
    }
    if (workPacketBudget == 0
        || workPacketBudget > maximumPacketBudget || (!widgetProfile&&menu_.selection() != 0)
        || (audio_->menuResources_->configuration.options[difficultyOption] != 0 && !audio_->playablePreferences_)
        || game_.artGroups.empty()) {
        throw std::invalid_argument("recovered Begin admits only default-difficulty native Enter press/release");
    }
    if ((input && input->pointer) || (input && (input->up || input->down))
        || (input && (input->left || input->right || input->cancel))
        || (!widgetProfile&&!input && audio_->workPhase_->sequenceSelection->recoveredMenuInput)
        || (input && audio_->workPhase_->sequenceSelection->beginDecisions != 0)
        || (pressedMenu_ && *pressedMenu_ != 0)) {
        throw std::invalid_argument("recovered menu input requires first-widget Enter levels without mixed providers");
    }
    const auto inputDecision=attractProvider==MenuAttractProvider::unavailable
        ? NormalWorkKind::playableMenuNoAttractDecision : NormalWorkKind::startupMenuInputDecision;
    const bool wasPressed = pressedMenu_.has_value();
    const bool activated = widgetProfile || (input && wasPressed && !input->confirm);
    const bool stream = sink && (!input || activated);
    const auto& entry = *audio_->menuEntry_;
    const auto& scratch = audio_->resources_->scratchBuffer;
    if (entry.pointer.x != pointerRectangle.x || entry.pointer.y != pointerRectangle.y
        || entry.oldRectangle.x != pointerRectangle.x || entry.oldRectangle.y != pointerRectangle.y
        || entry.oldRectangle.width != pointerRectangle.width || entry.oldRectangle.height != pointerRectangle.height
        || entry.drawing.widgetCount != MenuModel::itemCount || scratch.size() != 500) {
        throw std::invalid_argument("recovered Begin requires the retained first widget and pointer rectangle");
    }
    const auto word = [&](const std::size_t offset) {
        return static_cast<std::int16_t>(scratch[offset] | (std::uint16_t{scratch[offset+1]} << 8));
    };
    if (pointerRectangle.x < word(0) || pointerRectangle.y < word(2)
        || pointerRectangle.x > static_cast<std::int16_t>(word(0)+word(4))
        || pointerRectangle.y > static_cast<std::int16_t>(word(2)+word(6))) {
        throw std::invalid_argument("native Enter does not hit the retained first widget");
    }
    const auto front = menuPageSegment(NativeMenuPage::front);
    const auto back = menuPageSegment(NativeMenuPage::back);
    const auto pressed = (!input || (input->confirm && !wasPressed))
        ? std::optional{menuWidgetOperations(game_.artGroups[0],audio_->fonts_->firstFont,
            scratch,0,MenuWidgetState::pressed,back)} : std::nullopt;
    const auto released = (!input || activated)
        ? std::optional{menuWidgetOperations(game_.artGroups[0],audio_->fonts_->firstFont,
            scratch,0,MenuWidgetState::released,back)} : std::nullopt;
    const auto widgetCopy = (pressed || released)
        ? std::optional{menuWidgetCopyOperations(scratch,0,back,front)} : std::nullopt;
    const auto pointer = uiBitmapOperations(game_.artGroups[0][0],{pointerRectangle.x,pointerRectangle.y},display,back);
    NativeStartupBeginRun run{};
    run.inputProfile=profile;
    if (audio_->playablePreferences_) { run.difficulty = campaignInitialCharges(audio_->playableDifficulty()); }
    run.playableAudio = audio_->workPhase_->playableStartup;
    const auto execute = [&](const Execution execution) {
        const bool render = execution == Execution::render;
        auto phase = *audio_->workPhase_;
        if(attractProvider==MenuAttractProvider::unavailable) {phase.playableMenuNoAttract=true;}
        auto palette = entry.palette;
        auto pages = pages_;
        auto transfer = audio_->resources_->transferBuffer;
        const char* stage = "menu press";
        const auto append = [&](ClockedPresentationWork work, const IndexedScreen* frame = nullptr) {
            phase = work.timing.phase;
            run.guestPolls += work.timing.countdownPolls;
            run.samples.insert(run.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
            if (stream) {
                NativeSessionDelivery delivery;
                delivery.firstSample = work.audio.firstSample;
                delivery.samples = work.audio.samples;
                if (frame) { delivery.presentations.push_back({delivery.firstSample+delivery.samples.size(),*frame}); }
                sink(delivery);
            }
            run.work.push_back(std::move(work));
        };
        std::vector<NormalWorkOperation> pending;
        const auto flush = [&]() {
            for (const auto& packet : workPackets(pending,workPacketBudget)) {
                try {
                    if (render) { append(audio_->clock_.renderWork(phase,packet,beginWorkHorizon)); }
                    else { phase = normalWorkTiming(phase,packet,beginWorkHorizon).phase; }
                } catch (const std::invalid_argument& error) {
                    throw std::invalid_argument(std::string("Begin ")+stage+": "+error.what());
                }
            }
            pending.clear();
        };
        const auto normal = [&](const std::vector<NormalWorkOperation>& operations) {
            pending.insert(pending.end(),operations.begin(),operations.end());
            if (std::any_of(operations.begin(),operations.end(),[](const auto& operation) {
                return operation.kind != NormalWorkKind::instructions
                    && operation.kind != NormalWorkKind::string
                    && operation.kind != NormalWorkKind::repeatString
                    && operation.kind != NormalWorkKind::port8Write
                    && operation.kind != NormalWorkKind::port16Write;
            })) { flush(); }
        };
        const auto plain = [&](const std::uint64_t count) { normal({{NormalWorkKind::instructions,count}}); };
        const auto pointerCopy = [&]() {
            plain(1); //22A1 JMPF; outer CALLF is in82EA.
            normal(planarCopyOperations(pointerRectangle.x,pointerRectangle.width,pointerRectangle.height));
            pages.apply(std::vector<IndexedPageEffect>{RadarPageCopy{back,front,{160,69},24,21}});
        };
        const auto refreshPointer = [&]() {
            plain(20); //82EA..8324, no bottom clip.
            auto saved = uiPointerSaveOperations(pages,pointerRectangle,back,transfer);
            normal(saved.operations);
            transfer = std::move(saved.transfer);
            plain(7); //8329..833B pointer bitmap CALLF.
            normal(pointer.operations);
            pages.apply(pointer.effects);
            plain(8); //8340..8356 previous rectangle CALLF.
            pointerCopy();
            plain(6); //835B..8368 current rectangle CALLF, deliberately repeated.
            pointerCopy();
            plain(10); //836D..8388 restore CALLF.
            const auto restored = uiPointerRestoreOperations(pointerRectangle,back,transfer);
            normal(restored.operations);
            pages.apply(restored.effects);
            plain(12); //838D..83B6, oldY69+height21 does not take overflow arm.
        };
        enum class Poll { neutral, press, held, release };
        const auto poll = [&](const std::size_t index, const Poll state) {
            const bool down = state == Poll::press || state == Poll::held;
            plain(12); //5936..5951, explicit native03EE and held Enter query.
            plain(down ? 7 : 14); //Enter down skips Space; release queries both.
            plain(53); //8BD0 eight released direction queries; provider bodies untimed.
            plain(5+6+10); //597F..59C3,ordinary64A0,59C6..5A32 graphical callback.
            if (state == Poll::neutral) {
                plain(13); //5AC8 no button/no active widget ->5B58 RET.
            } else if (state == Poll::held) {
                plain(20+24); //5AC8 same active hit ->5B58; complete5A7C24.
            } else {
                if (state == Poll::press) {
                    plain(9+24+9); //5AC8 press, first-hit5A7C,583A CALL.
                } else {
                    plain(17); //5AC8 release through5B31 CALL583A.
                }
                const auto& widget = state == Poll::press ? pressed : released;
                normal(widget->operations);
                pages.apply(widget->effects);
                plain(5); //Callback cleanup/arguments through58FA CALL.
                normal(widgetCopy->operations);
                pages.apply(widgetCopy->effects);
                plain(state == Poll::press ? 5 : 8);
            }
            plain(2); //5A36 result;5A38 CALL82EA.
            refreshPointer();
            plain(state == Poll::press ? 16 : 10); //Only first native make dequeues1C0D; other polls03EE=0.
            flush();
            if (render) {
                run.polls[index] = {down,static_cast<std::uint16_t>(down ? 1 : 0),
                    static_cast<std::uint16_t>(down ? 1 : 0),static_cast<std::uint16_t>(state == Poll::release ? 1 : 0),
                    run.firstSample+run.samples.size(),pixels(pages,front),pixels(pages,back),transfer};
                if (stream) {
                    NativeSessionDelivery delivery;
                    delivery.firstSample = run.polls[index].sampleEnd;
                    delivery.presentations.push_back({delivery.firstSample,{run.polls[index].front,
                        expandVgaDacPalette(std::vector<std::uint8_t>(palette.dac.colors.begin(),palette.dac.colors.end()))}});
                    sink(delivery);
                }
            }
        };
        if (widgetProfile) {
            //The native source5AC8 provider already completed the actual widget0 release.
            //These fields describe its returned5936 result; no keyboard query/work receipt is emitted.
            phase.sequenceSelection->recoveredMenuInput=true;
            phase.sequenceSelection->recoveredMenuResult=1;
            phase.sequenceSelection->recoveredMenuActivated=true;
        } else if (input) {
            normal({{inputDecision,1,0}});
            const auto action = input->confirm ? (wasPressed ? Poll::held : Poll::press)
                : (wasPressed ? Poll::release : Poll::neutral);
            poll(0,action);
            //AX from the completed5936 RET; its instructions are already owned.
            phase.sequenceSelection->recoveredMenuResult = activated ? 1 : 0;
            if (!activated) {
                return std::make_tuple(std::move(phase),std::move(palette),std::move(pages),std::move(transfer));
            }
            normal({{inputDecision,1,1}});
        } else {
            normal({{NormalWorkKind::startupMenuDecision,1,0}});
            poll(0,Poll::press);
            stage = "menu release";
            normal({{NormalWorkKind::startupMenuDecision,1,0}});
            poll(1,Poll::release);
            normal({{NormalWorkKind::startupMenuDecision,1,1}});
        }
        stage = "first fade request";
        plain(4); //1656..165E first0478 operation0C CALLF.
        normal({{phase.playableStartup ? NormalWorkKind::playableStartupBeginFadeRequest : NormalWorkKind::startupBeginFadeRequest,1,1}});
        plain(3); //1663 cleanup,local return0,166B CALL9D4C.
        flush();
        BlackoutWorkState surface{};
        surface.palette = palette;
        surface.surface.clipping = BlackoutClip::enabled;
        try {
            if (render) {
                const auto fadePixels = pixels(pages,front);
                auto work = audio_->clock_.renderBlackout(phase,surface,front,pages,beginBlackoutHorizon,storyRoles_?BlackoutPageLayout::recoveredStartup:BlackoutPageLayout::compatibility);
                for (auto& step : work.steps) {
                    append(std::move(step.before));
                    run.paletteUploads.push_back(step.palette.state);
                    const IndexedScreen faded{fadePixels,expandVgaDacPalette(std::vector<std::uint8_t>(
                        step.palette.state.dac.colors.begin(),step.palette.state.dac.colors.end()))};
                    append(std::move(step.palette.work),&faded);
                }
                const IndexedScreen cleared{pixels(pages,front),expandVgaDacPalette(std::vector<std::uint8_t>(
                    work.state.palette.dac.colors.begin(),work.state.palette.dac.colors.end()))};
                append(std::move(work.clear),&cleared);
                palette = std::move(work.state.palette);
            } else {
                auto timing = blackoutClockTiming(phase,surface,front,beginBlackoutHorizon,storyRoles_?BlackoutPageLayout::recoveredStartup:BlackoutPageLayout::compatibility);
                phase = timing.clear.phase;
                palette = std::move(timing.state.palette);
            }
        } catch (const std::invalid_argument& error) {
            throw std::invalid_argument(std::string("Begin blackout: ")+error.what());
        }
        stage = "second fade request";
        plain(13+4); //166E..16AF return;014A..0152 second operation0C CALLF.
        normal({{phase.playableStartup ? NormalWorkKind::playableStartupBeginFadeRequest : NormalWorkKind::startupBeginFadeRequest,1,2}});
        plain(1); //0157 cleanup.
        normal({{NormalWorkKind::startupBeginCountdown,1,480}});
        plain(8+110+1+2); //0160..0175;8CEE/113A copy7;0178 CALL6638;6638 prologue.
        std::uint64_t polls = 0;
        stage = "countdown wait";
        while (!phase.sequenceSelection->beginCountdownReturned) {
            if (polls >= maximumWaitPolls) { throw std::invalid_argument("Begin wait exceeds source poll bound"); }
            normal({{NormalWorkKind::startupBeginWait,workPacketBudget}});
            polls += workPacketBudget;
        }
        plain(1); //017B CALLF51A6.
        stage = "sequence stop";
        normal({{phase.playableStartup && phase.playableMusicOff
            ? NormalWorkKind::playableStartupBeginEmptyReturn : NormalWorkKind::startupBeginStop,1}});
        return std::make_tuple(std::move(phase),std::move(palette),std::move(pages),std::move(transfer));
    };
    execute(Execution::qualify); //Qualify all fade/wait/stop callbacks before live mutation.
    run.firstSample = elapsedSamples();
    run.driverBefore = audio_->synth_.driverState();
    auto completed = execute(Execution::render);
    run.finalPhase = std::move(std::get<0>(completed));
    run.palette = std::move(std::get<1>(completed));
    pages_ = std::move(std::get<2>(completed));
    audio_->resources_->transferBuffer = std::move(std::get<3>(completed));
    run.driverAfter = audio_->synth_.driverState();
    frame_.pixels = menuPagePixels(NativeMenuPage::front);
    frame_.palette = expandVgaDacPalette(std::vector<std::uint8_t>(
        run.palette.dac.colors.begin(),run.palette.dac.colors.end()));
    audio_->workPhase_ = run.finalPhase;
    if (input) {
        NativeStartupMenuInputRun delivered;
        delivered.firstSample = run.firstSample;
        delivered.samples = run.samples;
        delivered.work = run.work;
        delivered.poll = run.polls[0];
        delivered.activated = activated;
        if (activated) {
            run.polls[1] = run.polls[0];
            if (audio_->menuInput_) { run.polls[0] = audio_->menuInput_->poll; }
        }
        audio_->menuInput_ = std::move(delivered);
        pressedMenu_ = input->confirm ? std::optional<std::size_t>{0} : std::nullopt;
        if (!activated) {
            phase_ = NativeStartupPhase::awaitingMenuDecision;
            return;
        }
    }
    difficulty_ = run.difficulty;
    audio_->begin_ = std::move(run);
    phase_ = NativeStartupPhase::awaitingGameplayLoader;
}

} // namespace stellar7
