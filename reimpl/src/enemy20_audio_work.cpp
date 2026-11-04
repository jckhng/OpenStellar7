#include "stellar7/enemy20_audio_work.hpp"
#include "playable_startup_provider.hpp"
#include "stellar7/audio.hpp"
#include "stellar7/bitmap_cache.hpp"
#include "stellar7/normal_clock.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::uint16_t processNameIndexBegin = 37;
constexpr std::size_t processNameBytes = 8;
constexpr std::uint16_t allocationTypeIndex = 29;
constexpr std::uint16_t allocationOwnerIndex = 30;
constexpr std::uint16_t preheaderRecentEnd = 21;
constexpr std::uint16_t preheaderRetiredEnd = 29;
constexpr std::uint16_t allocationParagraphIndex = 32;
constexpr std::uint16_t reservedIndexBegin = 34;
constexpr std::uint8_t nonterminalAllocationType = 'M';
constexpr std::size_t paragraphBytes = 16;
constexpr std::uint16_t mzPspParagraphs = 16;
constexpr std::size_t sequenceHeaderBytes = 36;

struct NativeDosProcessName {
    std::array<std::uint8_t, processNameBytes> values{};

    explicit NativeDosProcessName(const std::string_view executable) {
        std::size_t index = 0;
        for (const auto character : executable) {
            if (character < 0 || static_cast<unsigned char>(character) > 127) {
                throw std::invalid_argument("native DOS process identity requires ASCII");
            }
            if (character == ':' || character == '\\' || character == '/') {
                index = 0;
            } else if (index < values.size()) {
                values[index++] = static_cast<std::uint8_t>(character >= 'a' && character <= 'z'
                    ? character - 'a' + 'A' : character);
            }
        }
        for (std::size_t byte = 0; byte < values.size(); ++byte) {
            if (values[byte] == '.' || values[byte] == 0) {
                std::fill(values.begin() + byte, values.end(), 0);
                break;
            }
        }
        if (values.front() == 0) {
            throw std::invalid_argument("native DOS process identity requires a program name");
        }
    }
};
}

struct SoundBlasterEnemy20McbByte::Storage {
    std::array<std::uint8_t,reservedByteCount> reservedBytes;
    std::array<std::uint8_t,retiredTailByteCount> retiredTail;
    std::optional<std::array<std::uint8_t,preheaderRecentEnd>> recentTail;
    std::optional<NativeDosProcessName> processName;
    std::optional<std::uint16_t> paragraphs;
    std::optional<std::uint16_t> pspSegment;
    std::shared_ptr<const Storage> reservedGeneration;
};

SoundBlasterEnemy20McbByte::SoundBlasterEnemy20McbByte(const std::array<std::uint8_t,reservedByteCount> values,
    const std::array<std::uint8_t,retiredTailByteCount> retiredTail)
    : storage_(std::make_shared<const Storage>(Storage{values, retiredTail, std::nullopt,
        std::nullopt, std::nullopt, std::nullopt, {}})) {}

std::uint8_t SoundBlasterEnemy20McbByte::value() const noexcept { return storage_->reservedBytes[1]; }

std::optional<std::uint8_t> SoundBlasterEnemy20McbByte::nativeProcessByte(const std::uint16_t index) const noexcept {
    if (!storage_->processName || !storage_->paragraphs || !storage_->pspSegment) { return std::nullopt; }
    if (index < preheaderRecentEnd && storage_->recentTail) { return (*storage_->recentTail)[index]; }
    if (index >= preheaderRecentEnd && index < preheaderRetiredEnd) {
        return storage_->retiredTail[index - preheaderRecentEnd];
    }
    if (index == allocationTypeIndex) { return nonterminalAllocationType; }
    if (index == allocationOwnerIndex) { return static_cast<std::uint8_t>(*storage_->pspSegment); }
    if (index == allocationOwnerIndex + 1) { return static_cast<std::uint8_t>(*storage_->pspSegment >> 8); }
    if (index == allocationParagraphIndex) { return static_cast<std::uint8_t>(*storage_->paragraphs); }
    if (index == allocationParagraphIndex + 1) { return static_cast<std::uint8_t>(*storage_->paragraphs >> 8); }
    if (index >= reservedIndexBegin && index - reservedIndexBegin < storage_->reservedBytes.size()) {
        return storage_->reservedBytes[index - reservedIndexBegin];
    }
    if (index >= processNameIndexBegin && index - processNameIndexBegin < processNameBytes) {
        return storage_->processName->values[index - processNameIndexBegin];
    }
    return std::nullopt;
}

bool SoundBlasterEnemy20McbByte::isNativeIdentityOf(const SoundBlasterEnemy20McbByte& reserved) const noexcept {
    return storage_->processName && storage_->reservedGeneration == reserved.storage_;
}

SoundBlasterEnemy20McbByte SoundBlasterEnemy20McbByte::withNativeProcessIdentity(const std::string_view executable,
    const std::vector<std::uint8_t>& ownedSsm0,const IndexedBitmap& recentBitmap,
    const NativeDosInitialLoadProfile initialLoad) const {
    if (storage_->processName) { throw std::invalid_argument("native DOS process identity is already bound"); }
    if (initialLoad.placement!=DosLoadPlacement::afterPsp || initialLoad.imageSegment<=mzPspParagraphs) {
        throw std::invalid_argument("native allocation requires its initial DOS MZ load profile");
    }
    if (ownedSsm0.size() < sequenceHeaderBytes || ownedSsm0.front() != 0x84
        || ownedSsm0.size() > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) * paragraphBytes) {
        throw std::invalid_argument("native allocation requires its owned SSM0 payload");
    }
    constexpr std::uint16_t recentWidth=40;
    constexpr std::uint16_t recentHeight=11;
    constexpr std::size_t recentPlane=3;
    constexpr std::size_t recentFirstByte=89;
    std::array<std::uint8_t,preheaderRecentEnd> recentTail{};
    if (recentBitmap.width!=recentWidth || recentBitmap.height!=recentHeight
        || recentBitmap.pixels.size()!=static_cast<std::size_t>(recentWidth)*recentHeight) {
        throw std::invalid_argument("native allocation requires its retained startup bitmap");
    }
    for (std::size_t index=0; index<recentTail.size(); ++index) {
        recentTail[index]=recentBitmap.pixels[4*(recentFirstByte+index)+recentPlane];
    }
    const auto paragraphs = static_cast<std::uint16_t>((ownedSsm0.size() + paragraphBytes - 1) / paragraphBytes);
    const auto pspSegment=static_cast<std::uint16_t>(initialLoad.imageSegment-mzPspParagraphs);
    auto result = *this;
    result.storage_ = std::make_shared<const Storage>(Storage{storage_->reservedBytes,storage_->retiredTail,
        recentTail,NativeDosProcessName(executable),paragraphs,pspSegment,storage_});
    return result;
}

std::optional<SoundBlasterEnemy20McbByte> SoundBlasterEnemy20McbByte::fromCache(const BitmapCache& cache) {
    constexpr std::size_t frameIndex=2;
    constexpr std::size_t planeIndex=2;
    constexpr std::size_t firstLocalByte=509;
    constexpr std::uint16_t sourceWidth=120;
    constexpr std::uint16_t sourceHeight=18;
    constexpr std::uint16_t sourceCacheOffset=1764;
    constexpr std::uint16_t cacheSegment=0xac00;
    if(cache.initialCursor!=0 || cache.frames.size()<=frameIndex) {
        return std::nullopt;
    }
    const auto& frame=cache.frames[frameIndex];
    if(frame.header.segment!=cacheSegment || frame.header.offset!=sourceCacheOffset
        || frame.header.width!=sourceWidth || frame.header.height!=sourceHeight
        || frame.bitmap.width!=sourceWidth || frame.bitmap.height!=sourceHeight
        || frame.bitmap.pixels.size()!=static_cast<std::size_t>(sourceWidth)*sourceHeight) {
        return std::nullopt;
    }
    std::array<std::uint8_t,retiredTailByteCount> retiredTail{};
    constexpr std::size_t retiredFirstByte=496;
    for (std::size_t index=0; index<retiredTail.size(); ++index) {
        const auto localByte=retiredFirstByte+index;
        const auto offset=static_cast<std::size_t>(frame.header.offset)+localByte;
        const auto indexedPixel=4*localByte+planeIndex;
        if (cache.planes[planeIndex].size()<=offset
            || cache.planes[planeIndex][offset]!=frame.bitmap.pixels[indexedPixel]) {
            return std::nullopt;
        }
        retiredTail[index]=cache.planes[planeIndex][offset];
    }
    std::array<std::uint8_t,reservedByteCount> values{};
    for (std::size_t index=0; index<values.size(); ++index) {
        const auto localByte=firstLocalByte+index;
        const auto offset=static_cast<std::size_t>(frame.header.offset)+localByte;
        const auto indexedPixel=4*localByte+planeIndex;
        if (cache.planes[planeIndex].size()<=offset
            || cache.planes[planeIndex][offset]!=frame.bitmap.pixels[indexedPixel]) {
            return std::nullopt;
        }
        values[index]=cache.planes[planeIndex][offset];
    }
    return SoundBlasterEnemy20McbByte(values,retiredTail);
}

SoundBlasterEnemy20CallerWindow level1Enemy20CallerWindow(const std::vector<std::uint8_t>& ownedSsm0) {
    if(ownedSsm0.size()<sequenceHeaderBytes || ownedSsm0.front()!=0x84) {
        throw std::invalid_argument("enemy20 alias requires its owned resident SSM0 payload");
    }
    SoundBlasterEnemy20CallerWindow window{};
    std::copy_n(ownedSsm0.begin(),window.payloadPrefix.size(),window.payloadPrefix.begin());
    return window;
}

SoundBlasterClockPhase bindLevel1Enemy20Audio(SoundBlasterSynth& synth,const SoundBlasterClockPhase& phase) {
    const bool native = playableStartupLevel1Program(phase);
    if(!phase.sequenceSelection || (!phase.sequenceSelection->published && !native) || phase.sequenceSelection->songOrdinal!=2
        || !phase.sequenceSelection->level1DirectResetCompleted || !phase.callback || !phase.callback->program
        || phase.callback->program->enemy20CallerWindow || (!phase.callback->active && !native)
        || (phase.callback->domain!=SoundBlasterCallbackDomain::sequence && !native) || phase.callback->program->handleStatus!=0
        || (phase.callback->sequence!=phase.sequenceSelection->prepared.sequence && !native)
        || !phase.soundBlasterInitialization || !phase.soundBlasterInitialization->driverEnabled) {
        throw std::invalid_argument("enemy20 alias binding requires the retained Level1 reset/resource provider");
    }
    auto result=phase;
    const auto window=synth.configureEnemy20CallerWindow(*phase.callback);
    result.callback->program->enemy20CallerWindow=window;
    return result;
}

SoundBlasterClockPhase bindLevel1Enemy20McbAudio(SoundBlasterSynth& synth,const SoundBlasterClockPhase& phase,
    const SoundBlasterEnemy20McbByte& byte) {
    if(!phase.callback || !phase.callback->program || !phase.callback->program->enemy20CallerWindow
        || phase.callback->program->enemy20McbByte) {
        throw std::invalid_argument("enemy20 MCB binding requires its retained resource window");
    }
    auto result=phase;
    synth.configureEnemy20McbByte(*phase.callback,byte);
    result.callback->program->enemy20McbByte=byte;
    return result;
}
}
