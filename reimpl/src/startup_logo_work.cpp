#include "stellar7/startup_logo_work.hpp"

#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::uint32_t gameBase = 0x08240;
constexpr std::uint16_t cacheSegment = 0xac00;
constexpr std::uint16_t cachePlaneBytes = 0x4000;
constexpr std::size_t planeCount = 4;
constexpr std::size_t logoFrameCount = 8;
constexpr std::uint8_t logoCallCount = 17;
constexpr std::array<std::int16_t, logoFrameCount> logoX{{237,236,233,229,225,219,210,198}};
constexpr std::array<std::int16_t, logoFrameCount> logoY{{65,63,61,57,52,46,38,25}};
constexpr std::int16_t copyX = 198;
constexpr std::int16_t physicalCopyX = 196;
constexpr std::int16_t copyY = 25;
constexpr std::uint16_t copyWidth = 90;
constexpr std::uint16_t physicalCopyWidth = 96;
constexpr std::uint16_t copyHeight = 90;
constexpr std::uint16_t frameCountdown = 10;

std::array<IndexedBitmap, logoFrameCount> cachedRasters(const BitmapCache& cache) {
    if (cache.frames.size() != logoFrameCount || cache.initialCursor >= cachePlaneBytes
        || cache.nextCursor < cache.initialCursor || cache.nextCursor > cachePlaneBytes) {
        throw std::invalid_argument("unsupported startup logo cache ownership");
    }
    const auto ownedBytes = static_cast<std::size_t>(cache.nextCursor - cache.initialCursor);
    for (const auto& plane : cache.planes) {
        if (plane.size() != ownedBytes) {
            throw std::invalid_argument("startup logo cache planes differ from owned range");
        }
    }
    std::array<IndexedBitmap, logoFrameCount> result{};
    std::size_t cursor = cache.initialCursor;
    for (std::size_t index = 0; index < result.size(); ++index) {
        const auto& header = cache.frames[index].header;
        const auto pitch = header.width / planeCount;
        const auto width = pitch * planeCount;
        const auto storedBytes = static_cast<std::size_t>(header.width) * header.height / planeCount;
        if (header.segment != cacheSegment || header.offset != cursor || pitch == 0
            || header.width > 255 || header.height == 0 || header.height > 200
            || logoX[index] + width > 320 || logoY[index] + header.height > 200
            || cursor > cache.nextCursor || storedBytes > cache.nextCursor - cursor) {
            throw std::invalid_argument("startup logo outside unmirrored cached bitmap domain");
        }
        auto& raster = result[index];
        raster.width = static_cast<std::uint16_t>(width);
        raster.height = header.height;
        raster.pixels.reserve(width * header.height);
        //1280 uses floor(header.width/4) as source pitch and drawing width/4.
        for (std::size_t y = 0; y < header.height; ++y) {
            for (std::size_t x = 0; x < width; ++x) {
                const auto offset = cursor - cache.initialCursor + y * pitch + x / planeCount;
                raster.pixels.push_back(cache.planes[x % planeCount][offset]);
            }
        }
        cursor += storedBytes; //1E71 advances by floor(width*height/4).
    }
    if (cursor != cache.nextCursor) {
        throw std::invalid_argument("startup logo cache has unowned trailing bytes");
    }
    return result;
}
}

StartupLogoWork startupLogoOperations(const BitmapCache& cache) {
    StartupLogoWork result{{}, cachedRasters(cache)};
    const auto append = [&](const StartupLogoKind kind, const std::uint16_t pc,
        const std::uint8_t ordinal, const std::int16_t index,
        const std::uint64_t dispatches = 0) -> StartupLogoSegment& {
        result.segments.push_back({kind, gameBase + pc, {}, ordinal, index});
        auto& segment = result.segments.back();
        if (dispatches != 0) {
            segment.operations.push_back({NormalWorkKind::instructions, dispatches});
        }
        return segment;
    };
    const auto copy = [&](const StartupLogoKind kind, const std::uint16_t pc,
        const std::uint8_t ordinal, const std::int16_t index) {
        auto& segment = append(kind, pc, ordinal, index, 13); //Role/args/CALL and22A1 JMPF.
        const auto body = planarCopyOperations(copyX, copyWidth, copyHeight);
        segment.operations.insert(segment.operations.end(), body.begin(), body.end());
        segment.x = physicalCopyX;
        segment.y = copyY;
        segment.width = physicalCopyWidth;
        segment.height = copyHeight;
    };

    append(StartupLogoKind::instructions, 0x620b, 0, 0, 3);
    for (std::uint8_t ordinal = 0; ordinal < logoCallCount; ++ordinal) {
        const auto index = static_cast<std::int16_t>(ordinal < logoFrameCount ? ordinal : 15 - ordinal);
        if (ordinal == logoFrameCount) {
            append(StartupLogoKind::instructions, 0x6227, ordinal, index, 3);
        }
        append(StartupLogoKind::instructions, ordinal < logoFrameCount ? 0x6216 : 0x6230,
            ordinal, index, 3); //Arguments and near CALL5FFA.
        append(StartupLogoKind::instructions, 0x5ffa, ordinal, index, 4);
        append(StartupLogoKind::countdownStore, 0x6001, ordinal, index).value = frameCountdown;
        copy(StartupLogoKind::copyToCache, 0x6020, ordinal, index);
        append(StartupLogoKind::instructions, 0x6025, ordinal, index, 3); //Cleanup/index test.
        if (index > 0) {
            auto& segment = append(StartupLogoKind::bitmap, 0x603e, ordinal, index, 10);
            segment.x = logoX[index];
            segment.y = logoY[index];
            segment.width = result.bitmaps[index].width;
            segment.height = result.bitmaps[index].height;
            const auto body = planarBitmapOperations(result.bitmaps[index], segment.x,
                PlanarBlitMode::generalTransparent); //2C1B JMPF36AE -> selected1280, mode0.
            segment.operations.insert(segment.operations.end(), body.begin(), body.end());
            append(StartupLogoKind::instructions, 0x6043, ordinal, index, 1);
        }
        copy(StartupLogoKind::copyToFront, 0x605f, ordinal, index);
        append(StartupLogoKind::instructions, 0x6064, ordinal, index, 1);
        append(StartupLogoKind::wait, 0x663b, ordinal, index, 3); //6067 CALL and6638 prologue.
        append(StartupLogoKind::instructions, 0x606a, ordinal, index, 3);
        append(StartupLogoKind::frameReturn, 0x606e, ordinal, index);
        append(StartupLogoKind::instructions, ordinal < logoFrameCount ? 0x621c : 0x6236,
            ordinal, index, 6); //Cleanup, two SI updates, DI update, CMP/Jcc.
    }
    return result;
}

}  // namespace stellar7
