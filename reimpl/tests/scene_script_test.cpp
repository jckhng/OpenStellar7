#include "stellar7/scene_script.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>

namespace {

using namespace stellar7;
constexpr RadarPages pages{0xa800, 0xa400, 0xa000, 0xa400, 0xa400};

SceneCommand command(const SceneCommandKind kind, const std::array<std::uint16_t, 4> arguments = {}) {
    return {kind, arguments, {}, 0};
}

void word(std::vector<std::uint8_t>& bytes, const std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
}

template<class Function>
void rejects(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::exception&) {
        rejected = true;
    }
    assert(rejected);
}

void testDecodeBoundaries() {
    std::vector<std::uint8_t> bytes;
    word(bytes, 0xf10f);
    bytes.insert(bytes.end(), {'A', 'B', 0, 0x55});
    word(bytes, 0x1021);
    word(bytes, 6);
    word(bytes, 0x0ff0);
    const auto program = decodeSceneProgram(bytes, 1);
    assert(program.pages.size() == 1 && program.pages[0].size() == 3);
    assert(program.pages[0][0].text == "AB");
    assert(program.pages[0][1].sourceOffset == 6);
    rejects([&] { decodeSceneProgram(bytes, 2); });
    rejects([&] { decodeSceneProgram(bytes, 0); });
    bytes.pop_back();
    rejects([&] { decodeSceneProgram(bytes, 1); });
    rejects([] { decodeSceneProgram({0x0f, 0xf1, 'A'}, 1); });
    rejects([] { decodeSceneProgram({0x0f, 0xf1, 0}, 1); });
    rejects([] { decodeSceneProgram({0x01, 0x13, 0, 0, 0xf0, 0x0f}, 1); });
}

SceneResource fixture() {
    SceneResource resource;
    resource.assets.emplace_back(std::vector<Rgb>(256, {0, 0, 0}));
    resource.assets.emplace_back(std::vector<IndexedBitmap>{{8, 1, {7, 0, 7, 7, 7, 7, 7, 7}}});
    resource.assets.emplace_back(std::vector<std::uint8_t>(320 * 200, 1));
    resource.program.pages = {
        {command(SceneCommandKind::loadPalette), command(SceneCommandKind::loadBitmaps),
            command(SceneCommandKind::loadBackground), command(SceneCommandKind::endPage)},
        {command(SceneCommandKind::selectFrame), command(SceneCommandKind::drawBitmap, {5, 0, 8, 1}),
            command(SceneCommandKind::endPage)},
        {command(SceneCommandKind::selectFrame), command(SceneCommandKind::drawBitmap, {13, 0, 8, 1}),
            command(SceneCommandKind::endPage)},
        {command(SceneCommandKind::endPage)},
    };
    return resource;
}

void applyPixels(const ScenePageWork& work, const SceneResource& resource, IndexedPages& pixels,
    const BitmapFont& font = {}) {
    for (const auto& operation : work.operations) {
        if (std::holds_alternative<ScenePaletteChange>(operation)
            || std::holds_alternative<SceneSoundRequest>(operation)) {
            continue;
        }
        pixels.apply(scenePagePixelEffects(operation, resource, font, pages.back));
    }
}

void testRetainedDirtyPages() {
    const auto resource = fixture();
    ScenePageInterpreter interpreter(resource, pages, 0, 0);
    IndexedPages pixels;
    applyPixels(interpreter.execute(0), resource, pixels);
    applyPixels(interpreter.execute(1), resource, pixels);
    assert(pixels.read(pages.front, {5, 0}) == 7);
    assert(pixels.read(pages.front, {6, 0}) == 1); // Transparent zero retains the background.
    const auto moved = interpreter.execute(2);
    assert(moved.operations.size() == 4);
    const auto& restore = std::get<RadarPageCopy>(moved.operations[0]);
    assert(restore.sourcePage == pages.cached && restore.destinationPage == pages.back);
    assert(restore.position.x == 4 && restore.coveredWidth == 20 && restore.height == 3);
    const auto& newPresent = std::get<RadarPageCopy>(moved.operations[2]);
    const auto& oldPresent = std::get<RadarPageCopy>(moved.operations[3]);
    assert(newPresent.position.x == 12 && oldPresent.position.x == 4);
    applyPixels(moved, resource, pixels);
    assert(pixels.read(pages.front, {5, 0}) == 1);
    assert(pixels.read(pages.front, {13, 0}) == 7);
    assert(pixels.read(pages.cached, {13, 0}) == 1);
    applyPixels(interpreter.execute(3), resource, pixels);
    assert(pixels.read(pages.front, {13, 0}) == 1);
    const auto absent = interpreter.execute(4);
    assert(!absent.present && absent.operations.empty());
}

void testCacheCommitAndRejection() {
    auto resource = fixture();
    resource.program.pages[2] = {command(SceneCommandKind::cacheRectangle, {0, 0, 32, 4}),
        command(SceneCommandKind::endPage)};
    resource.program.pages.push_back({command(SceneCommandKind::selectFrame, {1}),
        command(SceneCommandKind::endPage)});
    ScenePageInterpreter interpreter(resource, pages, 0, 0);
    IndexedPages pixels;
    applyPixels(interpreter.execute(0), resource, pixels);
    applyPixels(interpreter.execute(1), resource, pixels);
    rejects([&] { interpreter.execute(4); });
    const auto commit = interpreter.execute(2);
    // Failed qualification did not consume the current restore/previous queue.
    assert(std::get<RadarPageCopy>(commit.operations[0]).sourcePage == pages.cached);
    applyPixels(commit, resource, pixels);
    assert(pixels.read(pages.front, {5, 0}) == 1);

    resource = fixture();
    resource.program.pages[1].insert(resource.program.pages[1].end() - 1,
        command(SceneCommandKind::cacheRectangle, {0, 0, 32, 4}));
    ScenePageInterpreter committed(resource, pages, 0, 0);
    IndexedPages persistent;
    applyPixels(committed.execute(0), resource, persistent);
    applyPixels(committed.execute(1), resource, persistent);
    applyPixels(committed.execute(3), resource, persistent);
    assert(persistent.read(pages.front, {5, 0}) == 7);
    assert(persistent.read(pages.cached, {5, 0}) == 7);
}

void testTimingSoundAndCapacity() {
    SceneResource resource;
    auto title = command(SceneCommandKind::text0);
    title.text = "TITLE";
    auto line = command(SceneCommandKind::text2);
    line.text = "BODY";
    resource.program.pages = {{command(SceneCommandKind::setDelay, {6}),
        command(SceneCommandKind::sound, {0, 3}), title, line,
        command(SceneCommandKind::setColors, {6, 5}),
        command(SceneCommandKind::dialogue, {9, 8, 187, 50}), command(SceneCommandKind::endPage)},
        {command(SceneCommandKind::metadata1111, {2}), command(SceneCommandKind::metadata1201, {2}),
            command(SceneCommandKind::sound), command(SceneCommandKind::endPage)}};
    ScenePageInterpreter interpreter(resource, pages, 0, 0);
    const auto first = interpreter.execute(0);
    assert(first.delayIrqCounts == 24);
    const auto& voice = std::get<SceneSoundRequest>(first.operations[0]);
    assert(voice.operation == SceneSoundOperation::loadVoice && voice.argument == 3);
    const auto& dialogue = std::get<SceneDialogueDraw>(first.operations[1]);
    assert(dialogue.text[0] == "TITLE" && dialogue.text[1].empty() && dialogue.text[2] == "BODY");
    assert(dialogue.primary == 6 && dialogue.secondary == 5);
    const auto next = interpreter.execute(1);
    assert(next.delayIrqCounts == 24); // Metadata words do not change persistent1021.
    const auto& sound = std::get<SceneSoundRequest>(next.operations[1]);
    assert(sound.operation == SceneSoundOperation::playSequence && sound.argument == 0);

    auto crowded = fixture();
    crowded.program.pages[1].assign(101, command(SceneCommandKind::drawBitmap, {0, 0, 8, 1}));
    crowded.program.pages[1].insert(crowded.program.pages[1].begin(), command(SceneCommandKind::selectFrame));
    crowded.program.pages[1].push_back(command(SceneCommandKind::endPage));
    ScenePageInterpreter bounded(crowded, pages, 0, 0);
    bounded.execute(0);
    rejects([&] { bounded.execute(1); });
    assert(bounded.execute(3).operations.empty());
}

void testDialoguePixels() {
    BitmapFont font{32, 8, 0, std::vector<IndexedBitmap>(96, {2, 8, std::vector<std::uint8_t>(16)})};
    font.glyphs['A' - font.firstCharacter].pixels[0] = 1;
    SceneDialogueDraw dialogue{{9, 8, 187, 50}, {}, 6, 5};
    dialogue.text[0] = "A";
    dialogue.text[2] = "AA";
    IndexedPages pixels;
    pixels.apply(scenePagePixelEffects(dialogue, {}, font, pages.back));
    assert(pixels.read(pages.back, {9, 8}) == 4);
    assert(pixels.read(pages.back, {10, 9}) == 3);
    assert(pixels.read(pages.back, {103, 17}) == 0); // Title: proportional width, centered.
    assert(pixels.read(pages.back, {104, 17}) == 5); // Transparent mask preserves the header.
    assert(pixels.read(pages.back, {22, 25}) == 6); // Slot1 intentionally stays blank.
    assert(pixels.read(pages.back, {22, 32}) == 0);
    assert(pixels.read(pages.back, {24, 32}) == 0);
    assert(pixels.read(pages.back, {23, 32}) == 6);
    dialogue.text[0] = std::string(89, 'A');
    IndexedPages gated;
    gated.apply(scenePagePixelEffects(dialogue, {}, font, pages.back));
    assert(gated.read(pages.back, {22, 32}) == 6); // Source gates every line by TITLE width.
}

std::vector<std::uint8_t> read(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("missing scene test resource: " + path.string());
    }
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void testSuppliedScene(const std::filesystem::path& root, const std::filesystem::path& output) {
    const auto resource = decodeSceneResource(read(root / "SCENED.RES"));
    const auto fonts = decodeFontResources(read(root / "STELLAR.RES"));
    assert(resource.program.pages.size() == 43 && resource.assets.size() == 3 && fonts.size() == 2);
    assert(std::get<std::vector<IndexedBitmap>>(resource.assets[1]).size() == 16);
    ScenePageInterpreter interpreter(resource, pages, 0, 0);
    IndexedPages pixels;
    std::filesystem::create_directories(output);
    std::ofstream report(output / "operations.jsonl");
    std::size_t commands = 0, bitmapDraws = 0, dialogueDraws = 0, soundCalls = 0;
    std::set<SceneCommandKind> kinds;
    for (std::size_t index = 0; index <= resource.program.pages.size(); ++index) {
        if (index < resource.program.pages.size()) {
            for (const auto& item : resource.program.pages[index]) {
                ++commands;
                kinds.insert(item.kind);
            }
        }
        const auto work = interpreter.execute(index);
        assert(work.present == (index < resource.program.pages.size()));
        for (const auto& operation : work.operations) {
            bitmapDraws += std::holds_alternative<SceneBitmapDraw>(operation);
            dialogueDraws += std::holds_alternative<SceneDialogueDraw>(operation);
            soundCalls += std::holds_alternative<SceneSoundRequest>(operation);
        }
        applyPixels(work, resource, pixels, fonts[1]);
        report << "{\"page\":" << index << ",\"operations\":" << work.operations.size()
            << ",\"present\":" << (work.present ? "true" : "false")
            << ",\"requested_irq_counts\":" << work.delayIrqCounts << "}\n";
        for (const auto& role : {std::pair<const char*, std::uint16_t>{"front", pages.front},
                {"back", pages.back}, {"cache", pages.cached}}) {
            std::ofstream frame(output / ("page-" + std::to_string(index) + "." + role.first + ".bin"), std::ios::binary);
            for (std::int16_t y = 0; y < 200; ++y) {
                for (std::int16_t x = 0; x < 320; ++x) {
                    frame.put(static_cast<char>(pixels.read(role.second, {x, y})));
                }
            }
        }
    }
    assert(commands == 356 && kinds.size() == 20 && bitmapDraws == 151 && dialogueDraws == 1 && soundCalls == 2);
    std::cout << "43 pages plus absent-step retirement;356 commands;20 families;151 bitmap draws;1 dialogue;2 sound requests;"
        "pixels only, audio and IRQ waits not executed\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        testDecodeBoundaries();
        testRetainedDirtyPages();
        testCacheCommitAndRejection();
        testTimingSoundAndCapacity();
        testDialoguePixels();
        if (argc == 3) {
            testSuppliedScene(argv[1], argv[2]);
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
