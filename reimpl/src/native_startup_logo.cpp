#include "stellar7/native_startup.hpp"

#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::int64_t logoWorkHorizon = 120000;
constexpr std::uint16_t frontPage = 0xa000;
constexpr std::uint16_t backPage = 0xa800;
constexpr std::uint16_t cachePage = 0xa400;
constexpr std::size_t maximumFramePolls = 1000000;

template<class Work, class Effect>
SoundBlasterClockPhase executeLogo(const StartupLogoWork& logo, SoundBlasterClockPhase phase,
    const std::size_t packetPolls, Work work, Effect effect) {
    const auto execute = [&](const std::vector<NormalWorkOperation>& operations) {
        if (!operations.empty()) { phase = work(phase, operations); }
    };
    for (const auto& segment : logo.segments) {
        execute(segment.operations);
        if (segment.kind == StartupLogoKind::countdownStore) {
            execute({{NormalWorkKind::startupLogoCountdown, 1, segment.ordinal}});
        } else if (segment.kind == StartupLogoKind::wait) {
            std::size_t polls = 0;
            while (!phase.sequenceSelection->countdownReturned) {
                if (polls >= maximumFramePolls) {
                    throw std::invalid_argument("logo frame wait exceeds admitted source poll bound");
                }
                execute({{NormalWorkKind::startupCountdownPoll, packetPolls}});
                polls += packetPolls;
            }
        } else if (segment.kind == StartupLogoKind::frameReturn) {
            execute({{NormalWorkKind::startupLogoFrameReturn, 1, segment.ordinal}});
        }
        effect(segment, phase);
    }
    return phase;
}
}

const NativeStartupLogoRun* NativeStartupSequence::recoveredLogo() const noexcept {
    return audio_ && audio_->logo_ ? &*audio_->logo_ : nullptr;
}

void NativeStartupSequence::animateLogo(const std::size_t pollsPerPacket) {
    if (phase_ != NativeStartupPhase::awaitingLogoAnimation || !audio_ || !audio_->workPhase_) {
        throw std::logic_error("logo animation requires the retained direct event");
    }
    if (pollsPerPacket == 0 || pollsPerPacket > 10000) {
        throw std::invalid_argument("logo animation packet exceeds source poll bound");
    }
    NativeStartupLogoRun run{};
    // Native decoded/cache provider; no claim of DOS allocation/cache-copy timing.
    run.cache = cacheBitmapGroup(resources_.logoFrames, 0);
    run.logo = startupLogoOperations(run.cache);
    executeLogo(run.logo, *audio_->workPhase_, pollsPerPacket,
        [](const auto& phase, const auto& operations) {
            return normalWorkTiming(phase, operations, logoWorkHorizon).phase;
        }, [](const auto&, const auto&) {});
    run.firstSample = elapsedSamples();
    run.finalPhase = executeLogo(run.logo, *audio_->workPhase_, pollsPerPacket,
        [&](const auto& phase, const auto& operations) {
            auto work = audio_->clock_.renderWork(phase, operations, logoWorkHorizon);
            run.samples.insert(run.samples.end(), work.audio.samples.begin(), work.audio.samples.end());
            auto next = work.timing.phase;
            run.work.push_back(std::move(work));
            return next;
        }, [&](const auto& segment, const auto& phase) {
            if (segment.kind == StartupLogoKind::copyToCache || segment.kind == StartupLogoKind::copyToFront) {
                const bool toCache = segment.kind == StartupLogoKind::copyToCache;
                pages_.apply(std::vector<IndexedPageEffect>{RadarPageCopy{
                    toCache ? backPage : cachePage, toCache ? cachePage : frontPage,
                    {segment.x, segment.y}, segment.width, segment.height}});
            } else if (segment.kind == StartupLogoKind::bitmap) {
                const auto& bitmap = run.logo.bitmaps[segment.frameIndex];
                for (std::uint16_t y = 0; y < bitmap.height; ++y) {
                    for (std::uint16_t x = 0; x < bitmap.width; ++x) {
                        const auto color = bitmap.pixels[y * bitmap.width + x];
                        if (color != 0) {
                            pages_.write(cachePage, {static_cast<std::int16_t>(segment.x + x),
                                static_cast<std::int16_t>(segment.y + y)}, color);
                        }
                    }
                }
            } else if (segment.kind == StartupLogoKind::frameReturn) {
                run.frames.push_back({segment.frameIndex, elapsedSamples(), phase,
                    {menuPagePixels(NativeMenuPage::front), menuPagePixels(NativeMenuPage::back),
                     menuPagePixels(NativeMenuPage::cache)}});
                frame_.pixels = run.frames.back().pages[0];
                ++logoSteps_;
            }
        });
    audio_->workPhase_ = run.finalPhase;
    audio_->logo_ = std::move(run);
    phase_ = NativeStartupPhase::awaitingLogoSoundEnd;
}

}  // namespace stellar7
