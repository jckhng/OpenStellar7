#include "playable_startup_resources.hpp"
#include "briefing_pages.hpp"
#include "briefing_preview.hpp"
#include "briefing_strip.hpp"

#include <array>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
using namespace stellar7;
constexpr std::uint16_t back = 0xa800;
constexpr std::array<std::size_t, briefingPageCount> lineCounts{
    4,2,2,2,3,3,3,2,2,3,2,2,3,3,3,3,2,6,3};
constexpr std::array<int, briefingPageCount> sourceTypes{
    -1,1,2,3,4,5,250,8,10,11,13,29,25,31,44,0,-1,-1,-1};

void require(const bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template<class Action>
void rejects(Action action, const char* message) {
    bool rejected = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, message);
}

std::vector<std::uint8_t> pane(const std::vector<IndexedPageEffect>& effects) {
    IndexedPages pages;
    pages.apply(effects);
    std::vector<std::uint8_t> pixels;
    pixels.reserve(246 * 75);
    for (std::int16_t y = 35; y <= 109; ++y) {
        for (std::int16_t x = 50; x <= 295; ++x) {
            pixels.push_back(pages.read(back, {x,y}));
        }
    }
    return pixels;
}

std::vector<std::uint8_t> image(BriefingViewer& viewer, const GameplayAssets& assets) {
    auto effects = briefingStripEffects(viewer.stripIndex(), assets, back);
    const auto model = viewer.poll(assets, back);
    for (const auto& effect : model) {
        const auto* write = std::get_if<RadarPixelWrite>(&effect);
        require(write && write->page == back && write->point.x >= 50 && write->point.x <= 294
            && write->point.y >= 35 && write->point.y <= 109, "preview model escaped source viewport/page");
    }
    effects.insert(effects.end(), model.begin(), model.end());
    return pane(effects);
}

void parser(const GameplayAssets& assets) {
    const auto original = assets.briefingText;
    const auto text = parseBriefingText(original);
    for (std::size_t page = 0; page < text.size(); ++page) {
        require(text[page].size() == lineCounts[page], "actual TXT0 page lost source line boundaries");
        const auto preview = briefingPreview(page);
        require(preview.has_value() == (sourceTypes[page] >= 0), "preview on intro/closing page");
        if (preview) {
            require(preview->model == sourceTypes[page] && !preview->name.empty(), "wrong source preview identity");
        }
        require(!briefingTextEffects(text,page,assets.scoreFont,back).empty(), "source body text painted nothing");
    }
    require(original == assets.briefingText, "parser mutated decoded resource");
    rejects([&] { parseBriefingText({}); }, "empty TXT0 accepted");
    auto invalid = original;
    invalid.front() = '0';
    rejects([&] { parseBriefingText(invalid); }, "early terminal page accepted");
    invalid = original;
    invalid[1] = '\n';
    rejects([&] { parseBriefingText(invalid); }, "invalid CRLF accepted");
    invalid = original;
    invalid.pop_back();
    rejects([&] { parseBriefingText(invalid); }, "truncated terminal CRLF accepted");
    invalid = original;
    invalid.push_back('X');
    rejects([&] { parseBriefingText(invalid); }, "trailing TXT bytes accepted");
    rejects([&] { briefingPreview(briefingPageCount); }, "twentieth page accepted");
}

void allModels(const GameplayAssets& assets) {
    std::size_t visited = 0;
    for (std::size_t page = 1; page <= 15; ++page) {
        BriefingViewer viewer(assets);
        viewer.select(page,assets);
        const auto effects = viewer.poll(assets,back);
        require(effects.empty() == (sourceTypes[page] == 250), "model missing, or fictional STALKER geometry");
        require(!briefingDetailsEffects(page,assets.titleFont,assets.scoreFont,back).empty(),
            "source model name/statistics missing");
        image(viewer,assets);
        viewer.control(BriefingViewerControl::zoomIn);
        image(viewer,assets);
        viewer.control(BriefingViewerControl::zoomOut);
        image(viewer,assets);
        ++visited;
    }
    require(visited == 15, "did not visit all fifteen source preview types");
    BriefingViewer hidden(assets);
    for (const std::size_t page : {0U,16U,17U,18U}) {
        hidden.select(page,assets);
        require(hidden.poll(assets,back).empty(), "intro/closing text drew a model");
    }
}

void controls(const GameplayAssets& assets) {
    BriefingViewer moving(assets);
    moving.select(1,assets); //Actual asymmetric SANDSLED model.
    const auto first = image(moving,assets);
    bool rotated = false;
    for (unsigned poll = 0; poll < 32; ++poll) {
        rotated = image(moving,assets) != first || rotated;
    }
    require(rotated, "source foreground polls did not rotate the real model");
    moving.control(BriefingViewerControl::stop);
    const auto stopped = image(moving,assets);
    for (unsigned poll = 0; poll < 8; ++poll) {
        require(image(moving,assets) == stopped, "STOP did not freeze primary preview pose");
    }
    BriefingViewer normal(assets);
    normal.select(1,assets);
    auto faster = normal;
    faster.control(BriefingViewerControl::faster);
    bool accelerated = false;
    for (unsigned poll = 0; poll < 32; ++poll) {
        accelerated = image(normal,assets) != image(faster,assets) || accelerated;
    }
    require(accelerated, "FASTER kept the normal pose sequence");

    BriefingViewer zoom(assets);
    zoom.select(1,assets);
    zoom.control(BriefingViewerControl::stop);
    const auto original = image(zoom,assets);
    require(!zoom.control(BriefingViewerControl::zoomIn), "first zoom unexpectedly hit strip boundary");
    require(image(zoom,assets) != original, "zoom changed neither model scale nor position");
    require(!zoom.control(BriefingViewerControl::zoomOut), "inverse zoom unexpectedly hit strip boundary");
    require(image(zoom,assets) == original, "inverse zoom failed to restore stopped source pose");
    for (unsigned step = 0; step < 10; ++step) {
        require(!zoom.control(BriefingViewerControl::zoomIn), "source ten-step near range shortened");
    }
    image(zoom,assets);
    require(zoom.control(BriefingViewerControl::zoomIn), "near-boundary strip branch missing");
    image(zoom,assets); //A foreground model can legitimately occlude changed grid pixels.
    //Strip itself is the independent observation: no new actor/state getters.
    require(pane(briefingStripEffects(0,assets,back)) != pane(briefingStripEffects(1,assets,back)),
        "source positive strip index is visually ignored");
    require(pane(briefingStripEffects(0,assets,back)) != pane(briefingStripEffects(-1,assets,back)),
        "source negative strip index is visually ignored");
}

void stagedRejection(const GameplayAssets& assets) {
    BriefingViewer actual(assets);
    actual.select(1,assets);
    image(actual,assets);
    auto expected = actual;
    auto invalid = assets;
    invalid.modelColorTables.clear();
    rejects([&] { actual.poll(invalid,back); }, "missing material tables accepted");
    require(image(actual,assets) == image(expected,assets), "rejected draw advanced retained yaw");

    BriefingViewer fuelbay(assets);
    fuelbay.select(13,assets);
    auto expectedFuelbay = fuelbay;
    invalid = assets;
    invalid.models.at(24).animationLimits.clear();
    rejects([&] { fuelbay.poll(invalid,back); }, "missing post-draw reference limits accepted");
    require(image(fuelbay,assets) == image(expectedFuelbay,assets),
        "rejection after reference draw committed actor/animation changes");

    invalid = assets;
    invalid.uiPalette.pop_back();
    rejects([&] { briefingStripEffects(0,invalid,back); }, "incomplete strip palette accepted");
}

void run(const std::filesystem::path& root) {
    const auto assets = naturalRoute::assets(root);
    require(assets.uiPalette.size() == 256, "actual resource fixture does not own all256 UI palette entries");
    parser(assets);
    allModels(assets);
    controls(assets);
    stagedRejection(assets);
    std::cout << "Briefing actual19pages/15types, controls and staged rejection PASS\n";
}
}

int main(const int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    try {
        run(argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
