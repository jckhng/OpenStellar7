#include "stellar7/resource_codec.hpp"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

void testMenuModel();
void testGameplay();
void testLevel1Entry();

namespace {

std::vector<std::uint8_t> packNineBitCodes(
    const std::vector<std::uint16_t>& codes
) {
    std::vector<std::uint8_t> output;
    std::uint32_t accumulator = 0;
    unsigned bitCount = 0;
    for (const std::uint16_t code : codes) {
        accumulator |= static_cast<std::uint32_t>(code) << bitCount;
        bitCount += 9;
        while (bitCount >= 8) {
            output.push_back(static_cast<std::uint8_t>(accumulator));
            accumulator >>= 8;
            bitCount -= 8;
        }
    }
    if (bitCount != 0) {
        output.push_back(static_cast<std::uint8_t>(accumulator));
    }
    return output;
}

void appendU32(
    std::vector<std::uint8_t>& output,
    const std::uint32_t value
) {
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8));
    output.push_back(static_cast<std::uint8_t>(value >> 16));
    output.push_back(static_cast<std::uint8_t>(value >> 24));
}

void appendU16(
    std::vector<std::uint8_t>& output,
    const std::uint16_t value
) {
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8));
}

void appendRecord(
    std::vector<std::uint8_t>& output,
    const char* tag,
    const std::vector<std::uint8_t>& payload,
    const bool container = false
) {
    output.insert(output.end(), tag, tag + 4);
    appendU32(
        output,
        static_cast<std::uint32_t>(payload.size())
            | (container ? 0x80000000U : 0U)
    );
    output.insert(output.end(), payload.begin(), payload.end());
}

std::vector<std::uint8_t> packVqtFields(
    const std::vector<std::pair<unsigned, unsigned>>& fields
) {
    std::vector<std::uint8_t> bytes;
    std::size_t offset = 0;
    for (const auto& field : fields) {
        for (unsigned bit = 0; bit < field.second; ++bit) {
            if (offset / 8 == bytes.size()) {
                bytes.push_back(0);
            }
            bytes[offset / 8] |= ((field.first >> bit) & 1U) << (offset % 8);
            ++offset;
        }
    }
    return bytes;
}

std::vector<std::uint8_t> vqtResource(const std::vector<std::uint8_t>& bytes) {
    std::vector<std::uint8_t> screen;
    appendRecord(screen, "VQT:", bytes);
    std::vector<std::uint8_t> resource;
    appendRecord(resource, "SCR:", screen, true);
    return resource;
}

void testVqtScreens() {
    const auto quadrantBytes = packVqtFields({
        {0, 4}, {0, 8}, {1, 8}, {0, 8}, {2, 8},
        {0, 8}, {3, 8}, {0, 8}, {4, 8},
    });
    const auto quadrants = stellar7::decodeFirstIndexedScreenResource(vqtResource(quadrantBytes));
    assert(quadrants[0] == 1 && quadrants[160] == 2);
    assert(quadrants[32000] == 3 && quadrants[32160] == 4);

    std::vector<std::pair<unsigned, unsigned>> fields{{0, 4}, {1, 8}, {5, 8}, {6, 8}};
    for (unsigned index = 0; index < 16000; ++index) {
        fields.push_back({index % 2, 1});
    }
    for (const unsigned color : {2U, 3U, 4U}) {
        fields.push_back({0, 8});
        fields.push_back({color, 8});
    }
    const auto paletted = stellar7::decodeFirstIndexedScreenResource(vqtResource(packVqtFields(fields)));
    assert(paletted[0] == 5 && paletted[1] == 5 && paletted[320] == 6);

    fields = {{0, 4}, {255, 8}};
    for (unsigned index = 0; index < 16000; ++index) {
        fields.push_back({index % 251, 8});
    }
    for (const unsigned color : {2U, 3U, 4U}) {
        fields.push_back({0, 8});
        fields.push_back({color, 8});
    }
    const auto raw = stellar7::decodeFirstIndexedScreenResource(vqtResource(packVqtFields(fields)));
    assert(raw[0] == 0 && raw[1] == 100 && raw[320] == 1);

    // Six top-left subdivisions reach 5x3, exercising both odd remainders.
    fields.assign(6, {8, 4});
    const std::vector<std::pair<unsigned, unsigned>> oddLeaves{
        {0, 4}, {0, 1}, {1, 8}, {0, 2}, {2, 8},
        {0, 2}, {3, 8}, {0, 3}, {4, 8},
    };
    fields.insert(fields.end(), oddLeaves.begin(), oddLeaves.end());
    for (const unsigned width : {4U, 6U, 8U, 8U, 8U, 8U}) {
        for (unsigned quadrant = 0; quadrant < 3; ++quadrant) {
            fields.push_back({0, width});
            fields.push_back({9, 8});
        }
    }
    const auto odd = stellar7::decodeFirstIndexedScreenResource(vqtResource(packVqtFields(fields)));
    assert(odd[0] == 1 && odd[2] == 2 && odd[320] == 3 && odd[322] == 4);
    assert(odd[5] == 9 && odd[640] == 3 && odd[642] == 4);

    for (const auto& malformed : {
        std::vector<std::uint8_t>{},
        packVqtFields({{0, 4}, {2, 8}, {1, 8}, {2, 8}, {3, 8}, {3, 2}}),
    }) {
        bool rejected = false;
        try {
            stellar7::decodeFirstIndexedScreenResource(vqtResource(malformed));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
}

}  // namespace

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    testMenuModel();
    testGameplay();
    testLevel1Entry();
    testVqtScreens();

    const auto palette = stellar7::expandVgaDacPalette(
        {0, 1, 2, 63, 62, 61}
    );
    assert(palette.size() == 2);
    assert((palette[0] == stellar7::Rgb{0, 4, 8}));
    assert((palette[1] == stellar7::Rgb{255, 251, 247}));
    std::vector<std::uint8_t> paletteResource;
    std::vector<std::uint8_t> palettePayload;
    appendRecord(palettePayload, "VGA:", {0, 1, 2, 63, 62, 61});
    appendRecord(paletteResource, "PAL:", palettePayload, true);
    const auto decodedPalettes = stellar7::decodePalettes(paletteResource);
    assert(decodedPalettes.size() == 1);
    assert(decodedPalettes[0] == palette);

    const auto indexed = stellar7::combineScreenPlanes(
        {0xfd, 0xff},
        {0x99, 0x88}
    );
    assert((indexed == std::vector<std::uint8_t>{0x9f, 0x9d, 0x8f, 0x8f}));

    auto encoded = packNineBitCodes({'A', 'B', 257});
    std::vector<std::uint8_t> wrapper{2, 4, 0, 0, 0};
    wrapper.insert(wrapper.end(), encoded.begin(), encoded.end());
    assert((
        stellar7::decodeCompressedPayload(wrapper)
        == std::vector<std::uint8_t>{'A', 'B', 'A', 'B'}
    ));

    encoded = packNineBitCodes({'A', 257});
    wrapper = {2, 3, 0, 0, 0};
    wrapper.insert(wrapper.end(), encoded.begin(), encoded.end());
    assert((stellar7::decodeCompressedPayload(wrapper)
        == std::vector<std::uint8_t>{'A', 'A', 'A'}));

    // Only the next free entry is the valid KwKwK forward reference.
    for (const auto invalid : {258U, 511U}) {
        encoded = packNineBitCodes({'A', static_cast<std::uint16_t>(invalid)});
        wrapper = {2, 3, 0, 0, 0};
        wrapper.insert(wrapper.end(), encoded.begin(), encoded.end());
        bool rejected = false;
        try {
            stellar7::decodeCompressedPayload(wrapper);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }

    encoded = packNineBitCodes(
        {'A', 'B', 256, 0, 0, 0, 0, 0, 0, 'C', 'D', 257}
    );
    wrapper = {2, 6, 0, 0, 0};
    wrapper.insert(wrapper.end(), encoded.begin(), encoded.end());
    assert((
        stellar7::decodeCompressedPayload(wrapper)
        == std::vector<std::uint8_t>{'A', 'B', 'C', 'D', 'C', 'D'}
    ));

    // Synthetic FNT resources exercise fixed and proportional storage without
    // embedding any specimen font. Proportional offsets are relative to the
    // bitmap block following count*2 offsets and count widths.
    std::vector<std::uint8_t> fontResource;
    appendRecord(fontResource, "FNT:", {3, 2, 'X', 2, 0xa0, 0x40, 0xe0, 0xa0}, false);
    const std::vector<std::uint8_t> fontData{0, 0, 2, 0, 2, 4, 0x80, 0x40, 0xa0, 0x50};
    std::vector<std::uint8_t> proportional{0xff, 4, 2, 1, 'X', 2, 10, 0, 0, 10, 0, 0, 0};
    proportional.insert(proportional.end(), fontData.begin(), fontData.end());
    appendRecord(fontResource, "FNT:", proportional, false);
    const auto fonts = stellar7::decodeFontResources(fontResource);
    assert(fonts.size() == 2);
    assert(fonts[0].firstCharacter == 'X' && fonts[0].height == 2);
    assert(fonts[0].glyphs.size() == 2 && fonts[0].glyphs[0].width == 3);
    assert((fonts[0].glyphs[0].pixels == std::vector<std::uint8_t>{1, 0, 1, 0, 1, 0}));
    assert(fonts[1].underlineRow == 1 && fonts[1].glyphs[0].width == 2);
    assert(fonts[1].glyphs[1].width == 4);
    assert((fonts[1].glyphs[1].pixels == std::vector<std::uint8_t>{1, 0, 1, 0, 0, 1, 0, 1}));

    std::vector<std::uint8_t> decodedModelTable;
    std::vector<std::uint8_t> arrayResource;
    std::vector<std::uint8_t> lookup(256);
    for (std::size_t index = 0; index < lookup.size(); ++index) {
        lookup[index] = static_cast<std::uint8_t>(index);
    }
    appendRecord(arrayResource, "ARR:", lookup);
    lookup.back() = 0;
    appendRecord(arrayResource, "ARR:", lookup);
    const auto arrays = stellar7::decodeByteArrays(arrayResource);
    assert(arrays.size() == 2 && arrays[0].back() == 255 && arrays[1].back() == 0);
    std::vector<std::uint8_t> truncatedArray;
    appendRecord(truncatedArray, "ARR:", {1, 2, 3});
    bool invalidArrayRejected = false;
    try {
        stellar7::decodeByteArrays(truncatedArray);
    } catch (const std::runtime_error&) {
        invalidArrayRejected = true;
    }
    assert(invalidArrayRejected);

    appendU16(decodedModelTable, 4);
    appendU16(decodedModelTable, 0);
    decodedModelTable.insert(
        decodedModelTable.end(),
        {
            0, 1,
            100, 0,
            20, 0,
            0, 0, 0, 0, 0, 0,
            1, 0,
            20, 0,
            30, 0, 128, 1, 26, 0,
            255, 255, 0, 3, 36, 0, 1, 0, 54, 0,
            0, 0, 0, 0, 100, 0,
            10, 0, 20, 0, 30, 0,
            40, 0, 50, 0, 60, 0,
            0, 1, 0, 61, 0, 0, 0,
            161, 72, 72, 21, 21, 0, 69, 0,
            0, 1, 2, 255,
        }
    );
    wrapper = {0};
    appendU32(
        wrapper,
        static_cast<std::uint32_t>(decodedModelTable.size())
    );
    wrapper.insert(
        wrapper.end(),
        decodedModelTable.begin(),
        decodedModelTable.end()
    );
    const auto models = stellar7::decodeModelTablePayload(wrapper, 1);
    assert(models.size() == 1);
    assert(models[0].boundingRadius == 100);
    assert(models[0].collisionRadius == 20);
    assert(models[0].animationLimits.empty());
    auto animatedTable = decodedModelTable;
    animatedTable.resize(101);
    animatedTable[100] = 6;
    std::vector<std::uint8_t> animatedWrapper{0};
    appendU32(animatedWrapper, static_cast<std::uint32_t>(animatedTable.size()));
    animatedWrapper.insert(animatedWrapper.end(), animatedTable.begin(), animatedTable.end());
    const auto animated = stellar7::decodeModelTablePayload(animatedWrapper, 1);
    assert(animated[0].flags == 1 && animated[0].animationLimits == std::vector<std::uint8_t>{6});
    // Two far pointers may share a body; the zero pair determines registry size.
    std::vector<std::uint8_t> sentinelTable{4, 0, 1, 0, 4, 0, 1, 0, 0, 0, 0, 0};
    sentinelTable.resize(20); // Preserve segment-relative pointers in the model body.
    sentinelTable.insert(sentinelTable.end(), decodedModelTable.begin() + 4, decodedModelTable.end());
    const auto modelResource = [](const std::vector<std::uint8_t>& decoded) {
        std::vector<std::uint8_t> compressed{0};
        appendU32(compressed, static_cast<std::uint32_t>(decoded.size()));
        compressed.insert(compressed.end(), decoded.begin(), decoded.end());
        std::vector<std::uint8_t> resource;
        appendRecord(resource, "TBL:", compressed);
        return resource;
    };
    const auto registry = stellar7::decodeFirstModelTableResource(modelResource(sentinelTable));
    assert(registry.size() == 2 && registry[1].boundingRadius == 100);
    assert(registry[0].lods[0].groups[0].vertexBlock == 16 + 36);
    assert(registry[0].lods[0].groups[0].vertexBlock
        == registry[1].lods[0].groups[0].vertexBlock);
    auto separateVertices = sentinelTable;
    const auto duplicateOffset = static_cast<std::uint16_t>(separateVertices.size() - 16);
    separateVertices.insert(separateVertices.end(), sentinelTable.begin() + 52,
        sentinelTable.begin() + 70);
    separateVertices[46] = static_cast<std::uint8_t>(duplicateOffset);
    separateVertices[47] = static_cast<std::uint8_t>(duplicateOffset >> 8);
    const auto duplicate = stellar7::decodeFirstModelTableResource(modelResource(separateVertices));
    assert(duplicate[0].lods[0].groups[0].vertices == registry[0].lods[0].groups[0].vertices);
    assert(duplicate[0].lods[0].groups[0].vertexBlock != registry[0].lods[0].groups[0].vertexBlock);
    const auto rejectsTable = [&modelResource](const std::vector<std::uint8_t>& decoded) {
        try {
            stellar7::decodeFirstModelTableResource(modelResource(decoded));
        } catch (const std::runtime_error&) {
            return true;
        }
        return false;
    };
    assert(rejectsTable({4, 0, 0, 0})); // No terminator.
    sentinelTable[2] = 0;
    assert(rejectsTable(sentinelTable)); // Pointer aliases table storage.
    sentinelTable[0] = 0xff;
    sentinelTable[1] = 0xff;
    assert(rejectsTable(sentinelTable));
    assert(models[0].lods.size() == 1);
    assert(models[0].lods[0].threshold == 30);
    assert(models[0].lods[0].groups.size() == 1);
    const auto& modelGroup = models[0].lods[0].groups[0];
    assert(modelGroup.vertices.size() == 3);
    assert(modelGroup.vertexBlock == 36);
    assert((modelGroup.vertices[1] == std::array<std::int16_t, 3>{10, 20, 30}));
    assert(modelGroup.primitives.size() == 1);
    assert(modelGroup.primitives[0].kind == 0);
    assert(modelGroup.primitives[0].shapes.size() == 1);
    assert((
        modelGroup.primitives[0].shapes[0].vertexIndices
        == std::vector<std::uint8_t>{0, 1, 2}
    ));

    const auto externalPaletteScreen = stellar7::applyVgaDacPalette(
        {0, 1},
        {0, 1, 2, 63, 62, 61}
    );
    assert((
        externalPaletteScreen.pixels
        == std::vector<std::uint8_t>{0, 1}
    ));
    assert(externalPaletteScreen.palette == palette);

    std::vector<std::uint8_t> bitmapPayload;
    appendRecord(bitmapPayload, "INF:", {1, 0, 2, 0, 1, 0});
    appendRecord(bitmapPayload, "BIN:", {0, 1, 0, 0, 0, 0x12});
    appendRecord(bitmapPayload, "VGA:", {0, 1, 0, 0, 0, 0x34});
    std::vector<std::uint8_t> bitmapResource;
    appendRecord(bitmapResource, "BMP:", bitmapPayload, true);
    std::vector<std::uint8_t> lowColorBitmapPayload;
    appendRecord(lowColorBitmapPayload, "INF:", {1, 0, 2, 0, 1, 0});
    appendRecord(
        lowColorBitmapPayload,
        "BIN:",
        {0, 1, 0, 0, 0, 0x12}
    );
    appendRecord(bitmapResource, "BMP:", lowColorBitmapPayload, true);
    const auto groups = stellar7::decodeBitmapGroups(bitmapResource);
    assert(groups.size() == 2);
    assert(groups[0].size() == 1);
    assert(groups[0][0].width == 2);
    assert(groups[0][0].height == 1);
    assert((
        groups[0][0].pixels
        == std::vector<std::uint8_t>{0x31, 0x42}
    ));
    assert((
        groups[1][0].pixels
        == std::vector<std::uint8_t>{0x01, 0x02}
    ));
}
