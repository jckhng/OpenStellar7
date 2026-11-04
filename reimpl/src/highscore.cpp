#include "stellar7/highscore.hpp"
#include "stellar7/cockpit_state.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace stellar7 {
namespace {
constexpr std::uint32_t scoreMask = 0xa1b2c3d4U;
constexpr std::uint32_t scoreSignBit = 0x80000000U;
constexpr std::uint8_t nameMask = 0x7f;
constexpr std::uint8_t completionMark = 0x80;
constexpr std::uint16_t editorColor = 239;

constexpr std::size_t maximumStagingAttempts=8;
class StagedHighscoreFile {
public:
    explicit StagedHighscoreFile(const std::filesystem::path& target):target_(target) {}
    ~StagedHighscoreFile() {
        if(!owned_) {return;}
        std::error_code ignored;
        if(!committed_) {std::filesystem::remove(file_,ignored);}
        std::filesystem::remove(directory_,ignored);
    }
    HighscoreSaveResult save(const HighscoreTable& table) {
        if(!create()) {return {HighscoreSaveStatus::openFailed,std::nullopt};}
        auto result=saveHighscoreTable(file_,table);
        if(result.status!=HighscoreSaveStatus::saved) {return result;}
        if(!result.reloaded||encodeHighscoreTable(*result.reloaded)!=encodeHighscoreTable(table)) {
            return {HighscoreSaveStatus::reloadFailed,std::nullopt};
        }
        if(!replace()) {return {HighscoreSaveStatus::writeFailed,std::nullopt};}
        committed_=true;
        return result; //The verified staged bytes are the committed bytes; no second failing reload.
    }
private:
    bool create() {
        for(std::size_t attempt=0;attempt<maximumStagingAttempts;++attempt) {
            directory_=target_.parent_path()/target_.filename();
            directory_+=".save-tmp-"+std::to_string(attempt);
            std::error_code error;
            owned_=std::filesystem::create_directory(directory_,error);
            if(owned_) {file_=directory_/target_.filename();return true;}
            if(error&&error!=std::errc::file_exists) {return false;}
        }
        return false;
    }
    bool replace() {
#ifdef _WIN32
        return MoveFileExW(file_.c_str(),target_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
        std::error_code error;
        std::filesystem::rename(file_,target_,error);
        return !error;
#endif
    }
    std::filesystem::path target_,directory_,file_;
    bool owned_=false,committed_=false;
};

bool validStoredName(const std::string_view name) noexcept {
    return name.size() < highscoreNameCapacity && name.find('\0') == std::string_view::npos
        && name.find(static_cast<char>(nameMask)) == std::string_view::npos;
}
}

HighscoreTable::HighscoreTable(Header header, Entries entries)
    : header_(header), entries_(std::move(entries)) {
    if (std::any_of(entries_.begin(), entries_.end(),
        [](const auto& entry) { return !validStoredName(entry.name); })) {
        throw std::invalid_argument("unsupported highscore name storage");
    }
}

std::optional<std::size_t> HighscoreTable::rank(const std::uint32_t score) const noexcept {
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        //330a compares signed high words, then unsigned low words on a tie.
        if ((score ^ scoreSignBit) > (entries_[index].score ^ scoreSignBit)) {
            return index;
        }
    }
    return std::nullopt;
}

bool HighscoreTable::insert(const std::uint32_t score, const std::string_view name,
    const HighscoreNameMark mark) {
    if (name.empty()) {
        return false;
    }
    if (name.size() > highscoreEnteredNameLimit || std::any_of(name.begin(), name.end(),
        [](const unsigned char character) { return character < 0x20 || character > 0x5a; })) {
        throw std::invalid_argument("highscore insertion requires source-edited name");
    }
    const auto position = rank(score);
    if (!position) {
        return false;
    }
    for (auto index = entries_.size() - 1; index > *position; --index) {
        entries_[index] = entries_[index - 1];
    }
    entries_[*position] = {score, std::string(name)};
    if (mark == HighscoreNameMark::flagged) {
        auto& first = entries_[*position].name.front();
        first = static_cast<char>(static_cast<std::uint8_t>(first) | completionMark);
    }
    return true;
}

void HighscoreTable::setConfiguration(const CockpitSettings& settings) noexcept {
    const auto putWord = [this](const std::size_t offset, const std::int16_t value) {
        const auto word = static_cast<std::uint16_t>(value);
        header_[offset] = static_cast<std::uint8_t>(word);
        header_[offset + 1] = static_cast<std::uint8_t>(word >> 8);
    };
    putWord(0, settings.viewSize);
    for (std::size_t index = 0; index < settings.options.size(); ++index) {
        putWord((index + 1) * sizeof(std::uint16_t), settings.options[index]);
    }
}

std::optional<HighscoreTable> decodeHighscoreTable(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() < highscoreHeaderBytes) {
        return std::nullopt;
    }
    HighscoreTable::Header header{};
    std::copy_n(bytes.begin(), header.size(), header.begin());
    HighscoreTable::Entries entries{};
    auto position = header.size();
    for (auto& entry : entries) {
        if (position == bytes.size()) {
            break; //Normal3010->319a->30d6 reload owns zero trailing scores.
        }
        if (bytes.size() - position < sizeof(entry.score)) {
            return std::nullopt;
        }
        for (std::size_t byte = 0; byte < sizeof(entry.score); ++byte) {
            entry.score |= static_cast<std::uint32_t>(bytes[position++]) << (byte * 8);
        }
        entry.score ^= scoreMask;
        const auto end = std::find(bytes.begin() + position, bytes.end(), 0);
        if (end == bytes.end() || static_cast<std::size_t>(end - bytes.begin() - position) >= highscoreNameCapacity) {
            return std::nullopt;
        }
        while (position < static_cast<std::size_t>(end - bytes.begin())) {
            entry.name.push_back(static_cast<char>(bytes[position++] ^ nameMask));
        }
        ++position;
        if (!validStoredName(entry.name)) {
            return std::nullopt;
        }
    }
    return position == bytes.size() ? std::optional<HighscoreTable>{HighscoreTable(header, std::move(entries))}
        : std::nullopt;
}

std::vector<std::uint8_t> encodeHighscoreTable(const HighscoreTable& table) {
    std::vector<std::uint8_t> bytes(table.header().begin(), table.header().end());
    for (const auto& entry : table.entries()) {
        if (entry.score == 0) {
            continue;
        }
        const auto encoded = entry.score ^ scoreMask;
        for (std::size_t byte = 0; byte < sizeof(encoded); ++byte) {
            bytes.push_back(static_cast<std::uint8_t>(encoded >> (byte * 8)));
        }
        for (const unsigned char character : entry.name) {
            bytes.push_back(character ^ nameMask);
        }
        bytes.push_back(0);
    }
    return bytes;
}

std::optional<HighscoreTable> loadHighscoreTable(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }
    return decodeHighscoreTable({std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()});
}

HighscoreSaveResult saveHighscoreTable(const std::filesystem::path& path, const HighscoreTable& table) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return {HighscoreSaveStatus::openFailed, std::nullopt};
    }
    const auto bytes = encodeHighscoreTable(table);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    output.close();
    if (!output) {
        return {HighscoreSaveStatus::writeFailed, std::nullopt};
    }
    auto reloaded = loadHighscoreTable(path);
    return {reloaded ? HighscoreSaveStatus::saved : HighscoreSaveStatus::reloadFailed, std::move(reloaded)};
}

FileHighscoreStore::FileHighscoreStore(std::filesystem::path path,const HighscoreWritePolicy policy)
    :path_(std::move(path)),policy_(policy) {
    if(policy!=HighscoreWritePolicy::recovered&&policy!=HighscoreWritePolicy::nativeStagedReplace) {
        throw std::invalid_argument("invalid highscore write policy");
    }
}

HighscoreSaveResult FileHighscoreStore::saveAndReload(const HighscoreTable& table) {
    if(policy_==HighscoreWritePolicy::nativeStagedReplace) {return StagedHighscoreFile(path_).save(table);}
    return saveHighscoreTable(path_, table);
}

HighscoreNameEntry::HighscoreNameEntry(const NameCaretState caret) noexcept : caret_(caret) {}

NameEntryEdit HighscoreNameEntry::edit(std::uint8_t character) {
    NameEntryEdit result{NameEntryChange::none, name_.size(), 0};
    if (character == 0) {
        const auto incoming = caret_.countdown;
        --caret_.countdown;
        if (incoming == 0) {
            result.change = NameEntryChange::caret;
            result.caretColor = static_cast<std::uint8_t>(caret_.nextColor);
            caret_.countdown = nameCaretReload;
            caret_.nextColor = caret_.nextColor == 0 ? editorColor : 0;
        }
        return result;
    }
    if (character >= 'a' && character <= 'z') {
        character &= 0x5f;
    }
    if (character == 8) {
        if (!name_.empty()) {
            name_.pop_back();
            result.change = NameEntryChange::erased;
        }
        return result;
    }
    if (character < 0x20 || character > 0x5a || name_.size() >= highscoreEnteredNameLimit) {
        return result;
    }
    name_.push_back(static_cast<char>(character));
    result.change = NameEntryChange::appended;
    return result;
}

}
