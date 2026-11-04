#include "stellar7/indexed_pages.hpp"
#include "stellar7/indexed_render.hpp"

#include <cassert>
#include <stdexcept>

namespace {
template<class Action>
void rejects(Action action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

void snapshots() {
    using namespace stellar7;
    constexpr std::int16_t width=320,height=200;
    constexpr std::size_t pixelCount=static_cast<std::size_t>(width)*height;
    IndexedPages pages;
    for(const std::uint16_t page:{0xa000,0xa400,0xa800}) {
        rejects([&] {pages.snapshot(page);});
        for(std::size_t index=0;index<pixelCount-1;++index) {
            pages.write(page,{static_cast<std::int16_t>(index%width),static_cast<std::int16_t>(index/width)},
                static_cast<std::uint8_t>(index*17));
        }
        rejects([&] {pages.snapshot(page);}); //The final unknown bit must still reject.
        pages.write(page,{width-1,height-1},19);
        auto bulk=pages.snapshot(page);assert(bulk.size()==pixelCount);
        for(std::int16_t y=0;y<height;++y) {
            for(std::int16_t x=0;x<width;++x) {assert(bulk[static_cast<std::size_t>(y)*width+x]==pages.read(page,{x,y}));}
        }
        bulk.front()=27;assert(pages.read(page,{0,0})==0);
        pages.write(page,{width-1,height-1},31);assert(bulk.back()==19);
    }
    for(const std::uint16_t page:{0x9c00,0xa001,0xac00}) {rejects([&] {pages.snapshot(page);});}
    //An owned page copy may replace known destination bytes with unknown source bytes.
    IndexedPages copied;
    for(std::int16_t y=0;y<height;++y) {
        for(std::int16_t x=0;x<width;++x) {copied.write(0xa000,{x,y},7);}
    }
    assert(copied.snapshot(0xa000).size()==pixelCount);
    copied.write(0xa400,{0,height-1},9);
    copied.apply(std::vector<IndexedPageEffect>{RadarPageCopy{0xa400,0xa000,{0,height-1},width,1}});
    assert(copied.read(0xa000,{0,height-1})==9 && !copied.known(0xa000,{width-1,height-1}));
    rejects([&] {copied.snapshot(0xa000);});
}

}

int main() {
    using namespace stellar7;
    snapshots();
    constexpr std::uint16_t back = 0xa400;
    constexpr std::uint16_t front = 0xa000;
    IndexedPages pages;
    assert(!pages.known(back, {113, 189}));
    rejects([&] { pages.read(back, {113, 189}); });
    const WorldViewSetupInput view{{0, 0, 200000}, {0xc000, 0, 0}, {0, 0, 0},
        0x88, 7, {112, 205, 146, 180}, {256, -256, 362}};
    const IndexedBitmap panel{120, 56, std::vector<std::uint8_t>(120 * 56, 7)};
    RadarRetainedState state{488, 0, {0xa800, back, front, back, 0xa800}};
    // Distinct gaps and aligned overcopy distinguish retained copy from bar repaint.
    pages.write(back, {113, 189}, 29);
    pages.write(back, {179, 195}, 31);
    pages.write(front, {115, 189}, 99);
    const auto startup = radarOperations(state, view, {}, 0, {}, {}, panel, {228, 138});
    pages.apply(startup);
    assert(pages.read(front, {112, 189}) == 239);
    assert(pages.read(front, {113, 189}) == 29);
    assert(pages.read(front, {179, 195}) == 31);
    assert(!pages.known(front, {115, 189}));
    rejects([&] { pages.read(front, {115, 189}); });
    assert(pages.read(back, {96, 127}) == 7);
    assert(!pages.known(front, {96, 127}));

    state = startup.state;
    state.pages.draw = back;
    state.armor = 680;
    pages.write(back, {201, 195}, 51);
    const auto growth = radarOperations(state, view, {}, 0, {}, {}, panel, {228, 138});
    pages.apply(growth);
    assert(pages.read(front, {196, 195}) == 239);
    assert(pages.read(front, {201, 195}) == 51);
    state = growth.state;
    state.pages.draw = back;
    state.armor = 488;
    pages.apply(radarOperations(state, view, {}, 0, {}, {}, panel, {228, 138}));
    assert(pages.read(front, {196, 195}) == 0);
    assert(pages.read(front, {201, 195}) == 51);
    assert(pages.read(front, {113, 189}) == 29);

    rejects([&] { pages.write(0xac00, {0, 0}, 1); });
    rejects([&] { pages.write(back, {320, 0}, 1); });
    rejects([&] { pages.read(back, {-1, 0}); });

    constexpr std::uint16_t cache = 0xa800;
    IndexedScreen cockpitScreen{std::vector<std::uint8_t>(320 * 200, 9), {}};
    cockpitScreen.pixels[0] = 0;
    IndexedPages resourcePage;
    resourcePage.write(cache, {0, 0}, 12);
    resourcePage.apply(cockpitResourcePageEffects(cockpitScreen, cache));
    assert(resourcePage.read(cache, {0, 0}) == 0);
    assert(resourcePage.read(cache, {319, 199}) == 9);
    assert(!resourcePage.known(back, {0, 0}));
    rejects([&] { cockpitResourcePageEffects(cockpitScreen, back); });
    constexpr std::uint16_t recoveredCache = 0xa400;
    constexpr std::uint16_t recoveredBack = 0xa800;
    constexpr auto recoveredLayout = CockpitPageLayout::recoveredStartup;
    constexpr auto invalidLayout = static_cast<CockpitPageLayout>(-1);
    IndexedPages recoveredResource;
    recoveredResource.write(recoveredCache, {0, 0}, 12);
    recoveredResource.write(recoveredBack, {0, 0}, 31);
    recoveredResource.apply(cockpitResourcePageEffects(cockpitScreen, recoveredCache, recoveredLayout));
    assert(recoveredResource.read(recoveredCache, {0, 0}) == 0);
    assert(recoveredResource.read(recoveredCache, {319, 199}) == 9);
    assert(recoveredResource.read(recoveredBack, {0, 0}) == 31);
    assert(!recoveredResource.known(front, {0, 0}));
    rejects([&] { cockpitResourcePageEffects(cockpitScreen, cache, recoveredLayout); });
    rejects([&] { cockpitResourcePageEffects(cockpitScreen, front, recoveredLayout); });
    rejects([&] { cockpitResourcePageEffects(cockpitScreen, recoveredCache, invalidLayout); });
    rejects([&] { cockpitResourcePageEffects(cockpitScreen, cache, invalidLayout); });
    cockpitScreen.pixels.pop_back();
    rejects([&] { cockpitResourcePageEffects(cockpitScreen, cache); });
    rejects([&] { cockpitResourcePageEffects(cockpitScreen, recoveredCache, recoveredLayout); });
    const ProjectedPoint copied{96, 24};
    pages.write(back, {97, 24}, 12);
    const std::vector<IndexedPageEffect> ordered{
        RadarPixelWrite{copied, 0, cache},
        RadarPageCopy{cache, back, copied, 2, 1},
        RadarPixelWrite{copied, 3, cache},
    };
    pages.apply(ordered);
    assert(pages.known(back, copied));
    assert(pages.read(back, copied) == 0);
    assert(pages.read(cache, copied) == 3);
    assert(!pages.known(back, {97, 24}));

    auto staged = pages;
    rejects([&] {
        staged.apply(std::vector<IndexedPageEffect>{
            RadarPixelWrite{copied, 99, back},
            RadarPageCopy{cache, back, {319, 24}, 2, 1},
        });
    });
    assert(pages.read(back, copied) == 0);

    const HorizonWorkState horizon{24, 145, 0, 80, 256, 18,
        HorizonBackground::solid, HorizonOverlay::enabled};
    std::vector<IndexedBitmap> panorama(4, {256, 18, std::vector<std::uint8_t>(256 * 18, 11)});
    panorama[0].pixels[0] = 0;
    panorama[1].pixels.assign(256 * 18, 22);
    IndexedPages composed;
    composed.apply(horizonPageEffects(horizon, panorama, {64, 78}, back));
    assert(composed.read(back, {24, 24}) == 64);
    assert(composed.read(back, {24, 65}) == 64);
    assert(composed.read(back, {24, 66}) == 0);
    assert(composed.read(back, {279, 83}) == 11);
    assert(composed.read(back, {280, 66}) == 22);
    assert(composed.read(back, {295, 83}) == 22);
    assert(composed.read(back, {24, 84}) == 78);
    assert(composed.read(back, {295, 145}) == 78);
    assert(!composed.known(back, {296, 66}));
    assert(!composed.known(back, {24, 146}));
    for (std::int16_t y = 24; y <= 145; ++y) {
        for (std::int16_t x = 24; x <= 295; ++x) {
            assert(composed.known(back, {x, y}));
        }
    }
    CockpitWorkBitmaps cockpit{};
    cockpit.strip = {4, 1, {0, 0, 0, 0}};
    cockpit.indicator = {4, 1, {0, 0, 0, 9}};
    cockpit.overlays.fill({4, 1, {0, 0, 0, 0}});
    const auto cockpitWork = cockpitOperations({140, 4, 80, CockpitPageRelation::differing}, cockpit);
    composed.apply(cockpitPageEffects(cockpitWork, cockpit, cache, back));
    assert(!composed.known(back, {96, 24}));
    assert(!composed.known(back, {215, 41}));
    assert(composed.read(back, {95, 24}) == 64);
    assert(composed.read(back, {128, 121}) == 0);
    assert(composed.read(back, {144, 122}) == 78);
    assert(composed.read(back, {147, 122}) == 9);

    cockpit.matchingPageBackground = {120, 18, std::vector<std::uint8_t>(120 * 18, 7)};
    cockpit.matchingPageBackground.pixels[0] = 0;
    const auto matching = cockpitOperations({140, 4, 80, CockpitPageRelation::matching}, cockpit);
    const auto ordinary = cockpitOperations({matching.state.indicatorX, matching.state.indicatorVelocity,
        80, CockpitPageRelation::differing}, cockpit);
    BitmapFont titleFont{32, 6, 0, std::vector<IndexedBitmap>(64, {8, 6, std::vector<std::uint8_t>(48)})};
    titleFont.glyphs['A' - titleFont.firstCharacter].pixels[0] = 1;
    IndexedPages initialized;
    initialized.write(back, {0, 0}, 99);
    initialized.apply(cockpitCachePageEffects(matching, ordinary, cockpit, titleFont,
        "ANTARES", 239, cache, back));
    assert(initialized.read(cache, {96, 24}) == 0);
    assert(initialized.read(back, {96, 24}) == 0);
    assert(initialized.read(cache, {132, 30}) == 239);
    assert(initialized.read(back, {132, 30}) == 239);
    assert(initialized.read(back, {133, 30}) == 7);
    assert(initialized.read(back, {24, 42}) == 0);
    assert(!initialized.known(cache, {24, 42}));
    assert(!initialized.known(back, {0, 0}));
    initialized.apply(std::vector<IndexedPageEffect>{RadarPageCopy{back, front, {0, 0}, 320, 200}});
    assert(initialized.read(front, {132, 30}) == 239);
    assert(!initialized.known(front, {0, 0}));
    rejects([&] { cockpitCachePageEffects(ordinary, ordinary, cockpit, titleFont, "ANTARES", 239, cache, back); });
    rejects([&] { cockpitCachePageEffects(matching, ordinary, cockpit, titleFont, "\x01", 239, cache, back); });
    IndexedPages recoveredInitialized;
    recoveredInitialized.write(recoveredCache, {0, 0}, 71);
    recoveredInitialized.write(recoveredBack, {0, 0}, 99);
    recoveredInitialized.apply(cockpitCachePageEffects(matching, ordinary, cockpit, titleFont,
        "ANTARES", 239, recoveredCache, recoveredBack, recoveredLayout));
    assert(recoveredInitialized.read(recoveredCache, {0, 0}) == 71);
    assert(recoveredInitialized.read(recoveredBack, {0, 0}) == 71);
    assert(recoveredInitialized.read(recoveredCache, {132, 30}) == 239);
    assert(recoveredInitialized.read(recoveredBack, {132, 30}) == 239);
    assert(recoveredInitialized.read(recoveredBack, {133, 30}) == 7);
    assert(recoveredInitialized.read(recoveredBack, {24, 42}) == 0);
    assert(!recoveredInitialized.known(recoveredCache, {24, 42}));
    assert(!recoveredInitialized.known(front, {0, 0}));
    rejects([&] { cockpitCachePageEffects(matching, ordinary, cockpit, titleFont,
        "ANTARES", 239, recoveredCache, recoveredBack); });
    rejects([&] { cockpitCachePageEffects(matching, ordinary, cockpit, titleFont,
        "ANTARES", 239, cache, back, recoveredLayout); });
    rejects([&] { cockpitCachePageEffects(matching, ordinary, cockpit, titleFont,
        "ANTARES", 239, recoveredCache, front, recoveredLayout); });
    IndexedPages retainedInitialized;
    retainedInitialized.write(recoveredCache, {0, 0}, 71);
    retainedInitialized.write(front, {0, 0}, 99);
    retainedInitialized.apply(cockpitCachePageEffects(matching, ordinary, cockpit, titleFont,
        "ANTARES", 239, recoveredCache, front, recoveredLayout, CockpitCacheDomain::retainedGameplay));
    assert(retainedInitialized.read(recoveredCache, {0, 0}) == 71);
    assert(retainedInitialized.read(front, {0, 0}) == 71);
    assert(retainedInitialized.read(front, {132, 30}) == 239);
    assert(!retainedInitialized.known(recoveredBack, {0, 0}));
    rejects([&] { cockpitCachePageEffects(matching, ordinary, cockpit, titleFont,
        "ANTARES", 239, recoveredCache, recoveredCache, recoveredLayout, CockpitCacheDomain::retainedGameplay); });
    rejects([&] { cockpitCachePageEffects(matching, ordinary, cockpit, titleFont,
        "ANTARES", 239, recoveredCache, front, recoveredLayout, static_cast<CockpitCacheDomain>(255)); });
    rejects([&] { cockpitCachePageEffects(matching, ordinary, cockpit, titleFont,
        "ANTARES", 239, recoveredCache, recoveredCache, recoveredLayout); });
    rejects([&] { cockpitCachePageEffects(matching, ordinary, cockpit, titleFont,
        "ANTARES", 239, recoveredCache, recoveredBack, invalidLayout); });
    rejects([&] { cockpitCachePageEffects(matching, ordinary, cockpit, titleFont,
        "ANTARES", 239, cache, back, invalidLayout); });
    titleFont.glyphs['A' - titleFont.firstCharacter].width = 4;
    rejects([&] { cockpitCachePageEffects(matching, ordinary, cockpit, titleFont, "ANTARES", 239, cache, back); });
    titleFont.glyphs['A' - titleFont.firstCharacter] = {7, 6, std::vector<std::uint8_t>(42)};
    titleFont.glyphs['A' - titleFont.firstCharacter].pixels[0] = 1;
    initialized.apply(cockpitCachePageEffects(matching, ordinary, cockpit, titleFont, "A", 239, cache, back));
    assert(initialized.read(cache, {157, 30}) == 239);
    assert(initialized.read(cache, {156, 30}) == 7);

    BitmapFont scoreFont;
    scoreFont.firstCharacter = 32;
    scoreFont.height = 8;
    scoreFont.glyphs.assign(96, {5, 8, std::vector<std::uint8_t>(40)});
    scoreFont.glyphs['1' - 32].pixels[0] = 1;
    scoreFont.glyphs['2' - 32].pixels[1] = 1;
    scoreFont.glyphs['0' - 32].pixels[2] = 1;
    scoreFont.glyphs['-' - 32].pixels[3] = 1;
    IndexedPages scorePages;
    scorePages.write(back, {270, 155}, 77);
    scorePages.write(back, {271, 155}, 78);
    const auto scoreEffects = scorePageEffects(12, scoreFont, back, front);
    const auto scoreCopy = std::get<RadarPageCopy>(scoreEffects.back());
    assert(scoreCopy.position.x == 240 && scoreCopy.coveredWidth == 32 && scoreCopy.height == 6);
    scorePages.apply(scoreEffects);
    assert(scorePages.read(front, {240, 155}) == 239);
    assert(scorePages.read(front, {246, 155}) == 239);
    assert(scorePages.read(front, {241, 155}) == 0);
    assert(scorePages.read(front, {270, 155}) == 77 && scorePages.read(front, {271, 155}) == 78);
    scorePages.apply(scorePageEffects(0, scoreFont, back, front));
    assert(scorePages.read(front, {242, 155}) == 239);
    assert(scorePages.read(front, {246, 155}) == 0);
    scorePages.apply(scorePageEffects(0xffffffffU, scoreFont, back, front));
    assert(scorePages.read(front, {243, 155}) == 239 && scorePages.read(front, {245, 155}) == 239);
    rejects([&] { scorePageEffects(0, scoreFont, cache, front); });
    auto malformedFont = scoreFont;
    malformedFont.glyphs['0' - 32].pixels.pop_back();
    rejects([&] { scorePageEffects(0, malformedFont, back, front); });
    malformedFont = scoreFont;
    malformedFont.glyphs['0' - 32].pixels[0] = 2;
    rejects([&] { scorePageEffects(0, malformedFont, back, front); });

    std::vector<IndexedBitmap> hudArt(57, {4, 1, {0, 0, 0, 0}});
    for (std::size_t index = 41; index < hudArt.size(); ++index) {
        hudArt[index].pixels[0] = static_cast<std::uint8_t>(index);
    }
    const std::array<std::int8_t, 7> statuses{0, 1, -2, 3, 0, 3, 1};
    const std::array<std::int16_t, 7> hudTimers{0, 1, 0, 0, -2, 0, 0};
    const auto hudEffects = statusPageEffects(statuses, hudTimers, -1, hudArt, back, front);
    const std::array<std::int16_t, 7> copyX{4, 4, 32, 60, 60, 4, 44};
    const std::array<std::uint16_t, 7> copyWidths{36, 36, 32, 32, 32, 48, 52};
    std::size_t copiedPanels = 0;
    for (const auto& effect : hudEffects) {
        if (const auto* copy = std::get_if<RadarPageCopy>(&effect)) {
            assert(copy->position.x == copyX[copiedPanels] && copy->coveredWidth == copyWidths[copiedPanels]);
            assert(copy->sourcePage == back && copy->destinationPage == front);
            ++copiedPanels;
        }
    }
    assert(copiedPanels == 7);
    IndexedPages hudPages;
    hudPages.write(back, {4, 151}, 91);
    hudPages.write(front, {5, 151}, 92);
    hudPages.apply(hudEffects);
    assert(hudPages.read(front, {4, 151}) == 91);
    assert(!hudPages.known(front, {5, 151}));
    assert(hudPages.read(front, {9, 153}) == 146);
    assert(hudPages.read(front, {9, 169}) == 176);
    assert(hudPages.read(front, {37, 153}) == 137);
    assert(hudPages.read(front, {62, 169}) == 176);
    assert(hudPages.read(front, {14, 156}) == 50);
    assert(hudPages.read(front, {17, 170}) == 42);
    assert(hudPages.read(front, {66, 172}) == 45);
    assert(hudPages.read(front, {42, 178}) == 49 && hudPages.read(front, {50, 178}) == 48);
    assert(hudPages.read(front, {15, 156}) == 146); // Transparent zero preserves the fill.
    assert(statuses[2] == -2); //8ed2 normalization does not mutate supplied ownership.
    rejects([&] { statusPageEffects(statuses, hudTimers, 0, hudArt, back, front); });
    auto invalidStatus = statuses;
    invalidStatus[0] = -128;
    rejects([&] { statusPageEffects(invalidStatus, hudTimers, -1, hudArt, back, front); });
    auto malformedArt = hudArt;
    malformedArt[50].pixels.pop_back();
    rejects([&] { statusPageEffects(statuses, hudTimers, -1, malformedArt, back, front); });
    rejects([&] { statusPageEffects(statuses, hudTimers, -1, {}, back, front); });

    for (const auto hudFront : {front, recoveredBack}) {
        const auto hudBack = hudFront == front ? recoveredBack : front;
        IndexedPages recoveredHud;
        recoveredHud.write(recoveredCache, {4, 151}, 87);
        recoveredHud.write(hudBack, {270, 155}, 77);
        recoveredHud.write(hudBack, {271, 155}, 78);
        const auto recoveredScore = scorePageEffects(12, scoreFont, hudBack, hudFront, recoveredLayout);
        const auto& recoveredScoreCopy = std::get<RadarPageCopy>(recoveredScore.back());
        assert(recoveredScoreCopy.sourcePage == hudBack && recoveredScoreCopy.destinationPage == hudFront);
        assert(recoveredScoreCopy.coveredWidth == 32 && recoveredScoreCopy.height == 6);
        recoveredHud.apply(recoveredScore);
        assert(recoveredHud.read(hudFront, {240, 155}) == 239);
        assert(recoveredHud.read(hudFront, {270, 155}) == 77);
        assert(recoveredHud.read(hudFront, {271, 155}) == 78);
        const auto recoveredStatus = statusPageEffects(statuses, hudTimers, -1, hudArt,
            hudBack, hudFront, recoveredLayout);
        std::size_t recoveredPanels = 0;
        for (const auto& effect : recoveredStatus) {
            if (const auto* copy = std::get_if<RadarPageCopy>(&effect)) {
                assert(copy->sourcePage == hudBack && copy->destinationPage == hudFront);
                assert(copy->coveredWidth == copyWidths[recoveredPanels]);
                ++recoveredPanels;
            }
        }
        assert(recoveredPanels == statuses.size());
        recoveredHud.apply(recoveredStatus);
        assert(recoveredHud.read(hudFront, {9, 153}) == 146);
        assert(recoveredHud.read(hudFront, {9, 169}) == 176);
        assert(recoveredHud.read(hudFront, {14, 156}) == 50);
        assert(recoveredHud.read(recoveredCache, {4, 151}) == 87);
        assert(!recoveredHud.known(recoveredCache, {240, 155}));

        IndexedPages repaired;
        const auto repair = statusRepairPageEffects(2, statuses, hudTimers, hudArt,
            hudBack, hudFront, recoveredLayout);
        const auto& repairCopy = std::get<RadarPageCopy>(repair.back());
        assert(repairCopy.sourcePage == hudBack && repairCopy.destinationPage == hudFront);
        assert(repairCopy.position.x == copyX[2] && repairCopy.coveredWidth == copyWidths[2]);
        repaired.apply(repair);
        assert(repaired.read(hudFront, {42, 155}) == 43);
        assert(!repaired.known(hudFront, {14, 156}));
        assert(!repaired.known(recoveredCache, {42, 155}));
    }
    const auto rejectsHudRoles = [&](const std::uint16_t hudBack, const std::uint16_t hudFront,
        const CockpitPageLayout layout) {
        rejects([&] { scorePageEffects(0, scoreFont, hudBack, hudFront, layout); });
        rejects([&] { statusPageEffects(statuses, hudTimers, -1, hudArt, hudBack, hudFront, layout); });
        rejects([&] { statusRepairPageEffects(2, statuses, hudTimers, hudArt, hudBack, hudFront, layout); });
    };
    rejectsHudRoles(recoveredBack, front, CockpitPageLayout::compatibility);
    rejectsHudRoles(recoveredBack, front, invalidLayout);
    rejectsHudRoles(recoveredCache, front, recoveredLayout);
    rejectsHudRoles(front, recoveredCache, recoveredLayout);
    rejectsHudRoles(recoveredCache, recoveredBack, recoveredLayout);
    rejectsHudRoles(recoveredBack, recoveredCache, recoveredLayout);
    rejectsHudRoles(recoveredBack, recoveredBack, recoveredLayout);
    rejectsHudRoles(front, front, recoveredLayout);
    rejectsHudRoles(0xac00, front, recoveredLayout);
}
