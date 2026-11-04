#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace stellar7 {

struct CockpitSettings;

inline constexpr std::size_t highscoreEntryCount = 10;
inline constexpr std::size_t highscoreHeaderBytes = 14;
inline constexpr std::size_t highscoreNameCapacity = 20;
inline constexpr std::size_t highscoreEnteredNameLimit = 15;
inline constexpr const char* highscoreFileName = "FAME.HAL";

struct HighscoreEntry {
    std::uint32_t score = 0;
    std::string name;
};

enum class HighscoreNameMark { ordinary, flagged };
enum class HighscoreReturn { emptyName, recordedName, rankMiss };

class HighscoreTable {
public:
    using Header = std::array<std::uint8_t, highscoreHeaderBytes>;
    using Entries = std::array<HighscoreEntry, highscoreEntryCount>;

    HighscoreTable(Header header, Entries entries);
    const Header& header() const noexcept { return header_; }
    const Entries& entries() const noexcept { return entries_; }
    std::optional<std::size_t> rank(std::uint32_t score) const noexcept;
    bool insert(std::uint32_t score, std::string_view name, HighscoreNameMark mark);
    void setConfiguration(const CockpitSettings& settings) noexcept;

private:
    Header header_;
    Entries entries_;
};

std::optional<HighscoreTable> decodeHighscoreTable(const std::vector<std::uint8_t>& bytes);
std::vector<std::uint8_t> encodeHighscoreTable(const HighscoreTable& table);
std::optional<HighscoreTable> loadHighscoreTable(const std::filesystem::path& path);

enum class HighscoreSaveStatus { saved, openFailed, writeFailed, reloadFailed };
struct HighscoreSaveResult {
    HighscoreSaveStatus status;
    std::optional<HighscoreTable> reloaded;
};
HighscoreSaveResult saveHighscoreTable(const std::filesystem::path& path, const HighscoreTable& table);

class HighscoreStore {
public:
    virtual ~HighscoreStore() = default;
    virtual HighscoreSaveResult saveAndReload(const HighscoreTable& table) = 0;
};

enum class HighscoreWritePolicy { recovered, nativeStagedReplace };

class FileHighscoreStore final : public HighscoreStore {
public:
    explicit FileHighscoreStore(std::filesystem::path path,
        HighscoreWritePolicy policy=HighscoreWritePolicy::recovered);
    HighscoreSaveResult saveAndReload(const HighscoreTable& table) override;

private:
    std::filesystem::path path_;
    HighscoreWritePolicy policy_;
};

struct NameCaretState {
    std::uint16_t countdown;
    std::uint16_t nextColor;
};
inline constexpr std::uint16_t nameCaretReload = 100;
inline constexpr NameCaretState initialNameCaretState{nameCaretReload, 0};

//Native functional text events, separate from held widget keys. This is not
//the original BIOS queue/poll-order or a recovered CPU/device-time interface.
struct NameEntryInput {
    std::vector<std::uint8_t> characters;
};

enum class NameEntryChange { none, appended, erased, caret };
struct NameEntryEdit {
    NameEntryChange change = NameEntryChange::none;
    std::size_t previousLength = 0;
    std::uint8_t caretColor = 0;
};

//33e6 consumes one dequeued ASCII byte; zero denotes an empty character poll.
//The caller owns BIOS/widget queue ordering and the inherited0460/0462 words.
class HighscoreNameEntry {
public:
    explicit HighscoreNameEntry(NameCaretState caret) noexcept;
    NameEntryEdit edit(std::uint8_t character);
    std::string_view name() const noexcept { return name_; }
    NameCaretState caret() const noexcept { return caret_; }

private:
    std::string name_;
    NameCaretState caret_;
};

}
