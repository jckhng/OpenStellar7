#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace stellar7 {

struct Rgb {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;

    friend bool operator==(const Rgb& left, const Rgb& right) noexcept {
        return left.red == right.red
            && left.green == right.green
            && left.blue == right.blue;
    }
};

struct WorldPresentation;
struct IndexedScreen {
    std::vector<std::uint8_t> pixels;
    std::vector<Rgb> palette;
    // Optional, read-only geometry for the enhanced host; never source state.
    std::shared_ptr<const WorldPresentation> presentation;
};

struct IndexedBitmap {
    std::uint16_t width;
    std::uint16_t height;
    std::vector<std::uint8_t> pixels;
};

struct BitmapFont {
    std::uint8_t firstCharacter = 0;
    std::uint8_t height = 0;
    std::uint8_t underlineRow = 0;
    // Each glyph's width is also its normal advance; pixels are binary masks.
    std::vector<IndexedBitmap> glyphs;
};

std::vector<BitmapFont> decodeFontResources(
    const std::vector<std::uint8_t>& resource
);

std::vector<std::array<std::uint8_t, 256>> decodeByteArrays(
    const std::vector<std::uint8_t>& resource
);

struct ModelShape {
    std::uint8_t flags;
    std::array<std::uint8_t, 4> colors;
    std::uint8_t normalVertex;
    std::vector<std::uint8_t> vertexIndices;
};

struct ModelPrimitive {
    std::uint8_t kind;
    std::uint16_t reserved;
    std::array<std::uint8_t, 6> arguments;
    std::vector<ModelShape> shapes;
};

struct ModelGroup {
    std::int16_t animationOffset;
    std::uint8_t originVertex;
    std::vector<std::array<std::int16_t, 3>> vertices;
    std::vector<ModelPrimitive> primitives;
    // Table-relative identity preserves the original renderer's vertex cache key.
    std::size_t vertexBlock = 0;
};

struct ModelLod {
    std::uint16_t threshold;
    std::uint8_t flags;
    std::vector<ModelGroup> groups;
};

struct Model3d {
    std::uint16_t pointerOffset;
    std::uint16_t relativeSegment;
    std::uint8_t scaleShift;
    std::uint8_t flags;
    std::uint16_t boundingRadius;
    std::uint16_t collisionRadius;
    std::array<std::int16_t, 3> origin;
    std::vector<ModelLod> lods;
    // CFBC/CFEC: count is flags; +2 is a segment-relative limits pointer.
    // Empty with a nonzero count means geometry-only metadata was available.
    std::vector<std::uint8_t> animationLimits;
};

std::vector<Rgb> expandVgaDacPalette(
    const std::vector<std::uint8_t>& data
);
std::vector<std::vector<Rgb>> decodePalettes(
    const std::vector<std::uint8_t>& resource
);
std::vector<std::uint8_t> decodeCompressedPayload(
    const std::vector<std::uint8_t>& payload
);
std::vector<std::vector<std::uint8_t>> decodeTaggedPayloads(
    const std::vector<std::uint8_t>& resource,
    const std::array<char, 4>& tag
);
// Zero-based top-level SSM selection, preserving its linked SNG record order.
std::vector<std::vector<std::uint8_t>> decodeSoundSequenceGroup(
    const std::vector<std::uint8_t>& resource, std::size_t groupIndex);
std::vector<std::uint8_t> combineScreenPlanes(
    const std::vector<std::uint8_t>& lowPlane,
    const std::vector<std::uint8_t>& highPlane
);
IndexedScreen applyVgaDacPalette(
    std::vector<std::uint8_t> pixels,
    const std::vector<std::uint8_t>& paletteData
);
std::vector<std::uint8_t> decodeIndexedScreenResource(
    const std::vector<std::uint8_t>& resource, std::size_t index
);
std::vector<std::uint8_t> decodeFirstIndexedScreenResource(
    const std::vector<std::uint8_t>& resource
);
IndexedScreen decodeFirstScreenResource(
    const std::vector<std::uint8_t>& resource
);
std::vector<std::vector<IndexedBitmap>> decodeBitmapGroups(
    const std::vector<std::uint8_t>& resource
);
std::vector<Model3d> decodeModelTablePayload(
    const std::vector<std::uint8_t>& payload,
    std::size_t modelCount
);
std::vector<Model3d> decodeFirstModelTableResource(
    const std::vector<std::uint8_t>& resource,
    std::size_t modelCount
);
// Original bf06 registry length ends at a zero offset/segment pair.
std::vector<Model3d> decodeFirstModelTableResource(
    const std::vector<std::uint8_t>& resource
);

}  // namespace stellar7
