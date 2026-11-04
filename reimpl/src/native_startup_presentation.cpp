#include "stellar7/native_startup.hpp"
#include "playable_startup_provider.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::int64_t presentationWorkHorizon = 120000;
constexpr std::uint64_t driverWrapperPrefix = 11;
constexpr std::uint64_t driverDispatchPrefix = 12;
constexpr std::uint64_t driverDispatchSuffix = 9;
constexpr std::uint64_t driverWrapperSuffix = 5;
constexpr std::uint64_t selectionWrapperPrefix = 19;
constexpr std::uint64_t selectionBeforePublication = 3;
constexpr std::uint64_t selectionAfterPublication = 5;
constexpr std::size_t screenWidth = 320;
constexpr std::size_t screenHeight = 200;

std::vector<NormalWorkOperation> driverCall(const DriverSequenceWork& body) {
    std::vector<NormalWorkOperation> result{{NormalWorkKind::instructions,
        driverWrapperPrefix + driverDispatchPrefix}};
    result.insert(result.end(), body.operations.begin(), body.operations.end());
    result.push_back({NormalWorkKind::instructions, driverDispatchSuffix + driverWrapperSuffix});
    return result;
}

template<class Work, class Palette, class Reset, class Prepare, class Provider>
SoundBlasterClockPhase executePresentation(NativeStartupPresentationRun& run, SoundBlasterClockPhase phase,
    Work renderWork, Palette renderPalette, Reset reset, Prepare prepare, Provider provider) {
    const auto work = [&](const std::vector<NormalWorkOperation>& operations) {
        if (operations.empty()) {
            return;
        }
        auto completed = renderWork(phase, operations);
        phase = completed.timing.phase;
        run.samples.insert(run.samples.end(), completed.audio.samples.begin(), completed.audio.samples.end());
        run.work.push_back(std::move(completed));
    };
    for (const auto& segment : run.presentation.segments) {
        if (run.audioProfile == StartupMusicProfile::playableUnpublished
            && segment.kind == StartupPresentationKind::countdown) {
            work({{NormalWorkKind::playableStartupLogoCountdown,1,segment.value}});
        } else {
            work(segment.operations);
        }
        switch (segment.kind) {
        case StartupPresentationKind::directReset:
            work(driverCall(run.directReset));
            // No-handle IRQ reads only the unchanged zero direct timers. These
            // source stores have no observer before return/current publication.
            phase.callback = reset(*phase.callback, run.directReset);
            break;
        case StartupPresentationKind::loadSequences:
            // Whole0086 is a native FILE/descriptor provider, with no selection.
            break;
        case StartupPresentationKind::retainLoadedHead:
            run.loadedHead = 0;
            break;
        case StartupPresentationKind::provider:
            run.providers.push_back(segment.provider);
            if (segment.provider == StartupPresentationProvider::selectPalette) {
                run.palette = completedFullPaletteUpload(run.palette, {0, 256, 0, 63});
            }
            provider(segment);
            break;
        case StartupPresentationKind::copy:
            run.drawPage = segment.page;
            provider(segment);
            break;
        case StartupPresentationKind::palette: {
            auto completed = renderPalette(phase, run.palette, segment.request);
            phase = completed.work.timing.phase;
            run.palette = completed.state;
            run.samples.insert(run.samples.end(), completed.work.audio.samples.begin(), completed.work.audio.samples.end());
            run.palettes.push_back(std::move(completed));
            break;
        }
        case StartupPresentationKind::activateSequence: {
            if (!run.loadedHead || *run.loadedHead != 0 || run.selectedHead) {
                throw std::invalid_argument("logo activation requires the loaded, unselected head");
            }
            run.songs[0].repeat = 0xffff; //030C, distinct from0325 current publication.
            work({{NormalWorkKind::instructions, selectionWrapperPrefix}});
            work(driverCall(run.sequenceInitialization));
            auto prepared = prepare(*phase.callback, run.songs[0].payload, run.sequenceInitialization);
            prepared.program = run.program;
            phase.callback = run.audioProfile == StartupMusicProfile::playableUnpublished
                ? playableStartupUnpublishedCallback(prepared)
                : applyDriverSequenceCallbackState(*phase.callback, run.sequenceInitialization);
            phase.sequenceSelection = AudioClockSequenceSelectionPhase{std::move(prepared)};
            if (run.audioProfile == StartupMusicProfile::playableUnpublished) {
                //Native publication policy retains the real loaded allocation without a0325 receipt.
                work({{NormalWorkKind::instructions,selectionBeforePublication + selectionAfterPublication}});
                break;
            }
            work({{NormalWorkKind::instructions, selectionBeforePublication},
                {NormalWorkKind::soundBlasterSequencePublish, 1},
                {NormalWorkKind::instructions, selectionAfterPublication}});
            run.selectedHead = 0;
            break;
        }
        default:
            break;
        }
    }
    return phase;
}
}

const NativeStartupPresentationRun* NativeStartupSequence::recoveredPresentation() const noexcept {
    return audio_ && audio_->presentation_ ? &*audio_->presentation_ : nullptr;
}

void NativeStartupSequence::initializePresentation() {
    if (phase_ != NativeStartupPhase::awaitingPresentationResources || !audio_
        || !audio_->resources_ || !audio_->workPhase_) {
        throw std::logic_error("presentation initialization requires the retained resource/audio return");
    }
    const auto& resourceState = audio_->resources_->resources.state;
    if (!resourceState.graphicsPagesSelected || !resourceState.callbackRegistered
        || resourceState.paletteCache != StartupResourceReference::artPalette
        || resourceState.graphics != StartupResourceReference::startupGraphics
        || resources_.logoScreen.pixels.size() != screenWidth * screenHeight
        || resources_.logoScreen.palette.size() != vgaPaletteColors
        || game_.presentationMusicSequence.size() < 36
        || game_.presentationMusicSequence[0] != 0x84 || game_.presentationMusicSequence[1] != 0
        || game_.presentationMusicSequence[2] != 0) {
        throw std::invalid_argument("unsupported native presentation resources");
    }
    NativeStartupPresentationRun run{};
    run.audioProfile = audio_->startupMusicProfile_;
    run.presentation = startupPresentationOperations();
    run.directReset = soundBlasterDirectResetOperations();
    run.sequenceInitialization = soundBlasterSequenceOperations(game_.logoMusicSequence);
    run.program.instrumentBank = audio_->resources_->resources.driverInitialization.state.bank;
    run.program.volumeTable = game_.soundBlasterVolumeTable;
    run.program.operators = audio_->resources_->resources.driverInitialization.state.operators;
    run.program.waveformMask = audio_->resources_->resources.driverInitialization.state.waveformMask;
    run.program.loopMarker = run.sequenceInitialization.state.repeatPosition.value_or(0);
    for (std::size_t channel = 0; channel < run.program.channelPrograms.size(); ++channel) {
        run.program.channelPrograms[channel] = static_cast<std::uint8_t>(
            run.sequenceInitialization.state.channels[channel].program.value_or(0));
    }
    for (std::size_t voice = 0; voice < run.program.voices.size(); ++voice) {
        run.program.voices[voice].program = static_cast<std::uint8_t>(
            run.sequenceInitialization.state.voices[voice].marker.value_or(0));
    }
    //0086 loads this one SNG chunk; the next chunk has an independent later allocation.
    run.songs = {{game_.logoMusicSequence, std::nullopt}};
    run.firstSample = elapsedSamples();
    for (std::size_t index = 0; index < vgaPaletteColors; ++index) {
        const auto& color = resources_.logoScreen.palette[index];
        run.palette.source[index * 3] = color.red >> 2;
        run.palette.source[index * 3 + 1] = color.green >> 2;
        run.palette.source[index * 3 + 2] = color.blue >> 2;
    }
    // The native graphics provider declares an initial zero DAC. The complete
    // first upload overwrites it; original mode-setting/DAC ancestry is separate.
    auto admission = run;
    const auto planned = executePresentation(admission, *audio_->workPhase_,
        [](const auto& phase, const auto& operations) {
            return ClockedPresentationWork{normalWorkTiming(phase, operations, presentationWorkHorizon), {}};
        },
        [](const auto& phase, const auto& palette, const auto& request) {
            auto result = paletteClockTiming(phase, palette, request, presentationWorkHorizon);
            return ClockedPaletteWork{std::move(result.state), {std::move(result.timing), {}}, result.highReads,
                result.waitEntryCycles, result.uploadStartCycles, result.uploadEndCycles,
                std::move(result.writes), std::move(result.commits)};
        }, applyDriverSequenceCallbackState, soundBlasterSequenceCallbackState, [](const auto&) {});

    const auto result = executePresentation(run, *audio_->workPhase_,
        [&](const auto& phase, const auto& operations) {
            return audio_->clock_.renderWork(phase, operations, presentationWorkHorizon);
        },
        [&](const auto& phase, const auto& palette, const auto& request) {
            return audio_->clock_.renderPalette(phase, palette, request, presentationWorkHorizon);
        },
        [&](const auto& incoming, const auto& work) {
            audio_->synth_.completeRecoveredDirectReset();
            return applyDriverSequenceCallbackState(incoming, work);
        },
        [&](const auto&, const auto&, const auto&) { return audio_->synth_.prepareRecoveredLogo(run.program,
            run.audioProfile == StartupMusicProfile::playableUnpublished
                ? PlayableMusicPublication::suspended : PlayableMusicPublication::current); },
        [&](const StartupPresentationSegment& segment) {
            const bool screen = segment.provider == StartupPresentationProvider::loadScreen;
            if (!screen && segment.kind != StartupPresentationKind::copy) {
                return;
            }
            const auto page = segment.page == StartupPresentationPage::front ? NativeMenuPage::front
                : segment.page == StartupPresentationPage::back ? NativeMenuPage::back : NativeMenuPage::cache;
            if (!screen) {
                pages_.apply(std::vector<IndexedPageEffect>{RadarPageCopy{menuPageSegment(NativeMenuPage::back),
                    menuPageSegment(page),{0,0},screenWidth,screenHeight}});
                return;
            }
            for (std::int16_t y = 0; y < screenHeight; ++y) {
                for (std::int16_t x = 0; x < screenWidth; ++x) {
                    pages_.write(menuPageSegment(page), {x, y}, resources_.logoScreen.pixels[y * screenWidth + x]);
                }
            }
        });
    if (result.remainingCycles != planned.remainingCycles || result.deferredCycles != planned.deferredCycles
        || result.timer->globalCount != planned.timer->globalCount) {
        throw std::logic_error("presentation execution differs from its admitted retained phase");
    }
    run.finalPhase = result;
    frame_ = resources_.logoScreen;
    frame_.palette = expandVgaDacPalette(std::vector<std::uint8_t>(run.palette.dac.colors.begin(), run.palette.dac.colors.end()));
    audio_->workPhase_ = result;
    audio_->presentation_.emplace(std::move(run));
    phase_ = NativeStartupPhase::awaitingFontResources;
}

}  // namespace stellar7
