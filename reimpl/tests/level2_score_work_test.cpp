#include "score_work_fixture.inc"

int main() {
    const auto scoreFont = font();
    auto pages = CockpitPageState::fromRecoveredMenu();
    for (unsigned parity = 0; parity < 2; ++parity) {
        for (const auto award : {10,25}) {
            for (const auto incoming : {0U,80U,65530U}) {
                rejects([&] { scoreOperations(incoming,award,0,scoreFont,pages); });
                const auto work = scoreOperations(incoming,static_cast<std::int16_t>(award),0,
                    scoreFont,pages,ScoreAwardDomain::level2Constructor);
                assert(work.score == incoming + award);
                assert(work.text == std::to_string(incoming + award) + ' ');
                assert(work.drawPage == pages.backPage() && work.copyPage == pages.frontPage());
                assert(work.fontSelections.size() == 2 && !work.operations.empty());
                IndexedPages actual,expected;
                for (const auto page : {pages.frontPage(),pages.backPage(),pages.cachePage()}) {
                    for (std::int16_t y = 0; y < 200; ++y) {
                        for (std::int16_t x = 0; x < 320; ++x) {
                            const auto color = static_cast<std::uint8_t>(x*7+y*13+(page>>4));
                            actual.write(page,{x,y},color);
                            expected.write(page,{x,y},color);
                        }
                    }
                }
                actual.apply(work.effects);
                expected.apply(scorePageEffects(work.score,scoreFont,pages.backPage(),pages.frontPage(),
                    CockpitPageLayout::recoveredStartup));
                for (const auto page : {pages.frontPage(),pages.backPage(),pages.cachePage()}) {
                    for (std::int16_t y = 0; y < 200; ++y) {
                        for (std::int16_t x = 0; x < 320; ++x) {
                            assert(actual.read(page,{x,y}) == expected.read(page,{x,y}));
                        }
                    }
                }
            }
        }
        pages.flip();
    }
    for (const auto award : {0,5,20,50,100,1000}) {
        rejects([&] { scoreOperations(0,static_cast<std::int16_t>(award),0,scoreFont,pages,
            ScoreAwardDomain::level2Constructor); });
    }
    rejects([&] { scoreOperations(2147483623U,25,0,scoreFont,pages,ScoreAwardDomain::level2Constructor); });
    rejects([&] { scoreOperations(0,25,1,scoreFont,pages,ScoreAwardDomain::level2Constructor); });
}
