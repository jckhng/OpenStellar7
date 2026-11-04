#include "stellar7/score_work.hpp"
#include "stellar7/indexed_render.hpp"

#include <cassert>
#include <limits>
#include <stdexcept>
#include <utility>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace {
using namespace stellar7;
template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

BitmapFont font() {
    BitmapFont result{32,8,0,{}};
    for (std::size_t index = 0; index < 96; ++index) {
        const auto width = static_cast<std::uint16_t>(index == 0 ? 3 : 2+index%4);
        IndexedBitmap glyph{width,8,std::vector<std::uint8_t>(width*8)};
        for (std::size_t pixel = 0; pixel < glyph.pixels.size(); ++pixel) {
            glyph.pixels[pixel] = index == 0 ? 0 : static_cast<std::uint8_t>((pixel+index)%3 == 0);
        }
        result.glyphs.push_back(std::move(glyph));
    }
    return result;
}
}

