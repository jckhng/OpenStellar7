#include "stellar7/resource_codec.hpp"
#include "stellar7/scene_script.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace stellar7 {
namespace {

constexpr std::uint32_t containerFlag = 0x80000000U;
constexpr std::uint32_t sizeMask = 0x7fffffffU;

std::uint16_t readU16(
    const std::vector<std::uint8_t>& data,
    const std::size_t offset
) {
    if (offset + 2 > data.size()) {
        throw std::runtime_error("truncated little-endian uint16");
    }
    return static_cast<std::uint16_t>(
        data[offset]
        | (static_cast<std::uint16_t>(data[offset + 1]) << 8)
    );
}

std::int16_t readS16(
    const std::vector<std::uint8_t>& data,
    const std::size_t offset
) {
    return static_cast<std::int16_t>(readU16(data, offset));
}

std::uint32_t readU32(
    const std::vector<std::uint8_t>& data,
    const std::size_t offset
) {
    if (offset + 4 > data.size()) {
        throw std::runtime_error("truncated little-endian uint32");
    }
    return static_cast<std::uint32_t>(data[offset])
        | (static_cast<std::uint32_t>(data[offset + 1]) << 8)
        | (static_cast<std::uint32_t>(data[offset + 2]) << 16)
        | (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

class BitReader {
public:
    explicit BitReader(const std::vector<std::uint8_t>& data) : data_(data) {}

    std::uint16_t read(const unsigned width) {
        if (bitOffset_ + width > data_.size() * 8) {
            throw std::runtime_error("truncated LZW code stream");
        }
        std::uint16_t value = 0;
        for (unsigned index = 0; index < width; ++index) {
            const std::size_t byteOffset = bitOffset_ / 8;
            const unsigned bitIndex = static_cast<unsigned>(bitOffset_ % 8);
            value |= static_cast<std::uint16_t>(
                (data_[byteOffset] >> bitIndex) & 1U
            ) << index;
            ++bitOffset_;
        }
        return value;
    }

    void alignByte() noexcept {
        bitOffset_ = (bitOffset_ + 7) & ~std::size_t{7};
    }

    void skip(const std::size_t bitCount) {
        bitOffset_ += bitCount;
        if (bitOffset_ > data_.size() * 8) {
            throw std::runtime_error("truncated LZW reset padding");
        }
    }

private:
    const std::vector<std::uint8_t>& data_;
    std::size_t bitOffset_ = 0;
};

constexpr std::size_t screenWidth = 320;
constexpr std::size_t screenHeight = 200;

unsigned bitWidth(std::size_t value) {
    unsigned width = 0;
    while (value != 0) {
        ++width;
        value >>= 1;
    }
    return width;
}

class VqtDecoder {
public:
    explicit VqtDecoder(const std::vector<std::uint8_t>& data)
        : reader_(data), pixels_(screenWidth * screenHeight, 0) {}

    std::vector<std::uint8_t> decode() {
        tree(0, 0, screenWidth, screenHeight);
        return std::move(pixels_);
    }

private:
    void leaf(
        const std::size_t x, const std::size_t y,
        const std::size_t width, const std::size_t height
    ) {
        const auto area = width * height;
        if (area == 0) {
            return;
        }
        if (area == 1) {
            pixels_[y * screenWidth + x] = static_cast<std::uint8_t>(reader_.read(8));
            return;
        }
        const auto count = reader_.read(std::min(bitWidth(area - 1), 8U)) + 1U;
        const unsigned indexBits = bitWidth(count - 1);
        const bool compressed = area * indexBits + count * 8 < area * 8;
        std::array<std::uint8_t, 256> colors{};
        if (compressed) {
            for (unsigned index = 0; index < count; ++index) {
                colors[index] = static_cast<std::uint8_t>(reader_.read(8));
            }
        }
        // Original 1f40:1f60 visits columns before rows.
        for (std::size_t column = x; column < x + width; ++column) {
            for (std::size_t row = y; row < y + height; ++row) {
                const auto index = reader_.read(compressed ? indexBits : 8);
                if (compressed && index >= count) {
                    throw std::runtime_error("VQT palette index exceeds color count");
                }
                pixels_[row * screenWidth + column] = compressed
                    ? colors[index] : static_cast<std::uint8_t>(index);
            }
        }
    }

    void tree(
        const std::size_t x, const std::size_t y,
        const std::size_t width, const std::size_t height
    ) {
        if (width == 0 || height == 0) {
            return;
        }
        const auto flags = reader_.read(4);
        const auto left = width / 2;
        const auto top = height / 2;
        const std::array<std::array<std::size_t, 4>, 4> quadrants{{
            {x, y, left, top}, {x + left, y, width - left, top},
            {x, y + top, left, height - top},
            {x + left, y + top, width - left, height - top},
        }};
        unsigned mask = 8;
        for (const auto& bounds : quadrants) {
            if ((flags & mask) != 0) {
                if (bounds[2] == width && bounds[3] == height) {
                    throw std::runtime_error("VQT subdivision does not shrink");
                }
                tree(bounds[0], bounds[1], bounds[2], bounds[3]);
            } else {
                leaf(bounds[0], bounds[1], bounds[2], bounds[3]);
            }
            mask >>= 1;
        }
    }

    BitReader reader_;
    std::vector<std::uint8_t> pixels_;
};

std::vector<std::uint8_t> decodeLzw(
    const std::vector<std::uint8_t>& encoded,
    const std::size_t expectedSize
) {
    if (expectedSize == 0) {
        return {};
    }

    BitReader reader(encoded);
    std::array<std::uint16_t, 4096> prefixes{};
    std::array<std::uint8_t, 4096> suffixes{};
    unsigned width = 9;
    std::uint16_t freeEntry = 257;
    std::uint16_t oldCode = reader.read(width);
    if (oldCode > 255) {
        throw std::runtime_error("invalid initial LZW code");
    }
    std::uint8_t lastByte = static_cast<std::uint8_t>(oldCode);
    std::vector<std::uint8_t> output{lastByte};
    unsigned codesAtWidth = 0;

    while (output.size() < expectedSize) {
        const std::uint16_t newCode = reader.read(width);
        ++codesAtWidth;
        if (newCode == 256) {
            const unsigned remainder = codesAtWidth % 8;
            if (remainder != 0) {
                reader.skip((8 - remainder) * width);
            }
            width = 9;
            freeEntry = 257;
            const std::uint16_t firstCode = reader.read(width);
            if (firstCode > 255) {
                throw std::runtime_error(
                    "invalid first LZW code after reset"
                );
            }
            output.push_back(static_cast<std::uint8_t>(firstCode));
            codesAtWidth = 1;
            oldCode = firstCode;
            lastByte = static_cast<std::uint8_t>(firstCode);
            continue;
        }

        std::uint16_t code = newCode;
        std::vector<std::uint8_t> stack;
        if (code > freeEntry) {
            throw std::runtime_error("undefined LZW dictionary code");
        }
        if (code == freeEntry) {
            stack.push_back(lastByte);
            code = oldCode;
        }
        while (code >= 256) {
            if (code == 256 || code >= freeEntry || stack.size() >= prefixes.size()) {
                throw std::runtime_error("invalid LZW dictionary code");
            }
            stack.push_back(suffixes[code]);
            code = prefixes[code];
        }
        stack.push_back(static_cast<std::uint8_t>(code));
        lastByte = static_cast<std::uint8_t>(code);
        if (stack.size() > expectedSize - output.size()) {
            throw std::runtime_error("LZW output exceeded its declared size");
        }
        output.insert(output.end(), stack.rbegin(), stack.rend());

        if (freeEntry < prefixes.size()) {
            prefixes[freeEntry] = oldCode;
            suffixes[freeEntry] = lastByte;
            ++freeEntry;
            if (freeEntry >= (1U << width) && width < 12) {
                ++width;
                codesAtWidth = 0;
            }
        }
        oldCode = newCode;
    }

    if (output.size() != expectedSize) {
        throw std::runtime_error("LZW output exceeded its declared size");
    }
    return output;
}

struct Record {
    std::string tag;
    std::size_t dataOffset;
    std::size_t endOffset;
    std::vector<Record> children;
};

std::vector<Record> parseRange(
    const std::vector<std::uint8_t>& data,
    const std::size_t start,
    const std::size_t end
) {
    std::vector<Record> records;
    std::size_t offset = start;
    while (offset < end) {
        if (end - offset < 8) {
            throw std::runtime_error("truncated resource record header");
        }
        std::string tag;
        for (std::size_t index = 0; index < 4; ++index) {
            const std::uint8_t value = data[offset + index];
            if (value < 0x20 || value > 0x7e) {
                throw std::runtime_error("invalid resource record tag");
            }
            tag.push_back(static_cast<char>(value));
        }
        const std::uint32_t encodedSize = readU32(data, offset + 4);
        const bool container = (encodedSize & containerFlag) != 0;
        const std::size_t size = encodedSize & sizeMask;
        const std::size_t dataOffset = offset + 8;
        const std::size_t endOffset = dataOffset + size;
        if (endOffset < dataOffset || endOffset > end) {
            throw std::runtime_error("resource record crosses its parent boundary");
        }
        records.push_back(
            Record{
                std::move(tag),
                dataOffset,
                endOffset,
                container ? parseRange(data, dataOffset, endOffset)
                          : std::vector<Record>{},
            }
        );
        offset = endOffset;
    }
    return records;
}

const Record& childWithTag(const Record& parent, const std::string& tag) {
    for (const Record& child : parent.children) {
        if (child.tag == tag) {
            return child;
        }
    }
    throw std::runtime_error("resource record is missing child " + tag);
}

void collectWithTag(
    const std::vector<Record>& records,
    const std::string& tag,
    std::vector<const Record*>& matches
) {
    for (const Record& record : records) {
        if (record.tag == tag) {
            matches.push_back(&record);
        }
        collectWithTag(record.children, tag, matches);
    }
}

const Record* childWithTagOrNull(
    const Record& parent,
    const std::string& tag
) {
    for (const Record& child : parent.children) {
        if (child.tag == tag) {
            return &child;
        }
    }
    return nullptr;
}

std::vector<std::uint8_t> payload(
    const std::vector<std::uint8_t>& data,
    const Record& record
) {
    return {
        data.begin() + static_cast<std::ptrdiff_t>(record.dataOffset),
        data.begin() + static_cast<std::ptrdiff_t>(record.endOffset),
    };
}

void requireRange(
    const std::vector<std::uint8_t>& data,
    const std::size_t offset,
    const std::size_t size,
    const char* label
) {
    if (offset > data.size() || size > data.size() - offset) {
        throw std::runtime_error(
            std::string(label) + " crosses the decoded model table"
        );
    }
}

std::vector<std::uint8_t> decodeVertexIndices(
    const std::vector<std::uint8_t>& data,
    const std::size_t segmentBase,
    const std::uint16_t offset,
    const std::size_t vertexCount
) {
    std::size_t cursor = segmentBase + offset;
    std::vector<std::uint8_t> indices;
    for (std::size_t count = 0; count < 256; ++count) {
        requireRange(data, cursor, 1, "model vertex-index list");
        const std::uint8_t index = data[cursor++];
        if (index == 0xff) {
            return indices;
        }
        if (index >= vertexCount) {
            throw std::runtime_error(
                "model shape references a vertex outside its group"
            );
        }
        indices.push_back(index);
    }
    throw std::runtime_error("model vertex-index list lacks its terminator");
}

ModelPrimitive decodeModelPrimitive(
    const std::vector<std::uint8_t>& data,
    const std::size_t segmentBase,
    const std::size_t offset,
    const std::size_t vertexCount
) {
    requireRange(data, offset, 7, "model primitive");
    ModelPrimitive primitive{
        data[offset],
        0,
        {},
        {},
    };
    std::copy_n(
        data.begin() + static_cast<std::ptrdiff_t>(offset + 1),
        primitive.arguments.size(),
        primitive.arguments.begin()
    );
    if (primitive.kind != 0) {
        return primitive;
    }

    const std::size_t shapeCount = readU16(data, offset + 1);
    const std::uint16_t shapesOffset = readU16(data, offset + 3);
    primitive.reserved = readU16(data, offset + 5);
    if (shapeCount > 1024) {
        throw std::runtime_error("implausible model shape count");
    }
    const std::size_t shapeTable = segmentBase + shapesOffset;
    requireRange(
        data,
        shapeTable,
        shapeCount * 8,
        "model shape table"
    );
    primitive.shapes.reserve(shapeCount);
    for (std::size_t index = 0; index < shapeCount; ++index) {
        const std::size_t shapeOffset = shapeTable + index * 8;
        ModelShape shape{
            data[shapeOffset],
            {
                data[shapeOffset + 1],
                data[shapeOffset + 2],
                data[shapeOffset + 3],
                data[shapeOffset + 4],
            },
            data[shapeOffset + 5],
            {},
        };
        shape.vertexIndices = decodeVertexIndices(
            data,
            segmentBase,
            readU16(data, shapeOffset + 6),
            vertexCount
        );
        primitive.shapes.push_back(std::move(shape));
    }
    return primitive;
}

ModelGroup decodeModelGroup(
    const std::vector<std::uint8_t>& data,
    const std::size_t segmentBase,
    const std::size_t offset
) {
    requireRange(data, offset, 10, "model group");
    const std::uint8_t originVertex = data[offset + 2];
    const std::size_t vertexCount = data[offset + 3];
    const std::uint16_t verticesOffset = readU16(data, offset + 4);
    const std::size_t variantCount = readU16(data, offset + 6);
    const std::uint16_t primitivesOffset = readU16(data, offset + 8);
    if (vertexCount == 0 || originVertex >= vertexCount) {
        throw std::runtime_error("invalid model group vertex metadata");
    }
    if (variantCount == 0 || variantCount > 256) {
        throw std::runtime_error("implausible model primitive variant count");
    }

    ModelGroup group{
        readS16(data, offset),
        originVertex,
        {},
        {},
    };
    const std::size_t vertexTable = segmentBase + verticesOffset;
    group.vertexBlock = vertexTable;
    requireRange(
        data,
        vertexTable,
        vertexCount * 6,
        "model vertex table"
    );
    group.vertices.reserve(vertexCount);
    for (std::size_t index = 0; index < vertexCount; ++index) {
        group.vertices.push_back(
            {
                readS16(data, vertexTable + index * 6),
                readS16(data, vertexTable + index * 6 + 2),
                readS16(data, vertexTable + index * 6 + 4),
            }
        );
    }

    const std::size_t primitiveTable = segmentBase + primitivesOffset;
    requireRange(
        data,
        primitiveTable,
        variantCount * 7,
        "model primitive table"
    );
    group.primitives.reserve(variantCount);
    for (std::size_t index = 0; index < variantCount; ++index) {
        group.primitives.push_back(
            decodeModelPrimitive(
                data,
                segmentBase,
                primitiveTable + index * 7,
                vertexCount
            )
        );
    }
    return group;
}

Model3d decodeModel(
    const std::vector<std::uint8_t>& data,
    const std::size_t index,
    const std::uint16_t pointerOffset,
    const std::uint16_t relativeSegment
) {
    const std::size_t segmentBase =
        static_cast<std::size_t>(relativeSegment) * 16;
    const std::size_t offset = segmentBase + pointerOffset;
    requireRange(data, offset, 16, "model header");
    const std::size_t lodCount = readU16(data, offset + 12);
    const std::uint16_t lodOffset = readU16(data, offset + 14);
    if (lodCount == 0 || lodCount > 64) {
        throw std::runtime_error("implausible model LOD count");
    }

    Model3d model{
        pointerOffset,
        relativeSegment,
        data[offset],
        data[offset + 1],
        readU16(data, offset + 2),
        readU16(data, offset + 4),
        {
            readS16(data, offset + 6),
            readS16(data, offset + 8),
            readS16(data, offset + 10),
        },
        {},
    };
    const auto limitsOffset = segmentBase + model.boundingRadius;
    if (model.flags != 0 && limitsOffset <= data.size() && model.flags <= data.size() - limitsOffset) {
        model.animationLimits.assign(data.begin() + limitsOffset, data.begin() + limitsOffset + model.flags);
    }
    const std::size_t lodTable = segmentBase + lodOffset;
    requireRange(data, lodTable, lodCount * 6, "model LOD table");
    model.lods.reserve(lodCount);
    for (std::size_t lodIndex = 0; lodIndex < lodCount; ++lodIndex) {
        const std::size_t lod = lodTable + lodIndex * 6;
        const std::size_t groupCount = data[lod + 3];
        const std::uint16_t groupsOffset = readU16(data, lod + 4);
        if (groupCount == 0) {
            throw std::runtime_error("model LOD has no groups");
        }
        const std::size_t groupTable = segmentBase + groupsOffset;
        requireRange(
            data,
            groupTable,
            groupCount * 10,
            "model group table"
        );
        ModelLod decodedLod{
            readU16(data, lod),
            data[lod + 2],
            {},
        };
        decodedLod.groups.reserve(groupCount);
        for (
            std::size_t groupIndex = 0;
            groupIndex < groupCount;
            ++groupIndex
        ) {
            decodedLod.groups.push_back(
                decodeModelGroup(
                    data,
                    segmentBase,
                    groupTable + groupIndex * 10
                )
            );
        }
        model.lods.push_back(std::move(decodedLod));
    }
    static_cast<void>(index);
    return model;
}

}  // namespace

std::vector<Rgb> expandVgaDacPalette(
    const std::vector<std::uint8_t>& data
) {
    if (data.size() % 3 != 0) {
        throw std::runtime_error("VGA DAC palette is not divisible by three");
    }
    std::vector<Rgb> palette;
    palette.reserve(data.size() / 3);
    for (std::size_t offset = 0; offset < data.size(); offset += 3) {
        Rgb color{};
        std::uint8_t* components[] = {&color.red, &color.green, &color.blue};
        for (std::size_t index = 0; index < 3; ++index) {
            const std::uint8_t component = data[offset + index];
            if (component > 63) {
                throw std::runtime_error("VGA DAC component exceeds 63");
            }
            *components[index] = static_cast<std::uint8_t>(
                (component << 2) | (component >> 4)
            );
        }
        palette.push_back(color);
    }
    return palette;
}

std::vector<std::vector<Rgb>> decodePalettes(
    const std::vector<std::uint8_t>& resource
) {
    const std::vector<Record> records = parseRange(resource, 0, resource.size());
    std::vector<std::vector<Rgb>> palettes;
    for (const Record& record : records) {
        if (record.tag == "PAL:") {
            palettes.push_back(
                expandVgaDacPalette(
                    payload(resource, childWithTag(record, "VGA:"))
                )
            );
        }
    }
    return palettes;
}

std::vector<std::uint8_t> decodeCompressedPayload(
    const std::vector<std::uint8_t>& payloadData
) {
    if (payloadData.size() < 5) {
        throw std::runtime_error("compression wrapper is shorter than five bytes");
    }
    const std::uint8_t compressionType = payloadData[0];
    const std::size_t expectedSize = readU32(payloadData, 1);
    const std::vector<std::uint8_t> encoded(
        payloadData.begin() + 5,
        payloadData.end()
    );
    std::vector<std::uint8_t> decoded;
    if (compressionType == 0) {
        decoded = encoded;
    } else if (compressionType == 2) {
        decoded = decodeLzw(encoded, expectedSize);
    } else {
        throw std::runtime_error("unsupported resource compression type");
    }
    if (decoded.size() != expectedSize) {
        throw std::runtime_error("decoded resource size differs from wrapper");
    }
    return decoded;
}

std::vector<std::array<std::uint8_t, 256>> decodeByteArrays(
    const std::vector<std::uint8_t>& resource
) {
    const auto records = parseRange(resource, 0, resource.size());
    std::vector<const Record*> matches;
    collectWithTag(records, "ARR:", matches);
    std::vector<std::array<std::uint8_t, 256>> arrays;
    for (const auto* record : matches) {
        const auto bytes = payload(resource, *record);
        std::array<std::uint8_t, 256> values{};
        if (bytes.size() != values.size()) {
            throw std::runtime_error("ARR lookup requires 256 bytes");
        }
        std::copy(bytes.begin(), bytes.end(), values.begin());
        arrays.push_back(values);
    }
    return arrays;
}

std::vector<BitmapFont> decodeFontResources(
    const std::vector<std::uint8_t>& resource
) {
    const auto records = parseRange(resource, 0, resource.size());
    std::vector<const Record*> matches;
    collectWithTag(records, "FNT:", matches);
    std::vector<BitmapFont> fonts;
    for (const auto* record : matches) {
        const auto encoded = payload(resource, *record);
        if (encoded.size() < 4) {
            throw std::runtime_error("truncated FNT header");
        }
        // Original far loader 1f40:4524: FF introduces a proportional
        // font with a compressed offset/width/bitmap block.
        const bool proportional = encoded[0] == 0xff;
        if (proportional && encoded.size() < 13) {
            throw std::runtime_error("truncated proportional FNT header");
        }
        const auto maxWidth = encoded[proportional ? 1 : 0];
        const auto height = encoded[proportional ? 2 : 1];
        const auto first = encoded[proportional ? 4 : 2];
        const auto count = encoded[proportional ? 5 : 3];
        if (maxWidth == 0 || height == 0 || count == 0
            || static_cast<unsigned>(first) + count > 256) {
            throw std::runtime_error("invalid FNT dimensions or character range");
        }
        std::vector<std::uint8_t> data;
        std::size_t bitmapBase = 0;
        if (proportional) {
            data = decodeCompressedPayload({encoded.begin() + 8, encoded.end()});
            bitmapBase = static_cast<std::size_t>(count) * 3;
            if (data.size() != readU16(encoded, 6) || data.size() < bitmapBase) {
                throw std::runtime_error("proportional FNT table size mismatch");
            }
        } else {
            data.assign(encoded.begin() + 4, encoded.end());
            if (data.size() != static_cast<std::size_t>((maxWidth + 7) / 8)
                    * height * count) {
                throw std::runtime_error("fixed FNT bitmap size mismatch");
            }
        }
        BitmapFont font{first, height,
            static_cast<std::uint8_t>(proportional ? encoded[3] : 0), {}};
        font.glyphs.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            const auto glyphWidth = proportional ? data[count * 2 + index] : maxWidth;
            const auto stride = static_cast<std::size_t>((glyphWidth + 7) / 8);
            const auto offset = proportional
                ? bitmapBase + readU16(data, index * 2) : stride * height * index;
            if (glyphWidth > maxWidth || offset > data.size()
                || stride * height > data.size() - offset) {
                throw std::runtime_error("FNT glyph outside bitmap block");
            }
            IndexedBitmap glyph{glyphWidth, height,
                std::vector<std::uint8_t>(static_cast<std::size_t>(glyphWidth) * height)};
            for (std::size_t y = 0; y < height; ++y) {
                for (std::size_t x = 0; x < glyphWidth; ++x) {
                    glyph.pixels[y * glyphWidth + x] =
                        (data[offset + y * stride + x / 8] >> (7 - x % 8)) & 1;
                }
            }
            font.glyphs.push_back(std::move(glyph));
        }
        fonts.push_back(std::move(font));
    }
    return fonts;
}

std::vector<std::vector<std::uint8_t>> decodeTaggedPayloads(
    const std::vector<std::uint8_t>& resource,
    const std::array<char, 4>& tag
) {
    const std::string tagText(tag.begin(), tag.end());
    const auto records = parseRange(resource, 0, resource.size());
    std::vector<const Record*> matches;
    collectWithTag(records, tagText, matches);
    std::vector<std::vector<std::uint8_t>> decoded;
    decoded.reserve(matches.size());
    for (const Record* record : matches) {
        decoded.push_back(
            decodeCompressedPayload(payload(resource, *record))
        );
    }
    return decoded;
}

std::vector<std::vector<std::uint8_t>> decodeSoundSequenceGroup(
    const std::vector<std::uint8_t>& resource, std::size_t groupIndex) {
    const auto records = parseRange(resource, 0, resource.size());
    for (const auto& record : records) {
        if (record.tag != "SSM:") {
            continue;
        }
        if (groupIndex != 0) {
            --groupIndex;
            continue;
        }
        std::vector<std::vector<std::uint8_t>> sequences;
        for (const auto& child : record.children) {
            if (child.tag != "SNG:") {
                break;
            }
            sequences.push_back(decodeCompressedPayload(payload(resource, child)));
        }
        if (sequences.empty()) {
            throw std::runtime_error("selected sound group lacks a leading SNG record");
        }
        return sequences;
    }
    throw std::runtime_error("sound group selection outside resource");
}

SceneResource decodeSceneResource(const std::vector<std::uint8_t>& resource) {
    const auto records = parseRange(resource, 0, resource.size());
    const Record* pageRecord = nullptr;
    const Record* scriptRecord = nullptr;
    SceneResource scene;
    for (const auto& record : records) {
        if (record.tag == "PAG:") {
            if (pageRecord != nullptr) {
                throw std::runtime_error("scene has duplicate PAG records");
            }
            pageRecord = &record;
        } else if (record.tag == "TT3:") {
            if (scriptRecord != nullptr) {
                throw std::runtime_error("scene has duplicate TT3 records");
            }
            scriptRecord = &record;
        } else if (record.tag == "PAL:" || record.tag == "BMP:" || record.tag == "SCR:") {
            const std::vector<std::uint8_t> embedded(resource.begin() + record.dataOffset - 8,
                resource.begin() + record.endOffset);
            if (record.tag == "PAL:") {
                scene.assets.emplace_back(decodePalettes(embedded).at(0));
            } else if (record.tag == "BMP:") {
                scene.assets.emplace_back(decodeBitmapGroups(embedded).at(0));
            } else {
                scene.assets.emplace_back(decodeFirstIndexedScreenResource(embedded));
            }
        }
    }
    if (pageRecord == nullptr || scriptRecord == nullptr
        || pageRecord->endOffset - pageRecord->dataOffset != 2) {
        throw std::runtime_error("scene lacks a single PAG count and TT3 script");
    }
    scene.program = decodeSceneProgram(decodeCompressedPayload(payload(resource, *scriptRecord)),
        readU16(resource, pageRecord->dataOffset));
    return scene;
}

std::vector<std::uint8_t> combineScreenPlanes(
    const std::vector<std::uint8_t>& lowPlane,
    const std::vector<std::uint8_t>& highPlane
) {
    if (lowPlane.size() != highPlane.size()) {
        throw std::runtime_error("screen planes have different lengths");
    }
    std::vector<std::uint8_t> indexed(lowPlane.size() * 2);
    for (std::size_t offset = 0; offset < lowPlane.size(); ++offset) {
        const std::uint8_t low = lowPlane[offset];
        const std::uint8_t high = highPlane[offset];
        indexed[offset * 2] = static_cast<std::uint8_t>(
            (high & 0xf0) | (low >> 4)
        );
        indexed[offset * 2 + 1] = static_cast<std::uint8_t>(
            ((high & 0x0f) << 4) | (low & 0x0f)
        );
    }
    return indexed;
}

IndexedScreen applyVgaDacPalette(
    std::vector<std::uint8_t> pixels,
    const std::vector<std::uint8_t>& paletteData
) {
    auto palette = expandVgaDacPalette(paletteData);
    for (const std::uint8_t index : pixels) {
        if (index >= palette.size()) {
            throw std::runtime_error("indexed pixel is outside the supplied palette");
        }
    }
    return {std::move(pixels), std::move(palette)};
}

std::vector<std::uint8_t> decodeFirstIndexedScreenResource(
    const std::vector<std::uint8_t>& resource
) {
    return decodeIndexedScreenResource(resource, 0);
}

std::vector<std::uint8_t> decodeIndexedScreenResource(
    const std::vector<std::uint8_t>& resource, std::size_t index
) {
    const std::vector<Record> records = parseRange(resource, 0, resource.size());
    const Record* screenRecord = nullptr;
    for (const Record& record : records) {
        if (record.tag != "SCR:") {
            continue;
        }
        if (index == 0) {
            screenRecord = &record;
            break;
        }
        --index;
    }
    if (screenRecord == nullptr) {
        throw std::runtime_error("resource lacks a screen record");
    }
    for (const auto& child : screenRecord->children) {
        if (child.tag == "VQT:") {
            return VqtDecoder(payload(resource, child)).decode();
        }
    }
    const auto lowPlane = decodeCompressedPayload(
        payload(resource, childWithTag(*screenRecord, "BIN:"))
    );
    const auto highPlane = decodeCompressedPayload(
        payload(resource, childWithTag(*screenRecord, "VGA:"))
    );
    auto pixels = combineScreenPlanes(lowPlane, highPlane);
    if (pixels.size() != 320 * 200) {
        throw std::runtime_error("first screen is not a 320x200 indexed image");
    }
    return pixels;
}

IndexedScreen decodeFirstScreenResource(
    const std::vector<std::uint8_t>& resource
) {
    const std::vector<Record> records = parseRange(resource, 0, resource.size());
    const Record* paletteRecord = nullptr;
    for (const Record& record : records) {
        if (record.tag == "PAL:") {
            paletteRecord = &record;
            break;
        }
    }
    if (paletteRecord == nullptr) {
        throw std::runtime_error("resource lacks a palette record");
    }
    return applyVgaDacPalette(
        decodeFirstIndexedScreenResource(resource),
        payload(resource, childWithTag(*paletteRecord, "VGA:"))
    );
}

std::vector<std::vector<IndexedBitmap>> decodeBitmapGroups(
    const std::vector<std::uint8_t>& resource
) {
    const std::vector<Record> records = parseRange(resource, 0, resource.size());
    std::vector<std::vector<IndexedBitmap>> groups;
    for (const Record& record : records) {
        if (record.tag != "BMP:") {
            continue;
        }
        const auto dimensions = payload(
            resource,
            childWithTag(record, "INF:")
        );
        if (dimensions.size() < 2) {
            throw std::runtime_error("bitmap INF record lacks its frame count");
        }
        const std::size_t count = readU16(dimensions, 0);
        if (dimensions.size() != 2 + count * 4) {
            throw std::runtime_error("bitmap INF record has an invalid size");
        }

        const auto lowPlane = decodeCompressedPayload(
            payload(resource, childWithTag(record, "BIN:"))
        );
        const Record* highRecord = childWithTagOrNull(record, "VGA:");
        const auto highPlane = highRecord == nullptr
            ? std::vector<std::uint8_t>(lowPlane.size(), 0)
            : decodeCompressedPayload(payload(resource, *highRecord));
        const auto pixels = combineScreenPlanes(lowPlane, highPlane);

        std::size_t expectedPixels = 0;
        std::vector<IndexedBitmap> frames;
        frames.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            const std::uint16_t width = readU16(
                dimensions,
                2 + index * 2
            );
            const std::uint16_t height = readU16(
                dimensions,
                2 + count * 2 + index * 2
            );
            if (width == 0 || height == 0) {
                throw std::runtime_error("bitmap INF contains a zero dimension");
            }
            const std::size_t framePixels =
                static_cast<std::size_t>(width) * height;
            if (expectedPixels + framePixels > pixels.size()) {
                throw std::runtime_error(
                    "bitmap frame crosses its decoded pixel data"
                );
            }
            frames.push_back(
                {
                    width,
                    height,
                    {
                        pixels.begin()
                            + static_cast<std::ptrdiff_t>(expectedPixels),
                        pixels.begin()
                            + static_cast<std::ptrdiff_t>(
                                expectedPixels + framePixels
                            ),
                    },
                }
            );
            expectedPixels += framePixels;
        }
        if (expectedPixels != pixels.size()) {
            throw std::runtime_error(
                "bitmap INF dimensions do not consume decoded pixels"
            );
        }
        groups.push_back(std::move(frames));
    }
    return groups;
}

namespace {
std::vector<Model3d> decodeModelTable(
    const std::vector<std::uint8_t>& decoded,
    const std::size_t modelCount
) {
    if (modelCount == 0 || modelCount > 1024) {
        throw std::runtime_error("invalid model count");
    }
    requireRange(
        decoded,
        0,
        modelCount * 4,
        "model pointer table"
    );
    std::vector<Model3d> models;
    models.reserve(modelCount);
    for (std::size_t index = 0; index < modelCount; ++index) {
        const std::size_t pointer = index * 4;
        models.push_back(
            decodeModel(
                decoded,
                index,
                readU16(decoded, pointer),
                readU16(decoded, pointer + 2)
            )
        );
    }
    return models;
}
}

std::vector<Model3d> decodeModelTablePayload(
    const std::vector<std::uint8_t>& payloadData,
    const std::size_t modelCount
) {
    return decodeModelTable(decodeCompressedPayload(payloadData), modelCount);
}

std::vector<Model3d> decodeFirstModelTableResource(const std::vector<std::uint8_t>& resource) {
    constexpr std::size_t farPointerBytes = 4;
    const auto records = parseRange(resource, 0, resource.size());
    for (const auto& record : records) {
        if (record.tag != "TBL:") {
            continue;
        }
        const auto decoded = decodeCompressedPayload(payload(resource, record));
        for (std::size_t cursor = 0; cursor + farPointerBytes <= decoded.size(); cursor += farPointerBytes) {
            if (readU16(decoded, cursor) != 0 || readU16(decoded, cursor + 2) != 0) {
                continue;
            }
            for (std::size_t pointer = 0; pointer < cursor; pointer += farPointerBytes) {
                const auto address = static_cast<std::size_t>(readU16(decoded, pointer))
                    + static_cast<std::size_t>(readU16(decoded, pointer + 2)) * 16;
                if (address < cursor + farPointerBytes || address >= decoded.size()) {
                    throw std::runtime_error("model pointer outside model body");
                }
            }
            return decodeModelTable(decoded, cursor / farPointerBytes);
        }
        throw std::runtime_error("unterminated model pointer table");
    }
    throw std::runtime_error("resource lacks a model-table record");
}

std::vector<Model3d> decodeFirstModelTableResource(
    const std::vector<std::uint8_t>& resource,
    const std::size_t modelCount
) {
    const std::vector<Record> records = parseRange(
        resource,
        0,
        resource.size()
    );
    for (const Record& record : records) {
        if (record.tag == "TBL:") {
            return decodeModelTablePayload(
                payload(resource, record),
                modelCount
            );
        }
    }
    throw std::runtime_error("resource lacks a model-table record");
}

}  // namespace stellar7
