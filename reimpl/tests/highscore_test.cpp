#include "stellar7/highscore.hpp"
#include "stellar7/cockpit_state.hpp"

#include <cassert>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>

using namespace stellar7;

int main(int argc, char** argv) {
    HighscoreTable::Header header{};
    for (std::size_t index = 0; index < header.size(); ++index) {
        header[index] = static_cast<std::uint8_t>(index + 1);
    }
    HighscoreTable::Entries entries{};
    const std::array<std::uint32_t, highscoreEntryCount> scores{
        5155, 4815, 4470, 3960, 2770, 2100, 1920, 1870, 1820, 1800};
    for (std::size_t index = 0; index < entries.size(); ++index) {
        entries[index] = {scores[index], "PILOT" + std::to_string(index)};
    }
    HighscoreTable table(header, entries);
    auto configured = table;
    configured.setConfiguration({0x1234, {1, 0x5678, -1, 0, 2, 255}});
    const HighscoreTable::Header liveHeader{0x34, 0x12, 1, 0, 0x78, 0x56, 0xff, 0xff,
        0, 0, 2, 0, 0xff, 0};
    assert(configured.header() == liveHeader && configured.entries()[5].score == 2100);
    assert(table.rank(2540) == 5);
    assert(table.rank(2100) == 6);
    assert(!table.rank(1800));
    assert(!table.rank(0x80000000U));
    assert(!table.insert(2540, "", HighscoreNameMark::ordinary));
    assert(table.entries()[5].score == 2100);
    assert(table.insert(2540, "ABC", HighscoreNameMark::ordinary));
    assert(table.entries()[5].score == 2540 && table.entries()[5].name == "ABC");
    assert(table.entries()[6].score == 2100 && table.entries()[9].score == 1820);
    const auto encoded = encodeHighscoreTable(table);
    const auto decoded = decodeHighscoreTable(encoded);
    assert(decoded && decoded->header() == header);
    for (std::size_t index = 0; index < entries.size(); ++index) {
        assert(decoded->entries()[index].score == table.entries()[index].score);
        assert(decoded->entries()[index].name == table.entries()[index].name);
    }
    HighscoreTable::Entries one{};
    one[0] = {2540, "ABC"};
    const auto compact = encodeHighscoreTable(HighscoreTable(header, one));
    std::vector<std::uint8_t> expected(header.begin(), header.end());
    expected.insert(expected.end(), {0x38, 0xca, 0xb2, 0xa1, 0x3e, 0x3d, 0x3c, 0});
    assert(compact == expected); //3010 omits zero scores; no host padding.
    const auto partial = decodeHighscoreTable(compact);
    assert(partial && partial->entries()[0].name == "ABC" && partial->entries()[1].score == 0);
    auto invalid = compact;
    invalid.pop_back();
    assert(!decodeHighscoreTable(invalid));
    invalid = compact;
    invalid.push_back(1);
    assert(!decodeHighscoreTable(invalid));
    assert(!decodeHighscoreTable(std::vector<std::uint8_t>(13)));
    HighscoreTable flagged(header, entries);
    assert(flagged.insert(6000, "ACE", HighscoreNameMark::flagged));
    assert(static_cast<std::uint8_t>(flagged.entries()[0].name.front()) == 0xc1);
    assert(decodeHighscoreTable(encodeHighscoreTable(flagged))->entries()[0].name == flagged.entries()[0].name);

    HighscoreNameEntry editor({0, 0});
    auto edit = editor.edit(0);
    assert(edit.change == NameEntryChange::caret && edit.caretColor == 0);
    assert(editor.caret().countdown == 100 && editor.caret().nextColor == 239);
    for (int poll = 0; poll < 100; ++poll) {
        assert(editor.edit(0).change == NameEntryChange::none);
    }
    edit = editor.edit(0);
    assert(edit.change == NameEntryChange::caret && edit.caretColor == 239);
    assert(editor.caret().countdown == 100 && editor.caret().nextColor == 0);
    for (const auto character : std::string("abz @[_")) {
        editor.edit(static_cast<std::uint8_t>(character));
    }
    assert(editor.name() == "ABZ @");
    assert(editor.caret().countdown == 100 && editor.caret().nextColor == 0);
    edit = editor.edit(8);
    assert(edit.change == NameEntryChange::erased && edit.previousLength == 5);
    assert(editor.name() == "ABZ ");
    for (int count = 0; count < 20; ++count) {
        editor.edit('x');
    }
    assert(editor.name().size() == highscoreEnteredNameLimit);
    assert(editor.edit('A').change == NameEntryChange::none);
    assert(editor.edit(27).change == NameEntryChange::none);
    assert(editor.edit(13).change == NameEntryChange::none);
    assert(editor.edit(0xff).change == NameEntryChange::none);
    for (int count = 0; count < 20; ++count) {
        editor.edit(8);
    }
    assert(editor.name().empty() && editor.edit(8).change == NameEntryChange::none);

    if (argc >= 2) {
        const auto directory = std::filesystem::path(argv[1]) / "highscore-store-test";
        std::filesystem::create_directories(directory);
        const auto path = directory / "FAME.HAL";
        const auto saved = saveHighscoreTable(path, table);
        assert(saved.status == HighscoreSaveStatus::saved && saved.reloaded);
        assert(saved.reloaded->entries()[5].name == "ABC");
        assert(!loadHighscoreTable(directory / "missing"));
        const auto failed = saveHighscoreTable(directory / "absent" / "FAME.HAL", table);
        assert(failed.status == HighscoreSaveStatus::openFailed && !failed.reloaded);
        FileHighscoreStore store(directory / "configured.hal");
        const auto stored = store.saveAndReload(configured);
        assert(stored.status == HighscoreSaveStatus::saved && stored.reloaded->header() == liveHeader);
    }
    if (argc >= 3) {
        const auto original = loadHighscoreTable(argv[2]);
        assert(original && original->rank(2540) == 5);
        std::ifstream file(argv[2], std::ios::binary);
        const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        assert(encodeHighscoreTable(*original) == bytes);
    }
}
