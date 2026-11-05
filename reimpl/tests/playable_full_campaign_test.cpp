#include "gameplay_asset_loader.hpp"
#include "resource_file.hpp"
#include "playable_campaign_physical_comparison.hpp"
#include "stellar7/recovered_frontend.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
using namespace stellar7;
using test::requireCampaignComparison;
constexpr unsigned campaignCalls = 27733;
constexpr unsigned levelCount = 7;
constexpr unsigned allLevels = (1U << levelCount) - 1U;
constexpr unsigned historyInterval = 256;

class MemoryStore final : public HighscoreStore {
public:
    HighscoreSaveResult saveAndReload(const HighscoreTable& table) override {
        return {HighscoreSaveStatus::saved, table};
    }
};

struct InputRun {
    unsigned count, phase, bank, keys, action;
    std::string text;
};

std::vector<InputRun> readInputs(const std::filesystem::path& path) {
    std::ifstream file(path);
    std::string line;
    requireCampaignComparison(bool(std::getline(file, line)) && line == "stellar7-campaign-inputs-v1",
        "missing or unsupported campaign input fixture");
    std::vector<InputRun> result;
    unsigned calls = 0;
    while (std::getline(file, line)) {
        InputRun run{};
        std::istringstream row(line);
        requireCampaignComparison(bool(row >> run.count >> run.phase >> run.bank >> run.keys >> run.action >> run.text),
            "invalid campaign input run");
        row >> std::ws;
        requireCampaignComparison(row.eof() && run.count > 0 && run.count <= campaignCalls - calls
            && run.phase <= static_cast<unsigned>(GameplayPhase::playable) && run.bank >= 1 && run.bank <= levelCount + 1
            && run.keys < 64 && run.action <= static_cast<unsigned>(GameplaySystemAction::calibrateJoystick),
            "campaign input run outside supported domain");
        requireCampaignComparison(run.text == "-" || (run.text.size() <= highscoreEnteredNameLimit
            && run.text.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") == std::string::npos),
            "invalid campaign name input");
        calls += run.count;
        result.push_back(std::move(run));
    }
    requireCampaignComparison(file.eof() && calls == campaignCalls, "incomplete campaign input fixture");
    return result;
}

// FNV-1a over explicit little-endian fields; never hash C++ object padding.
class Digest {
public:
    void byte(const std::uint8_t value) {
        value_ ^= value;
        value_ *= prime;
    }
    void word(const std::uint64_t value) {
        for (unsigned shift = 0; shift < 64; shift += 8) {
            byte(static_cast<std::uint8_t>(value >> shift));
        }
    }
    template<class T> void field(const T value) {
        static_assert(std::is_integral_v<T> || std::is_enum_v<T>);
        word(static_cast<std::uint64_t>(value));
    }
    template<class T, std::size_t Size> void field(const std::array<T, Size>& values) {
        for (const auto& value : values) {
            field(value);
        }
    }
    void actor(const Level1Entity& value) {
        std::apply([this](const auto&... fields) { (field(fields), ...); },
            test::campaignActorFields(value));
    }
    std::uint64_t value() const { return value_; }
private:
    static constexpr std::uint64_t prime = 1099511628211ULL;
    static constexpr std::uint64_t offsetBasis = 14695981039346656037ULL;
    std::uint64_t value_ = offsetBasis;
};

struct Observations {
    Digest state, pixels, audio, events, projectiles;
    std::size_t oplCursor = 0;

    void screen(const IndexedScreen& frame) {
        pixels.word(frame.pixels.size());
        for (const auto pixel : frame.pixels) { pixels.byte(pixel); }
        pixels.word(frame.palette.size());
        for (const auto color : frame.palette) {
            pixels.byte(color.red); pixels.byte(color.green); pixels.byte(color.blue);
        }
    }
    void delivery(const NativeSessionDelivery& value) {
        audio.word(value.firstSample);
        audio.word(value.samples.size());
        for (const auto sample : value.samples) {
            const auto bits = static_cast<std::uint16_t>(sample);
            audio.byte(static_cast<std::uint8_t>(bits));
            audio.byte(static_cast<std::uint8_t>(bits >> 8U));
        }
        pixels.word(value.entry.size());
        for (const auto& entry : value.entry) { screen(entry.indexed); }
        pixels.word(value.presentations.size());
        for (const auto& presentation : value.presentations) { screen(presentation.indexed); }
        events.word(value.events.size());
        for (const auto& event : value.events) {
            events.word(event.loopTick); events.word(event.eventId);
        }
    }
    void frame(const NativeGameplaySession& session) {
        const auto& model = session.model();
        state.word(static_cast<unsigned>(model.phase())); state.word(model.campaignLevel());
        state.word(model.level2LoopTick()); state.word(model.playerArmor());
        state.word(model.score()); state.word(model.remainingContinues());
        state.word(model.level2Rng().calls()); state.word(model.failureRng().calls());
        const auto& writes = session.synth().registerWrites();
        events.word(writes.size() - oplCursor);
        for (; oplCursor < writes.size(); ++oplCursor) {
            events.byte(writes[oplCursor].registerNumber); events.byte(writes[oplCursor].value);
        }
        state.word(session.synth().driverState().normalClockCycles);

        const auto [pool, active, activeAtStart, slots] = GameplayModelTestAccess::projectileStorage(model);
        projectiles.field(active);
        projectiles.field(activeAtStart);
        for (const auto& record : pool) {
            projectiles.actor(record);
        }
        const auto& entities = model.level2Entities();
        requireCampaignComparison(slots.size() == entities.size(), "projectile slot map does not match live entities");
        projectiles.word(slots.size());
        for (std::size_t index = 0; index < slots.size(); ++index) {
            projectiles.word(slots[index]);
            if (slots[index] < pool.size()) {
                projectiles.actor(entities[index]);
            }
        }
    }
};

bool ordinary(const GameplayPhase phase) {
    return phase == GameplayPhase::playable || phase == GameplayPhase::level2
        || phase == GameplayPhase::level3Native || phase == GameplayPhase::level5;
}

void begin(RecoveredFrontend& frontend, std::vector<std::int16_t>& pcm, Observations* observations) {
    unsigned menuPoll = 0;
    constexpr unsigned startupCallLimit = 20000;
    for (unsigned call = 0; !frontend.session() && call < startupCallLimit; ++call) {
        NativeMenuInput input{};
        if (frontend.startup().phase() == NativeStartupPhase::awaitingIntroInput || frontend.startup().storyActive()) {
            input.cancel = true;
        } else if (frontend.startup().phase() == NativeStartupPhase::awaitingMenuDecision) {
            input.confirm = menuPoll++ == 1;
        }
        const auto first = frontend.elapsedSamples();
        const auto delivery = frontend.advanceStartup(input);
        requireCampaignComparison(delivery.firstSample == first
            && first + delivery.samples.size() == frontend.elapsedSamples(), "startup sample epoch changed");
        pcm.insert(pcm.end(), delivery.samples.begin(), delivery.samples.end());
        if (observations) { observations->delivery(delivery); }
    }
    requireCampaignComparison(frontend.session() != nullptr, "startup did not reach Begin");
}

void run(const std::filesystem::path& root, const std::vector<InputRun>& inputs,
    const std::filesystem::path& expectedPath, unsigned& call) {
    HighscoreTable profile({}, {});
    // Owned, empty test profile: easy, keyboard, music/story off. No FAME.HAL reads or writes.
    profile.setConfiguration(CockpitSettings{81, {1, 0, 0, 1, 0, 0}});
    const auto files = resolveNativeData(root);
    if (!files.validation.ok) { throw std::runtime_error(files.validation.message); }
    auto data = loadGameplayAssets(root, {}, {}, profile, &files);
    data.storyScenes = std::make_shared<const StorySceneAssets>(decodeStorySceneAssets(
        readFile(files.file("SCENEB1.RES")), readFile(files.file("SCENEA.RES")),
        {readFile(files.file("SCENEB2.RES")), readFile(files.file("SCENEB3.RES")), readFile(files.file("SCENEB5.RES"))},
        readFile(files.file("SCENEC.RES")), readFile(files.file("VOICE.RES")), readFile(files.file("DRAXON.RES")), readFile(files.file("SCENEE.RES"))));
    const auto art = decodeNativeStartupAssets(readFile(files.file("STELART.RES")));
    const auto vga = decodeTaggedPayloads(readFile(files.file("STELLAR.RES")), {'V', 'G', 'A', ':'});
    requireCampaignComparison(vga.size() == 1, "missing actual VGA driver");
    const auto copy = decodeFrameCopyProfile(vga.front());
    auto a = std::make_unique<RecoveredFrontend>(data, art, copy, 10000, std::make_shared<MemoryStore>(),
        CampaignExecution::playable, NativePointerCapability::unavailable, NativeMenuFeatures::releaseMenus);
    auto b = std::make_unique<RecoveredFrontend>(data, art, copy, 257, std::make_shared<MemoryStore>(),
        CampaignExecution::playable, NativePointerCapability::unavailable, NativeMenuFeatures::releaseMenus);
    test::CampaignBudgetComparison comparison;
    Observations observations;
    begin(*a, comparison.leftPcm, &observations);
    begin(*b, comparison.rightPcm, nullptr);
    requireCampaignComparison(comparison.leftPcm == comparison.rightPcm, "startup PCM differs by work budget");
    const auto* left = a->session(); const auto* right = b->session();
    const auto* synth = &left->synth();
    unsigned visited = 0, deaths = 0, retries = 0, newFrames = 0, shields = 0, decoys = 0;
    bool ending = false, title = false, restarted = false;
    for (const auto& run : inputs) {
        GameplayInput input{};
        input.left = run.keys & 1; input.right = run.keys & 2;
        input.forward = run.keys & 4; input.reverse = run.keys & 8;
        input.fire = run.keys & 16; input.confirm = run.keys & 32;
        input.systemAction = static_cast<GameplaySystemAction>(run.action);
        NameEntryInput text{};
        if (run.text != "-") { text.characters.assign(run.text.begin(), run.text.end()); }
        for (unsigned repeat = 0; repeat < run.count; ++repeat) {
            ++call;
            const auto& model = left->model();
            const auto phase = model.phase(); const auto bank = model.campaignLevel();
            const auto world = model.level2LoopTick(); const auto stock = model.remainingContinues();
            if (static_cast<unsigned>(phase) != run.phase || bank + 1 != run.bank) {
                throw std::runtime_error("input boundary changed: expected phase=" + std::to_string(run.phase)
                    + " bank=" + std::to_string(run.bank) + ", got phase=" + std::to_string(static_cast<unsigned>(phase))
                    + " bank=" + std::to_string(bank + 1));
            }
            shields += input.systemAction == GameplaySystemAction::shield;
            decoys += input.systemAction == GameplaySystemAction::decoy;
            const auto first = a->elapsedSamples();
            const auto cycles = synth->driverState().normalClockCycles;
            const auto x = a->advanceGameplay(input, {}, text);
            const auto y = b->advanceGameplay(input, {}, text);
            const auto& after = left->model();
            requireCampaignComparison(a->session() == left && b->session() == right && &left->synth() == synth
                && x.firstSample == first && first + x.samples.size() == a->elapsedSamples()
                && synth->driverState().normalClockCycles >= cycles && !x.exitRequested && !y.exitRequested,
                "lost retained gameplay/device owners, sample epoch or clock");
            const bool boundary = after.phase() != phase || after.campaignLevel() != bank
                || call % historyInterval == 0 || input.systemAction != GameplaySystemAction::none;
            comparison.compare(*left, *right, x, y,
                boundary ? test::CampaignComparisonStage::boundary : test::CampaignComparisonStage::frame);
            GameplayModelTestAccess::compare(after, right->model());
            observations.delivery(x); observations.frame(*left);
            if (ordinary(after.phase()) && !restarted) {
                requireCampaignComparison(after.campaignLevel() < levelCount, "invalid gameplay bank");
                visited |= 1U << after.campaignLevel();
            }
            if (ordinary(phase) && after.phase() == GameplayPhase::failurePreExplosionBlackout) { ++deaths; }
            if (after.remainingContinues() < stock) { ++retries; }
            ending = ending || after.phase() == GameplayPhase::endingEntry;
            if (phase == GameplayPhase::nativeMenu) {
                requireCampaignComparison(ending, "returned to title before ending");
                title = true;
            }
            if (title && phase == GameplayPhase::nativeMenu && after.phase() == GameplayPhase::playableLoaderReturn
                && after.campaignLevel() == 0) { restarted = true; }
            if (restarted && ordinary(phase) && ordinary(after.phase())) {
                requireCampaignComparison(after.campaignLevel() == 0
                    && after.level2LoopTick() == static_cast<std::uint16_t>(world + 1), "restart frames not adjacent");
                ++newFrames;
            }
            requireCampaignComparison(after.phase() != GameplayPhase::campaignEnded, "campaign stock exhausted");
        }
    }
    requireCampaignComparison(visited == allLevels && ending && title && restarted && newFrames == 2
        && deaths == 2 && retries == 2 && shields == 3 && decoys == 3, "campaign route coverage changed");
    const std::array<std::uint64_t, 6> actual{a->elapsedSamples(), observations.state.value(), observations.pixels.value(),
        observations.audio.value(), observations.events.value(), observations.projectiles.value()};
    constexpr std::array names{"sample count", "state", "indexed video", "PCM", "audio events", "projectile storage"};
    std::cout << "calls=" << call << " levels=7 deaths=" << deaths << " retries=" << retries << " restart_frames=" << newFrames << '\n';
    for (const auto value : actual) { std::cout << value << '\n'; }
    std::ifstream expectedFile(expectedPath);
    for (std::size_t index = 0; index < actual.size(); ++index) {
        std::uint64_t expected = 0;
        if (!(expectedFile >> expected) || expected != actual[index]) {
            throw std::runtime_error(std::string("campaign baseline changed: ") + names[index]);
        }
    }
    expectedFile >> std::ws;
    requireCampaignComparison(expectedFile.eof(), "unexpected campaign baseline fields");
}
} // namespace

int main(const int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "usage: full-campaign DATA_DIR INPUTS EXPECTED\n";
        return 2;
    }
    unsigned call = 0;
    try {
        const auto inputs = readInputs(argv[2]);
        run(argv[1], inputs, argv[3], call);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "call=" << call << " error=" << error.what() << '\n';
        return 1;
    }
}
