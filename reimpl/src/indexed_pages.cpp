#include "stellar7/indexed_pages.hpp"

#include <algorithm>
#include <stdexcept>

namespace stellar7 {

std::size_t IndexedPages::pageIndex(const std::uint16_t page) {
    constexpr std::uint16_t firstPage = 0xa000;
    constexpr std::uint16_t pageSpacing = 0x400;
    constexpr std::uint16_t lastPage = firstPage + 2 * pageSpacing;
    if (page < firstPage || page > lastPage || (page - firstPage) % pageSpacing != 0) {
        throw std::invalid_argument("unsupported indexed page selector");
    }
    return (page - firstPage) / pageSpacing;
}

std::size_t IndexedPages::pixelIndex(const ProjectedPoint point) {
    if (point.x < 0 || point.y < 0 || point.x >= width || point.y >= height) {
        throw std::invalid_argument("indexed page coordinate outside display");
    }
    return static_cast<std::size_t>(point.y) * width + point.x;
}

void IndexedPages::write(const std::uint16_t page, const ProjectedPoint point,
    const std::uint8_t color) {
    auto& target = pages_.at(pageIndex(page));
    const auto index = pixelIndex(point);
    target.pixels[index] = color;
    target.known[index] = true;
}

bool IndexedPages::known(const std::uint16_t page, const ProjectedPoint point) const {
    return pages_.at(pageIndex(page)).known[pixelIndex(point)];
}

std::uint8_t IndexedPages::read(const std::uint16_t page, const ProjectedPoint point) const {
    const auto& source = pages_.at(pageIndex(page));
    const auto index = pixelIndex(point);
    if (!source.known[index]) {
        throw std::invalid_argument("indexed page pixel has no owned write history");
    }
    return source.pixels[index];
}

std::vector<std::uint8_t> IndexedPages::snapshot(const std::uint16_t page) const {
    const auto& source=pages_.at(pageIndex(page));
    const auto visibleEnd=source.known.begin()+width*height;
    if(std::find(source.known.begin(),visibleEnd,false)!=visibleEnd) {
        throw std::invalid_argument("indexed page pixel has no owned write history");
    }
    return {source.pixels.begin(),source.pixels.begin()+width*height};
}

void IndexedPages::copyPlanar(const RadarPageCopy& region) {
    if(region.coveredWidth==0 || region.height==0 || region.position.x<0 || region.position.y<0
        || region.position.x%pixelsPerPlanarByte!=0 || region.coveredWidth%pixelsPerPlanarByte!=0
        || region.position.x>=width || region.position.y>=height
        || region.coveredWidth>width+pixelsPerPlanarByte
        || static_cast<std::size_t>(region.position.x)+region.coveredWidth>width+pixelsPerPlanarByte
        || static_cast<std::size_t>(region.position.y)+region.height>height) {
        throw std::invalid_argument("unsupported source planar page copy");
    }
    auto& destination=pages_.at(pageIndex(region.destinationPage));
    const auto& source=pages_.at(pageIndex(region.sourcePage));
    const auto last=(static_cast<std::size_t>(region.position.y)+region.height-1)*width
        +region.position.x+region.coveredWidth;
    if(last>pagePixels) {throw std::invalid_argument("planar page copy exceeds physical page storage");}
    for(std::size_t row=0;row<region.height;++row) {
        const auto start=(static_cast<std::size_t>(region.position.y)+row)*width+region.position.x;
        for(std::size_t column=0;column<region.coveredWidth;++column) {
            const auto index=start+column;
            destination.pixels[index]=source.pixels[index];destination.known[index]=source.known[index];
        }
    }
}

void IndexedPages::copy(const RadarPageCopy& region) {
    auto& destination = pages_.at(pageIndex(region.destinationPage));
    const auto& source = pages_.at(pageIndex(region.sourcePage));
    if (region.coveredWidth == 0 || region.height == 0
        || region.position.x < 0 || region.position.y < 0
        || static_cast<std::size_t>(region.position.x) + region.coveredWidth > width
        || static_cast<std::size_t>(region.position.y) + region.height > height) {
        throw std::invalid_argument("indexed page copy outside display");
    }
    for (std::uint16_t y = 0; y < region.height; ++y) {
        for (std::uint16_t x = 0; x < region.coveredWidth; ++x) {
            const auto index = (static_cast<std::size_t>(region.position.y) + y) * width
                + region.position.x + x;
            destination.pixels[index] = source.pixels[index];
            destination.known[index] = source.known[index];
        }
    }
}

void IndexedPages::apply(const std::vector<IndexedPageEffect>& effects) {
    for (const auto& effect : effects) {
        if (const auto* pixel = std::get_if<RadarPixelWrite>(&effect)) {
            write(pixel->page, pixel->point, pixel->color);
        } else {
            copy(std::get<RadarPageCopy>(effect));
        }
    }
}

void IndexedPages::apply(const RadarWork& work) {
    for (const auto& pixel : work.pixels) {
        write(pixel.page, pixel.point, pixel.color);
    }
    if (work.pageCopy) {
        copy(*work.pageCopy);
    }
}

}
