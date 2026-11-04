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

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace stellar7;
    const auto scoreFont = font();
    auto roles = CockpitPageState::fromRecoveredMenu();
    for (unsigned parity = 0; parity < 2; ++parity) {
        for (const auto incoming : {0U,50U,65520U,2147483597U}) {
            const auto work = scoreOperations(incoming,50,0,scoreFont,roles,ScoreAwardDomain::initialOrdinary);
            assert(work.score == incoming+50 && work.text == std::to_string(incoming+50)+' ');
            assert(work.drawPage == roles.backPage() && work.copyPage == roles.frontPage());
            assert(work.pages.drawPage() == roles.drawPage() && work.pages.copyPage() == roles.copyPage());
            assert(work.textColor == 239 && work.fontSelections.size() == 2);
            assert(work.fontSelections[0].selection == ScoreFontSelection::score);
            assert(work.fontSelections[1].selection == ScoreFontSelection::primary);
            assert(work.fontSelections[0].operationOffset < work.fontSelections[1].operationOffset);
            for (const auto& provider : work.fontSelections) {
                assert(!provider.guestElapsedQualified && provider.operationOffset < work.operations.size());
            }
            IndexedPages expected,actual;
            for (const auto page : {roles.frontPage(),roles.backPage(),roles.cachePage()}) {
                for (std::int16_t y = 0; y < 200; ++y) {
                    for (std::int16_t x = 0; x < 320; ++x) {
                        const auto color = static_cast<std::uint8_t>(x*7+y*13+(page>>4));
                        expected.write(page,{x,y},color); actual.write(page,{x,y},color);
                    }
                }
            }
            expected.apply(scorePageEffects(work.score,scoreFont,roles.backPage(),roles.frontPage(),
                CockpitPageLayout::recoveredStartup));
            actual.apply(work.effects);
            for (const auto page : {roles.frontPage(),roles.backPage(),roles.cachePage()}) {
                for (std::int16_t y = 0; y < 200; ++y) {
                    for (std::int16_t x = 0; x < 320; ++x) { assert(expected.read(page,{x,y}) == actual.read(page,{x,y})); }
                }
            }
        }
        roles.flip();
    }
    rejects([&] { scoreOperations(0,50,0,scoreFont,roles); });
    rejects([&] { scoreOperations(0,5,0,scoreFont,roles,ScoreAwardDomain::initialOrdinary); });
    rejects([&] { scoreOperations(0,0,0,scoreFont,roles,ScoreAwardDomain::initialOrdinary); });
    rejects([&] { scoreOperations(0,50,1,scoreFont,roles,ScoreAwardDomain::initialOrdinary); });
    rejects([&] { scoreOperations(2147483598U,50,0,scoreFont,roles,ScoreAwardDomain::initialOrdinary); });
    rejects([&] { scoreOperations(0,50,0,scoreFont,CockpitPageState{},ScoreAwardDomain::initialOrdinary); });
    rejects([&] { scoreOperations(0,20,0,scoreFont,roles,static_cast<ScoreAwardDomain>(99)); });
    const auto ordinary=scoreOperations(80,20,0,scoreFont,roles);
    const auto explicitOrdinary=scoreOperations(80,20,0,scoreFont,roles,ScoreAwardDomain::initialOrdinary);
    assert(ordinary.score==explicitOrdinary.score&&ordinary.text==explicitOrdinary.text);
    assert(ordinary.operations.size()==explicitOrdinary.operations.size());
    for(std::size_t index=0;index<ordinary.operations.size();++index){
        const auto a=ordinary.operations[index],b=explicitOrdinary.operations[index];
        assert(a.kind==b.kind&&a.count==b.count&&a.value==b.value);
    }
    return 0;
}
