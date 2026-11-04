#pragma once

#include "stellar7/radar_work.hpp"

#include <array>
#include <cstdint>
#include <vector>
#include <variant>

namespace stellar7 {

using IndexedPageEffect = std::variant<RadarPixelWrite, RadarPageCopy>;

// Physical display/cache pages. Unknown contents are never materialized as zero.
class IndexedPages {
public:
    void write(std::uint16_t page, ProjectedPoint point, std::uint8_t color);
    void apply(const RadarWork& work);
    //Mode-X106A latch copy: aligned groups may cross the raster's right edge.
    //Physical page padding remains in this owner; ordinary copy guards are unchanged.
    void copyPlanar(const RadarPageCopy& region);
    void apply(const std::vector<IndexedPageEffect>& effects);
    bool known(std::uint16_t page, ProjectedPoint point) const;
    std::uint8_t read(std::uint16_t page, ProjectedPoint point) const;
    //Copies the physical page only when every pixel has owned write history.
    std::vector<std::uint8_t> snapshot(std::uint16_t page) const;

private:
    friend struct IndexedPagesTestAccess;
    static constexpr std::size_t width = 320;
    static constexpr std::size_t height = 200;
    static constexpr std::size_t planePageBytes = 0x4000;
    static constexpr std::size_t pixelsPerPlanarByte = 4;
    static constexpr std::size_t pagePixels = pixelsPerPlanarByte*planePageBytes;
    struct Page {
        std::vector<std::uint8_t> pixels = std::vector<std::uint8_t>(pagePixels);
        std::vector<bool> known = std::vector<bool>(pagePixels, false);
    };
    static std::size_t pageIndex(std::uint16_t page);
    static std::size_t pixelIndex(ProjectedPoint point);
    void copy(const RadarPageCopy& region);
    std::array<Page, 3> pages_;
};

}
