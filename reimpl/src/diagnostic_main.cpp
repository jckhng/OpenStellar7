#include "gameplay_asset_loader.hpp"
#include "native_player_entry.hpp"
#include "resource_file.hpp"
#include "stellar7/story_scene.hpp"
#include <SDL_main.h>
#include "stellar7/native_game_rate.hpp"
#include "stellar7/native_player.hpp"
#include "stellar7/native_profile.hpp"
#include "stellar7/native_install.hpp"
#include "stellar7/recovered_frontend.hpp"
#include "stellar7/frontend.hpp"
#include "stellar7/controlled_frontend.hpp"
#include "stellar7/presentation_transport.hpp"
#include "stellar7/audio.hpp"
#include "stellar7/audio_clock.hpp"
#include "stellar7/audio_work.hpp"
#include "stellar7/gameplay.hpp"
#include "stellar7/level1_trace.hpp"
#include "stellar7/menu.hpp"
#include "stellar7/native_checkpoint.hpp"
#include "stellar7/native_session.hpp"
#include "stellar7/ending_scene.hpp"
#include "stellar7/native_startup.hpp"
#include "stellar7/normal_clock.hpp"
#include "stellar7/presentation_clock.hpp"
#include "stellar7/render_work.hpp"
#include "stellar7/resource_codec.hpp"
#include "stellar7/timer_work.hpp"
#include "stellar7/world_scene_work.hpp"
#include "stellar7/sight_work.hpp"
#include "stellar7/cockpit_state.hpp"
#include "stellar7/radar_state.hpp"
#include "stellar7/indexed_pages.hpp"
#include "stellar7/indexed_render.hpp"
#include "stellar7/frame_copy_work.hpp"
#include "stellar7/frame_controller.hpp"
#include "stellar7/palette_clock.hpp"
#include "stellar7/level2_frame.hpp"
#include "stellar7/retained_frame.hpp"
#include "stellar7/retained_level2.hpp"
#include "quiet_scanout.hpp"
#include "stellar7/controlled_session.hpp"
#include "gameplay_analysis.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <string>
#include <utility>
#include <vector>

namespace {

using stellar7::loadGameplayAssets;
using stellar7::readFile;
using stellar7::runInstalledPlayer;


void writePpm(
    const std::string& path,
    const stellar7::IndexedScreen& screen
) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("cannot open output file: " + path);
    }
    output << "P6\n320 200\n255\n";
    for (const std::uint8_t index : screen.pixels) {
        const stellar7::Rgb color = screen.palette[index];
        output.put(static_cast<char>(color.red));
        output.put(static_cast<char>(color.green));
        output.put(static_cast<char>(color.blue));
    }
    if (!output) {
        throw std::runtime_error("failed while writing output file: " + path);
    }
}

void writePpm(
    const std::string& path,
    const stellar7::RgbFrame& frame
) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("cannot open output file: " + path);
    }
    output << "P6\n" << frame.width << ' ' << frame.height << "\n255\n";
    for (const stellar7::Rgb color : frame.pixels) {
        output.put(static_cast<char>(color.red));
        output.put(static_cast<char>(color.green));
        output.put(static_cast<char>(color.blue));
    }
    if (!output) {
        throw std::runtime_error("failed while writing output file: " + path);
    }
}

void writeText(const std::string& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("cannot open output file: " + path);
    }
    output << text;
    if (!output) {
        throw std::runtime_error("failed while writing output file: " + path);
    }
}

void writeBytes(
    const std::string& path,
    const std::vector<std::uint8_t>& data
) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("cannot open output file: " + path);
    }
    output.write(
        reinterpret_cast<const char*>(data.data()),
        static_cast<std::streamsize>(data.size())
    );
    if (!output) {
        throw std::runtime_error("failed while writing output file: " + path);
    }
}

void writeWave(
    const std::string& path,
    const std::vector<std::int16_t>& samples
) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("cannot open output file: " + path);
    }
    const auto writeU16 = [&output](const std::uint16_t value) {
        output.put(static_cast<char>(value & 0xff));
        output.put(static_cast<char>((value >> 8) & 0xff));
    };
    const auto writeU32 = [&output](const std::uint32_t value) {
        output.put(static_cast<char>(value & 0xff));
        output.put(static_cast<char>((value >> 8) & 0xff));
        output.put(static_cast<char>((value >> 16) & 0xff));
        output.put(static_cast<char>((value >> 24) & 0xff));
    };
    const std::uint32_t dataBytes = static_cast<std::uint32_t>(
        samples.size() * sizeof(std::int16_t)
    );
    output.write("RIFF", 4);
    writeU32(36 + dataBytes);
    output.write("WAVEfmt ", 8);
    writeU32(16);
    writeU16(1);
    writeU16(1);
    writeU32(stellar7::PcSpeakerSynth::sampleRate);
    writeU32(stellar7::PcSpeakerSynth::sampleRate * 2);
    writeU16(2);
    writeU16(16);
    output.write("data", 4);
    writeU32(dataBytes);
    output.write(
        reinterpret_cast<const char*>(samples.data()),
        static_cast<std::streamsize>(dataBytes)
    );
    if (!output) {
        throw std::runtime_error("failed while writing output file: " + path);
    }
}

struct RecordedInputEvent {
    std::uint32_t tick;
    bool pressed;
    std::string key;
};

struct ParsedLevel1InputEvent {
    std::uint32_t tick;
    bool pressed;
    std::vector<std::string> keys;
    bool postCheckpoint = false;
};

constexpr std::array<
    std::pair<const char*, bool stellar7::Level1Input::*>,
    5
> level1InputKeys{{
    {"left", &stellar7::Level1Input::left},
    {"right", &stellar7::Level1Input::right},
    {"up", &stellar7::Level1Input::forward},
    {"down", &stellar7::Level1Input::reverse},
    {"spc", &stellar7::Level1Input::fire},
}};

enum class InputScriptDomain { gameplay, campaign };

std::vector<std::string> parseLevel1InputKeys(
    const std::string& keySpec,
    const InputScriptDomain domain
) {
    std::vector<std::string> keys;
    std::size_t start = 0;
    while (start <= keySpec.size()) {
        const auto separator = keySpec.find('+', start);
        const auto key = keySpec.substr(
            start,
            separator == std::string::npos
                ? std::string::npos
                : separator - start
        );
        if (
            key.empty()
            || (
                key != "left"
                && key != "right"
                && key != "up"
                && key != "down"
                && key != "spc"
                && !(domain == InputScriptDomain::campaign
                    && (key == "enter" || key == "esc"))
            )
            || std::find(keys.begin(), keys.end(), key) != keys.end()
        ) {
            throw std::runtime_error(
                "unsupported input script key specification"
            );
        }
        keys.push_back(key);
        if (separator == std::string::npos) {
            break;
        }
        start = separator + 1;
    }
    return keys;
}

ParsedLevel1InputEvent parseLevel1InputEvent(
    const std::string& eventText,
    const InputScriptDomain domain = InputScriptDomain::gameplay
) {
    const auto assignment = eventText.find('=');
    const auto separator = eventText.find(
        '.',
        assignment == std::string::npos ? 0 : assignment + 1
    );
    if (
        assignment == std::string::npos
        || separator == std::string::npos
    ) {
        throw std::runtime_error("invalid input script event");
    }
    const auto tick = std::stoul(eventText.substr(0, assignment));
    const auto operation = eventText.substr(
        assignment + 1,
        separator - assignment - 1
    );
    if (tick == 0 || (operation != "down" && operation != "up")) {
        throw std::runtime_error("invalid input script event");
    }
    return {
        static_cast<std::uint32_t>(tick),
        operation == "down",
        parseLevel1InputKeys(eventText.substr(separator + 1), domain),
    };
}

void validateLevel1InputEvents(
    const std::vector<ParsedLevel1InputEvent>& events
) {
    if (events.empty()) {
        throw std::runtime_error("input script requires events");
    }
    std::set<std::string> held;
    std::uint32_t previousTick = 0;
    for (const auto& event : events) {
        if (event.tick < previousTick) {
            throw std::runtime_error(
                "input script events must be tick ordered"
            );
        }
        previousTick = event.tick;
        for (const auto& key : event.keys) {
            if (
                (event.pressed && held.count(key) != 0)
                || (!event.pressed && held.count(key) == 0)
            ) {
                throw std::runtime_error(
                    "input script key state is inconsistent"
                );
            }
            if (event.pressed) {
                held.insert(key);
            } else {
                held.erase(key);
            }
        }
        if (
            (held.count("left") != 0 && held.count("right") != 0)
            || (held.count("up") != 0 && held.count("down") != 0)
        ) {
            throw std::runtime_error(
                "input script cannot hold opposing directions"
            );
        }
    }
    if (!held.empty()) {
        throw std::runtime_error(
            "input script must release every held key"
        );
    }
}

std::vector<ParsedLevel1InputEvent> parseLevel1InputEventSpec(
    const std::string& eventSpec
) {
    std::vector<ParsedLevel1InputEvent> events;
    std::size_t start = 0;
    while (start <= eventSpec.size()) {
        const auto end = eventSpec.find('~', start);
        events.push_back(parseLevel1InputEvent(eventSpec.substr(
            start,
            end == std::string::npos ? std::string::npos : end - start
        )));
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    validateLevel1InputEvents(events);
    return events;
}

std::vector<ParsedLevel1InputEvent> loadLevel1InputScript(
    const std::string& path,
    const InputScriptDomain domain = InputScriptDomain::gameplay
) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open input script: " + path);
    }
    std::vector<ParsedLevel1InputEvent> events;
    std::string line;
    bool observedHeader = false;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        if (!observedHeader) {
            if (line != "dos-re-state-input-script-v1") {
                throw std::runtime_error(
                    "input script requires dos-re-state-input-script-v1"
                );
            }
            observedHeader = true;
            continue;
        }
        if (line.front() == '#') {
            continue;
        }
        auto event = parseLevel1InputEvent(line, domain);
        // GameplayInputRecorder observes the held keys immediately before
        // GameplayModel::tick. Its transition at N must therefore be applied
        // before replay tick N as well. The controller metadata identifies
        // the producer; it does not change the transition phase.
        events.push_back(std::move(event));
    }
    if (!observedHeader) {
        throw std::runtime_error(
            "input script requires dos-re-state-input-script-v1"
        );
    }
    validateLevel1InputEvents(events);
    return events;
}

std::vector<std::uint32_t> parseLevel1TraceTicks(
    const std::string& tickSpec
) {
    std::vector<std::uint32_t> ticks;
    std::size_t start = 0;
    while (start <= tickSpec.size()) {
        const auto separator = tickSpec.find(',', start);
        const auto token = tickSpec.substr(
            start,
            separator == std::string::npos
                ? std::string::npos
                : separator - start
        );
        std::size_t consumed = 0;
        const auto parsed = std::stoul(token, &consumed);
        if (
            token.empty()
            || consumed != token.size()
            || parsed == 0
            || parsed > std::numeric_limits<std::uint32_t>::max()
            || (!ticks.empty() && parsed <= ticks.back())
        ) {
            throw std::runtime_error(
                "trace ticks must be positive and strictly increasing"
            );
        }
        ticks.push_back(static_cast<std::uint32_t>(parsed));
        if (separator == std::string::npos) {
            break;
        }
        start = separator + 1;
    }
    if (ticks.empty()) {
        throw std::runtime_error("trace ticks cannot be empty");
    }
    return ticks;
}

void applyLevel1InputEventsAtCheckpoint(
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t tick,
    std::size_t& nextEvent,
    std::set<std::string>& held
) {
    while (
        nextEvent < events.size()
        && events[nextEvent].tick == tick
    ) {
        for (const auto& key : events[nextEvent].keys) {
            if (events[nextEvent].pressed) {
                held.insert(key);
            } else {
                held.erase(key);
            }
        }
        ++nextEvent;
    }
}

void applyLevel1InputEventsBeforeTick(
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t tick,
    std::size_t& nextEvent,
    std::set<std::string>& held
) {
    if (!events.empty() && events.front().postCheckpoint) {
        return;
    }
    applyLevel1InputEventsAtCheckpoint(events, tick, nextEvent, held);
}

void applyLevel1InputEventsAfterCheckpoint(
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t tick,
    std::size_t& nextEvent,
    std::set<std::string>& held
) {
    if (events.empty() || !events.front().postCheckpoint) {
        return;
    }
    applyLevel1InputEventsAtCheckpoint(events, tick, nextEvent, held);
}

template <typename Observer>
void replayLevel1InputScript(
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t count,
    stellar7::Level1State& state,
    Observer&& observer
) {
    if (count == 0 || events.empty()) {
        throw std::runtime_error(
            "input script requires events and a positive tick count"
        );
    }
    std::set<std::string> held;
    std::size_t nextEvent = 0;
    for (std::uint32_t tick = 1; tick <= count; ++tick) {
        applyLevel1InputEventsBeforeTick(events, tick, nextEvent, held);
        state.advanceTick({
            held.count("left") != 0,
            held.count("right") != 0,
            held.count("up") != 0,
            held.count("down") != 0,
            held.count("spc") != 0,
        });
        observer(tick, state);
        // DOSBox checkpoint scripts deliver a transition after the
        // checkpoint has been captured. The next simulation boundary is the
        // first one that observes the changed keyboard state.
        applyLevel1InputEventsAfterCheckpoint(events, tick, nextEvent, held);
    }
}

void writeLevel1InputScriptTrace(
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t count,
    const std::string& outputPath,
    const std::string& boundary
) {
    stellar7::Level1State state;
    replayLevel1InputScript(
        events,
        count,
        state,
        [](std::uint32_t, const stellar7::Level1State&) {}
    );
    writeText(
        outputPath,
        stellar7::level1StateTraceJson(state, boundary)
    );
}

void writeLevel1InputScriptTraceSeries(
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::vector<std::uint32_t>& ticks,
    const std::string& outputDirectory,
    const std::string& boundary
) {
    const std::filesystem::path root(outputDirectory);
    std::filesystem::create_directories(root);
    stellar7::Level1State state;
    std::size_t nextTick = 0;
    replayLevel1InputScript(
        events,
        ticks.back(),
        state,
        [
            &boundary,
            &nextTick,
            &root,
            &ticks
        ](
            const std::uint32_t tick,
            const stellar7::Level1State& current
        ) {
            if (tick != ticks[nextTick]) {
                return;
            }
            writeText(
                (
                    root / ("tick-" + std::to_string(tick) + ".json")
                ).string(),
                stellar7::level1StateTraceJson(current, boundary)
            );
            ++nextTick;
        }
    );
}

stellar7::GameplayModel replayLevel1InputScriptGameplay(
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t count,
    const std::optional<std::int16_t> level2Armor = std::nullopt,
    const std::function<void(const stellar7::GameplayModel&)>& observe = {},
    const bool holdLevel2Armor = false
) {
    if (count == 0 || events.empty()) {
        throw std::runtime_error(
            "input script requires events and a positive tick count"
        );
    }
    stellar7::GameplayModel model;
    std::set<std::string> held;
    std::size_t nextEvent = 0;
    bool appliedLevel2Armor = false;
    for (std::uint32_t tick = 1; tick <= count; ++tick) {
        applyLevel1InputEventsBeforeTick(events, tick, nextEvent, held);
        model.tick({
            held.count("left") != 0,
            held.count("right") != 0,
            held.count("up") != 0,
            held.count("down") != 0,
            held.count("spc") != 0,
            held.count("esc") != 0,
        });
        if (
            level2Armor.has_value()
            && (!appliedLevel2Armor || holdLevel2Armor)
            && (
                model.phase() == stellar7::GameplayPhase::level2Handoff
                || model.phase() == stellar7::GameplayPhase::level2
            )
        ) {
            stellar7::GameplayAnalysisAccess::applyLevel2Armor(
                model,
                *level2Armor
            );
            appliedLevel2Armor = true;
        }
        if (observe) {
            observe(model);
        }
        applyLevel1InputEventsAfterCheckpoint(events, tick, nextEvent, held);
    }
    return model;
}

stellar7::GameplayModel replayLevel2ControlledActorWarp(
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t count,
    const std::uint16_t warpLoopTick,
    const std::size_t allocationIndex,
    const stellar7::FixedPosition position,
    const std::uint8_t health,
    const std::int16_t armor
) {
    if (count == 0 || events.empty()) {
        throw std::runtime_error(
            "controlled actor warp requires input events and a positive tick"
        );
    }
    stellar7::GameplayModel model;
    std::set<std::string> held;
    std::size_t nextEvent = 0;
    bool applied = false;
    for (std::uint32_t tick = 1; tick <= count; ++tick) {
        applyLevel1InputEventsBeforeTick(events, tick, nextEvent, held);
        model.tick({
            held.count("left") != 0,
            held.count("right") != 0,
            held.count("up") != 0,
            held.count("down") != 0,
            held.count("spc") != 0,
            held.count("esc") != 0,
        });
        if (
            !applied
            && model.phase() == stellar7::GameplayPhase::level2
            && model.level2LoopTick() == warpLoopTick
        ) {
            stellar7::GameplayAnalysisAccess::applyLevel2ActorWarp(
                model,
                allocationIndex,
                position,
                health,
                armor
            );
            applied = true;
        }
        applyLevel1InputEventsAfterCheckpoint(events, tick, nextEvent, held);
    }
    if (!applied) {
        throw std::runtime_error(
            "controlled actor warp loop tick was not reached"
        );
    }
    return model;
}

void writeLevel1InputScriptIndexedFrame(
    const stellar7::GameplayAssets& assets,
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t count,
    const std::string& outputPath,
    const std::optional<std::int16_t> level2Armor = std::nullopt,
    const bool holdLevel2Armor = false
) {
    const auto model = replayLevel1InputScriptGameplay(
        events,
        count,
        level2Armor,
        {},
        holdLevel2Armor
    );
    writeBytes(
        outputPath,
        stellar7::renderGameplayIndexed(assets, model).pixels
    );
}

void writeLevel1InputScriptRgbFrame(
    const stellar7::GameplayAssets& assets,
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t count,
    const std::string& outputPath
) {
    const auto model = replayLevel1InputScriptGameplay(events, count);
    const auto frame = stellar7::renderGameplay(assets, model);
    std::vector<std::uint8_t> rgb;
    rgb.reserve(frame.pixels.size() * 3);
    for (const auto color : frame.pixels) {
        rgb.push_back(color.red);
        rgb.push_back(color.green);
        rgb.push_back(color.blue);
    }
    writeBytes(outputPath, rgb);
}

const char* gameplayPhaseName(const stellar7::GameplayPhase phase) {
    switch (phase) {
    case stellar7::GameplayPhase::level1:
        return "level1";
    case stellar7::GameplayPhase::successPresentation:
        return "success-presentation";
    case stellar7::GameplayPhase::level2Handoff:
        return "level2-handoff";
    case stellar7::GameplayPhase::level2:
        return "level2";
    case stellar7::GameplayPhase::level2SuccessPresentation:
        return "level2-success-presentation";
    case stellar7::GameplayPhase::level2SuccessReturn:
        return "level2-success-return";
    case stellar7::GameplayPhase::level3LoaderReturn:
        return "level3-loader-return";
    case stellar7::GameplayPhase::level3PreloopReturn:
        return "level3-preloop-return";
    case stellar7::GameplayPhase::level3Native:
        return "level3-native";
    case stellar7::GameplayPhase::level1SuccessTailReturn:
        return "level1-success-tail-return";
    case stellar7::GameplayPhase::level2LoaderReturn:
        return "level2-loader-return";
    case stellar7::GameplayPhase::level2MusicReturn:
        return "level2-music-return";
    case stellar7::GameplayPhase::level2PreloopReturn:
        return "level2-preloop-return";
    case stellar7::GameplayPhase::level3Handoff:
        return "level3-handoff";
    case stellar7::GameplayPhase::level3:
        return "level3";
    case stellar7::GameplayPhase::level3SuccessPresentation:
        return "level3-success-presentation";
    case stellar7::GameplayPhase::level4Handoff:
        return "level4-handoff";
    case stellar7::GameplayPhase::level5LoaderReturn:
        return "level5-loader-return";
    case stellar7::GameplayPhase::level5:
        return "level5";
    case stellar7::GameplayPhase::highscoreNameEntry:
        return "highscore-name-entry";
    case stellar7::GameplayPhase::highscoreHall:
        return "highscore-hall";
    case stellar7::GameplayPhase::level5SuccessReturn:
        return "level5-success-return";
    case stellar7::GameplayPhase::endingEntry:
        return "ending-entry";
    case stellar7::GameplayPhase::endingPresentation:
        return "ending-presentation";
    case stellar7::GameplayPhase::endingTeardown:
        return "ending-teardown";
    case stellar7::GameplayPhase::endingReturn:
        return "ending-return";
    case stellar7::GameplayPhase::nativeMenu:
        return "native-menu";
    case stellar7::GameplayPhase::nativeBeginWait:
        return "native-begin-wait";
    case stellar7::GameplayPhase::level1BeginHandoff:
        return "level1-begin-handoff";
    case stellar7::GameplayPhase::failurePreExplosionBlackout:
        return "failure-pre-explosion-blackout";
    case stellar7::GameplayPhase::failureExplosion:
        return "failure-explosion";
    case stellar7::GameplayPhase::failurePostExplosionBlackout:
        return "failure-post-explosion-blackout";
    case stellar7::GameplayPhase::continuePrompt:
        return "continue-prompt";
    case stellar7::GameplayPhase::level1RetryHandoff:
        return "level1-retry-handoff";
    case stellar7::GameplayPhase::campaignEnded:
        return "campaign-ended";
    }
    return "unknown";
}

void writeLevel3NativeGuardianForwardFireProbe(
    const std::vector<std::uint8_t>& checkpoint,
    const std::uint32_t count,
    const std::string& outputPath
) {
    stellar7::GameplayModel model;
    stellar7::loadLevel3NativeCheckpoint(model, checkpoint);
    const auto guardianHealth = [&model]() -> std::optional<unsigned> {
        const auto guardian = std::find_if(
            model.level2Entities().begin(),
            model.level2Entities().end(),
            [](const stellar7::Level1Entity& entity) {
                return entity.type == 44 || entity.type == 45;
            }
        );
        if (guardian == model.level2Entities().end()) {
            return std::nullopt;
        }
        return guardian->stateBytes4dTo52[1];
    };
    const auto initialLoopTick = model.level2LoopTick();
    const auto initialHealth = guardianHealth();
    std::optional<std::uint16_t> firstHealthChangeLoopTick;
    std::optional<unsigned> firstChangedHealth;
    for (std::uint32_t tick = 0; tick < count; ++tick) {
        model.tick({false, false, true, false, true, false});
        const auto health = guardianHealth();
        if (
            !firstHealthChangeLoopTick.has_value()
            && health != initialHealth
        ) {
            firstHealthChangeLoopTick = model.level2LoopTick();
            firstChangedHealth = health;
        }
    }
    const auto finalHealth = guardianHealth();
    const auto livePlayerProjectiles = static_cast<std::size_t>(std::count_if(
        model.level2Entities().begin(),
        model.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 23 && entity.active();
        }
    ));
    std::ostringstream output;
    output
        << "{\"format_version\":2"
        << ",\"initial_loop_tick\":" << initialLoopTick
        << ",\"terminal_loop_tick\":" << model.level2LoopTick()
        << ",\"ticks_simulated\":" << count
        << ",\"initial_guardian_health\":";
    if (initialHealth.has_value()) {
        output << *initialHealth;
    } else {
        output << "null";
    }
    output << ",\"first_health_change_loop_tick\":";
    if (firstHealthChangeLoopTick.has_value()) {
        output << *firstHealthChangeLoopTick;
    } else {
        output << "null";
    }
    output << ",\"first_changed_health\":";
    if (firstChangedHealth.has_value()) {
        output << *firstChangedHealth;
    } else {
        output << "null";
    }
    output << ",\"final_guardian_health\":";
    if (finalHealth.has_value()) {
        output << *finalHealth;
    } else {
        output << "null";
    }
    output
        << ",\"live_player_projectiles\":" << livePlayerProjectiles
        << ",\"phase\":\"" << gameplayPhaseName(model.phase()) << "\""
        << ",\"player\":{\"position\":["
        << model.level2Player().position.x << ','
        << model.level2Player().position.y << ','
        << model.level2Player().position.z << "],\"yaw\":"
        << model.level2Player().yaw << ",\"yaw_velocity\":"
        << model.level2Player().yawVelocity << ",\"forward_velocity\":"
        << model.level2Player().forwardVelocity << "}"
        << ",\"entities\":[";
    for (std::size_t index = 0;
         index < model.level2Entities().size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        const auto& entity = model.level2Entities()[index];
        output
            << "{\"allocation_index\":" << entity.allocationIndex
            << ",\"type\":" << entity.type
            << ",\"health\":"
            << static_cast<unsigned>(entity.stateBytes4dTo52[1])
            << ",\"position\":[" << entity.position.x << ','
            << entity.position.y << ',' << entity.position.z
            << "],\"yaw\":" << entity.yaw
            << ",\"yaw_velocity\":" << entity.yawVelocity
            << ",\"forward_velocity\":" << entity.forwardVelocity
            << '}';
    }
    output << "]}\n";
    writeText(outputPath, output.str());
}

stellar7::GameplayModel replayLevel3NativeInputScript(
    const std::vector<std::uint8_t>& checkpoint,
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t terminalTick,
    const std::function<void(const stellar7::GameplayModel&)>& observe = {}
) {
    stellar7::GameplayModel model;
    stellar7::loadLevel3NativeCheckpoint(model, checkpoint);
    if (terminalTick < model.ticks()) {
        throw std::runtime_error(
            "terminal tick precedes native checkpoint"
        );
    }
    std::size_t nextEvent = 0;
    std::set<std::string> held;
    while (
        nextEvent < events.size()
        && events[nextEvent].tick <= model.ticks()
    ) {
        for (const auto& key : events[nextEvent].keys) {
            if (events[nextEvent].pressed) {
                held.insert(key);
            } else {
                held.erase(key);
            }
        }
        ++nextEvent;
    }
    while (model.ticks() < terminalTick) {
        const auto tick = static_cast<std::uint32_t>(
            model.ticks() + 1U
        );
        applyLevel1InputEventsAtCheckpoint(
            events,
            tick,
            nextEvent,
            held
        );
        model.tick({
            held.count("left") != 0,
            held.count("right") != 0,
            held.count("up") != 0,
            held.count("down") != 0,
            held.count("spc") != 0,
            false,
        });
        if (observe) {
            observe(model);
        }
    }
    return model;
}

void writeLevel3ModelState(
    const stellar7::GameplayModel& model,
    const std::string& outputPath
) {
    std::ostringstream output;
    output
        << "{\"format_version\":1"
        << ",\"total_tick\":" << model.ticks()
        << ",\"loop_tick\":" << model.level2LoopTick()
        << ",\"phase\":\"" << gameplayPhaseName(model.phase()) << "\""
        << ",\"presentation_frame\":" << model.presentationFrame()
        << ",\"completion_by_pickup\":"
        << (model.level3CompletionByPickup() ? "true" : "false")
        << ",\"score\":" << model.score()
        << ",\"player_armor\":" << model.playerArmor()
        << ",\"raw_entity_count\":" << model.level2Entities().size()
        << ",\"retired_effect_semantic_count\":"
        << stellar7::GameplayAnalysisAccess::level2RetiredEffectSemanticCount(
            model
        )
        << ",\"retired_effect_rebuild_reported\":"
        << (
            stellar7::GameplayAnalysisAccess::level2RetiredEffectRebuildReported(
                model
            )
                ? "true"
                : "false"
        )
        << ",\"rng\":{\"index_a\":" << model.level2Rng().indexA()
        << ",\"index_b\":" << model.level2Rng().indexB()
        << ",\"last\":" << model.level2Rng().last() << "}"
        << ",\"ground\":[";
    const auto& groundPoints = model.level2Ground().points();
    for (std::size_t index = 0; index < groundPoints.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << '[' << groundPoints[index].x << ','
               << groundPoints[index].y << ']';
    }
    output
        << ']'
        << ",\"player\":{\"position\":["
        << model.level2Player().position.x << ','
        << model.level2Player().position.y << ','
        << model.level2Player().position.z << "],\"angles\":["
        << model.level2Player().pitch << ','
        << model.level2Player().roll << ','
        << model.level2Player().yaw << "],\"yaw_velocity\":"
        << model.level2Player().yawVelocity
        << ",\"linear_velocity\":["
        << model.level2Player().xVelocity << ','
        << model.level2Player().yVelocity << ','
        << model.level2Player().zVelocity << ']'
        << ",\"forward_velocity\":"
        << model.level2Player().forwardVelocity << "}"
        << ",\"player_collision_momentum\":"
        << stellar7::GameplayAnalysisAccess::level2PlayerCollisionMomentum(
            model
        )
        << ",\"type46_contact_slowdown\":"
        << (
            stellar7::GameplayAnalysisAccess::level2Type46ContactSlowdown(
                model
            )
                ? "true"
                : "false"
        )
        << ",\"pending_input\":[";
    const auto& pendingInput =
        stellar7::GameplayAnalysisAccess::level2PendingInput(model);
    output
        << (pendingInput.left ? 1 : 0) << ','
        << (pendingInput.right ? 1 : 0) << ','
        << (pendingInput.forward ? 1 : 0) << ','
        << (pendingInput.reverse ? 1 : 0) << ','
        << (pendingInput.fire ? 1 : 0) << ']'
        << ",\"allocation_order\":[";
    const auto allocationOrder =
        stellar7::GameplayAnalysisAccess::level2SemanticAllocationListOrder(
            model
        );
    for (std::size_t index = 0; index < allocationOrder.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << allocationOrder[index];
    }
    output << "],\"raw_active_order\":[";
    const auto& rawActiveOrder = model.level2ActiveListOrder();
    for (std::size_t index = 0; index < rawActiveOrder.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << rawActiveOrder[index];
    }
    output << "],\"active_order\":[";
    const auto activeOrder =
        stellar7::GameplayAnalysisAccess::level2SemanticActiveListOrder(model);
    for (std::size_t index = 0; index < activeOrder.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << activeOrder[index];
    }
    output << "],\"entities\":[";
    const auto entities =
        stellar7::GameplayAnalysisAccess::level2SemanticEntities(model);
    for (std::size_t index = 0; index < entities.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        const auto& entity = entities[index];
        output
            << "{\"allocation_index\":" << entity.allocationIndex
            << ",\"active\":" << (entity.active() ? "true" : "false")
            << ",\"flags\":" << static_cast<unsigned>(entity.flags)
            << ",\"type\":" << entity.type
            << ",\"position\":[" << entity.position.x << ','
            << entity.position.y << ',' << entity.position.z << ']'
            << ",\"angles\":[" << entity.pitch << ',' << entity.roll
            << ',' << entity.yaw << ']'
            << ",\"collision_radius\":" << entity.collisionRadius
            << ",\"velocities\":[" << entity.pitchVelocity << ','
            << entity.rollVelocity << ',' << entity.yawVelocity << ','
            << entity.xVelocity << ',' << entity.yVelocity << ','
            << entity.zVelocity << ',' << entity.forwardVelocity << ']'
            << ",\"behavior_timer\":"
            << static_cast<unsigned>(entity.behaviorTimer)
            << ",\"presenter_distance\":" << entity.presenterDistance
            << ",\"target_heading\":" << entity.targetHeading
            << ",\"weapon_slot_count\":"
            << static_cast<unsigned>(entity.weaponSlotCount)
            << ",\"weapon_cooldown\":"
            << static_cast<unsigned>(entity.weaponCooldown)
            << ",\"weapon_slot_timers\":["
            << static_cast<unsigned>(entity.weaponSlotTimers[0]) << ','
            << static_cast<unsigned>(entity.weaponSlotTimers[1]) << ','
            << static_cast<unsigned>(entity.weaponSlotTimers[2]) << ']'
            << ",\"state_bytes_4d_52\":[";
        for (std::size_t stateIndex = 0;
             stateIndex < entity.stateBytes4dTo52.size(); ++stateIndex) {
            if (stateIndex != 0) {
                output << ',';
            }
            output << static_cast<unsigned>(
                entity.stateBytes4dTo52[stateIndex]
            );
        }
        output << "],\"collision_contact_allocation_index\":";
        if (
            entity.collisionContactAllocationIndex
                == static_cast<std::size_t>(-1)
        ) {
            output << "null";
        } else {
            output << entity.collisionContactAllocationIndex;
        }
        output << ",\"collision_contact_is_player\":"
               << (entity.collisionContactIsPlayer ? "true" : "false")
               << '}';
    }
    output << "]}\n";
    writeText(outputPath, output.str());
}

void writeLevel3NativeInputScriptState(
    const std::vector<std::uint8_t>& checkpoint,
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t terminalTick,
    const std::string& outputPath
) {
    writeLevel3ModelState(
        replayLevel3NativeInputScript(checkpoint, events, terminalTick), outputPath
    );
}

void writeLevel3NativeConstantInputProbe(
    const std::vector<std::uint8_t>& checkpoint,
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t controlStartTick,
    const std::uint32_t count,
    const std::string& keySpec,
    const std::string& outputPath
) {
    auto model = replayLevel3NativeInputScript(
        checkpoint,
        events,
        controlStartTick
    );
    std::set<std::string> held;
    std::size_t start = 0;
    while (start <= keySpec.size()) {
        const auto separator = keySpec.find('+', start);
        const auto key = keySpec.substr(
            start,
            separator == std::string::npos
                ? std::string::npos
                : separator - start
        );
        if (
            key != "left" && key != "right" && key != "up"
            && key != "down" && key != "spc"
        ) {
            throw std::runtime_error("unsupported constant input key");
        }
        held.insert(key);
        if (separator == std::string::npos) {
            break;
        }
        start = separator + 1U;
    }
    const stellar7::GameplayInput desired{
        held.count("left") != 0,
        held.count("right") != 0,
        held.count("up") != 0,
        held.count("down") != 0,
        held.count("spc") != 0,
        false,
    };
    std::ostringstream transitions;
    std::size_t transitionCount = 0;
    std::vector<std::pair<std::size_t, unsigned>> previousGuardians;
    const auto guardians = [&model]() {
        std::vector<std::pair<std::size_t, unsigned>> result;
        for (const auto& entity : model.level2Entities()) {
            if (
                entity.active()
                && (entity.type == 44 || entity.type == 45)
            ) {
                result.emplace_back(
                    entity.allocationIndex,
                    entity.stateBytes4dTo52[1]
                );
            }
        }
        return result;
    };
    previousGuardians = guardians();
    for (std::uint32_t offset = 1; offset <= count; ++offset) {
        model.tick(desired);
        if (model.phase() == stellar7::GameplayPhase::level3) {
            stellar7::GameplayAnalysisAccess::applyLevel3Armor(model, 680);
        }
        const auto current = guardians();
        if (current != previousGuardians) {
            if (transitionCount != 0) {
                transitions << ',';
            }
            transitions
                << "{\"total_tick\":" << model.ticks()
                << ",\"guardians\":[";
            for (std::size_t index = 0; index < current.size(); ++index) {
                if (index != 0) {
                    transitions << ',';
                }
                transitions
                    << "{\"allocation_index\":" << current[index].first
                    << ",\"health\":" << current[index].second << '}';
            }
            transitions << "]}";
            ++transitionCount;
            previousGuardians = current;
        }
        if (model.phase() != stellar7::GameplayPhase::level3) {
            break;
        }
    }
    const auto finalGuardians = guardians();
    std::ostringstream output;
    output
        << "{\"format_version\":1"
        << ",\"control_start_tick\":" << controlStartTick
        << ",\"requested_ticks\":" << count
        << ",\"constant_input\":\"" << keySpec << "\""
        << ",\"final_tick\":" << model.ticks()
        << ",\"phase\":\"" << gameplayPhaseName(model.phase()) << "\""
        << ",\"score\":" << model.score()
        << ",\"guardian_transition_count\":" << transitionCount
        << ",\"guardian_transitions\":[" << transitions.str() << ']'
        << ",\"final_guardians\":[";
    for (std::size_t index = 0; index < finalGuardians.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output
            << "{\"allocation_index\":" << finalGuardians[index].first
            << ",\"health\":" << finalGuardians[index].second << '}';
    }
    output << "]}\n";
    writeText(outputPath, output.str());
}

void renderLevel3NativeInputScriptIndexed(
    const stellar7::GameplayAssets& assets,
    const std::vector<std::uint8_t>& checkpoint,
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t terminalLoopTick,
    const std::string& outputPath
) {
    const auto model = replayLevel3NativeInputScript(
        checkpoint,
        events,
        terminalLoopTick
    );
    writeBytes(
        outputPath,
        stellar7::renderGameplayIndexed(assets, model).pixels
    );
}

void writeLevel1InputScriptGameplayPhase(
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t count,
    const std::string& outputPath
) {
    const auto model = replayLevel1InputScriptGameplay(events, count);
    std::ostringstream output;
    output
        << "{\"format_version\":1"
        << ",\"tick\":" << model.ticks()
        << ",\"loop_tick\":" << model.level1State().loopTick()
        << ",\"phase\":\"" << gameplayPhaseName(model.phase()) << "\""
        << ",\"presentation_frame\":" << model.presentationFrame()
        << "}\n";
    writeText(outputPath, output.str());
}

void writeGameplayInputScriptPhases(
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t count,
    const std::string& outputPath
) {
    if (count == 0 || events.empty()) {
        throw std::invalid_argument("phase trace requires input events and positive tick count");
    }
    stellar7::GameplayModel model;
    std::set<std::string> held;
    std::size_t nextEvent = 0;
    std::ostringstream output;
    output << "{\"format_version\":1,\"scope\":\"semantic-only natural default model\","
           << "\"success_wait\":\"headless fixed-frame fallback; no audio owner\",\"records\":[";
    bool first = true;
    const auto record = [&](const char* kind) {
        output << (first ? "" : ",") << "{\"kind\":\"" << kind << "\",\"tick\":" << model.ticks()
               << ",\"phase\":\"" << gameplayPhaseName(model.phase()) << "\",\"level1_loop_tick\":"
               << model.level1State().loopTick() << ",\"level2_loop_tick\":" << model.level2LoopTick()
               << ",\"presentation_frame\":" << model.presentationFrame()
               << ",\"score\":" << model.score() << ",\"armor\":" << model.playerArmor()
               << ",\"remaining_continues\":" << model.remainingContinues()
               << ",\"level1_rng_calls\":" << model.level1State().rng().calls()
               << ",\"level2_rng_calls\":" << model.level2Rng().calls() << '}';
        first = false;
    };
    record("initial");
    for (std::uint32_t tick = 1; tick <= count; ++tick) {
        applyLevel1InputEventsBeforeTick(events, tick, nextEvent, held);
        const auto previous = model.phase();
        model.tick({held.count("left") != 0, held.count("right") != 0,
            held.count("up") != 0, held.count("down") != 0,
            held.count("spc") != 0, held.count("esc") != 0,
            held.count("enter") != 0});
        if (model.phase() != previous) {
            record("phase-change");
        }
        if (model.phase() == stellar7::GameplayPhase::campaignEnded) {
            break;
        }
        applyLevel1InputEventsAfterCheckpoint(events, tick, nextEvent, held);
    }
    record("terminal");
    output << "]}\n";
    writeText(outputPath, output.str());
}

void writeLevel2ModelState(
    const stellar7::GameplayModel& model,
    const std::string& outputPath
) {
    if (
        model.phase() != stellar7::GameplayPhase::level2Handoff
        && model.phase() != stellar7::GameplayPhase::level2
        && model.phase()
            != stellar7::GameplayPhase::level2SuccessPresentation
        && model.phase() != stellar7::GameplayPhase::level3Handoff
        && model.phase()
            != stellar7::GameplayPhase::level3SuccessPresentation
        && model.phase() != stellar7::GameplayPhase::level4Handoff
        && model.phase() != stellar7::GameplayPhase::level5LoaderReturn
        && model.phase() != stellar7::GameplayPhase::level5
        && model.phase() != stellar7::GameplayPhase::level5SuccessReturn
        && model.phase() != stellar7::GameplayPhase::level2SuccessReturn
        && model.phase() != stellar7::GameplayPhase::level3LoaderReturn
        && model.phase() != stellar7::GameplayPhase::level3PreloopReturn
        && model.phase() != stellar7::GameplayPhase::level3Native
        && model.phase() != stellar7::GameplayPhase::endingEntry
        && model.phase() != stellar7::GameplayPhase::endingPresentation
        && model.phase() != stellar7::GameplayPhase::endingTeardown
        && model.phase() != stellar7::GameplayPhase::endingReturn
        && model.phase() != stellar7::GameplayPhase::nativeMenu
        && model.phase() != stellar7::GameplayPhase::highscoreNameEntry
        && model.phase() != stellar7::GameplayPhase::highscoreHall
        && !(
            (
                model.phase()
                    == stellar7::GameplayPhase::failurePreExplosionBlackout
                || model.phase()
                    == stellar7::GameplayPhase::failureExplosion
                || (model.failureFromLevel2()
                    && model.phase() == stellar7::GameplayPhase::continuePrompt)
            )
            && model.level2LoopTick() != 0
        )
    ) {
        throw std::runtime_error("replay has not reached Level 2");
    }
    const auto& player = model.level2Player();
    std::ostringstream output;
    const auto writeFailureEntity = [&output](
        const stellar7::Level1Entity& entity
    ) {
        output
            << "{\"allocation_index\":" << entity.allocationIndex
            << ",\"type\":" << entity.type
            << ",\"collision_radius\":" << entity.collisionRadius
            << ",\"position\":[" << entity.position.x << ','
            << entity.position.y << ',' << entity.position.z
            << "],\"angles\":[" << entity.pitch << ',' << entity.roll
            << ',' << entity.yaw << "],\"angular_velocity\":["
            << entity.pitchVelocity << ',' << entity.rollVelocity << ','
            << entity.yawVelocity << "],\"linear_velocity\":["
            << entity.xVelocity << ',' << entity.yVelocity << ','
            << entity.zVelocity << "],\"forward_velocity\":"
            << entity.forwardVelocity << ",\"state_bytes_4d_52\":[";
        for (std::size_t byte = 0;
             byte < entity.stateBytes4dTo52.size(); ++byte) {
            if (byte != 0) {
                output << ',';
            }
            output << static_cast<unsigned>(entity.stateBytes4dTo52[byte]);
        }
        output << "]}";
    };
    const auto semanticEntities =
        stellar7::GameplayAnalysisAccess::level2SemanticEntities(model);
    const auto semanticActiveOrder =
        stellar7::GameplayAnalysisAccess::level2SemanticActiveListOrder(model);
    output
        << "{\"format_version\":1"
        << ",\"tick\":" << model.ticks()
        << ",\"loop_tick\":" << model.level2LoopTick()
        << ",\"level_index\":" << model.campaignLevel()
        << ",\"score\":" << model.score()
        << ",\"player_armor\":" << model.playerArmor()
        << ",\"active_enemy_count\":"
        << stellar7::GameplayAnalysisAccess::level2ActiveEnemyCount(model)
        << ",\"spawned_enemy_count\":"
        << stellar7::GameplayAnalysisAccess::level2SpawnedEnemyCount(model)
        << ",\"phase\":\"" << gameplayPhaseName(model.phase()) << "\""
        << ",\"player\":{\"position\":["
        << player.position.x << ',' << player.position.y << ','
        << player.position.z << "],\"angles\":["
        << player.pitch << ',' << player.roll << ',' << player.yaw
        << "],\"angular_velocity\":["
        << player.pitchVelocity << ',' << player.rollVelocity << ','
        << player.yawVelocity << "],\"forward_velocity\":"
        << player.forwardVelocity << "},\"player_weapon_timers\":["
        << static_cast<unsigned>(player.weaponCooldown) << ","
        << static_cast<unsigned>(player.weaponSlotTimers[0]) << ','
        << static_cast<unsigned>(player.weaponSlotTimers[1]) << ','
        << static_cast<unsigned>(player.weaponSlotTimers[2])
        << "],\"active_allocation_order\":[";
    for (std::size_t index = 0;
         index < semanticActiveOrder.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << semanticActiveOrder[index];
    }
    output << "],\"allocation_list_order\":[";
    const auto allocationOrder =
        stellar7::GameplayAnalysisAccess::level2SemanticAllocationListOrder(
            model
        );
    for (std::size_t index = 0; index < allocationOrder.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << allocationOrder[index];
    }
    output << "],\"projectile_pool\":[";
    const auto& projectilePool =
        stellar7::GameplayAnalysisAccess::level2ProjectilePool(model);
    const auto& projectilePoolActive =
        stellar7::GameplayAnalysisAccess::level2ProjectilePoolActive(model);
    const auto& entityProjectilePoolSlots =
        stellar7::GameplayAnalysisAccess::level2EntityProjectilePoolSlots(
            model
        );
    for (std::size_t slot = 0; slot < projectilePool.size(); ++slot) {
        if (slot != 0) {
            output << ',';
        }
        const auto entitySlot = std::find(
            entityProjectilePoolSlots.begin(),
            entityProjectilePoolSlots.end(),
            slot
        );
        const auto entityIndex = static_cast<std::size_t>(
            entitySlot - entityProjectilePoolSlots.begin()
        );
        const auto& projectile = entitySlot != entityProjectilePoolSlots.end()
            ? model.level2Entities()[entityIndex]
            : projectilePool[slot];
        output
            << "{\"slot\":" << slot
            << ",\"active\":"
            << (projectilePoolActive[slot] ? "true" : "false")
            << ",\"type\":" << projectile.type
            << ",\"position\":[" << projectile.position.x << ','
            << projectile.position.y << ',' << projectile.position.z
            << "],\"presenter_distance\":"
            << projectile.presenterDistance
            << ",\"target_heading\":" << projectile.targetHeading
            << ",\"lifetime\":"
            << static_cast<unsigned>(projectile.stateBytes4dTo52[1]);
        if (entitySlot != entityProjectilePoolSlots.end()) {
            output << ",\"entity_allocation_index\":"
                   << model.level2Entities()[entityIndex].allocationIndex;
        }
        output << '}';
    }
    output << "],\"death_effect_pool\":[";
    const auto& deathEffectPool =
        stellar7::GameplayAnalysisAccess::level2DeathEffectPool(model);
    const auto& deathEffectPoolActive =
        stellar7::GameplayAnalysisAccess::level2DeathEffectPoolActive(model);
    const auto& deathEffectPoolPending =
        stellar7::GameplayAnalysisAccess::level2DeathEffectPoolPendingRetirement(
            model
        );
    const auto& entityDeathEffectPoolSlots =
        stellar7::GameplayAnalysisAccess::level2EntityDeathEffectPoolSlots(
            model
        );
    for (std::size_t slot = 0; slot < deathEffectPool.size(); ++slot) {
        if (slot != 0) {
            output << ',';
        }
        const auto entitySlot = std::find(
            entityDeathEffectPoolSlots.begin(),
            entityDeathEffectPoolSlots.end(),
            slot
        );
        const auto entityIndex = static_cast<std::size_t>(
            entitySlot - entityDeathEffectPoolSlots.begin()
        );
        const auto& effect = entitySlot != entityDeathEffectPoolSlots.end()
            ? model.level2Entities()[entityIndex]
            : deathEffectPool[slot];
        output
            << "{\"slot\":" << slot
            << ",\"active\":"
            << (deathEffectPoolActive[slot] ? "true" : "false")
            << ",\"pending_retirement\":"
            << (deathEffectPoolPending[slot] ? "true" : "false")
            << ",\"position\":[" << effect.position.x << ','
            << effect.position.y << ',' << effect.position.z
            << "],\"presenter_distance\":" << effect.presenterDistance
            << ",\"target_heading\":" << effect.targetHeading
            << ",\"frame\":"
            << static_cast<unsigned>(effect.stateBytes4dTo52[1]);
        if (entitySlot != entityDeathEffectPoolSlots.end()) {
            output << ",\"entity_allocation_index\":"
                   << model.level2Entities()[entityIndex].allocationIndex;
        }
        output << '}';
    }
    output << "],\"entities\":[";
    for (std::size_t index = 0; index < semanticEntities.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        const auto& entity = semanticEntities[index];
        output
            << "{\"allocation_index\":" << entity.allocationIndex
            << ",\"type\":" << entity.type
            << ",\"collision_radius\":" << entity.collisionRadius
            << ",\"position\":[" << entity.position.x << ','
            << entity.position.y << ',' << entity.position.z
            << "],\"angles\":[" << entity.pitch << ',' << entity.roll
            << ',' << entity.yaw << "],\"angular_velocity\":["
            << entity.pitchVelocity << ',' << entity.rollVelocity << ','
            << entity.yawVelocity << "],\"linear_velocity\":["
            << entity.xVelocity << ',' << entity.yVelocity << ','
            << entity.zVelocity << "],\"forward_velocity\":"
            << entity.forwardVelocity << ",\"presenter_distance\":"
            << entity.presenterDistance << ",\"target_heading\":"
            << entity.targetHeading << ",\"state_bytes_4d_52\":[";
        for (std::size_t byte = 0;
             byte < entity.stateBytes4dTo52.size(); ++byte) {
            if (byte != 0) {
                output << ',';
            }
            output << static_cast<unsigned>(entity.stateBytes4dTo52[byte]);
        }
        output << "],\"weapon_slot_count\":"
            << static_cast<unsigned>(entity.weaponSlotCount)
            << ",\"weapon_cooldown\":"
            << static_cast<unsigned>(entity.weaponCooldown)
            << ",\"behavior_timer\":"
            << static_cast<unsigned>(entity.behaviorTimer)
            << ",\"weapon_slot_timers\":[";
        for (std::size_t slot = 0; slot < entity.weaponSlotCount; ++slot) {
            if (slot != 0) {
                output << ',';
            }
            output << static_cast<unsigned>(entity.weaponSlotTimers[slot]);
        }
        output << "]}";
    }
    output << "],\"contact_diagnostics\":[";
    for (std::size_t index = 0; index < semanticEntities.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        const auto& entity = semanticEntities[index];
        output << "{\"allocation_index\":" << entity.allocationIndex
               << ",\"contact_allocation_index\":";
        if (
            entity.collisionContactAllocationIndex
                == static_cast<std::size_t>(-1)
        ) {
            output << "null";
        } else {
            output << entity.collisionContactAllocationIndex;
        }
        output << ",\"contact_is_player\":"
               << (entity.collisionContactIsPlayer ? "true" : "false")
               << '}';
    }
    output << "],\"ground_points\":[";
    const auto& points = model.level2Ground().points();
    for (std::size_t index = 0; index < points.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << '[' << points[index].x << ',' << points[index].y << ']';
    }
    output
        << "],\"rng\":{\"index_a\":" << model.level2Rng().indexA()
        << ",\"index_b\":" << model.level2Rng().indexB()
        << ",\"last\":" << model.level2Rng().last()
        << ",\"calls_since_reset\":"
        << (model.level2Rng().calls() % stellar7::StellarRng::wordCount)
        << '}';
    auto futureRng = model.level2Rng();
    output << ",\"rng_future\":[";
    for (std::size_t index = 0; index < stellar7::StellarRng::wordCount; ++index) {
        output << (index == 0 ? "" : ",") << futureRng.next();
    }
    output << ']';
    if (model.phase() == stellar7::GameplayPhase::failureExplosion
        || (model.failureFromLevel2() && model.phase() == stellar7::GameplayPhase::continuePrompt)) {
        output << ",\"failure_state\":{\"presentation_frame\":"
            << model.presentationFrame() << ",\"player\":";
        writeFailureEntity(model.failurePlayer());
        output << ",\"mission_entities\":[";
        for (std::size_t index = 0;
             index < model.failureMissionEntities().size(); ++index) {
            if (index != 0) {
                output << ',';
            }
            writeFailureEntity(model.failureMissionEntities()[index]);
        }
        output << "],\"debris\":[";
        for (std::size_t index = 0;
             index < model.failureDebris().size(); ++index) {
            if (index != 0) {
                output << ',';
            }
            writeFailureEntity(model.failureDebris()[index]);
        }
        output << "],\"bursts\":[";
        for (std::size_t index = 0;
             index < model.failureBursts().size(); ++index) {
            if (index != 0) {
                output << ',';
            }
            const auto& burst = model.failureBursts()[index];
            output << "{\"position\":[" << burst.position.x << ','
                << burst.position.y << ',' << burst.position.z
                << "],\"spawn_frame\":" << burst.spawnFrame << '}';
        }
        output << "],\"visible_items\":[";
        for (std::size_t index = 0;
             index < model.failureVisibleItems().size(); ++index) {
            if (index != 0) {
                output << ',';
            }
            const auto& item = model.failureVisibleItems()[index];
            output
                << "{\"kind\":" << static_cast<unsigned>(item.kind)
                << ",\"index\":" << item.index << '}';
        }
        output
            << "],\"rng\":{\"index_a\":"
            << model.failureRng().indexA()
            << ",\"index_b\":" << model.failureRng().indexB()
            << ",\"last\":" << model.failureRng().last()
            << ",\"calls_since_reset\":"
            << (model.failureRng().calls() % stellar7::StellarRng::wordCount)
            << "}}";
    }
    output << "}\n";
    writeText(outputPath, output.str());
}

void writeLevel2InputScriptState(
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t count,
    const std::string& outputPath,
    const std::optional<std::int16_t> level2Armor = std::nullopt
) {
    writeLevel2ModelState(
        replayLevel1InputScriptGameplay(events, count, level2Armor), outputPath
    );
}

void writeAcceptanceStateSeries(
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::vector<std::uint32_t>& ticks,
    const std::string& outputDirectory,
    const std::optional<std::int16_t> armor,
    const std::vector<std::uint8_t>& checkpoint = {},
    const bool holdLevel2Armor = false
) {
    const bool level3 = !checkpoint.empty();
    if (level3) {
        stellar7::GameplayModel initial;
        stellar7::loadLevel3NativeCheckpoint(initial, checkpoint);
        if (ticks.front() <= initial.ticks()) {
            throw std::runtime_error("series ticks must follow the native checkpoint");
        }
    }
    std::filesystem::create_directories(outputDirectory);
    std::size_t next = 0;
    const auto observe = [&](const stellar7::GameplayModel& model) {
        if (next < ticks.size() && model.ticks() == ticks[next]) {
            const auto output = (std::filesystem::path(outputDirectory)
                / ("state-" + std::to_string(ticks[next]) + ".json")).string();
            if (level3) {
                writeLevel3ModelState(model, output);
            } else {
                writeLevel2ModelState(model, output);
            }
            ++next;
        }
    };
    if (level3) {
        replayLevel3NativeInputScript(checkpoint, events, ticks.back(), observe);
    } else {
        replayLevel1InputScriptGameplay(events, ticks.back(), armor, observe,
                                       holdLevel2Armor);
    }
    if (next != ticks.size()) {
        throw std::runtime_error("replay did not emit every requested checkpoint");
    }
}

stellar7::GameplayInput gameplayInputFromHeld(
    const std::set<std::string>& held
) {
    return {
        held.count("left") != 0,
        held.count("right") != 0,
        held.count("up") != 0,
        held.count("down") != 0,
        held.count("spc") != 0,
        held.count("esc") != 0,
    };
}

void writeCanonicalInputEvent(
    std::ostream& output,
    const std::uint32_t tick,
    const bool pressed,
    const std::vector<std::string>& keys
) {
    if (keys.empty()) {
        return;
    }
    output << tick << '=' << (pressed ? "down." : "up.");
    for (std::size_t index = 0; index < keys.size(); ++index) {
        if (index != 0) {
            output << '+';
        }
        output << keys[index];
    }
    output << '\n';
}

void emitInputTransition(
    std::ostream& output,
    const std::uint32_t tick,
    std::set<std::string>& held,
    const stellar7::GameplayInput desired
) {
    constexpr std::array<const char*, 6> keyNames{{
        "left", "right", "up", "down", "spc", "esc",
    }};
    const std::array<bool, 6> desiredState{{
        desired.left,
        desired.right,
        desired.forward,
        desired.reverse,
        desired.fire,
        desired.exit,
    }};
    std::vector<std::string> released;
    std::vector<std::string> pressed;
    for (std::size_t index = 0; index < keyNames.size(); ++index) {
        const std::string key(keyNames[index]);
        const auto isHeld = held.count(key) != 0;
        if (isHeld && !desiredState[index]) {
            released.push_back(key);
            held.erase(key);
        } else if (!isHeld && desiredState[index]) {
            pressed.push_back(key);
            held.insert(key);
        }
    }
    writeCanonicalInputEvent(output, tick, false, released);
    writeCanonicalInputEvent(output, tick, true, pressed);
}

void exportLevel2SuccessfulControllerRoute(
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::int16_t armor,
    const std::uint32_t controlStartTick,
    const std::uint32_t maximumTick,
    const std::string& outputScriptPath,
    const std::string& outputResultPath,
    const bool includeEarlyHostiles = false
) {
    if (controlStartTick == 0 || maximumTick <= controlStartTick) {
        throw std::runtime_error(
            "Level 2 controller requires 0 < start < maximum tick"
        );
    }
    stellar7::GameplayModel model;
    std::set<std::string> held;
    std::size_t nextEvent = 0;
    bool appliedArmor = false;
    for (std::uint32_t tick = 1; tick <= controlStartTick; ++tick) {
        applyLevel1InputEventsBeforeTick(events, tick, nextEvent, held);
        model.tick(gameplayInputFromHeld(held));
        if (
            !appliedArmor
            && (
                model.phase() == stellar7::GameplayPhase::level2Handoff
                || model.phase() == stellar7::GameplayPhase::level2
            )
        ) {
            stellar7::GameplayAnalysisAccess::applyLevel2Armor(model, armor);
            appliedArmor = true;
        }
        applyLevel1InputEventsAfterCheckpoint(events, tick, nextEvent, held);
    }
    if (model.phase() != stellar7::GameplayPhase::level2) {
        throw std::runtime_error(
            "Level 2 controller start did not reach active Level 2"
        );
    }

    std::ostringstream script;
    script
        << "dos-re-state-input-script-v1\n"
        << "# state_field=global_tick\n"
        << "# controller=level2-closed-loop-success-v1\n"
        << "# control_start=" << controlStartTick << '\n';
    for (const auto& event : events) {
        if (event.tick > controlStartTick) {
            break;
        }
        writeCanonicalInputEvent(
            script,
            event.tick,
            event.pressed,
            event.keys
        );
    }

    std::optional<std::uint32_t> guardianDestroyedTick;
    std::optional<std::uint32_t> resultContactTick;
    std::optional<std::uint32_t> terminalTick;
    auto observedGuardian = std::any_of(
        model.level2Entities().begin(),
        model.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 44 || entity.type == 45;
        }
    );
    for (
        std::uint32_t tick = controlStartTick + 1;
        tick <= maximumTick;
        ++tick
    ) {
        const auto target = std::find_if(
            model.level2Entities().begin(),
            model.level2Entities().end(),
            [](const stellar7::Level1Entity& entity) {
                return entity.type == 44
                    || entity.type == 45
                    || entity.type == 25
                    || entity.type == 26;
            }
        );
        const auto earlyTarget = includeEarlyHostiles
            ? std::find_if(
                model.level2Entities().begin(),
                model.level2Entities().end(),
                [](const stellar7::Level1Entity& entity) {
                    return entity.type == 8 || entity.type == 30;
                }
            )
            : model.level2Entities().end();
        const auto selectedTarget = target != model.level2Entities().end()
            ? target
            : earlyTarget;
        stellar7::GameplayInput desired{};
        if (selectedTarget != model.level2Entities().end()) {
            const auto& player = model.level2Player();
            const auto desiredHeading = stellar7::presenterTargetHeading(
                player.position,
                selectedTarget->position
            );
            const auto headingError = static_cast<std::int16_t>(
                desiredHeading - player.yaw
            );
            const auto steeringError = static_cast<std::int32_t>(headingError)
                - static_cast<std::int32_t>(player.yawVelocity) * 4;
            if (steeringError > 80) {
                desired.left = true;
            } else if (steeringError < -80) {
                desired.right = true;
            }
            const auto distance = stellar7::approximatePlanarDistance(
                player.position,
                selectedTarget->position
            );
            const auto absoluteHeadingError = std::abs(
                static_cast<std::int32_t>(headingError)
            );
            const auto guardian = selectedTarget->type == 44
                || selectedTarget->type == 45;
            const auto earlyHostile = selectedTarget->type == 8
                || selectedTarget->type == 30;
            desired.fire = (guardian || earlyHostile)
                && absoluteHeadingError < 0x600;
            const auto holdRange = guardian ? 7000U : 200U;
            if (earlyHostile) {
                // Keep the proven Level 2 survival axis while turning and
                // firing at the stationary early enemies. Closing distance
                // is unsafe because their projectiles share the player lane.
                desired.reverse = true;
            } else {
                desired.forward = absoluteHeadingError < 0x1000
                    && distance > holdRange;
            }
        }
        emitInputTransition(script, tick, held, desired);
        model.tick(desired);

        const auto hasGuardian = std::any_of(
            model.level2Entities().begin(),
            model.level2Entities().end(),
            [](const stellar7::Level1Entity& entity) {
                return entity.type == 44 || entity.type == 45;
            }
        );
        if (observedGuardian && !hasGuardian && !guardianDestroyedTick) {
            guardianDestroyedTick = tick;
        }
        observedGuardian = observedGuardian || hasGuardian;
        if (
            !resultContactTick
            && model.phase()
                == stellar7::GameplayPhase::level2SuccessPresentation
        ) {
            resultContactTick = tick;
        }
        if (model.phase() == stellar7::GameplayPhase::level3Handoff) {
            terminalTick = tick;
            emitInputTransition(script, tick + 1, held, {});
            break;
        }
        if (
            model.phase()
                == stellar7::GameplayPhase::failurePreExplosionBlackout
            || model.phase() == stellar7::GameplayPhase::failureExplosion
            || model.phase()
                == stellar7::GameplayPhase::failurePostExplosionBlackout
            || model.phase() == stellar7::GameplayPhase::continuePrompt
        ) {
            emitInputTransition(script, tick + 1, held, {});
            break;
        }
    }
    writeText(outputScriptPath, script.str());
    std::ostringstream result;
    result
        << "{\"format_version\":1"
        << ",\"success\":" << (terminalTick ? "true" : "false")
        << ",\"control_start_tick\":" << controlStartTick
        << ",\"final_tick\":" << model.ticks()
        << ",\"level2_loop_tick\":" << model.level2LoopTick()
        << ",\"phase\":\"" << gameplayPhaseName(model.phase()) << "\""
        << ",\"guardian_destroyed_tick\":";
    if (guardianDestroyedTick) {
        result << *guardianDestroyedTick;
    } else {
        result << "null";
    }
    result << ",\"result_contact_tick\":";
    if (resultContactTick) {
        result << *resultContactTick;
    } else {
        result << "null";
    }
    result << ",\"terminal_tick\":";
    if (terminalTick) {
        result << *terminalTick;
    } else {
        result << "null";
    }
    result << "}\n";
    writeText(outputResultPath, result.str());
}

void exportLevel3SuccessfulControllerRoute(
    const std::uint32_t maximumLoop,
    const std::string& outputScriptPath,
    const std::string& outputResultPath
) {
    if (maximumLoop < 2) {
        throw std::runtime_error(
            "Level 3 controller requires maximum loop >= 2"
        );
    }

    stellar7::GameplayModel model;
    model.startLevel3();
    const stellar7::GameplayInput entryInput{
        true, false, true, false, false, false,
    };
    model.tick(entryInput);
    if (
        model.phase() != stellar7::GameplayPhase::level3
        || model.level2LoopTick() != 1
    ) {
        throw std::runtime_error(
            "Level 3 controller did not reach the exact loop-1 entry"
        );
    }

    std::ostringstream script;
    script
        << "dos-re-state-input-script-v1\n"
        << "# state_field=loop_tick\n"
        << "# controller=level3-closed-loop-success-v1\n"
        << "# initial_held_qcodes=left+up\n"
        << "# preapplied_through=0\n"
        << "# resume_value=0\n"
        << "# first_hook_value=1\n";
    std::set<std::string> held{"left", "up"};
    std::optional<std::uint32_t> firstCompletionObjectLoop;
    std::optional<std::uint32_t> firstType44Loop;
    std::optional<std::uint32_t> firstType47Loop;
    std::optional<std::uint32_t> firstPickupLoop;
    std::optional<std::uint32_t> terminalLoop;
    std::int16_t minimumArmor = model.playerArmor();

    const auto isWaveTarget = [](const stellar7::Level1Entity& entity) {
        return entity.type == 2
            || entity.type == 3
            || entity.type == 12;
    };
    for (std::uint32_t loop = 2; loop <= maximumLoop; ++loop) {
        const auto& entities = model.level2Entities();
        const auto pickup = std::find_if(
            entities.begin(),
            entities.end(),
            [](const stellar7::Level1Entity& entity) {
                return entity.type == 26;
            }
        );
        const auto guardian = std::find_if(
            entities.begin(),
            entities.end(),
            [](const stellar7::Level1Entity& entity) {
                return entity.type == 44 || entity.type == 47;
            }
        );
        auto waveTarget = entities.end();
        std::uint32_t waveTargetDistance =
            std::numeric_limits<std::uint32_t>::max();
        for (auto candidate = entities.begin(); candidate != entities.end();
             ++candidate) {
            if (!isWaveTarget(*candidate)) {
                continue;
            }
            const auto distance = stellar7::approximatePlanarDistance(
                model.level2Player().position,
                candidate->position
            );
            if (distance < waveTargetDistance) {
                waveTarget = candidate;
                waveTargetDistance = distance;
            }
        }
        const auto selectedTarget = pickup != entities.end()
            ? pickup
            : guardian != entities.end() ? guardian : waveTarget;

        stellar7::GameplayInput desired{};
        if (selectedTarget != entities.end()) {
            const auto& player = model.level2Player();
            const auto desiredHeading = stellar7::presenterTargetHeading(
                player.position,
                selectedTarget->position
            );
            const auto headingError = static_cast<std::int16_t>(
                desiredHeading - player.yaw
            );
            const auto steeringError = static_cast<std::int32_t>(headingError)
                - static_cast<std::int32_t>(player.yawVelocity) * 4;
            if (steeringError > 80) {
                desired.right = true;
            } else if (steeringError < -80) {
                desired.left = true;
            }
            const auto distance = stellar7::approximatePlanarDistance(
                player.position,
                selectedTarget->position
            );
            const auto absoluteHeadingError = std::abs(
                static_cast<std::int32_t>(headingError)
            );
            const auto isPickup = selectedTarget->type == 26;
            desired.fire = !isPickup
                && absoluteHeadingError < 0x500
                && distance < 20000;
            if (isPickup) {
                desired.forward = absoluteHeadingError < 0x1000
                    && distance > 160;
            } else {
                desired.forward = absoluteHeadingError < 0x1800
                    && distance > 2500;
                desired.reverse = distance < 1000;
            }
        }

        emitInputTransition(
            script,
            model.level2LoopTick(),
            held,
            desired
        );
        model.tick(desired);
        minimumArmor = std::min(minimumArmor, model.playerArmor());

        const auto observeType = [&model](const std::int16_t type) {
            return std::any_of(
                model.level2Entities().begin(),
                model.level2Entities().end(),
                [type](const stellar7::Level1Entity& entity) {
                    return entity.type == type;
                }
            );
        };
        if (!firstCompletionObjectLoop && observeType(7)) {
            firstCompletionObjectLoop = model.level2LoopTick();
        }
        if (!firstType44Loop && observeType(44)) {
            firstType44Loop = model.level2LoopTick();
        }
        if (!firstType47Loop && observeType(47)) {
            firstType47Loop = model.level2LoopTick();
        }
        if (!firstPickupLoop && observeType(26)) {
            firstPickupLoop = model.level2LoopTick();
        }
        if (
            model.phase()
                == stellar7::GameplayPhase::level3SuccessPresentation
        ) {
            terminalLoop = model.level2LoopTick();
            emitInputTransition(
                script,
                model.level2LoopTick(),
                held,
                {}
            );
            break;
        }
        if (
            model.phase()
                == stellar7::GameplayPhase::failurePreExplosionBlackout
            || model.phase() == stellar7::GameplayPhase::failureExplosion
            || model.phase()
                == stellar7::GameplayPhase::failurePostExplosionBlackout
            || model.phase() == stellar7::GameplayPhase::continuePrompt
        ) {
            emitInputTransition(
                script,
                model.level2LoopTick(),
                held,
                {}
            );
            break;
        }
    }

    writeText(outputScriptPath, script.str());
    const auto writeOptional = [](std::ostream& output, const auto value) {
        if (value) {
            output << *value;
        } else {
            output << "null";
        }
    };
    std::ostringstream result;
    result
        << "{\"format_version\":1"
        << ",\"success\":" << (terminalLoop ? "true" : "false")
        << ",\"final_loop\":" << model.level2LoopTick()
        << ",\"phase\":\"" << gameplayPhaseName(model.phase()) << "\""
        << ",\"player_armor\":" << model.playerArmor()
        << ",\"minimum_armor\":" << minimumArmor
        << ",\"score\":" << model.score()
        << ",\"first_completion_object_loop\":";
    writeOptional(result, firstCompletionObjectLoop);
    result << ",\"first_type44_loop\":";
    writeOptional(result, firstType44Loop);
    result << ",\"first_type47_loop\":";
    writeOptional(result, firstType47Loop);
    result << ",\"first_pickup_loop\":";
    writeOptional(result, firstPickupLoop);
    result << ",\"terminal_loop\":";
    writeOptional(result, terminalLoop);
    result << "}\n";
    writeText(outputResultPath, result.str());
}

struct Level3ExactPickupSearchResult {
    std::vector<stellar7::GameplayInput> plan;
    std::uint64_t bestSquaredResidual =
        std::numeric_limits<std::uint64_t>::max();
    std::int32_t bestRelativeX = 0;
    std::int32_t bestRelativeY = 0;
    unsigned bestDepth = 0;
    std::size_t finalFrontierSize = 0;
};

Level3ExactPickupSearchResult findLevel3ExactPickupPlan(
    const stellar7::Level1Entity& sourcePlayer,
    const std::int32_t initialRelativeX,
    const std::int32_t initialRelativeY,
    const unsigned maximumDepth = 160,
    const std::size_t beamWidth = 16384
) {
    Level3ExactPickupSearchResult searchResult;
    searchResult.bestSquaredResidual = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(initialRelativeX) * initialRelativeX
        + static_cast<std::int64_t>(initialRelativeY) * initialRelativeY
    );
    searchResult.bestRelativeX = initialRelativeX;
    searchResult.bestRelativeY = initialRelativeY;
    const std::array<stellar7::GameplayInput, 9> controls{{
        {},
        {true, false, false, false, false, false},
        {false, true, false, false, false, false},
        {false, false, true, false, false, false},
        {false, false, false, true, false, false},
        {true, false, true, false, false, false},
        {false, true, true, false, false, false},
        {true, false, false, true, false, false},
        {false, true, false, true, false, false},
    }};
    struct SearchPath {
        std::uint32_t parent;
        std::uint8_t action;
    };
    struct SearchNode {
        std::int32_t relativeX;
        std::int32_t relativeY;
        std::uint16_t yaw;
        std::int16_t yawVelocity;
        std::int16_t forwardVelocity;
        std::uint32_t path;
    };
    struct SearchCandidate {
        SearchNode node;
        std::uint32_t parentPath;
        std::uint8_t action;
        std::uint64_t score;
        std::uint8_t residueBucket;
    };
    std::vector<SearchPath> paths;
    paths.push_back({std::numeric_limits<std::uint32_t>::max(), 0});
    std::vector<SearchNode> beam;
    beam.push_back({
        initialRelativeX,
        initialRelativeY,
        sourcePlayer.yaw,
        sourcePlayer.yawVelocity,
        sourcePlayer.forwardVelocity,
        0,
    });
    const auto reconstruct = [&paths, &controls](
        std::uint32_t parentPath,
        const std::uint8_t finalAction
    ) {
        std::vector<std::uint8_t> reversed{finalAction};
        while (parentPath != 0) {
            reversed.push_back(paths[parentPath].action);
            parentPath = paths[parentPath].parent;
        }
        std::reverse(reversed.begin(), reversed.end());
        std::vector<stellar7::GameplayInput> result;
        result.reserve(reversed.size());
        for (const auto action : reversed) {
            result.push_back(controls[action]);
        }
        return result;
    };
    const auto better = [](const SearchCandidate& left,
                           const SearchCandidate& right) {
        if (left.score != right.score) {
            return left.score < right.score;
        }
        if (left.node.relativeX != right.node.relativeX) {
            return left.node.relativeX < right.node.relativeX;
        }
        if (left.node.relativeY != right.node.relativeY) {
            return left.node.relativeY < right.node.relativeY;
        }
        if (left.node.yaw != right.node.yaw) {
            return left.node.yaw < right.node.yaw;
        }
        if (left.node.yawVelocity != right.node.yawVelocity) {
            return left.node.yawVelocity < right.node.yawVelocity;
        }
        if (left.node.forwardVelocity != right.node.forwardVelocity) {
            return left.node.forwardVelocity < right.node.forwardVelocity;
        }
        return left.action < right.action;
    };
    for (unsigned depth = 0; depth < maximumDepth; ++depth) {
        std::vector<SearchCandidate> candidates;
        candidates.reserve(beam.size() * controls.size());
        for (const auto& source : beam) {
            for (std::uint8_t action = 0; action < controls.size(); ++action) {
                auto yawVelocity = source.yawVelocity;
                auto forwardVelocity = source.forwardVelocity;
                // GameplayModel::tick installs the current input before the
                // shared Level 3 updater. Unlike Level 2, this phase does not
                // apply the preceding boundary's pending input.
                const auto& applied = controls[action];
                const bool directional = applied.left || applied.right
                    || applied.forward || applied.reverse;
                if (applied.left || applied.right) {
                    const auto adjusted = static_cast<std::int32_t>(
                        yawVelocity
                    ) + (applied.left ? 0x50 : -0x50);
                    yawVelocity = static_cast<std::int16_t>(
                        std::clamp(adjusted, -0x200, 0x200)
                    );
                } else if (yawVelocity > 0) {
                    yawVelocity = static_cast<std::int16_t>(
                        std::max(0, yawVelocity - 0x50)
                    );
                } else if (yawVelocity < 0) {
                    yawVelocity = static_cast<std::int16_t>(
                        std::min(0, yawVelocity + 0x50)
                    );
                }
                if (applied.forward || applied.reverse) {
                    const auto adjusted = static_cast<std::int32_t>(
                        forwardVelocity
                    ) + (applied.reverse ? -10 : 10);
                    forwardVelocity = static_cast<std::int16_t>(
                        std::clamp(adjusted, -200, 200)
                    );
                } else if (!directional && forwardVelocity > 0) {
                    forwardVelocity = static_cast<std::int16_t>(
                        std::max(0, forwardVelocity - 10)
                    );
                } else if (!directional && forwardVelocity < 0) {
                    forwardVelocity = static_cast<std::int16_t>(
                        std::min(0, forwardVelocity + 10)
                    );
                }
                const auto yaw = static_cast<std::uint16_t>(
                    source.yaw + yawVelocity
                );
                auto movement = sourcePlayer;
                movement.position = {};
                movement.yaw = yaw;
                movement.yawVelocity = 0;
                movement.forwardVelocity = forwardVelocity;
                stellar7::advanceEntityKinematics(movement);
                const auto relativeX = static_cast<std::int32_t>(
                    source.relativeX - movement.position.x
                );
                const auto relativeY = static_cast<std::int32_t>(
                    source.relativeY - movement.position.y
                );
                const auto residual = static_cast<std::uint64_t>(
                    static_cast<std::int64_t>(relativeX) * relativeX
                    + static_cast<std::int64_t>(relativeY) * relativeY
                );
                if (residual < searchResult.bestSquaredResidual) {
                    searchResult.bestSquaredResidual = residual;
                    searchResult.bestRelativeX = relativeX;
                    searchResult.bestRelativeY = relativeY;
                    searchResult.bestDepth = depth + 1U;
                }
                if (relativeX == 0 && relativeY == 0) {
                    searchResult.plan = reconstruct(source.path, action);
                    searchResult.finalFrontierSize = beam.size();
                    return searchResult;
                }
                if (
                    std::abs(static_cast<std::int64_t>(relativeX)) > 4096
                    || std::abs(static_cast<std::int64_t>(relativeY)) > 4096
                ) {
                    continue;
                }
                const auto score = residual * 1024U
                    + static_cast<std::uint64_t>(std::abs(
                        static_cast<std::int32_t>(forwardVelocity)
                    )) * 16U
                    + static_cast<std::uint64_t>(std::abs(
                        static_cast<std::int32_t>(yawVelocity)
                    ));
                const auto residueX = static_cast<unsigned>(
                    (relativeX % 16 + 16) % 16
                );
                const auto residueY = static_cast<unsigned>(
                    (relativeY % 16 + 16) % 16
                );
                candidates.push_back({
                    {
                        relativeX,
                        relativeY,
                        yaw,
                        yawVelocity,
                        forwardVelocity,
                        0,
                    },
                    source.path,
                    action,
                    score,
                    static_cast<std::uint8_t>(residueX * 16U + residueY),
                });
            }
        }
        if (candidates.empty()) {
            break;
        }
        std::vector<bool> retained(candidates.size(), false);
        std::vector<std::size_t> global(candidates.size());
        for (std::size_t index = 0; index < global.size(); ++index) {
            global[index] = index;
        }
        const auto globalCount = std::min(
            beamWidth / 2U,
            global.size()
        );
        if (globalCount < global.size()) {
            std::nth_element(
                global.begin(),
                global.begin() + static_cast<std::ptrdiff_t>(globalCount),
                global.end(),
                [&candidates, &better](const std::size_t left,
                                       const std::size_t right) {
                    return better(candidates[left], candidates[right]);
                }
            );
        }
        for (std::size_t index = 0; index < globalCount; ++index) {
            retained[global[index]] = true;
        }
        std::array<std::vector<std::size_t>, 256> buckets;
        for (std::size_t index = 0; index < candidates.size(); ++index) {
            buckets[candidates[index].residueBucket].push_back(index);
        }
        const auto perBucket = std::max<std::size_t>(
            1,
            beamWidth / (2U * buckets.size())
        );
        for (auto& bucket : buckets) {
            const auto count = std::min(perBucket, bucket.size());
            if (count < bucket.size()) {
                std::nth_element(
                    bucket.begin(),
                    bucket.begin() + static_cast<std::ptrdiff_t>(count),
                    bucket.end(),
                    [&candidates, &better](const std::size_t left,
                                           const std::size_t right) {
                        return better(candidates[left], candidates[right]);
                    }
                );
            }
            for (std::size_t index = 0; index < count; ++index) {
                retained[bucket[index]] = true;
            }
        }
        std::vector<SearchNode> next;
        next.reserve(beamWidth);
        for (std::size_t index = 0;
             index < candidates.size() && next.size() < beamWidth;
             ++index) {
            if (!retained[index]) {
                continue;
            }
            auto node = candidates[index].node;
            node.path = static_cast<std::uint32_t>(paths.size());
            paths.push_back({
                candidates[index].parentPath,
                candidates[index].action,
            });
            next.push_back(node);
        }
        beam = std::move(next);
    }
    searchResult.finalFrontierSize = beam.size();
    return searchResult;
}

void exportLevel3NativeInputScriptControllerRoute(
    const std::vector<std::uint8_t>& checkpoint,
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t controlStartTick,
    const std::uint32_t maximumTick,
    const std::string& outputScriptPath,
    const std::string& outputResultPath
) {
    if (maximumTick <= controlStartTick) {
        throw std::runtime_error(
            "Level 3 checkpoint controller requires start < maximum tick"
        );
    }
    stellar7::GameplayModel model;
    stellar7::loadLevel3NativeCheckpoint(model, checkpoint);
    if (controlStartTick < model.ticks()) {
        throw std::runtime_error(
            "Level 3 controller start precedes native checkpoint"
        );
    }
    std::set<std::string> held;
    std::size_t nextEvent = 0;
    while (
        nextEvent < events.size()
        && events[nextEvent].tick <= model.ticks()
    ) {
        for (const auto& key : events[nextEvent].keys) {
            if (events[nextEvent].pressed) {
                held.insert(key);
            } else {
                held.erase(key);
            }
        }
        ++nextEvent;
    }
    while (model.ticks() < controlStartTick) {
        const auto tick = static_cast<std::uint32_t>(model.ticks() + 1U);
        applyLevel1InputEventsAtCheckpoint(events, tick, nextEvent, held);
        model.tick(gameplayInputFromHeld(held));
    }
    if (model.phase() != stellar7::GameplayPhase::level3) {
        throw std::runtime_error(
            "Level 3 checkpoint controller did not reach active gameplay"
        );
    }

    std::ostringstream script;
    script
        << "dos-re-state-input-script-v1\n"
        << "# state_field=loop_tick\n"
        << "# controller=level3-native-checkpoint-closed-loop-success-v1\n"
        << "# control_start=" << controlStartTick << '\n';
    for (const auto& event : events) {
        if (event.tick > controlStartTick) {
            break;
        }
        writeCanonicalInputEvent(
            script,
            event.tick,
            event.pressed,
            event.keys
        );
    }

    const auto hasType = [&model](const std::int16_t type) {
        return std::any_of(
            model.level2Entities().begin(),
            model.level2Entities().end(),
            [type](const stellar7::Level1Entity& entity) {
                return entity.active() && entity.type == type;
            }
        );
    };
    bool observedGuardian = hasType(44) || hasType(45);
    std::optional<std::uint32_t> guardianDestroyedTick;
    std::optional<std::uint32_t> pickupObservedTick;
    std::optional<std::uint32_t> terminalTick;
    std::int16_t minimumArmor = model.playerArmor();
    stellar7::GameplayInput pickupPlan{};
    unsigned pickupPlanTicks = 0;
    std::vector<stellar7::GameplayInput> pickupExactPlan;
    std::size_t pickupExactPlanIndex = 0;
    std::size_t pickupExactPlanLength = 0;
    std::uint32_t pickupExactNextSearchTick = controlStartTick + 1U;
    std::uint64_t pickupSearchBestSquaredResidual =
        std::numeric_limits<std::uint64_t>::max();
    std::int32_t pickupSearchBestRelativeX = 0;
    std::int32_t pickupSearchBestRelativeY = 0;
    unsigned pickupSearchBestDepth = 0;
    std::size_t pickupSearchFinalFrontierSize = 0;
    stellar7::GameplayInput guardianPlan{};
    unsigned guardianPlanTicks = 0;

    for (
        std::uint32_t tick = controlStartTick + 1;
        tick <= maximumTick;
        ++tick
    ) {
        const auto& entities = model.level2Entities();
        auto guardian = entities.end();
        auto guardianDistance = std::numeric_limits<std::uint32_t>::max();
        auto guardianHasCollisionVolume = false;
        for (auto candidate = entities.begin(); candidate != entities.end();
             ++candidate) {
            if (
                !candidate->active()
                || (candidate->type != 44 && candidate->type != 45)
            ) {
                continue;
            }
            const auto distance = stellar7::approximatePlanarDistance(
                model.level2Player().position,
                candidate->position
            );
            const auto hasCollisionVolume = candidate->collisionRadius != 0;
            if (
                (hasCollisionVolume && !guardianHasCollisionVolume)
                || (
                    hasCollisionVolume == guardianHasCollisionVolume
                    && distance < guardianDistance
                )
            ) {
                guardian = candidate;
                guardianDistance = distance;
                guardianHasCollisionVolume = hasCollisionVolume;
            }
        }
        const auto pickup = std::find_if(
            entities.begin(),
            entities.end(),
            [](const stellar7::Level1Entity& entity) {
                return entity.active()
                    && (entity.type == 25 || entity.type == 26);
            }
        );
        const auto target = pickup != entities.end() ? pickup : guardian;

        stellar7::GameplayInput desired{};
        if (target != entities.end()) {
            const auto& player = model.level2Player();
            const auto distance = stellar7::approximatePlanarDistance(
                player.position,
                target->position
            );
            const auto isPickup = target->type == 25 || target->type == 26;
            auto aimPosition = target->position;
            const auto desiredHeading = stellar7::presenterTargetHeading(
                player.position,
                aimPosition
            );
            const auto headingError = static_cast<std::int16_t>(
                desiredHeading - player.yaw
            );
            const auto steeringError = static_cast<std::int32_t>(headingError)
                - static_cast<std::int32_t>(player.yawVelocity) * 4;
            if (steeringError > 80) {
                desired.left = true;
            } else if (steeringError < -80) {
                desired.right = true;
            }
            const auto absoluteHeadingError = std::abs(
                static_cast<std::int32_t>(headingError)
            );
            if (isPickup) {
                guardianPlanTicks = 0;
                auto usingExactPickupPlan = false;
                if (
                    distance <= 4096U
                    && (
                        pickupExactPlanIndex < pickupExactPlan.size()
                        || tick >= pickupExactNextSearchTick
                    )
                ) {
                    // Level 3 gives both the player and Type 25 a zero
                    // collision radius, so completion requires exact XYZ
                    // coincidence. Constant-input MPC converges near the
                    // pickup but stalls on a nonzero integer residual. Use a
                    // bounded state-diverse beam only in the post-guardian
                    // lattice neighborhood.
                    pickupPlanTicks = 0;
                    if (pickupExactPlanIndex < pickupExactPlan.size()) {
                        desired = pickupExactPlan[pickupExactPlanIndex++];
                        usingExactPickupPlan = true;
                    } else {
                        pickupExactNextSearchTick = tick + 16U;
                        auto search = findLevel3ExactPickupPlan(
                            model.level2Player(),
                            target->position.x - player.position.x,
                            target->position.y - player.position.y,
                            160,
                            16384
                        );
                        pickupExactPlan = std::move(search.plan);
                        pickupSearchBestSquaredResidual =
                            search.bestSquaredResidual;
                        pickupSearchBestRelativeX = search.bestRelativeX;
                        pickupSearchBestRelativeY = search.bestRelativeY;
                        pickupSearchBestDepth = search.bestDepth;
                        pickupSearchFinalFrontierSize =
                            search.finalFrontierSize;
                        if (!pickupExactPlan.empty()) {
                            pickupExactPlanLength = pickupExactPlan.size();
                            desired = pickupExactPlan[0];
                            pickupExactPlanIndex = 1;
                            usingExactPickupPlan = true;
                        } else {
                            // The exact solver is a bounded diagnostic. Fall
                            // back to the ordinary approach controller when
                            // it cannot prove a route from this state.
                            pickupPlanTicks = 0;
                        }
                    }
                }
                if (!usingExactPickupPlan && pickupPlanTicks == 0) {
                    const std::array<stellar7::GameplayInput, 9> candidates{{
                        {},
                        {true, false, false, false, false, false},
                        {false, true, false, false, false, false},
                        {false, false, true, false, false, false},
                        {false, false, false, true, false, false},
                        {true, false, true, false, false, false},
                        {false, true, true, false, false, false},
                        {true, false, false, true, false, false},
                        {false, true, false, true, false, false},
                    }};
                    auto bestDistance = std::numeric_limits<std::uint32_t>::max();
                    for (const auto candidate : candidates) {
                        auto trial = model;
                        auto candidateDistance = bestDistance;
                        for (unsigned probe = 0; probe < 64; ++probe) {
                            trial.tick(candidate);
                            if (
                                trial.phase()
                                    != stellar7::GameplayPhase::level3
                                && trial.phase()
                                    != stellar7::GameplayPhase::level3SuccessPresentation
                            ) {
                                candidateDistance =
                                    std::numeric_limits<std::uint32_t>::max();
                                break;
                            }
                            if (
                                trial.phase()
                                    == stellar7::GameplayPhase::level3SuccessPresentation
                            ) {
                                candidateDistance = 0;
                                break;
                            }
                            const auto trialPickup = std::find_if(
                                trial.level2Entities().begin(),
                                trial.level2Entities().end(),
                                [](const stellar7::Level1Entity& entity) {
                                    return entity.active()
                                        && (entity.type == 25
                                            || entity.type == 26);
                                }
                            );
                            if (trialPickup == trial.level2Entities().end()) {
                                candidateDistance = 0;
                                break;
                            }
                            candidateDistance = std::min(
                                candidateDistance,
                                stellar7::approximatePlanarDistance(
                                    trial.level2Player().position,
                                    trialPickup->position
                                )
                            );
                        }
                        if (candidateDistance < bestDistance) {
                            bestDistance = candidateDistance;
                            pickupPlan = candidate;
                        }
                    }
                    pickupPlanTicks = 4;
                }
                if (!usingExactPickupPlan) {
                    desired = pickupPlan;
                    --pickupPlanTicks;
                }
            } else {
                pickupPlanTicks = 0;
                if (guardianPlanTicks == 0) {
                    const std::array<stellar7::GameplayInput, 9> candidates{{
                        {false, false, false, false, true, false},
                        {true, false, false, false, true, false},
                        {false, true, false, false, true, false},
                        {false, false, true, false, true, false},
                        {false, false, false, true, true, false},
                        {true, false, true, false, true, false},
                        {false, true, true, false, true, false},
                        {true, false, false, true, true, false},
                        {false, true, false, true, true, false},
                    }};
                    auto bestHealth = std::numeric_limits<unsigned>::max();
                    auto bestContactHazard =
                        std::numeric_limits<unsigned>::max();
                    auto bestArmorLoss =
                        std::numeric_limits<unsigned>::max();
                    auto bestHeadingError =
                        std::numeric_limits<std::uint32_t>::max();
                    auto bestRangeError =
                        std::numeric_limits<std::uint32_t>::max();
                    auto bestSurvived = false;
                    for (const auto candidate : candidates) {
                        auto trial = model;
                        auto candidateSurvived = true;
                        const auto startingArmor = trial.playerArmor();
                        auto lowestArmor = startingArmor;
                        auto minimumHealth =
                            std::numeric_limits<unsigned>::max();
                        auto contactHazard = 0U;
                        auto minimumHeadingError =
                            std::numeric_limits<std::uint32_t>::max();
                        auto minimumRangeError =
                            std::numeric_limits<std::uint32_t>::max();
                        for (unsigned probe = 0; probe < 48; ++probe) {
                            trial.tick(candidate);
                            lowestArmor = std::min(
                                lowestArmor,
                                trial.playerArmor()
                            );
                            if (
                                trial.phase()
                                    != stellar7::GameplayPhase::level3
                                && trial.phase()
                                    != stellar7::GameplayPhase::level3SuccessPresentation
                            ) {
                                candidateSurvived = false;
                                break;
                            }
                            for (const auto& trialEntity :
                                 trial.level2Entities()) {
                                if (
                                    trialEntity.active()
                                    && trialEntity.type == 47
                                    && !trialEntity.collisionContactIsPlayer
                                    && trialEntity
                                            .collisionContactAllocationIndex
                                        != static_cast<std::size_t>(-1)
                                ) {
                                    ++contactHazard;
                                }
                            }
                            auto trialGuardian =
                                trial.level2Entities().end();
                            for (
                                auto candidateGuardian =
                                    trial.level2Entities().begin();
                                candidateGuardian
                                    != trial.level2Entities().end();
                                ++candidateGuardian
                            ) {
                                if (
                                    candidateGuardian->active()
                                    && (candidateGuardian->type == 44
                                        || candidateGuardian->type == 45)
                                    && candidateGuardian->collisionRadius != 0
                                ) {
                                    trialGuardian = candidateGuardian;
                                    break;
                                }
                            }
                            if (
                                trialGuardian
                                    == trial.level2Entities().end()
                            ) {
                                minimumHealth = 0;
                                minimumHeadingError = 0;
                                minimumRangeError = 0;
                                break;
                            }
                            minimumHealth = std::min<unsigned>(
                                minimumHealth,
                                trialGuardian->stateBytes4dTo52[1]
                            );
                            const auto trialHeading =
                                stellar7::presenterTargetHeading(
                                    trial.level2Player().position,
                                    trialGuardian->position
                                );
                            const auto trialError = static_cast<std::int16_t>(
                                trialHeading - trial.level2Player().yaw
                            );
                            minimumHeadingError = std::min<std::uint32_t>(
                                minimumHeadingError,
                                static_cast<std::uint32_t>(std::abs(
                                    static_cast<std::int32_t>(trialError)
                                ))
                            );
                            const auto trialDistance =
                                stellar7::approximatePlanarDistance(
                                    trial.level2Player().position,
                                    trialGuardian->position
                                );
                            minimumRangeError = std::min<std::uint32_t>(
                                minimumRangeError,
                                trialDistance > 8000U
                                    ? trialDistance - 8000U
                                    : 8000U - trialDistance
                            );
                        }
                        const auto armorLoss = static_cast<unsigned>(
                            std::max<int>(
                                0,
                                static_cast<int>(startingArmor)
                                    - static_cast<int>(lowestArmor)
                            )
                        );
                        if (
                            (candidateSurvived && !bestSurvived)
                            || (
                                candidateSurvived == bestSurvived
                                && armorLoss < bestArmorLoss
                            )
                            || (
                                candidateSurvived == bestSurvived
                                && armorLoss == bestArmorLoss
                                && contactHazard < bestContactHazard
                            )
                            || (
                                candidateSurvived == bestSurvived
                                && armorLoss == bestArmorLoss
                                && contactHazard == bestContactHazard
                                && minimumHealth < bestHealth
                            )
                            || (
                                candidateSurvived == bestSurvived
                                && armorLoss == bestArmorLoss
                                && contactHazard == bestContactHazard
                                &&
                                minimumHealth == bestHealth
                                && minimumHeadingError < bestHeadingError
                            )
                            || (
                                candidateSurvived == bestSurvived
                                && armorLoss == bestArmorLoss
                                && contactHazard == bestContactHazard
                                &&
                                minimumHealth == bestHealth
                                && minimumHeadingError == bestHeadingError
                                && minimumRangeError < bestRangeError
                            )
                        ) {
                            bestHealth = minimumHealth;
                            bestArmorLoss = armorLoss;
                            bestContactHazard = contactHazard;
                            bestHeadingError = minimumHeadingError;
                            bestRangeError = minimumRangeError;
                            bestSurvived = candidateSurvived;
                            guardianPlan = candidate;
                        }
                    }
                    guardianPlanTicks = 8;
                }
                desired = guardianPlan;
                --guardianPlanTicks;
            }
        }

        emitInputTransition(script, tick, held, desired);
        model.tick(desired);
        minimumArmor = std::min(minimumArmor, model.playerArmor());

        const auto hasGuardian = hasType(44) || hasType(45);
        if (observedGuardian && !hasGuardian && !guardianDestroyedTick) {
            guardianDestroyedTick = tick;
        }
        observedGuardian = observedGuardian || hasGuardian;
        if (!pickupObservedTick && (hasType(25) || hasType(26))) {
            pickupObservedTick = tick;
        }
        if (
            model.phase()
                == stellar7::GameplayPhase::level3SuccessPresentation
        ) {
            terminalTick = tick;
            emitInputTransition(script, tick + 1U, held, {});
            break;
        }
        if (
            model.phase()
                == stellar7::GameplayPhase::failurePreExplosionBlackout
            || model.phase() == stellar7::GameplayPhase::failureExplosion
            || model.phase()
                == stellar7::GameplayPhase::failurePostExplosionBlackout
            || model.phase() == stellar7::GameplayPhase::continuePrompt
        ) {
            emitInputTransition(script, tick + 1U, held, {});
            break;
        }
    }

    emitInputTransition(script, maximumTick + 1U, held, {});
    writeText(outputScriptPath, script.str());
    const auto writeOptional = [](std::ostream& output, const auto value) {
        if (value) {
            output << *value;
        } else {
            output << "null";
        }
    };
    std::ostringstream result;
    result
        << "{\"format_version\":1"
        << ",\"success\":" << (terminalTick ? "true" : "false")
        << ",\"control_start_tick\":" << controlStartTick
        << ",\"final_tick\":" << model.ticks()
        << ",\"loop_tick\":" << model.level2LoopTick()
        << ",\"phase\":\"" << gameplayPhaseName(model.phase()) << "\""
        << ",\"player_armor\":" << model.playerArmor()
        << ",\"minimum_armor\":" << minimumArmor
        << ",\"controlled_armor_value\":null"
        << ",\"score\":" << model.score()
        << ",\"pickup_exact_plan_length\":"
        << pickupExactPlanLength
        << ",\"pickup_search_best_squared_residual\":";
    if (
        pickupSearchBestSquaredResidual
        == std::numeric_limits<std::uint64_t>::max()
    ) {
        result << "null";
    } else {
        result << pickupSearchBestSquaredResidual;
    }
    result
        << ",\"pickup_search_best_relative\":["
        << pickupSearchBestRelativeX << ','
        << pickupSearchBestRelativeY << ']'
        << ",\"pickup_search_best_depth\":"
        << pickupSearchBestDepth
        << ",\"pickup_search_final_frontier_size\":"
        << pickupSearchFinalFrontierSize
        << ",\"guardian_destroyed_tick\":";
    writeOptional(result, guardianDestroyedTick);
    result << ",\"pickup_observed_tick\":";
    writeOptional(result, pickupObservedTick);
    result << ",\"terminal_tick\":";
    writeOptional(result, terminalTick);
    result << "}\n";
    writeText(outputResultPath, result.str());
}

void writeLevel1InputScriptRgbSequence(
    const stellar7::GameplayAssets& assets,
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t count,
    const std::string& rgbPath,
    const std::string& manifestPath
) {
    if (count == 0 || events.empty()) {
        throw std::runtime_error(
            "input script sequence requires events and a positive tick count"
        );
    }
    const std::filesystem::path rgbOutputPath(rgbPath);
    const std::filesystem::path manifestOutputPath(manifestPath);
    if (rgbOutputPath.has_parent_path()) {
        std::filesystem::create_directories(rgbOutputPath.parent_path());
    }
    if (manifestOutputPath.has_parent_path()) {
        std::filesystem::create_directories(
            manifestOutputPath.parent_path()
        );
    }
    std::ofstream rgbOutput(rgbOutputPath, std::ios::binary);
    if (!rgbOutput) {
        throw std::runtime_error("cannot open RGB sequence: " + rgbPath);
    }

    stellar7::GameplayModel model;
    std::set<std::string> held;
    std::size_t nextEvent = 0;
    std::ostringstream frames;
    for (std::uint32_t tick = 1; tick <= count; ++tick) {
        applyLevel1InputEventsBeforeTick(events, tick, nextEvent, held);
        model.tick({
            held.count("left") != 0,
            held.count("right") != 0,
            held.count("up") != 0,
            held.count("down") != 0,
            held.count("spc") != 0,
            false,
        });
        const auto frame = stellar7::renderGameplayIndexed(assets, model);
        for (const std::uint8_t index : frame.pixels) {
            const auto color = frame.palette[index];
            rgbOutput.put(static_cast<char>(color.red));
            rgbOutput.put(static_cast<char>(color.green));
            rgbOutput.put(static_cast<char>(color.blue));
        }
        if (!rgbOutput) {
            throw std::runtime_error(
                "failed while writing RGB sequence: " + rgbPath
            );
        }
        if (tick != 1) {
            frames << ',';
        }
        frames
            << "{\"frame\":" << (tick - 1)
            << ",\"tick\":" << model.ticks()
            << ",\"loop_tick\":" << model.level1State().loopTick()
            << ",\"armor\":" << model.playerArmor()
            << ",\"damage_flash_active\":"
            << (model.damageFlashActive() ? "true" : "false")
            << ",\"damage_flash_age\":" << model.damageFlashAge()
            << ",\"phase\":\"" << gameplayPhaseName(model.phase()) << "\""
            << ",\"presentation_frame\":" << model.presentationFrame()
            << ",\"byte_offset\":"
            << (
                static_cast<std::uint64_t>(tick - 1)
                * 320U * 200U * 3U
            )
            << '}';
        applyLevel1InputEventsAfterCheckpoint(events, tick, nextEvent, held);
    }
    rgbOutput.close();

    std::ostringstream manifest;
    manifest
        << "{\"format_version\":1"
        << ",\"width\":320"
        << ",\"height\":200"
        << ",\"pixel_format\":\"rgb24\""
        << ",\"frame_rate\":30"
        << ",\"frame_count\":" << count
        << ",\"raw_rgb_filename\":\""
        << rgbOutputPath.filename().string()
        << "\",\"frames\":[" << frames.str() << "]}\n";
    writeText(manifestPath, manifest.str());
}

void writeLevel1InputScriptIndexedSequence(
    const stellar7::GameplayAssets& assets,
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t count,
    const std::string& outputPath
) {
    if (count == 0 || events.empty()) {
        throw std::runtime_error(
            "input script indexed sequence requires events and a positive "
            "tick count"
        );
    }
    const std::filesystem::path outputFile(outputPath);
    if (outputFile.has_parent_path()) {
        std::filesystem::create_directories(outputFile.parent_path());
    }
    std::ofstream output(outputFile, std::ios::binary);
    if (!output) {
        throw std::runtime_error(
            "cannot open indexed sequence: " + outputPath
        );
    }

    stellar7::GameplayModel model;
    std::set<std::string> held;
    std::size_t nextEvent = 0;
    for (std::uint32_t tick = 1; tick <= count; ++tick) {
        applyLevel1InputEventsBeforeTick(events, tick, nextEvent, held);
        model.tick({
            held.count("left") != 0,
            held.count("right") != 0,
            held.count("up") != 0,
            held.count("down") != 0,
            held.count("spc") != 0,
            false,
        });
        const auto frame = stellar7::renderGameplayIndexed(assets, model);
        output.write(
            reinterpret_cast<const char*>(frame.pixels.data()),
            static_cast<std::streamsize>(frame.pixels.size())
        );
        if (!output) {
            throw std::runtime_error(
                "failed while writing indexed sequence: " + outputPath
            );
        }
        applyLevel1InputEventsAfterCheckpoint(events, tick, nextEvent, held);
    }
}

std::uint8_t collapseVgaDacComponent(const std::uint8_t component) {
    for (std::uint8_t raw = 0; raw <= 63; ++raw) {
        if (
            static_cast<std::uint8_t>((raw << 2) | (raw >> 4))
            == component
        ) {
            return raw;
        }
    }
    throw std::runtime_error("palette component is not a VGA DAC expansion");
}

void writeLevel1InputScriptDacSequence(
    const stellar7::GameplayAssets& assets,
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t count,
    const std::string& outputPath
) {
    if (count == 0 || events.empty()) {
        throw std::runtime_error(
            "input script DAC sequence requires events and a positive tick count"
        );
    }
    const std::filesystem::path outputFile(outputPath);
    if (outputFile.has_parent_path()) {
        std::filesystem::create_directories(outputFile.parent_path());
    }
    std::ofstream output(outputFile, std::ios::binary);
    if (!output) {
        throw std::runtime_error("cannot open DAC sequence: " + outputPath);
    }

    stellar7::GameplayModel model;
    std::set<std::string> held;
    std::size_t nextEvent = 0;
    for (std::uint32_t tick = 1; tick <= count; ++tick) {
        applyLevel1InputEventsBeforeTick(events, tick, nextEvent, held);
        model.tick({
            held.count("left") != 0,
            held.count("right") != 0,
            held.count("up") != 0,
            held.count("down") != 0,
            held.count("spc") != 0,
            false,
        });
        const auto palette = stellar7::renderGameplayIndexed(assets, model).palette;
        if (palette.size() != 256) {
            throw std::runtime_error("gameplay palette does not have 256 entries");
        }
        for (const auto& color : palette) {
            output.put(static_cast<char>(collapseVgaDacComponent(color.red)));
            output.put(static_cast<char>(collapseVgaDacComponent(color.green)));
            output.put(static_cast<char>(collapseVgaDacComponent(color.blue)));
        }
        applyLevel1InputEventsAfterCheckpoint(events, tick, nextEvent, held);
    }
    if (!output) {
        throw std::runtime_error("failed while writing DAC sequence: " + outputPath);
    }
}

void writeLevel1InputScriptSoundBlaster(
    const stellar7::GameplayAssets& assets,
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t count,
    const std::string& outputPath,
    const std::optional<std::int16_t> level2Armor = std::nullopt
) {
    if (count == 0 || events.empty()) {
        throw std::runtime_error(
            "input script audio requires events and a positive tick count"
        );
    }
    constexpr std::size_t samplesPerFrame =
        stellar7::gameplayAudioSamplesPerFrame;

    stellar7::SoundBlasterSynth synth(
        assets.soundSequences,
        assets.soundBlasterInstrumentResource,
        assets.musicForLevel(0),
        assets.presentationMusicSequence,
        assets.musicForLevel(1),
        assets.musicForLevel(2), assets.logoMusicSequence
    );
    synth.startLevel1();
    stellar7::PresentationClock clock(synth);
    stellar7::GameplayModel model;
    std::set<std::string> held;
    std::size_t nextEvent = 0;
    bool appliedLevel2Armor = false;
    std::size_t audioEventCursor = 0;
    std::size_t level2AudioEventCursor = 0;
    std::size_t failureAudioEventCursor = 0;
    auto output = clock.renderEntry();
    output.reserve(output.size() + static_cast<std::size_t>(count) * samplesPerFrame);
    for (std::uint32_t tick = 1; tick <= count; ++tick) {
        applyLevel1InputEventsBeforeTick(events, tick, nextEvent, held);
        const auto previousPhase = model.phase();
        model.setSuccessAudioActive(synth.successActive());
        model.tick({
            held.count("left") != 0,
            held.count("right") != 0,
            held.count("up") != 0,
            held.count("down") != 0,
            held.count("spc") != 0,
            false,
        });
        if (
            level2Armor.has_value()
            && !appliedLevel2Armor
            && (
                model.phase() == stellar7::GameplayPhase::level2Handoff
                || model.phase() == stellar7::GameplayPhase::level2
            )
        ) {
            stellar7::GameplayAnalysisAccess::applyLevel2Armor(
                model,
                *level2Armor
            );
            appliedLevel2Armor = true;
        }
        const auto enteredSuccessPresentation =
            (
                previousPhase
                    != stellar7::GameplayPhase::successPresentation
                && model.phase()
                    == stellar7::GameplayPhase::successPresentation
            )
            || (
                previousPhase
                    != stellar7::GameplayPhase::level2SuccessPresentation
                && model.phase()
                    == stellar7::GameplayPhase::level2SuccessPresentation
            )
            || (
                previousPhase
                    != stellar7::GameplayPhase::level3SuccessPresentation
                && model.phase()
                    == stellar7::GameplayPhase::level3SuccessPresentation
            );
        if (enteredSuccessPresentation) {
            synth.startSuccess();
        }
        if (
            previousPhase == stellar7::GameplayPhase::successPresentation
            && model.phase() == stellar7::GameplayPhase::level2Handoff
        ) {
            synth.stopSuccess();
            synth.switchLevel2();
        }
        if (
            previousPhase
                == stellar7::GameplayPhase::level2SuccessPresentation
            && model.phase() == stellar7::GameplayPhase::level3Handoff
        ) {
            synth.stopSuccess();
            synth.switchLevel3();
        }
        if (
            previousPhase
                == stellar7::GameplayPhase::level3SuccessPresentation
            && model.phase() == stellar7::GameplayPhase::level4Handoff
        ) {
            synth.stopSuccess();
        }
        if (previousPhase == stellar7::GameplayPhase::continuePrompt
            && model.phase() == stellar7::GameplayPhase::level2Handoff) {
            synth.stopMusic();
            synth.switchLevel2();
            level2AudioEventCursor = 0;
            failureAudioEventCursor = 0;
        }
        if (previousPhase == stellar7::GameplayPhase::continuePrompt
            && model.phase() == stellar7::GameplayPhase::level1RetryHandoff) {
            synth.stopMusic();
            synth.startMusic();
            audioEventCursor = 0;
            failureAudioEventCursor = 0;
        }
        if (previousPhase == stellar7::GameplayPhase::failurePreExplosionBlackout
            && model.phase() == stellar7::GameplayPhase::failureExplosion) {
            failureAudioEventCursor = 0;
        }
        const auto& audioEvents = model.level1State().audioEvents();
        while (audioEventCursor < audioEvents.size()) {
            synth.triggerLevel1Event(
                audioEvents[audioEventCursor].eventId
            );
            ++audioEventCursor;
        }
        const auto& level2AudioEvents = model.level2AudioEvents();
        while (level2AudioEventCursor < level2AudioEvents.size()) {
            synth.triggerLevel1Event(
                level2AudioEvents[level2AudioEventCursor].eventId
            );
            ++level2AudioEventCursor;
        }
        const auto& failureAudioEvents = model.failureAudioEvents();
        while (failureAudioEventCursor < failureAudioEvents.size()) {
            synth.triggerLevel1Event(
                failureAudioEvents[failureAudioEventCursor].eventId
            );
            ++failureAudioEventCursor;
        }
        if (stellar7::startsLevelEntry(previousPhase, model.phase())) {
            const auto entry = clock.renderEntry();
            output.insert(output.end(), entry.begin(), entry.end());
        }
        auto frame = clock.renderFrame();
        output.insert(output.end(), frame.begin(), frame.end());
        applyLevel1InputEventsAfterCheckpoint(events, tick, nextEvent, held);
    }
    writeWave(outputPath, output);
}

void writeLevel2InputScriptSoundBlaster(
    const stellar7::GameplayAssets& assets,
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t count,
    const std::string& outputPath
) {
    if (count == 0 || events.empty()) {
        throw std::runtime_error(
            "Level 2 input script audio requires events and a positive "
            "tick count"
        );
    }
    constexpr std::size_t samplesPerFrame =
        stellar7::gameplayAudioSamplesPerFrame;

    stellar7::SoundBlasterSynth synth(
        assets.soundSequences,
        assets.soundBlasterInstrumentResource,
        assets.musicForLevel(0),
        assets.presentationMusicSequence,
        assets.musicForLevel(1), assets.musicForLevel(2), assets.logoMusicSequence
    );
    synth.startLevel2();
    stellar7::PresentationClock clock(synth);
    stellar7::GameplayModel model;
    model.startLevel2();
    std::set<std::string> held;
    std::size_t nextEvent = 0;
    std::size_t level2AudioEventCursor = 0;
    std::size_t failureAudioEventCursor = 0;
    auto output = clock.renderEntry();
    output.reserve(output.size() + static_cast<std::size_t>(count) * samplesPerFrame);
    for (std::uint32_t tick = 1; tick <= count; ++tick) {
        applyLevel1InputEventsBeforeTick(events, tick, nextEvent, held);
        model.tick({
            held.count("left") != 0,
            held.count("right") != 0,
            held.count("up") != 0,
            held.count("down") != 0,
            held.count("spc") != 0,
            false,
        });
        const auto& level2AudioEvents = model.level2AudioEvents();
        while (level2AudioEventCursor < level2AudioEvents.size()) {
            synth.triggerLevel1Event(
                level2AudioEvents[level2AudioEventCursor].eventId
            );
            ++level2AudioEventCursor;
        }
        const auto& failureAudioEvents = model.failureAudioEvents();
        while (failureAudioEventCursor < failureAudioEvents.size()) {
            synth.triggerLevel1Event(
                failureAudioEvents[failureAudioEventCursor].eventId
            );
            ++failureAudioEventCursor;
        }
        auto frame = clock.renderFrame();
        output.insert(output.end(), frame.begin(), frame.end());
        applyLevel1InputEventsAfterCheckpoint(events, tick, nextEvent, held);
    }
    writeWave(outputPath, output);
}

void writeLevel1InputScriptSoundBlasterSuccessWrites(
    const stellar7::GameplayAssets& assets,
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t count,
    const std::uint32_t successSeconds,
    const std::string& outputPath
) {
    if (count == 0 || successSeconds == 0) {
        throw std::runtime_error(
            "success audio trace requires a positive "
            "tick count, and a positive success duration"
        );
    }
    constexpr std::size_t frameRate = 30;
    static_assert(
        stellar7::SoundBlasterSynth::sampleRate % frameRate == 0
    );
    constexpr std::size_t samplesPerFrame =
        stellar7::SoundBlasterSynth::sampleRate / frameRate;

    stellar7::SoundBlasterSynth synth(
        assets.soundSequences,
        assets.soundBlasterInstrumentResource,
        assets.musicForLevel(0),
        assets.presentationMusicSequence,
        assets.musicForLevel(1), assets.musicForLevel(2), assets.logoMusicSequence
    );
    synth.startLevel1();

    stellar7::GameplayModel model;
    std::set<std::string> held;
    std::size_t nextEvent = 0;
    std::size_t audioEventCursor = 0;
    std::size_t level2AudioEventCursor = 0;
    for (std::uint32_t tick = 1; tick <= count; ++tick) {
        applyLevel1InputEventsBeforeTick(events, tick, nextEvent, held);
        model.tick({
            held.count("left") != 0,
            held.count("right") != 0,
            held.count("up") != 0,
            held.count("down") != 0,
            held.count("spc") != 0,
            false,
        });
        const auto& audioEvents = model.level1State().audioEvents();
        while (audioEventCursor < audioEvents.size()) {
            synth.triggerLevel1Event(
                audioEvents[audioEventCursor].eventId
            );
            ++audioEventCursor;
        }
        const auto& level2AudioEvents = model.level2AudioEvents();
        while (level2AudioEventCursor < level2AudioEvents.size()) {
            synth.triggerLevel1Event(
                level2AudioEvents[level2AudioEventCursor].eventId
            );
            ++level2AudioEventCursor;
        }
        synth.render(samplesPerFrame);
        applyLevel1InputEventsAfterCheckpoint(events, tick, nextEvent, held);
    }

    synth.clearRegisterWrites();
    synth.startSuccess();
    synth.render(
        stellar7::SoundBlasterSynth::sampleRate
        * static_cast<std::size_t>(successSeconds)
    );
    synth.stopSuccess();

    std::vector<std::uint8_t> pairs;
    pairs.reserve(synth.registerWrites().size() * 2);
    for (const auto write : synth.registerWrites()) {
        pairs.push_back(write.registerNumber);
        pairs.push_back(write.value);
    }
    writeBytes(outputPath, pairs);
}

void writeSoundBlasterDriverState(
    std::ostringstream& output,
    const stellar7::SoundBlasterDriverState& state
) {
    const auto writeArray = [&output](const auto& values) {
        output << '[';
        for (std::size_t index = 0; index < values.size(); ++index) {
            output << (index == 0 ? "" : ",") << static_cast<unsigned>(values[index]);
        }
        output << ']';
    };
    output << "{\"notes\":";
    writeArray(state.notes);
    output << ",\"velocities\":";
    writeArray(state.velocities);
    output << ",\"channel_voice_cursors\":";
    writeArray(state.channelVoiceCursors);
    output << ",\"direct_timers\":";
    writeArray(state.directTimers);
    output << ",\"direct_level_latch\":" << (state.directLevelLatch ? "true" : "false")
           << ",\"clock_phase\":" << state.clockPhase
           << ",\"clock_period\":" << state.clockPeriod << '}';
}

void writeMusicCallbackTrace(
    const stellar7::GameplayAssets& assets,
    const unsigned level,
    const std::uint32_t count,
    const std::string& outputPath
) {
    if (level > 3 || count == 0) {
        throw std::runtime_error("Music callback trace requires level 0..3 (0=presentation) and positive count");
    }
    stellar7::SoundBlasterSynth synth(
        assets.soundSequences, assets.soundBlasterInstrumentResource,
        level == 0 ? assets.presentationMusicSequence : assets.musicForLevel(0),
        assets.presentationMusicSequence,
        assets.musicForLevel(1), assets.musicForLevel(2), assets.logoMusicSequence
    );
    if (level == 0) {
        synth.startMusic();
    } else if (level == 1) {
        synth.startLevel1();
    } else if (level == 2) {
        synth.startLevel2();
    } else {
        synth.startLevel3();
    }
    const auto writeList = [](std::ostringstream& output, const auto& writes) {
        output << '[';
        for (std::size_t index = 0; index < writes.size(); ++index) {
            output << (index == 0 ? "" : ",") << "{\"register\":"
                   << static_cast<unsigned>(writes[index].registerNumber)
                   << ",\"value\":" << static_cast<unsigned>(writes[index].value) << '}';
        }
        output << ']';
    };
    std::ostringstream output;
    output << "{\"format_version\":1,\"sample_rate\":" << stellar7::SoundBlasterSynth::sampleRate
           << ",\"level\":" << level
           << ",\"scope\":\"Music-only cold bootstrap; no native state import or gameplay timing proof\""
           << ",\"startup_writes\":";
    writeList(output, synth.registerWrites());
    output << ",\"callbacks\":[";
    constexpr std::uint64_t pitInputClock = 1193182;
    std::uint64_t renderedSamples = 0;
    for (std::uint32_t callback = 0; callback < count; ++callback) {
        synth.clearRegisterWrites();
        const auto state = synth.driverState();
        const auto untilCallback = state.clockPhase >= state.clockPeriod ? 0
            : (state.clockPeriod - state.clockPhase + pitInputClock - 1) / pitInputClock;
        synth.render(static_cast<std::size_t>(untilCallback + 1));
        output << (callback == 0 ? "" : ",") << "{\"callback\":" << callback
               << ",\"sample\":" << renderedSamples + untilCallback << ",\"writes\":";
        writeList(output, synth.registerWrites());
        output << ",\"driver_after\":";
        const auto after = synth.driverState();
        writeSoundBlasterDriverState(output, after);
        output << ",\"sequence_after\":{\"position\":" << after.sequencePosition
               << ",\"wait\":" << after.sequenceWait
               << ",\"running_status\":" << static_cast<unsigned>(after.runningStatus) << "}}";
        renderedSamples += untilCallback + 1;
    }
    output << "]}\n";
    writeText(outputPath, output.str());
}

void writeDirectExpiryTrace(const stellar7::GameplayAssets& assets) {
    constexpr int longEffect = 9;
    constexpr int shortEffect = 14;
    constexpr unsigned alignCallbacks = 18;
    constexpr unsigned preExpiryCallbacks = 11;
    constexpr std::uint64_t pitInputClock = 1193182;
    stellar7::SoundBlasterSynth synth(assets.soundSequences, assets.soundBlasterInstrumentResource);
    const auto callback = [&synth, pitInputClock]() {
        const auto state = synth.driverState();
        const auto until = state.clockPhase >= state.clockPeriod ? 0
            : (state.clockPeriod - state.clockPhase + pitInputClock - 1) / pitInputClock;
        synth.render(static_cast<std::size_t>(until + 1));
    };
    synth.triggerLevel1Event(longEffect);
    synth.triggerLevel1Event(longEffect);
    for (unsigned index = 0; index < alignCallbacks; ++index) {
        callback();
    }
    // Original event9 lasts30 callbacks, event14 lasts12. Align their counters
    // without importing native RAM or introducing a driver-state setter.
    synth.triggerLevel1Event(shortEffect);
    for (unsigned index = 0; index < preExpiryCallbacks; ++index) {
        callback();
    }
    const auto before = synth.driverState();
    synth.clearRegisterWrites();
    callback();
    std::ostringstream output;
    output << "{\"before\":";
    writeSoundBlasterDriverState(output, before);
    output << ",\"after\":";
    writeSoundBlasterDriverState(output, synth.driverState());
    output << ",\"writes\":[";
    for (std::size_t index = 0; index < synth.registerWrites().size(); ++index) {
        const auto write = synth.registerWrites()[index];
        output << (index == 0 ? "" : ",") << '[' << static_cast<unsigned>(write.registerNumber)
               << ',' << static_cast<unsigned>(write.value) << ']';
    }
    output << "],\"scope\":\"Selected simultaneous-expiry ABI exercised through live synth events; no native state import, full incoming history, guest-work timing or waveform proof\"}\n";
    std::cout << output.str();
}

enum class SoundBlasterStateDetail { semantic, boundedWork };
enum class SoundBlasterCampaignTrace { legacy, ownedWorkState };

struct CockpitWorkInput {
    stellar7::CockpitWorkState state;
    stellar7::CockpitWorkBitmaps bitmaps;
};

enum class ClockedRenderKind { cockpit, horizon, horizonGround, worldSetup, worldTraversal, worldScene, worldSight, worldCockpit, worldRadar };
enum class ClockedRenderEnd { component, display, nextFrame };
enum class IncomingFrontDomain { priorOwnedViewport = 1, beforeRaw5 = 2 };
enum class ClockedRenderOrigin { renderEntry, priorReturnedRender };
enum class ScanoutDomain { disabled, quietLevel2 };
enum class ScanoutEnd { displayReturn, renderReturn };
constexpr std::uint16_t returnedRenderLoopTick = 4;
constexpr std::size_t levelIntroductionOption = 5;

struct ClockedRenderInput {
    ClockedRenderKind kind = ClockedRenderKind::cockpit;
    std::optional<stellar7::HorizonWorkState> horizon;
    CockpitWorkInput cockpit;
    std::optional<stellar7::CockpitSettings> settings;
    stellar7::SoundBlasterClockPhase phase;
    std::int64_t horizonCycles;
    ClockedRenderEnd end = ClockedRenderEnd::component;
    unsigned frameCount = 2;
    stellar7::FrameCopyProfile copyProfile = stellar7::FrameCopyProfile::unknown;
    std::optional<std::array<std::uint8_t, stellar7::vgaPaletteBytes>> paletteSource;
    std::optional<IncomingFrontDomain> incomingFront;
    ClockedRenderOrigin origin = ClockedRenderOrigin::renderEntry;
    ScanoutDomain scanout = ScanoutDomain::disabled;
    ScanoutEnd scanoutEnd = ScanoutEnd::displayReturn;
    std::array<std::uint16_t, stellar7::frameKeyboardSlotCount> initialKeyboardSlots{};
};

ClockedRenderInput readClockedRenderInput(ClockedRenderKind kind, const std::filesystem::path& dataDirectory,
    ClockedRenderEnd end = ClockedRenderEnd::component, std::uint64_t frameCount = 2);
void printBiosClockPhase(const stellar7::SoundBlasterClockPhase& phase, std::ostream& output = std::cout);
void printGroundState(std::ostream& output, const stellar7::Level1GroundState& state, stellar7::StellarRng rng);
// Diagnostic snapshot only; the runtime already executed this body.
struct ClockedRenderObservation {
    const stellar7::Level2FramePlan& plan;
    const stellar7::ClockedPresentationWork& body;
    const stellar7::SoundBlasterCallbackState& entry;
};

std::string writeClockedRenderTrace(stellar7::PresentationClock& clock,
    stellar7::SoundBlasterSynth& synth, const ClockedRenderInput& input,
    const stellar7::GameplayAssets& assets, const stellar7::GameplayModel& model,
    stellar7::Level2RenderState* retainedFrame = nullptr, stellar7::CockpitPageState* pages = nullptr,
    stellar7::RadarState* radar = nullptr, stellar7::IndexedPages* pixels = nullptr,
    std::optional<stellar7::SoundBlasterClockPhase>* returnedPhase = nullptr,
    std::uint8_t* returnedRasterStyle = nullptr,
    std::optional<std::uint8_t>* returnedEnemyFlag = nullptr,
    const stellar7::PaletteWorkState* palette = nullptr, QuietScanoutCollector* scanout = nullptr,
    stellar7::RetainedFrameExecutor* executor = nullptr,
    const ClockedRenderObservation* observation = nullptr);

void printClockedContinuation(std::ostream& output, const stellar7::ClockedPresentationWork& work,
    const stellar7::SoundBlasterSynth& synth);

stellar7::RadarPages radarPageRoles(const stellar7::CockpitPageState& pages) {
    return {pages.cachePage(), pages.backPage(), pages.frontPage(), pages.drawPage(), pages.copyPage()};
}

stellar7::CockpitWorkBitmaps cockpitWorkBitmaps(const stellar7::GameplayAssets& assets) {
    constexpr std::size_t panelGroup = 3;
    constexpr std::size_t stripIndex = 1;
    constexpr std::size_t backgroundIndex = 2;
    constexpr std::size_t overlayGroup = 4;
    constexpr std::size_t indicatorIndex = 6;
    if (assets.artGroups.size() <= overlayGroup
        || assets.artGroups[panelGroup].size() <= stripIndex
        || assets.artGroups[overlayGroup].size() <= indicatorIndex) {
        throw std::invalid_argument("owned cockpit requires original bitmaps");
    }
    stellar7::CockpitWorkBitmaps bitmaps{};
    bitmaps.strip = assets.artGroups[panelGroup][stripIndex];
    bitmaps.indicator = assets.artGroups[overlayGroup][indicatorIndex];
    if (assets.artGroups[panelGroup].size() > backgroundIndex) {
        bitmaps.matchingPageBackground = assets.artGroups[panelGroup][backgroundIndex];
    }
    for (std::size_t index = 0; index < bitmaps.overlays.size(); ++index) {
        bitmaps.overlays[index] = assets.artGroups[overlayGroup][index];
    }
    return bitmaps;
}

std::vector<stellar7::RadarStateActor> radarActors(const std::vector<stellar7::Level1Entity>& entities,
    const std::vector<std::size_t>& order) {
    if (entities.size() != 7 || order.size() != 7) {
        throw std::invalid_argument("owned radar requires the retained seven-allocation Level2 path");
    }
    std::vector<stellar7::RadarStateActor> actors;
    for (const auto index : order) {
        const auto& actor = entities.at(index);
        actors.push_back({actor.allocationIndex, {actor.flags, actor.position}, actor.collisionRadius,
            actor.type, static_cast<std::int8_t>(actor.stateBytes4dTo52[0]), 0,
            actor.renderProfile});
    }
    return actors;
}

void applyRadarPages(const stellar7::RadarWork& work, stellar7::CockpitPageState& pages,
    stellar7::IndexedPages& pixels) {
    pixels.apply(work);
    if (work.pageCopy) {
        pages.selectArmorCopy();
    }
}

void printIndexedPage(std::ostream& output, const stellar7::IndexedPages& pixels,
    const std::uint16_t page) {
    constexpr std::int16_t width = 320;
    constexpr std::int16_t height = 200;
    std::size_t known = 0;
    output << "{\"page\":" << page << ",\"width\":" << width << ",\"height\":" << height
           << ",\"pixels\":[";
    for (std::int16_t y = 0; y < height; ++y) {
        for (std::int16_t x = 0; x < width; ++x) {
            if (x != 0 || y != 0) {
                output << ',';
            }
            if (pixels.known(page, {x, y})) {
                output << static_cast<unsigned>(pixels.read(page, {x, y}));
                ++known;
            } else {
                output << "null";
            }
        }
    }
    output << "],\"known_pixels\":" << known << ",\"complete_page_qualified\":false}";
}

void printRetainedPalette(std::ostream& output, const stellar7::PaletteWorkState& palette) {
    const auto bank = [&output](const auto& bytes) {
        output << '[';
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            output << (index == 0 ? "" : ",") << static_cast<unsigned>(bytes[index]);
        }
        output << ']';
    };
    output << "{\"origin\":\"source-level2-entry-effects\",\"cold_phase_qualified\":false,\"source\":";
    bank(palette.source);
    output << ",\"output\":";
    bank(palette.output);
    output << ",\"dac\":{\"colors\":";
    bank(palette.dac.colors);
    output << ",\"mode\":" << static_cast<unsigned>(palette.dac.mode)
           << ",\"write_index\":" << palette.dac.writeIndex
           << ",\"read_index\":" << palette.dac.readIndex
           << ",\"pel_index\":" << static_cast<unsigned>(palette.dac.pelIndex)
           << ",\"latch\":";
    if (palette.dac.latch) {
        bank(*palette.dac.latch);
    } else {
        output << "null";
    }
    output << "}}";
}

void printRadarWork(std::ostream& output, const stellar7::RadarState& entry,
    const stellar7::RadarPages& entryPages, const std::vector<stellar7::RadarStateActor>& actors,
    const stellar7::RadarWork& work) {
    const auto position = [&output](const stellar7::FixedPosition value) {
        output << '[' << value.x << ',' << value.y << ',' << value.z << ']';
    };
    const auto pages = [&output](const stellar7::RadarPages& value) {
        output << '[' << value.cached << ',' << value.back << ',' << value.front
               << ',' << value.draw << ',' << value.copy << ']';
    };
    const auto projection = [&output](const stellar7::RadarActorOutput& value) {
        output << '[' << value.projectedX << ',' << value.projectedY << ',' << value.projectedRadius << ']';
    };
    const auto viewport = [&output](const stellar7::Viewport value) {
        output << '[' << value.left << ',' << value.right << ',' << value.top << ',' << value.bottom << ']';
    };
    output << "{\"entry_strip\":" << entry.armorStrip() << ",\"return_strip\":" << *work.state.armorStrip
           << ",\"armor\":" << work.state.armor << ",\"entry_pages\":";
    pages(entryPages);
    output << ",\"return_pages\":";
    pages(work.state.pages);
    const auto& view = work.setup.view;
    output << ",\"camera\":[" << view.origin.x << ',' << view.origin.y << ',' << view.origin.z
           << ",0,0,0],\"view\":{\"origin\":";
    position(view.origin);
    output << ",\"matrix\":[";
    for (std::size_t index = 0; index < view.matrix.values.size(); ++index) {
        output << (index == 0 ? "" : ",") << view.matrix.values[index];
    }
    output << "],\"mode\":" << view.matrix.mode << ",\"viewport\":";
    viewport(view.viewport);
    output << ",\"perspective_scale\":" << view.perspectiveScale << ",\"light\":";
    position(work.setup.transformedLight);
    output << "},\"actors\":[";
    for (std::size_t index = 0; index < actors.size(); ++index) {
        const auto& actor = actors[index];
        const auto& previous = entry.projection(actor.allocation);
        const auto& returned = work.actors[index];
        output << (index == 0 ? "{" : ",{") << "\"allocation\":" << actor.allocation
               << ",\"flags\":" << static_cast<unsigned>(returned.flags)
               << ",\"saved_flags\":" << static_cast<unsigned>(actor.actor.flags)
               << ",\"type\":" << actor.type << ",\"category\":" << actor.category
               << ",\"radius\":" << actor.radius << ",\"position\":";
        position(actor.actor.position);
        output << ",\"entry_cached_center\":";
        position(previous.cachedCenter);
        output << ",\"entry_projection\":";
        projection(previous);
        output << ",\"cached_center\":";
        position(returned.cachedCenter);
        output << ",\"center\":";
        position(returned.center);
        output << ",\"projection\":";
        projection(returned);
        output << ",\"projected\":" << (returned.projected ? "true" : "false") << '}';
    }
    output << "],\"viewport\":";
    viewport(work.viewport);
    output << ",\"enemy_flag\":" << static_cast<unsigned>(work.enemyFlag)
           << ",\"raster_style\":" << static_cast<unsigned>(work.rasterStyle)
           << ",\"draw_color\":" << static_cast<unsigned>(work.drawColor)
           << ",\"fill_color\":" << static_cast<unsigned>(work.fillColor)
           << ",\"radar_pass\":" << static_cast<unsigned>(work.radarPass) << ",\"pixels\":[";
    for (std::size_t index = 0; index < work.pixels.size(); ++index) {
        const auto& pixel = work.pixels[index];
        output << (index == 0 ? "[" : ",[") << pixel.page << ',' << pixel.point.x << ','
               << pixel.point.y << ',' << static_cast<unsigned>(pixel.color) << ']';
    }
    output << "],\"page_copy\":";
    if (work.pageCopy) {
        const auto& copy = *work.pageCopy;
        output << "{\"source\":" << copy.sourcePage << ",\"destination\":" << copy.destinationPage
               << ",\"x\":" << copy.position.x << ",\"y\":" << copy.position.y
               << ",\"width\":" << copy.coveredWidth << ",\"height\":" << copy.height << '}';
    } else {
        output << "null";
    }
    const auto& panel = entry.cache().frames.front();
    output << ",\"panel\":{\"segment\":" << panel.header.segment << ",\"offset\":" << panel.header.offset
           << ",\"width\":" << panel.header.width << ",\"height\":" << panel.header.height << ",\"pixels\":[";
    for (std::size_t index = 0; index < panel.bitmap.pixels.size(); ++index) {
        output << (index == 0 ? "" : ",") << static_cast<unsigned>(panel.bitmap.pixels[index]);
    }
    output << "]},\"physical_page_phase_qualified\":false,\"startup_clock_qualified\":false,"
              "\"whole_page_history_qualified\":false}";
}

class ClockedLevel2Renderer final : public stellar7::Level2FrameRenderer {
public:
    explicit ClockedLevel2Renderer(std::function<void(stellar7::Level2RenderState)> render,
        std::function<std::optional<stellar7::StationaryLevel2PrefixWork>(
            const stellar7::StationaryLevel2PrefixInput&)> prepare = {},
        std::function<std::optional<stellar7::FrameTailState>(
            const stellar7::FrameTailState&, const stellar7::FrameTailGuards&)> finish = {})
        : render_(std::move(render)), prepare_(std::move(prepare)), finish_(std::move(finish)) {}

    void render(stellar7::Level2RenderState state) override {
        render_(state);
    }

    stellar7::Level2PrefixExecution prefixExecution() const noexcept override {
        return prepare_ ? stellar7::Level2PrefixExecution::source : stellar7::Level2PrefixExecution::semantic;
    }

    std::optional<stellar7::StationaryLevel2PrefixWork> prepare(
        const stellar7::StationaryLevel2PrefixInput& input) override {
        return prepare_ ? prepare_(input) : std::nullopt;
    }

    std::optional<stellar7::FrameTailState> finish(const stellar7::FrameTailState& state,
        const stellar7::FrameTailGuards& guards) override {
        return finish_ ? finish_(state, guards) : std::nullopt;
    }

private:
    std::function<void(stellar7::Level2RenderState)> render_;
    std::function<std::optional<stellar7::StationaryLevel2PrefixWork>(
        const stellar7::StationaryLevel2PrefixInput&)> prepare_;
    std::function<std::optional<stellar7::FrameTailState>(
        const stellar7::FrameTailState&, const stellar7::FrameTailGuards&)> finish_;
};

class RetainedTraceObserver final : public stellar7::RetainedLevel2Observer {
public:
    std::function<void(const stellar7::Level2FramePlan&, const stellar7::Level2RenderState&)> onRenderEntry;
    std::function<void(const stellar7::Level2FramePlan&, const stellar7::ClockedPresentationWork&)> onBody;
    std::function<void(const stellar7::CockpitPageState&, const stellar7::IndexedPages&,
        const stellar7::ClockedPageFlipWork&)> onDisplay;
    std::function<void(const stellar7::CockpitPageState&, const stellar7::IndexedPages&,
        const stellar7::ClockedPresentationWork&)> onPostcopy;
    std::function<void(const stellar7::Level2FramePlan&)> onRenderComplete;
    std::function<void(const stellar7::StationaryLevel2PrefixInput&)> onPrefixEntry;
    std::function<void(const stellar7::RetainedPrefixExecution&)> onPrefix;
    std::function<void(const stellar7::FrameTailState&, const stellar7::FrameTailGuards&)> onTailEntry;
    std::function<void(const stellar7::RetainedTailExecution&)> onTail;

    void renderEntry(const stellar7::Level2FramePlan& plan, const stellar7::Level2RenderState& state) override {
        if (onRenderEntry) { onRenderEntry(plan, state); }
    }
    void body(const stellar7::Level2FramePlan& plan, const stellar7::ClockedPresentationWork& work) override {
        if (onBody) { onBody(plan, work); }
    }
    void display(const stellar7::CockpitPageState& pages, const stellar7::IndexedPages& pixels,
        const stellar7::ClockedPageFlipWork& work) override {
        if (onDisplay) { onDisplay(pages, pixels, work); }
    }
    void postcopy(const stellar7::CockpitPageState& pages, const stellar7::IndexedPages& pixels,
        const stellar7::ClockedPresentationWork& work) override {
        if (onPostcopy) { onPostcopy(pages, pixels, work); }
    }
    void renderComplete(const stellar7::Level2FramePlan& plan) override {
        if (onRenderComplete) { onRenderComplete(plan); }
    }
    void prefixEntry(const stellar7::StationaryLevel2PrefixInput& input) override {
        if (onPrefixEntry) { onPrefixEntry(input); }
    }
    void prefix(const stellar7::RetainedPrefixExecution& work) override {
        if (onPrefix) { onPrefix(work); }
    }
    void tailEntry(const stellar7::FrameTailState& state, const stellar7::FrameTailGuards& guards) override {
        if (onTailEntry) { onTailEntry(state, guards); }
    }
    void tail(const stellar7::RetainedTailExecution& work) override {
        if (onTail) { onTail(work); }
    }
};

void renderPriorLevel2Page(stellar7::Level2RenderState state, const stellar7::GameplayModel& model,
    const stellar7::GameplayAssets& assets, const ClockedRenderInput& input,
    stellar7::CockpitPageState& pages, stellar7::IndexedPages& pixels) {
    const std::uint16_t priorLoopTick = input.incomingFront == IncomingFrontDomain::beforeRaw5
        ? returnedRenderLoopTick : 5;
    constexpr std::size_t groundGroup = 4;
    constexpr std::size_t firstGroundBitmap = 15;
    constexpr std::uint8_t sightColor = 95;
    constexpr stellar7::HorizonColors level2Colors{64, 78};
    if (model.phase() != stellar7::GameplayPhase::level2 || model.level2LoopTick() != priorLoopTick
        || !input.horizon || !input.settings || state.player.yaw != input.horizon->cameraYaw
        || state.player.pitch != 0 || state.player.roll != 0
        || !model.level2EntryPaletteActive() || model.damageFlashActive()
        || state.entities.size() != 7 || state.order.size() != 7) {
        throw std::invalid_argument("unsupported owned incoming-front producer");
    }
    std::array<stellar7::IndexedBitmap, 8> groundBitmaps;
    for (std::size_t index = 0; index < groundBitmaps.size(); ++index) {
        groundBitmaps[index] = assets.artGroups.at(groundGroup).at(firstGroundBitmap + index);
    }
    const auto ground = stellar7::groundClippedExecution({state.ground.points(), state.player.yawVelocity,
        state.player.forwardVelocity, state.player.position.z}, state.rng, groundBitmaps, {24, 295, 24, 145});
    const auto setup = stellar7::worldViewSetupOperations({state.player.position, {0, 0, 0},
        {state.player.pitch, state.player.roll, state.player.yaw}, 0x88, 7, {24, 294, 24, 145}, {256, -256, 362}});
    std::vector<stellar7::WorldSceneActor> actors;
    stellar7::SightRefreshInput refresh{{}, state.player.yaw};
    for (const auto index : state.order) {
        const auto& actor = state.entities.at(index);
        if (actor.renderProfile != stellar7::WorldActorRenderProfile::noHandles) {
            throw std::invalid_argument("incoming-front producer requires owned no-handle actors");
        }
        actors.push_back({{actor.flags, actor.position}, {actor.pitch, actor.roll, actor.yaw},
            actor.type, actor.collisionRadius, 0, 0});
        refresh.targets.push_back({actor.type, actor.targetHeading});
    }
    const auto world = stellar7::worldSceneOperations(assets, setup.view, actors,
        {assets.sharedModelRegistry[0], assets.sharedModelRegistry[1], 0, 0});
    const auto sight = stellar7::sightOperations(model.level2LoopTick(), state.alternateSightPhase,
        setup.view.viewport, sightColor, refresh);
    auto returnedPages = pages;
    returnedPages.beginFrame();
    returnedPages.selectCockpitCopy();
    const auto relation = returnedPages.cachedPageDiffersFromDraw()
        ? stellar7::CockpitPageRelation::differing : stellar7::CockpitPageRelation::matching;
    const auto bitmaps = cockpitWorkBitmaps(assets);
    const auto cockpit = stellar7::cockpitOperations({state.cockpitIndicatorX,
        state.cockpitIndicatorVelocity, input.settings->viewSize, relation}, bitmaps);
    const auto drawPage = returnedPages.drawPage();
    auto effects = stellar7::horizonPageEffects(*input.horizon,
        assets.horizonFramesForLevel(1), level2Colors, drawPage);
    const auto appendPixels = [&](const auto& writes) {
        for (const auto& pixel : writes) {
            effects.emplace_back(stellar7::RadarPixelWrite{pixel.point, pixel.color, drawPage});
        }
    };
    appendPixels(ground.pixels);
    appendPixels(world.pixels);
    appendPixels(sight.pixels);
    const auto cockpitEffects = stellar7::cockpitPageEffects(cockpit, bitmaps,
        returnedPages.cachePage(), drawPage);
    effects.insert(effects.end(), cockpitEffects.begin(), cockpitEffects.end());
    auto returnedPixels = pixels;
    returnedPixels.apply(effects);
    // Completed pixel effects only; the earlier CPU/device/audio phase is unqualified.
    state.ground = ground.ground;
    state.rng = ground.rng;
    state.alternateSightPhase = sight.alternatePhase;
    state.sightExecution = stellar7::Level2SightExecution::rendered;
    state.cockpitIndicatorX = cockpit.state.indicatorX;
    state.cockpitIndicatorVelocity = cockpit.state.indicatorVelocity;
    state.cockpitExecution = stellar7::Level2CockpitExecution::rendered;
    pages = returnedPages;
    pixels = std::move(returnedPixels);
}

void printLoopEntity(std::ostream& output, const stellar7::Level1Entity& entity,
    const std::optional<std::size_t> allocation) {
    output << "{\"allocation\":";
    if (allocation) {
        output << *allocation;
    } else {
        output << "\"player\"";
    }
    output << ",\"flags\":" << static_cast<unsigned>(entity.flags)
           << ",\"type\":" << entity.type << ",\"radius\":" << entity.collisionRadius
           << ",\"position\":[" << entity.position.x << ',' << entity.position.y << ',' << entity.position.z
           << "],\"angles\":[" << entity.pitch << ',' << entity.roll << ',' << entity.yaw
           << "],\"velocities\":[" << entity.pitchVelocity << ',' << entity.rollVelocity << ','
           << entity.yawVelocity << ',' << entity.xVelocity << ',' << entity.yVelocity << ','
           << entity.zVelocity << ',' << entity.forwardVelocity << "],\"state_bytes\":[";
    for (std::size_t index = 0; index < entity.stateBytes4dTo52.size(); ++index) {
        output << (index == 0 ? "" : ",") << static_cast<unsigned>(entity.stateBytes4dTo52[index]);
    }
    output << "],\"distance\":" << entity.presenterDistance << ",\"heading\":" << entity.targetHeading
           << ",\"weapon_slot_count\":" << static_cast<unsigned>(entity.weaponSlotCount)
           << ",\"cooldown\":" << static_cast<unsigned>(entity.weaponCooldown) << ",\"weapon_slots\":[";
    for (std::size_t index = 0; index < entity.weaponSlotTimers.size(); ++index) {
        output << (index == 0 ? "" : ",") << static_cast<unsigned>(entity.weaponSlotTimers[index]);
    }
    output << "],\"contact\":";
    if (entity.collisionContactIsPlayer) {
        output << "\"player\"";
    } else if (entity.collisionContactAllocationIndex != std::numeric_limits<std::size_t>::max()) {
        output << entity.collisionContactAllocationIndex;
    } else {
        output << "null";
    }
    output << '}';
}

std::string loopSnapshot(const stellar7::GameplayModel& model,
    const stellar7::CockpitPageState& pages, const stellar7::RadarState& radar,
    const std::uint8_t rasterStyle) {
    std::ostringstream output;
    output << "{\"loop_tick\":" << model.level2LoopTick() << ",\"player\":";
    printLoopEntity(output, model.level2Player(), std::nullopt);
    output << ",\"active_order\":[";
    for (std::size_t index = 0; index < model.level2ActiveListOrder().size(); ++index) {
        output << (index == 0 ? "" : ",")
               << model.level2Entities().at(model.level2ActiveListOrder()[index]).allocationIndex;
    }
    output << "],\"actors\":[";
    for (std::size_t index = 0; index < model.level2Entities().size(); ++index) {
        output << (index == 0 ? "" : ",");
        const auto& actor = model.level2Entities()[index];
        printLoopEntity(output, actor, actor.allocationIndex);
    }
    output << "],\"pages\":[" << pages.cachePage() << ',' << pages.backPage() << ',' << pages.frontPage()
           << ',' << pages.drawPage() << ',' << pages.copyPage() << "],\"raster_style\":"
           << static_cast<unsigned>(rasterStyle) << ",\"ground\":";
    printGroundState(output, model.level2Ground(), model.level2Rng());
    output << ",\"alternate_sight_phase\":" << (model.alternateSightPhase() ? "true" : "false")
           << ",\"cockpit\":{\"indicator_x\":" << model.level2CockpitIndicatorX()
           << ",\"indicator_velocity\":" << model.level2CockpitIndicatorVelocity()
           << "},\"radar\":{\"strip\":" << radar.armorStrip() << ",\"actors\":[";
    for (std::size_t index = 0; index < model.level2Entities().size(); ++index) {
        const auto allocation = model.level2Entities()[index].allocationIndex;
        const auto& projection = radar.projection(allocation);
        output << (index == 0 ? "[" : ",[") << allocation << ',' << projection.cachedCenter.x << ','
               << projection.cachedCenter.y << ',' << projection.cachedCenter.z << ','
               << projection.projectedX << ',' << projection.projectedY << ',' << projection.projectedRadius
               << ',' << static_cast<unsigned>(projection.flags) << ']';
    }
    output << "]}}";
    return output.str();
}

void printSoundBlasterCallbackState(const stellar7::SoundBlasterCallbackState& state,
    std::ostream& output = std::cout, SoundBlasterStateDetail detail = SoundBlasterStateDetail::semantic);

enum class ControlledSessionCheck { trace, deliveryAndFailure };
constexpr unsigned controlledSessionFullFrames = 64;
constexpr unsigned controlledSessionTraceFrames = 80;

class ControlledSessionTrace final : public stellar7::ControlledSessionObserver {
public:
    std::string startupState, entryState, arrivalState, readyState;
    std::vector<std::string> ticks, frames, deliveryTimeline;
    std::vector<std::int16_t> samples;
    stellar7::ControlledLevel2Session* activeSession = nullptr;
    bool reentryChecked = false;

    void startup(const stellar7::ControlledSessionView& view) override {
        startupState = driver(view);
    }
    void initialEntry(const stellar7::ControlledSessionView& view, const std::size_t count) override {
        std::ostringstream out;
        out << "{\"sample_count\":" << count << ",\"state\":" << driver(view) << '}';
        entryState = out.str();
    }
    void bootstrapTick(const stellar7::ControlledSessionView& view,
        const stellar7::ControlledBootstrapTick& tick) override {
        std::ostringstream out;
        out << "{\"tick\":" << tick.tick << ",\"entry_fade_samples\":" << tick.entryFadeSamples
            << ",\"elapsed_samples\":" << view.elapsedSamples << ",\"driver_after\":";
        writeSoundBlasterDriverState(out, view.synth.driverState());
        out << ",\"writes\":";
        writes(out, view.synth.registerWrites());
        out << ",\"audio_events\":[";
        for (std::size_t i = 0; i < tick.audioEvents.size(); ++i) {
            const auto& event = tick.audioEvents[i];
            out << (i == 0 ? "" : ",") << "{\"loop_tick\":" << event.loopTick
                << ",\"event_id\":" << event.eventId << ",\"dispatch_tick\":" << tick.tick << '}';
        }
        out << "],\"success_entry_driver\":";
        if (tick.successEntry) {
            writeSoundBlasterDriverState(out, *tick.successEntry);
        } else {
            out << "null";
        }
        out << '}';
        ticks.push_back(out.str());
    }
    void arrival(const stellar7::ControlledSessionView& view) override {
        arrivalState = snapshot(view);
    }
    void ready(const stellar7::ControlledSessionView& view) override {
        readyState = snapshot(view);
        arrivalState = "{\"entry_state\":" + arrivalState + ",\"tail_state\":" + readyState
            + ",\"flip\":" + displayState + ",\"post_flip\":" + postcopyState
            + ",\"tail\":" + tailState + '}';
        clearFrame();
    }
    void prefixEntry(const stellar7::ControlledSessionView& view,
        const stellar7::StationaryLevel2PrefixInput&) override {
        if (!reentryChecked && activeSession != nullptr) {
            const auto before = snapshot(view);
            const auto ticksBefore = view.model.ticks();
            bool advanceRejected = false, drainRejected = false;
            try { activeSession->advance({}); }
            catch (const std::logic_error&) { advanceRejected = true; }
            try { activeSession->takeDelivery(); }
            catch (const std::logic_error&) { drainRejected = true; }
            if (!advanceRejected || !drainRejected || snapshot(view) != before || view.model.ticks() != ticksBefore) {
                throw std::logic_error("controlled session callback reentry changed owners");
            }
            reentryChecked = true;
        }
        prefixInput = snapshot(view);
    }
    void prefix(const stellar7::ControlledSessionView& view,
        const stellar7::RetainedPrefixExecution& result) override {
        prefixState = work(view, result.work);
    }
    void renderEntry(const stellar7::ControlledSessionView& view,
        const stellar7::Level2FramePlan&, const stellar7::Level2RenderState&) override {
        renderInput = snapshot(view);
        std::ostringstream out;
        printSoundBlasterCallbackState(view.synth.callbackWorkState(), out, SoundBlasterStateDetail::boundedWork);
        bodyEntry = out.str();
    }
    void body(const stellar7::ControlledSessionView& view, const stellar7::Level2FramePlan&,
        const stellar7::ClockedPresentationWork& result) override {
        bodyState = work(view, result);
    }
    void display(const stellar7::ControlledSessionView& view,
        const stellar7::ClockedPageFlipWork& result) override {
        std::ostringstream out;
        out << "{\"low_reads\":" << result.lowReads << ",\"high_reads\":" << result.highReads
            << ",\"flip\":" << work(view, result.work) << '}';
        displayState = out.str();
    }
    void postcopy(const stellar7::ControlledSessionView& view,
        const stellar7::ClockedPresentationWork& result) override {
        postcopyState = work(view, result);
        std::ostringstream out;
        out << "{\"indexed_frame\":";
        printIndexedPage(out, view.pixels, view.pages.drawPage());
        out << ",\"indexed_previous_front\":";
        printIndexedPage(out, view.pixels, view.pages.frontPage());
        out << ",\"palette\":";
        printRetainedPalette(out, view.palette);
        out << '}';
        completedPixels = out.str();
    }
    void tail(const stellar7::ControlledSessionView& view,
        const stellar7::RetainedTailExecution& result) override {
        tailState = work(view, result.work);
    }
    void advanced(const stellar7::ControlledSessionView& view) override {
        std::ostringstream out;
        out << "{\"entry_state\":" << renderInput << ",\"prefix_input_state\":" << prefixInput
            << ",\"prefix\":" << prefixState << ",\"owned_entry\":" << bodyEntry
            << ",\"body\":" << bodyState << ",\"display\":" << displayState
            << ",\"postcopy\":" << postcopyState << ",\"pixels\":" << completedPixels
            << ",\"tail\":" << tailState << ",\"tail_state\":" << snapshot(view) << '}';
        frames.push_back(out.str());
        clearFrame();
    }

private:
    std::string prefixInput, prefixState, renderInput, bodyEntry, bodyState;
    std::string displayState, postcopyState, completedPixels, tailState;
    static std::string snapshot(const stellar7::ControlledSessionView& view) {
        auto result = loopSnapshot(view.model, view.pages, view.radar, view.rasterStyle);
        if (!view.keyboardState) { return result; }
        const auto& keyboard = *view.keyboardState;
        std::ostringstream out;
        out << result.substr(0, result.size() - 1) << ",\"keyboard_owner\":{\"metadata_48\":"
            << static_cast<unsigned>(keyboard.metadata48) << ",\"metadata_alt_38\":"
            << static_cast<unsigned>(keyboard.metadataAlt38) << ",\"axis_roles\":["
            << static_cast<unsigned>(keyboard.axisRoles[0]) << ',' << static_cast<unsigned>(keyboard.axisRoles[1])
            << "],\"last_key\":" << keyboard.lastKey << ",\"consumed_scan\":" << keyboard.consumedScan
            << ",\"head\":" << keyboard.head << ",\"tail\":" << keyboard.tail
            << ",\"ring_begin\":" << keyboard.ringBegin << ",\"ring_end\":" << keyboard.ringEnd
            << ",\"slots\":[";
        for (std::size_t i = 0; i < keyboard.slots.size(); ++i) { out << (i == 0 ? "" : ",") << keyboard.slots[i]; }
        out << "]}}";
        return out.str();
    }
    static void writes(std::ostream& out, const std::vector<stellar7::OplRegisterWrite>& values) {
        out << '[';
        for (std::size_t i = 0; i < values.size(); ++i) {
            out << (i == 0 ? "" : ",") << "{\"register\":" << static_cast<unsigned>(values[i].registerNumber)
                << ",\"value\":" << static_cast<unsigned>(values[i].value) << '}';
        }
        out << ']';
    }
    static std::string driver(const stellar7::ControlledSessionView& view) {
        std::ostringstream out;
        out << "{\"driver\":";
        writeSoundBlasterDriverState(out, view.synth.driverState());
        out << ",\"writes\":";
        writes(out, view.synth.registerWrites());
        out << '}';
        return out.str();
    }
    std::string work(const stellar7::ControlledSessionView& view,
        const stellar7::ClockedPresentationWork& result) {
        samples.insert(samples.end(), result.audio.samples.begin(), result.audio.samples.end());
        std::ostringstream timeline;
        timeline << std::setprecision(17) << "{\"first_sample\":" << result.audio.firstSample
            << ",\"samples\":" << result.audio.samples.size() << ",\"elapsed_cycles\":" << result.timing.total.elapsedCycles
            << ",\"vga\":[";
        for (std::size_t i = 0; i < result.timing.vgaServices.size(); ++i) {
            const auto& receipt = result.timing.vgaServices[i];
            timeline << (i == 0 ? "" : ",") << "{\"kind\":" << static_cast<unsigned>(receipt.kind)
                << ",\"cycle_offset\":" << receipt.cycleOffset << ",\"sample_position\":" << result.audio.vgaSamplePositions.at(i)
                << ",\"line\":" << receipt.line << ",\"origin_ms\":" << receipt.frameStartMilliseconds
                << ",\"completion_ms\":" << receipt.servicedMilliseconds
                << ",\"scheduled_completion_ms\":" << receipt.scheduledMilliseconds << '}';
        }
        timeline << "]}";
        deliveryTimeline.push_back(timeline.str());
        std::ostringstream out;
        out << std::setprecision(17);
        printClockedContinuation(out, result, view.synth);
        return out.str();
    }
    void clearFrame() {
        prefixInput.clear(); prefixState.clear(); renderInput.clear(); bodyEntry.clear(); bodyState.clear();
        displayState.clear(); postcopyState.clear(); completedPixels.clear(); tailState.clear();
    }
};

stellar7::ControlledBootstrapRequest controlledBootstrapRequest(
    const std::vector<ParsedLevel1InputEvent>& events, const ClockedRenderInput& input, const unsigned frames) {
    if (events.empty() || frames == 0 || frames > controlledSessionTraceFrames || !input.horizon || !input.settings
        || input.origin != ClockedRenderOrigin::priorReturnedRender || input.incomingFront != IncomingFrontDomain::beforeRaw5) {
        throw std::invalid_argument("controlled session requires the admitted raw4 route and 1..80 complete frames");
    }
    stellar7::ControlledBootstrapRequest request;
    request.delivery = events.front().postCheckpoint ? stellar7::ControlledInputDelivery::afterCheckpoint
        : stellar7::ControlledInputDelivery::beforeTick;
    for (const auto& event : events) {
        if (event.postCheckpoint != events.front().postCheckpoint) {
            throw std::invalid_argument("controlled session requires one input delivery phase");
        }
        stellar7::ControlledInputEvent converted{event.tick, event.pressed, {}};
        for (const auto& key : event.keys) {
            if (key == "left") { converted.keys.push_back(stellar7::ControlledInputKey::left); }
            else if (key == "right") { converted.keys.push_back(stellar7::ControlledInputKey::right); }
            else if (key == "up") { converted.keys.push_back(stellar7::ControlledInputKey::forward); }
            else if (key == "down") { converted.keys.push_back(stellar7::ControlledInputKey::reverse); }
            else if (key == "spc") { converted.keys.push_back(stellar7::ControlledInputKey::fire); }
            else { throw std::invalid_argument("unsupported controlled input key"); }
        }
        request.events.push_back(std::move(converted));
    }
    request.returnedRaw4Phase = input.phase;
    request.initialKeyboardSlots = input.initialKeyboardSlots;
    request.horizon = *input.horizon;
    request.settings = *input.settings;
    request.copyProfile = input.copyProfile;
    request.horizonCycles = input.horizonCycles;
    return request;
}

struct ReturnKeyEvent {
    std::uint16_t tick;
    stellar7::KeyboardEdge edge;
};

std::vector<ReturnKeyEvent> loadReturnKeys(const std::string& path) {
    std::ifstream input(path);
    if (!input) { throw std::runtime_error("cannot read render-return key script"); }
    std::vector<ReturnKeyEvent> events;
    unsigned previous = 3;
    auto expected = stellar7::KeyboardEdge::press;
    unsigned tick = 0;
    std::string action;
    while (input >> tick >> action) {
        if (tick <= previous || tick > std::numeric_limits<std::uint16_t>::max()
            || (action != "press" && action != "release")) {
            throw std::invalid_argument("render-return keys require increasing ticks and press/release");
        }
        const auto edge = action == "press" ? stellar7::KeyboardEdge::press : stellar7::KeyboardEdge::release;
        if (edge != expected) { throw std::invalid_argument("render-return keys require alternating Up edges"); }
        events.push_back({static_cast<std::uint16_t>(tick), edge});
        previous = tick;
        expected = edge == stellar7::KeyboardEdge::press ? stellar7::KeyboardEdge::release : stellar7::KeyboardEdge::press;
    }
    if (!input.eof()) { throw std::invalid_argument("invalid render-return key script"); }
    return events;
}

class ReturnKeyboardInput final : public stellar7::ControlledKeyboardSource {
public:
    explicit ReturnKeyboardInput(std::vector<ReturnKeyEvent> events) : events_(std::move(events)) {}
    std::optional<stellar7::KeyboardEdge> forwardAtRenderReturn(const stellar7::ControlledKeyboardBoundary& boundary) override {
        std::optional<stellar7::KeyboardEdge> edge;
        if (next_ < events_.size()) {
            if (events_[next_].tick < boundary.loopTick) { throw std::logic_error("missed render-return input"); }
            if (events_[next_].tick == boundary.loopTick) { edge = events_[next_++].edge; }
        }
        returns.push_back({boundary.loopTick, static_cast<int>(boundary.forward), edge ? static_cast<int>(*edge) : -1});
        return edge;
    }
    bool complete() const { return next_ == events_.size(); }
    std::vector<std::array<int,3>> returns;
private:
    std::vector<ReturnKeyEvent> events_;
    std::size_t next_ = 0;
};

void writeControlledSessionTrace(stellar7::GameplayAssets assets,
    const std::vector<ParsedLevel1InputEvent>& events, const ClockedRenderInput& input,
    const unsigned frames, const std::string& path, const ControlledSessionCheck check,
    const stellar7::ControlledArrivalInput arrivalInput = stellar7::ControlledArrivalInput::released,
    const std::vector<ReturnKeyEvent>& hostEvents = {}) {
    auto request = controlledBootstrapRequest(events, input, frames);
    request.arrivalInput = arrivalInput;
    std::optional<stellar7::GameplayAssets> controlAssets;
    std::optional<stellar7::ControlledBootstrapRequest> controlRequest;
    if (check == ControlledSessionCheck::deliveryAndFailure) {
        controlAssets = assets;
        controlRequest = request;
    }
    ControlledSessionTrace observer;
    std::optional<ReturnKeyboardInput> hostInput;
    if (arrivalInput == stellar7::ControlledArrivalInput::hostForward) { hostInput.emplace(hostEvents); }
    stellar7::ControlledLevel2Session session(std::move(assets), std::move(request), &observer,
        hostInput ? &*hostInput : nullptr);
    auto delivery = session.takeDelivery();
    if (delivery.firstSample != 0) {
        throw std::logic_error("controlled session changed initial PCM origin");
    }
    struct DeliveryRange { std::uint64_t firstSample; std::size_t samples, scans; };
    std::vector<DeliveryRange> ranges{{delivery.firstSample, delivery.samples.size(), delivery.scans.size()}};
    std::vector<std::int16_t> samples = std::move(delivery.samples);
    std::vector<stellar7::ControlledScan> scans = std::move(delivery.scans);
    // Moving the outer handle must retain every borrowed owner address.
    auto retained = std::move(session);
    bool movedAdvanceRejected = false, movedDrainRejected = false;
    try { session.advance({}); }
    catch (const std::logic_error&) { movedAdvanceRejected = true; }
    try { session.takeDelivery(); }
    catch (const std::logic_error&) { movedDrainRejected = true; }
    if (!movedAdvanceRejected || !movedDrainRejected) {
        throw std::logic_error("moved session still owns a runtime");
    }
    for (const auto member : {&stellar7::GameplayInput::left, &stellar7::GameplayInput::right,
        &stellar7::GameplayInput::forward, &stellar7::GameplayInput::reverse,
        &stellar7::GameplayInput::fire, &stellar7::GameplayInput::exit, &stellar7::GameplayInput::confirm}) {
        stellar7::GameplayInput unsupported{};
        unsupported.*member = true;
        bool rejected = false;
        try { retained.advance(unsupported); }
        catch (const std::logic_error&) { rejected = true; }
        if (!rejected) { throw std::logic_error("controlled session accepted unsupported gameplay input"); }
    }
    observer.activeSession = &retained;
    for (unsigned frame = 0; frame < frames; ++frame) {
        auto next = retained.advance({});
        if (next.firstSample != samples.size()) {
            throw std::logic_error("controlled session changed PCM batch origin");
        }
        ranges.push_back({next.firstSample, next.samples.size(), next.scans.size()});
        samples.insert(samples.end(), next.samples.begin(), next.samples.end());
        for (auto& scan : next.scans) { scans.push_back(std::move(scan)); }
    }
    const auto empty = retained.takeDelivery();
    if (hostInput && !hostInput->complete()) { throw std::logic_error("render-return script extends beyond this replay"); }
    if (samples != observer.samples || empty.firstSample != samples.size() || !empty.samples.empty() || !empty.scans.empty() || !observer.reentryChecked) {
        throw std::logic_error("controlled session PCM delivery lost or duplicated work");
    }
    if (check == ControlledSessionCheck::deliveryAndFailure) {
        class CheckingPcm final : public stellar7::PcmSink {
        public:
            explicit CheckingPcm(const std::vector<std::int16_t>& expected) : expected_(expected) {}
            stellar7::TransportResult trySubmit(const std::vector<std::int16_t>& part) override {
                if (busy_++ % 3 != 2) { return stellar7::TransportResult::busy; }
                if (part.size() > expected_.size() - submitted ||
                    !std::equal(part.begin(), part.end(), expected_.begin() + submitted)) {
                    throw std::logic_error("transport changed or duplicated session PCM");
                }
                submitted += part.size();
                return stellar7::TransportResult::accepted;
            }
            std::uint64_t playedSamples() override {
                constexpr std::uint64_t testProgress = 170;
                played = std::min<std::uint64_t>(submitted, played + testProgress);
                return played;
            }
            std::size_t submitted = 0;
            std::uint64_t played = 0;
        private:
            const std::vector<std::int16_t>& expected_;
            unsigned busy_ = 0;
        } pcm(samples);
        class CheckingScans final : public stellar7::ScanSink {
        public:
            CheckingScans(const std::vector<stellar7::ControlledScan>& expected, const CheckingPcm& pcm)
                : expected_(expected), pcm_(pcm) {}
            stellar7::TransportResult tryPresent(const stellar7::QuietScanout::CompletedScan& scan) override {
                if (busy_++ % 2 == 0) { return stellar7::TransportResult::busy; }
                if (presented >= expected_.size()) { throw std::logic_error("transport duplicated session scan"); }
                const auto& expected = expected_[presented];
                if (pcm_.played < expected.samplePosition || scan.rgb != expected.scan.rgb ||
                    scan.page != expected.scan.page || scan.originMilliseconds != expected.scan.originMilliseconds ||
                    scan.completionMilliseconds != expected.scan.completionMilliseconds ||
                    scan.scheduledCompletionMilliseconds != expected.scan.scheduledCompletionMilliseconds) {
                    throw std::logic_error("transport changed session scan or clock order");
                }
                ++presented;
                return stellar7::TransportResult::accepted;
            }
            std::size_t presented = 0;
        private:
            const std::vector<stellar7::ControlledScan>& expected_;
            const CheckingPcm& pcm_;
            unsigned busy_ = 0;
        } projected(scans, pcm);
        stellar7::PresentationTransport transport(pcm, projected);
        std::size_t scanOffset = 0;
        for (const auto& range : ranges) {
            stellar7::ControlledSessionDelivery batch;
            batch.firstSample = range.firstSample;
            batch.samples.assign(samples.begin() + range.firstSample, samples.begin() + range.firstSample + range.samples);
            batch.scans.assign(scans.begin() + scanOffset, scans.begin() + scanOffset + range.scans);
            scanOffset += range.scans;
            if (transport.tryAccept(batch) != stellar7::TransportResult::accepted) {
                throw std::logic_error("transport unexpectedly retained a prior session batch");
            }
            unsigned attempts = 0;
            while (transport.pump() == stellar7::TransportResult::busy) {
                if (++attempts > samples.size()) { throw std::logic_error("synthetic transport did not drain"); }
            }
        }
        if (pcm.submitted != samples.size() || projected.presented != scans.size() || transport.hasPending()) {
            throw std::logic_error("transport lost session output");
        }
        std::optional<ReturnKeyboardInput> unobservedInput;
        if (hostInput) { unobservedInput.emplace(hostEvents); }
        stellar7::ControlledLevel2Session unobserved(*controlAssets, *controlRequest, nullptr,
            unobservedInput ? &*unobservedInput : nullptr);
        auto delivered = unobserved.takeDelivery();
        for (unsigned frame = 0; frame < frames; ++frame) {
            auto next = unobserved.advance({});
            if (next.firstSample != delivered.samples.size()) {
                throw std::logic_error("unobserved session changed PCM batch origin");
            }
            delivered.samples.insert(delivered.samples.end(), next.samples.begin(), next.samples.end());
            for (auto& scan : next.scans) { delivered.scans.push_back(std::move(scan)); }
        }
        if (delivered.samples != samples || delivered.scans.size() != scans.size()) {
            throw std::logic_error("session delivery depends on diagnostic observers");
        }
        for (std::size_t i = 0; i < scans.size(); ++i) {
            const auto& a = scans[i].scan;
            const auto& b = delivered.scans[i].scan;
            if (scans[i].samplePosition != delivered.scans[i].samplePosition) {
                throw std::logic_error("unobserved session changed scan timing");
            }
            if (a.rgb != b.rgb || a.page != b.page || a.originMilliseconds != b.originMilliseconds
                || a.completionMilliseconds != b.completionMilliseconds
                || a.scheduledCompletionMilliseconds != b.scheduledCompletionMilliseconds) {
                throw std::logic_error("unobserved session changed scan delivery");
            }
        }
        struct ObserverStop {};
        class FailureObserver final : public stellar7::ControlledSessionObserver {
        public:
            std::uint64_t readySamples = 0, stoppedSamples = 0;
            unsigned bodies = 0;
            void ready(const stellar7::ControlledSessionView& view) override { readySamples = view.elapsedSamples; }
            void body(const stellar7::ControlledSessionView& view, const stellar7::Level2FramePlan&,
                const stellar7::ClockedPresentationWork&) override {
                ++bodies;
                stoppedSamples = view.elapsedSamples;
                throw ObserverStop{};
            }
        } failure;
        std::optional<ReturnKeyboardInput> rejectedInput;
        if (hostInput) { rejectedInput.emplace(hostEvents); }
        stellar7::ControlledLevel2Session rejected(std::move(*controlAssets), std::move(*controlRequest), &failure,
            rejectedInput ? &*rejectedInput : nullptr);
        rejected.takeDelivery();
        bool stopped = false, advanceRejected = false, drainRejected = false;
        try { rejected.advance({}); }
        catch (const ObserverStop&) { stopped = true; }
        try { rejected.advance({}); }
        catch (const std::logic_error&) { advanceRejected = true; }
        try { rejected.takeDelivery(); }
        catch (const std::logic_error&) { drainRejected = true; }
        if (!stopped || !advanceRejected || !drainRejected || failure.bodies != 1
            || failure.stoppedSamples <= failure.readySamples) {
            throw std::logic_error("failed session allowed partial output or continued execution");
        }
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) { throw std::runtime_error("cannot write controlled session trace"); }
    out << std::setprecision(17) << "{\"startup\":" << observer.startupState
        << ",\"entry\":" << observer.entryState << ",\"ticks\":[";
    for (std::size_t i = 0; i < observer.ticks.size(); ++i) { out << (i == 0 ? "" : ",") << observer.ticks[i]; }
    out << "],\"arrival\":" << observer.arrivalState << ",\"frames\":[";
    for (std::size_t i = 0; i < observer.frames.size(); ++i) { out << (i == 0 ? "" : ",") << observer.frames[i]; }
    out << "],\"delivered_samples\":" << samples.size() << ",\"scans\":[";
    std::ofstream rgb(path + ".scan-rgb.bin", std::ios::binary);
    if (!rgb) { throw std::runtime_error("cannot write controlled session scans"); }
    std::size_t offset = 0;
    for (std::size_t i = 0; i < scans.size(); ++i) {
        const auto& scan = scans[i].scan;
        out << (i == 0 ? "" : ",") << "{\"origin_ms\":" << scan.originMilliseconds
            << ",\"completion_ms\":" << scan.completionMilliseconds
            << ",\"scheduled_completion_ms\":" << scan.scheduledCompletionMilliseconds
            << ",\"page\":" << scan.page << ",\"offset\":" << offset << ",\"bytes\":" << scan.rgb.size() << '}';
        rgb.write(reinterpret_cast<const char*>(scan.rgb.data()), static_cast<std::streamsize>(scan.rgb.size()));
        offset += scan.rgb.size();
    }
    out << "],\"delivery_timeline\":[";
    for (std::size_t i = 0; i < observer.deliveryTimeline.size(); ++i) {
        out << (i == 0 ? "" : ",") << observer.deliveryTimeline[i];
    }
    out << "],\"scan_sample_positions\":[";
    for (std::size_t i = 0; i < scans.size(); ++i) {
        out << (i == 0 ? "" : ",") << scans[i].samplePosition;
    }
    out << "],\"delivery_failure_controls\":" << (check == ControlledSessionCheck::deliveryAndFailure ? "true" : "false");
    if (arrivalInput == stellar7::ControlledArrivalInput::residentForward) {
        out << ",\"resident_input\":\"forward_restored_at_raw4_return\",\"original_keyboard_irq_qualified\":false";
    }
    if (hostInput) {
        out << ",\"host_input_returns\":[";
        for (std::size_t i = 0; i < hostInput->returns.size(); ++i) {
            const auto& record = hostInput->returns[i];
            out << (i == 0 ? "" : ",") << '[' << record[0] << ',' << record[1] << ',' << record[2] << ']';
        }
        out << ']';
    }
    out << "}\n";
    if (!out || !rgb) { throw std::runtime_error("controlled session trace write failed"); }
}


void writeLevel1InputScriptSoundBlasterTrace(
    const stellar7::GameplayAssets& assets,
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t count,
    const std::string& outputPath,
    const std::optional<std::int16_t> level2Armor = std::nullopt,
    const bool holdLevel2Armor = false,
    const std::optional<std::size_t> presentationCallbacks = std::nullopt,
    const SoundBlasterCampaignTrace trace = SoundBlasterCampaignTrace::legacy,
    const std::optional<ClockedRenderInput>& clockedRender = std::nullopt
) {
    if (count == 0 || events.empty()) {
        throw std::runtime_error(
            "input script audio trace requires events and a positive "
            "tick count"
        );
    }
    constexpr std::size_t samplesPerFrame =
        stellar7::gameplayAudioSamplesPerFrame;

    stellar7::SoundBlasterSynth synth(
        assets.soundSequences,
        assets.soundBlasterInstrumentResource,
        assets.musicForLevel(0),
        assets.presentationMusicSequence,
        assets.musicForLevel(1),
        assets.musicForLevel(2), assets.logoMusicSequence
    );
    if (presentationCallbacks.has_value()) {
        synth.startLevel1(*presentationCallbacks);
    } else {
        synth.startLevel1();
    }
    const auto startupDriver = synth.driverState();
    const auto startupWrites = synth.registerWrites();
    synth.clearRegisterWrites();
    stellar7::PresentationClock clock(synth);
    const auto entrySamples = clock.renderEntry().size();
    const auto entryDriver = synth.driverState();
    const auto entryWrites = synth.registerWrites();
    synth.clearRegisterWrites();

    const auto writeList = [](
        std::ostringstream& output,
        const std::vector<stellar7::OplRegisterWrite>& writes
    ) {
        output << '[';
        for (std::size_t index = 0; index < writes.size(); ++index) {
            if (index != 0) {
                output << ',';
            }
            output
                << "{\"register\":"
                << static_cast<unsigned>(writes[index].registerNumber)
                << ",\"value\":"
                << static_cast<unsigned>(writes[index].value)
                << '}';
        }
        output << ']';
    };

    std::ostringstream output;
    output
        << "{\"format_version\":1,\"sample_rate\":"
        << stellar7::SoundBlasterSynth::sampleRate
        << ",\"sequence_rate\":"
        << stellar7::SoundBlasterSynth::sequenceRate
        << ",\"startup_writes\":";
    writeList(output, startupWrites);
    output << ",\"presentation_callbacks\":";
    if (presentationCallbacks.has_value()) {
        output << *presentationCallbacks;
    } else {
        output << "null";
    }
    output << ",\"startup_history_scope\":\"Resource pre-roll only; no sample-clock or loading-interval restore\""
           << ",\"startup_driver\":";
    writeSoundBlasterDriverState(output, startupDriver);
    output << ",\"entry_presentation\":{\"game_tick\":0,\"steps\":" << stellar7::levelEntryFadeSteps
           << ",\"sample_count\":" << entrySamples
           << ",\"timing_scope\":\"Pinned normal-core base budget; cold video origin; IRQ/service overhead unresolved\""
           << ",\"driver_after\":";
    writeSoundBlasterDriverState(output, entryDriver);
    output << ",\"writes\":";
    writeList(output, entryWrites);
    output << '}';
    output << ",\"ticks\":[";

    stellar7::GameplayModel model;
    std::set<std::string> held;
    std::size_t nextEvent = 0;
    bool appliedLevel2Armor = false;
    std::size_t audioEventCursor = 0;
    std::size_t level2AudioEventCursor = 0;
    std::size_t failureAudioEventCursor = 0;
    std::optional<stellar7::RadarState> radar;
    std::vector<std::size_t> radarAllocations;
    stellar7::CockpitPageState radarPages;
    stellar7::IndexedPages radarPixels;
    std::string radarHistoryError;
    std::string priorRenderPage;
    std::optional<stellar7::SoundBlasterClockPhase> arrivalPhase;
    std::string arrivalSegment;
    std::optional<stellar7::PaletteWorkState> retainedPalette;
    std::optional<stellar7::RetainedFrameExecutor> frameExecutor;
    std::optional<QuietScanoutCollector> scanout;
    if (clockedRender && clockedRender->scanout == ScanoutDomain::quietLevel2) {
        scanout.emplace();
    }
    const auto returnedOrigin = clockedRender
        && clockedRender->origin == ClockedRenderOrigin::priorReturnedRender;
    constexpr std::uint8_t solidRasterStyle = 1;
    const auto retainEntryPalette = [&]() {
    if (clockedRender && clockedRender->paletteSource) {
        if (model.phase() != stellar7::GameplayPhase::level2
            || !model.level2EntryPaletteActive() || model.damageFlashActive()
            || assets.cockpitDacPalette.size() != stellar7::vgaPaletteBytes
            || !std::equal(clockedRender->paletteSource->begin(), clockedRender->paletteSource->end(),
                assets.cockpitDacPalette.begin())) {
            throw std::invalid_argument("unsupported retained Level2 entry palette history");
        }
        if (!retainedPalette) {
            stellar7::PaletteWorkState initial{};
            initial.source = *clockedRender->paletteSource;
            // Completed9d9e effects precede the imported clock.
            // Project effects only; cold CPU/device/upload ancestry stays unqualified.
            for (unsigned step = 0; step < stellar7::levelEntryFadeSteps; ++step) {
                initial = stellar7::completedFullPaletteUpload(initial,
                    {0, stellar7::vgaPaletteColors, 0, static_cast<std::uint8_t>(
                        step * stellar7::levelEntryFadeIntensityStep)});
            }
            retainedPalette = initial;
        }
    }
    };
    const auto continueLoop = clockedRender && clockedRender->end == ClockedRenderEnd::nextFrame;
    std::optional<stellar7::FrameController> loopController;
    std::string loopHistoryError;
    std::size_t emptyTailReads = 0;
    if (clockedRender && clockedRender->kind == ClockedRenderKind::worldRadar) {
        radar.emplace(assets.artGroups.at(3));
        // GameplayModel construction includes the first ordinary Level1 display.
        radarPages.flip();
    }
    for (std::uint32_t tick = 1; tick <= count; ++tick) {
        bool arrivalExecuted = false;
        const auto beforeInputEvent = nextEvent;
        applyLevel1InputEventsBeforeTick(events, tick, nextEvent, held);
        const auto previousPhase = model.phase();
        const auto previousLoopTick = model.level2LoopTick();
        model.setSuccessAudioActive(synth.successActive());
        const stellar7::GameplayInput gameplayInput{
            held.count("left") != 0,
            held.count("right") != 0,
            held.count("up") != 0,
            held.count("down") != 0,
            held.count("spc") != 0,
            false,
        };
        if (clockedRender && clockedRender->incomingFront && tick == count) {
            if (!radar || previousPhase != stellar7::GameplayPhase::level2 || !radarHistoryError.empty()) {
                throw std::invalid_argument("incoming-front producer requires retained Level2 page history");
            }
            const auto priorAudioEvents = model.level2AudioEvents().size();
            ClockedLevel2Renderer priorRenderer([&](stellar7::Level2RenderState state) {
                renderPriorLevel2Page(state, model, assets, *clockedRender, radarPages, radarPixels);
            }, {}, [&](const stellar7::FrameTailState& state, const stellar7::FrameTailGuards& guards)
                -> std::optional<stellar7::FrameTailState> {
                if (!returnedOrigin) {
                    return std::nullopt;
                }
                if (!continueLoop || !loopController || !loopHistoryError.empty() || !held.empty()
                    || state.loopTick != returnedRenderLoopTick || model.level2AudioEvents().size() != priorAudioEvents
                    || priorAudioEvents != level2AudioEventCursor
                    || model.level1State().audioEvents().size() != audioEventCursor
                    || model.failureAudioEvents().size() != failureAudioEventCursor
                    || previousPhase != stellar7::GameplayPhase::level2
                    || model.phase() != stellar7::GameplayPhase::level2
                    || static_cast<std::uint16_t>(loopController->expectedLoopTick() + 1) != state.loopTick) {
                    throw std::invalid_argument("unsupported returned-render clock arrival");
                }
                // Preserve the existing sample pre-roll through its verified raw4 return.
                clock.renderFrame();
                retainEntryPalette();
                radarPages.beginFrame();
                radarPages.selectCockpitCopy();
                const auto& player = model.level2Player();
                const auto radarWork = radar->draw(model.playerArmor(), player.position, player.yaw,
                    radarActors(model.level2PresentationEntities(), model.level2PresentationActiveListOrder()),
                    radarPageRoles(radarPages), {0, 0});
                applyRadarPages(radarWork, radarPages, radarPixels);
                if (radarWork.pageCopy) {
                    throw std::invalid_argument("returned-render arrival requires no front-page copy");
                }
                loopController->consumeEmptyTail(loopController->expectedLoopTick());
                ++emptyTailReads;
                std::ostringstream segment;
                segment << "{\"boundary\":\"raw4/8518\",\"cold_phase_qualified\":false,\"entry_state\":"
                        << loopSnapshot(model, radarPages, *radar, solidRasterStyle)
                        << ",\"owned_entry\":";
                printSoundBlasterCallbackState(synth.callbackWorkState(), segment, SoundBlasterStateDetail::boundedWork);
                segment << ",\"palette\":";
                printRetainedPalette(segment, *retainedPalette);
                frameExecutor.emplace(clock, clockedRender->phase, clockedRender->horizonCycles);
                const auto flipped = frameExecutor->display(radarPages);
                if (scanout) {
                    scanout->consume(flipped.work.timing.vgaServices, radarPixels, *retainedPalette);
                }
                segment << ",\"flip\":";
                printClockedContinuation(segment, flipped.work, synth);
                const auto postFlip = frameExecutor->postcopy(clockedRender->copyProfile);
                if (scanout) {
                    scanout->consume(postFlip.timing.vgaServices, radarPixels, *retainedPalette);
                }
                segment << ",\"post_flip\":";
                printClockedContinuation(segment, postFlip, synth);
                radarPages.flip();
                auto ownedGuards = loopController->augmentGuards(guards);
                const auto finished = frameExecutor->finish(state, ownedGuards);
                const auto& tail = finished.state;
                const auto& execution = finished.work;
                if (scanout) {
                    scanout->consume(execution.timing.vgaServices, radarPixels, *retainedPalette);
                }
                segment << ",\"tail\":";
                printClockedContinuation(segment, execution, synth);
                arrivalPhase = execution.timing.phase;
                arrivalSegment = segment.str();
                loopController->consumeEmptyTail(state.loopTick);
                ++emptyTailReads;
                arrivalExecuted = true;
                return tail.state;
            });
            model.tickWithRenderer(gameplayInput, priorRenderer);
        } else {
            model.tick(gameplayInput);
        }
        if (continueLoop && loopHistoryError.empty()) {
            try {
                if (!loopController && previousPhase == stellar7::GameplayPhase::successPresentation
                    && model.phase() == stellar7::GameplayPhase::level2Handoff) {
                    if (!held.empty() || model.level2LoopTick() != 0) {
                        throw std::invalid_argument("restored controller requires released initial loop0");
                    }
                    // Explicit restored65536 diagnostic owner; not cold campaign input ancestry.
                    constexpr std::uint16_t restoredKeyboardHead = 0x36;
                    loopController.emplace();
                    loopController->initialize({stellar7::FrameControllerOrigin::restoredDiagnosticInitial,
                        0, restoredKeyboardHead, restoredKeyboardHead, {}, 0,
                        false, false, false, false, false, {}});
                } else if (loopController) {
                    if (beforeInputEvent != nextEvent || !held.empty()) {
                        loopController->delivery(stellar7::FrameControllerDelivery::keyboard);
                    }
                    if ((previousPhase != stellar7::GameplayPhase::level2Handoff
                            && previousPhase != stellar7::GameplayPhase::level2)
                        || model.phase() != stellar7::GameplayPhase::level2) {
                        throw std::invalid_argument("restored controller left ongoing Level2");
                    }
                    if (!arrivalExecuted) {
                        loopController->consumeEmptyTail(previousLoopTick);
                        ++emptyTailReads;
                    }
                }
            } catch (const std::exception& error) {
                loopHistoryError = error.what();
            }
        }
        if (radar && model.phase() == stellar7::GameplayPhase::level1) {
            // 0784 clears1299 on success;6f52/516b never exchange pages.
            radarPages.flip();
        }
        if (radar && radarHistoryError.empty()
            && previousPhase == stellar7::GameplayPhase::successPresentation
            && model.phase() == stellar7::GameplayPhase::level2Handoff) {
            try {
                if (!clockedRender->settings) {
                    throw std::invalid_argument("cockpit initialization requires source settings");
                }
                const auto& initialization = model.lastCockpitInitialization();
                if (initialization.levelIndex != 1
                    || initialization.states.back().x != model.level2CockpitIndicatorX()
                    || initialization.states.back().velocity != model.level2CockpitIndicatorVelocity()) {
                    throw std::invalid_argument("cockpit initialization record is not the current Level2 entry");
                }
                const auto bitmaps = cockpitWorkBitmaps(assets);
                std::array<stellar7::CockpitWork, 3> draws;
                for (std::size_t index = 0; index < draws.size(); ++index) {
                    const auto& entry = initialization.states[index];
                    draws[index] = stellar7::cockpitOperations({entry.x, entry.velocity,
                        clockedRender->settings->viewSize, index == 0
                            ? stellar7::CockpitPageRelation::matching
                            : stellar7::CockpitPageRelation::differing}, bitmaps);
                    const auto& returned = initialization.states[index + 1];
                    if (draws[index].state.indicatorX != returned.x
                        || draws[index].state.indicatorVelocity != returned.velocity) {
                        throw std::invalid_argument("cockpit initialization effects differ from owned advance");
                    }
                }
                //50c4 effects consume recorded advances; startup CPU phase stays unqualified.
                constexpr std::uint8_t levelTitleColor = 239;
                radarPixels.apply(stellar7::cockpitResourcePageEffects(assets.cockpit, radarPages.cachePage()));
                //4e06's enabled Level2 scene restores12ca on return; disabled scenes retain12cc.
                // This projects the normal-return font effect, not the scene's CPU/device history.
                const auto& titleFont = clockedRender->settings->options[levelIntroductionOption] != 0
                    ? assets.titleFont : assets.scoreFont;
                radarPixels.apply(stellar7::cockpitCachePageEffects(draws[0], draws[1], bitmaps,
                    titleFont, "ANTARES", levelTitleColor, radarPages.cachePage(), radarPages.backPage()));
                radar->resetArmorStrip();
                for (const auto& actor : model.level2Entities()) {
                    radar->initializeAllocation(actor.allocationIndex);
                    radarAllocations.push_back(actor.allocationIndex);
                }
                radarPages.beginFrame();
                radarPages.selectCockpitCopy();
                const auto& player = model.level2Player();
                const auto startup = radar->initialize(model.playerArmor(), player.position,
                    player.yaw, radarPageRoles(radarPages));
                applyRadarPages(startup, radarPages, radarPixels);
                radarPixels.apply(std::vector<stellar7::IndexedPageEffect>{stellar7::RadarPageCopy{
                    radarPages.backPage(), radarPages.frontPage(), {0, 0}, 320, 200}});
                //01b7/01c0 redraw owned HUD state before the first ordinary cockpit draw.
                radarPixels.apply(stellar7::scorePageEffects(model.score(), assets.scoreFont,
                    radarPages.backPage(), radarPages.frontPage()));
                const auto& systems = model.level1State();
                radarPixels.apply(stellar7::statusPageEffects(systems.systemStatusLevels(),
                    systems.systemTimers(), systems.selectedSystem(), assets.artGroups.at(4),
                    radarPages.backPage(), radarPages.frontPage()));
                radarPixels.apply(stellar7::cockpitPageEffects(draws[2], bitmaps,
                    radarPages.cachePage(), radarPages.backPage()));
            } catch (const std::exception& error) {
                radarHistoryError = error.what();
            }
        }
        if (
            level2Armor.has_value()
            && (!appliedLevel2Armor || holdLevel2Armor)
            && (
                model.phase() == stellar7::GameplayPhase::level2Handoff
                || model.phase() == stellar7::GameplayPhase::level2
            )
        ) {
            stellar7::GameplayAnalysisAccess::applyLevel2Armor(
                model,
                *level2Armor
            );
            appliedLevel2Armor = true;
        }
        if (radar && radarHistoryError.empty()
            && (model.phase() == stellar7::GameplayPhase::level2Handoff
                || model.phase() == stellar7::GameplayPhase::level2)) {
            try {
                if (!arrivalExecuted) {
                    radarPages.beginFrame();
                    radarPages.selectCockpitCopy();
                    const auto& player = model.level2Player();
                    const auto work = radar->draw(model.playerArmor(), player.position, player.yaw,
                        radarActors(model.level2PresentationEntities(), model.level2PresentationActiveListOrder()),
                        radarPageRoles(radarPages), {0, 0});
                    applyRadarPages(work, radarPages, radarPixels);
                    radarPages.flip();
                }
                if (clockedRender->incomingFront && tick == count) {
                    std::ostringstream producer;
                    producer << "{\"loop_tick\":" << (returnedOrigin ? returnedRenderLoopTick : model.level2LoopTick())
                             << ",\"cold_phase_qualified\":false,\"page\":";
                    printIndexedPage(producer, radarPixels, radarPages.frontPage());
                    producer << '}';
                    priorRenderPage = producer.str();
                }
                if (arrivalExecuted) {
                    arrivalSegment += ",\"tail_state\":" + loopSnapshot(model, radarPages, *radar, solidRasterStyle) + '}';
                }
            } catch (const std::exception& error) {
                radarHistoryError = error.what();
            }
        }
        const auto enteredSuccessPresentation =
            (
                previousPhase
                    != stellar7::GameplayPhase::successPresentation
                && model.phase()
                    == stellar7::GameplayPhase::successPresentation
            )
            || (
                previousPhase
                    != stellar7::GameplayPhase::level2SuccessPresentation
                && model.phase()
                    == stellar7::GameplayPhase::level2SuccessPresentation
            )
            || (
                previousPhase
                    != stellar7::GameplayPhase::level3SuccessPresentation
                && model.phase()
                    == stellar7::GameplayPhase::level3SuccessPresentation
            );
        std::optional<stellar7::SoundBlasterDriverState> successEntryDriver;
        if (enteredSuccessPresentation) {
            successEntryDriver = synth.driverState();
            synth.startSuccess();
        }
        if (
            previousPhase == stellar7::GameplayPhase::successPresentation
            && model.phase() == stellar7::GameplayPhase::level2Handoff
        ) {
            synth.stopSuccess();
            synth.switchLevel2();
        }
        if (
            previousPhase
                == stellar7::GameplayPhase::level2SuccessPresentation
            && model.phase() == stellar7::GameplayPhase::level3Handoff
        ) {
            synth.stopSuccess();
            synth.switchLevel3();
        }
        if (
            previousPhase
                == stellar7::GameplayPhase::level3SuccessPresentation
            && model.phase() == stellar7::GameplayPhase::level4Handoff
        ) {
            synth.stopSuccess();
        }
        if (previousPhase == stellar7::GameplayPhase::continuePrompt
            && model.phase() == stellar7::GameplayPhase::level2Handoff) {
            synth.stopMusic();
            synth.switchLevel2();
            level2AudioEventCursor = 0;
            failureAudioEventCursor = 0;
        }
        if (previousPhase == stellar7::GameplayPhase::continuePrompt
            && model.phase() == stellar7::GameplayPhase::level1RetryHandoff) {
            synth.stopMusic();
            synth.startMusic();
            audioEventCursor = 0;
            failureAudioEventCursor = 0;
        }
        if (previousPhase == stellar7::GameplayPhase::failurePreExplosionBlackout
            && model.phase() == stellar7::GameplayPhase::failureExplosion) {
            failureAudioEventCursor = 0;
        }
        const auto& audioEvents = model.level1State().audioEvents();
        std::vector<stellar7::Level1AudioEvent> tickAudioEvents;
        while (audioEventCursor < audioEvents.size()) {
            const auto event = audioEvents[audioEventCursor];
            tickAudioEvents.push_back(event);
            synth.triggerLevel1Event(event.eventId);
            ++audioEventCursor;
        }
        const auto& level2AudioEvents = model.level2AudioEvents();
        while (level2AudioEventCursor < level2AudioEvents.size()) {
            const auto event = level2AudioEvents[level2AudioEventCursor];
            tickAudioEvents.push_back(event);
            synth.triggerLevel1Event(event.eventId);
            ++level2AudioEventCursor;
        }
        const auto& failureAudioEvents = model.failureAudioEvents();
        while (failureAudioEventCursor < failureAudioEvents.size()) {
            const auto event = failureAudioEvents[failureAudioEventCursor];
            tickAudioEvents.push_back(event);
            synth.triggerLevel1Event(event.eventId);
            ++failureAudioEventCursor;
        }
        const auto entryFadeSamples = stellar7::startsLevelEntry(previousPhase, model.phase())
            ? clock.renderEntry().size() : 0;
        if (!arrivalExecuted) {
            clock.renderFrame();
        }
        output
            << (tick == 1 ? "" : ",")
            << "{\"tick\":" << tick << ",\"sample_count\":" << samplesPerFrame
            << ",\"entry_fade_samples\":" << entryFadeSamples
            << ",\"elapsed_samples\":" << clock.elapsedSamples()
            << ",\"phase\":\"" << gameplayPhaseName(model.phase())
            << "\",\"presentation_frame\":" << model.presentationFrame()
            << ",\"success_audio_active\":" << (synth.successActive() ? "true" : "false")
            << ",\"audio_events\":[";
        for (std::size_t index = 0; index < tickAudioEvents.size(); ++index) {
            if (index != 0) {
                output << ',';
            }
            output
                << "{\"loop_tick\":"
                << tickAudioEvents[index].loopTick
                << ",\"event_id\":"
                << tickAudioEvents[index].eventId
                << ",\"dispatch_tick\":" << tick << '}';
        }
        output << "],\"writes\":";
        writeList(output, synth.registerWrites());
        output << ",\"driver_after\":";
        writeSoundBlasterDriverState(output, synth.driverState());
        if (trace == SoundBlasterCampaignTrace::ownedWorkState
            && (model.phase() == stellar7::GameplayPhase::level2Handoff || model.phase() == stellar7::GameplayPhase::level2)) {
            try {
                const auto state = synth.callbackWorkState();
                output << ",\"work_state\":";
                printSoundBlasterCallbackState(state, output, SoundBlasterStateDetail::boundedWork);
            } catch (const std::invalid_argument&) {
                output << ",\"work_state\":null";
            }
        }
        output << ",\"success_entry_driver\":";
        if (successEntryDriver.has_value()) {
            writeSoundBlasterDriverState(output, *successEntryDriver);
        } else {
            output << "null";
        }
        output << '}';
        synth.clearRegisterWrites();
        const auto afterInputEvent = nextEvent;
        applyLevel1InputEventsAfterCheckpoint(events, tick, nextEvent, held);
        if (continueLoop && loopController && afterInputEvent != nextEvent) {
            loopController->delivery(stellar7::FrameControllerDelivery::keyboard);
        }
    }
    output << ']';
    if (clockedRender && (clockedRender->kind == ClockedRenderKind::worldTraversal
        || clockedRender->kind == ClockedRenderKind::worldScene || clockedRender->kind == ClockedRenderKind::worldSight || clockedRender->kind == ClockedRenderKind::worldCockpit || clockedRender->kind == ClockedRenderKind::worldRadar)) {
        std::string rendered;
        if (clockedRender->incomingFront) {
            if (priorRenderPage.empty()) {
                throw std::invalid_argument("incoming-front producer did not complete");
            }
            output << ",\"front_producer\":" << priorRenderPage;
        }
        if (returnedOrigin) {
            if (!arrivalPhase || arrivalSegment.empty()) {
                throw std::invalid_argument("returned-render arrival did not complete");
            }
            output << ",\"clocked_arrival\":" << arrivalSegment;
        }
        stellar7::CockpitPageState pages = radar ? radarPages : stellar7::CockpitPageState{};
        if (!returnedOrigin) {
            pages.beginFrame();
        }
        const auto previousAudioEvents = model.level2AudioEvents().size();
        auto activeRender = *clockedRender;
        std::optional<stellar7::SoundBlasterClockPhase> retainedPhase = arrivalPhase;
        std::optional<std::uint8_t> retainedEnemyFlag;
        std::string loopEntry, tailState, prefixInputState, prefixState;
        std::string tailSegment, prefixSegment, secondRendered, loopError;
        unsigned renderCalls = 0;
        bool loopSupported = false;
        bool secondRenderSupported = false;
        const auto frameCount = continueLoop ? clockedRender->frameCount : 1U;
        const auto traceChain = continueLoop && frameCount > 2;
        std::vector<std::string> frameChain;
        std::string renderEntry, latestRender, rejectedRender;
        std::uint8_t rasterStyle = 1; // Main prefix selects solid raster mode.
        const auto beginRenderObservation = [&]() {
            if (!radarHistoryError.empty()) {
                throw std::invalid_argument("retained radar history: " + radarHistoryError);
            }
            if (model.level2AudioEvents().size() != previousAudioEvents) {
                throw std::invalid_argument("world traversal requires no new simulation audio dispatch");
            }
            if (continueLoop) {
                if (!radar || !loopController || !loopHistoryError.empty()) {
                    throw std::invalid_argument("retained loop history: " + loopHistoryError);
                }
                if (renderCalls == 0) {
                    if (returnedOrigin) {
                        if (!retainedPhase || prefixSegment.empty()
                            || loopController->expectedLoopTick() != model.level2LoopTick()) {
                            throw std::invalid_argument("render requires the owned arrival prefix");
                        }
                        activeRender.phase = *retainedPhase;
                    } else {
                        // The selected first tick still executes its deferred historical tail.
                        const auto deferredTick = loopController->expectedLoopTick();
                        if (static_cast<std::uint16_t>(deferredTick + 1) != model.level2LoopTick()) {
                            throw std::invalid_argument("controller history does not reach render entry");
                        }
                        loopController->consumeEmptyTail(deferredTick);
                        ++emptyTailReads;
                    }
                    loopEntry = loopSnapshot(model, pages, *radar, rasterStyle);
                } else {
                    if (!retainedPhase || prefixSegment.empty()) {
                        throw std::invalid_argument("next rendering requires owned prefix phase");
                    }
                    prefixState = loopSnapshot(model, pages, *radar, rasterStyle);
                    loopSupported = true;
                    activeRender.phase = *retainedPhase;
                }
            }
            renderEntry = radar ? loopSnapshot(model, pages, *radar, rasterStyle) : std::string{};
            retainEntryPalette();
            if (scanout && renderCalls + 1 == frameCount) {
                activeRender.scanoutEnd = ScanoutEnd::renderReturn;
            }
        };
        const auto recordRenderObservation = [&](const std::string& result) {
            latestRender = result;
            if (traceChain && !retainedPhase) {
                rejectedRender = result;
                throw std::invalid_argument("retained render rejected; see rejected_render");
            }
            if (renderCalls == 0) {
                rendered = result;
            } else {
                secondRendered = result;
                secondRenderSupported = retainedPhase.has_value();
            }
            ++renderCalls;
        };
        ClockedLevel2Renderer renderer([&](stellar7::Level2RenderState state) {
            beginRenderObservation();
            const auto result = writeClockedRenderTrace(clock, synth, activeRender, assets, model, &state, &pages,
                radar ? &*radar : nullptr, radar ? &radarPixels : nullptr, &retainedPhase, &rasterStyle,
                &retainedEnemyFlag, retainedPalette ? &*retainedPalette : nullptr,
                scanout ? &*scanout : nullptr, frameExecutor ? &*frameExecutor : nullptr);
            recordRenderObservation(result);
        }, [&](const stellar7::StationaryLevel2PrefixInput& input)
            -> std::optional<stellar7::StationaryLevel2PrefixWork> {
            if (!continueLoop || (renderCalls == 0 && !returnedOrigin)
                || renderCalls >= frameCount || !retainedPhase || !radar) {
                throw std::invalid_argument("prefix requires the preceding owned frame tail");
            }
            prefixInputState = loopSnapshot(model, pages, *radar, rasterStyle);
            const auto owned = frameExecutor
                ? std::optional<stellar7::RetainedPrefixExecution>{frameExecutor->prepare(input)} : std::nullopt;
            const auto work = owned ? owned->state : stellar7::stationaryLevel2PrefixOperations(input);
            const auto execution = owned ? owned->work
                : clock.renderWork(*retainedPhase, work.operations, activeRender.horizonCycles);
            if (scanout) {
                scanout->consume(execution.timing.vgaServices, radarPixels, *retainedPalette);
            }
            std::ostringstream segment;
            printClockedContinuation(segment, execution, synth);
            prefixSegment = segment.str();
            retainedPhase = execution.timing.phase;
            pages.beginFrame();
            rasterStyle = 1;
            return work;
        }, [&](const stellar7::FrameTailState& state, const stellar7::FrameTailGuards& guards)
            -> std::optional<stellar7::FrameTailState> {
            if (!continueLoop || renderCalls == 0 || renderCalls >= frameCount) {
                return std::nullopt;
            }
            if (!retainedPhase || !loopController || !held.empty()) {
                throw std::invalid_argument("tail requires owned display phase and released controller");
            }
            if (state.loopTick != loopController->expectedLoopTick()) {
                throw std::invalid_argument("tail/controller sequence mismatch");
            }
            auto ownedGuards = loopController->augmentGuards(guards);
            if (ownedGuards.periodic) {
                ownedGuards.periodic->enemyFlag = retainedEnemyFlag;
            }
            const auto owned = frameExecutor
                ? std::optional<stellar7::RetainedTailExecution>{frameExecutor->finish(state, ownedGuards)} : std::nullopt;
            const auto work = owned ? owned->state : stellar7::frameTailOperations(state, ownedGuards);
            const auto execution = owned ? owned->work
                : clock.renderWork(*retainedPhase, work.operations, activeRender.horizonCycles);
            if (scanout) {
                scanout->consume(execution.timing.vgaServices, radarPixels, *retainedPalette);
            }
            std::ostringstream segment;
            printClockedContinuation(segment, execution, synth);
            tailSegment = segment.str();
            retainedPhase = execution.timing.phase;
            loopController->consumeEmptyTail(state.loopTick);
            ++emptyTailReads;
            return work.state;
        });
        RetainedTraceObserver runtimeObserver;
        std::optional<stellar7::RetainedLevel2Renderer> runtime;
        std::optional<stellar7::Level2RenderState> runtimeFrame;
        std::optional<stellar7::SoundBlasterCallbackState> runtimeEntry;
        std::optional<stellar7::PageFlipClockState> runtimeFlip;
        std::ostringstream runtimeOutput;
        if (returnedOrigin && continueLoop && frameCount > 1 && frameExecutor && radar && loopController
            && activeRender.kind == ClockedRenderKind::worldRadar && activeRender.horizon && activeRender.settings) {
            runtimeObserver.onRenderEntry = [&](const stellar7::Level2FramePlan&,
                const stellar7::Level2RenderState& state) {
                beginRenderObservation();
                runtimeFrame.emplace(state);
                runtimeEntry = synth.callbackWorkState();
                runtimeOutput.str({});
                runtimeOutput.clear();
                runtimeOutput << std::setprecision(17);
            };
            runtimeObserver.onBody = [&](const stellar7::Level2FramePlan& plan,
                const stellar7::ClockedPresentationWork& work) {
                const ClockedRenderObservation observation{plan, work, *runtimeEntry};
                runtimeOutput << writeClockedRenderTrace(clock, synth, activeRender, assets, model,
                    &*runtimeFrame, &pages, &*radar, &radarPixels, nullptr, nullptr, nullptr,
                    retainedPalette ? &*retainedPalette : nullptr, scanout ? &*scanout : nullptr,
                    nullptr, &observation);
            };
            runtimeObserver.onDisplay = [&](const stellar7::CockpitPageState&,
                const stellar7::IndexedPages& completedPixels, const stellar7::ClockedPageFlipWork& flipped) {
                if (scanout) {
                    scanout->consume(flipped.work.timing.vgaServices, completedPixels, *retainedPalette);
                }
                runtimeFlip = flipped.state;
                runtimeOutput << ",\"display_end\":{\"boundary\":\"8518->8552\",\"native_phase_qualified\":false"
                    << ",\"low_reads\":" << flipped.lowReads << ",\"high_reads\":" << flipped.highReads << ",\"flip\":";
                printClockedContinuation(runtimeOutput, flipped.work, synth);
            };
            runtimeObserver.onPostcopy = [&](const stellar7::CockpitPageState& completedPages,
                const stellar7::IndexedPages& completedPixels, const stellar7::ClockedPresentationWork& work) {
                if (scanout) {
                    scanout->consume(work.timing.vgaServices, completedPixels, *retainedPalette);
                }
                runtimeOutput << ",\"post_flip\":";
                printClockedContinuation(runtimeOutput, work, synth);
                runtimeOutput << ",\"rectangle_effect\":\"none\",\"front_page\":" << runtimeFlip->frontPage
                    << ",\"back_page\":" << runtimeFlip->backPage << '}';
                runtimeOutput << ",\"indexed_frame\":";
                printIndexedPage(runtimeOutput, completedPixels, completedPages.drawPage());
                runtimeOutput << ",\"indexed_previous_front\":";
                printIndexedPage(runtimeOutput, completedPixels, completedPages.frontPage());
                if (retainedPalette) {
                    runtimeOutput << ",\"palette\":";
                    printRetainedPalette(runtimeOutput, *retainedPalette);
                }
                runtimeOutput << '}';
                retainedPhase = work.timing.phase;
            };
            runtimeObserver.onRenderComplete = [&](const stellar7::Level2FramePlan&) {
                recordRenderObservation(runtimeOutput.str());
                runtimeFrame.reset();
                runtimeEntry.reset();
            };
            runtimeObserver.onPrefixEntry = [&](const stellar7::StationaryLevel2PrefixInput&) {
                if (renderCalls >= frameCount || !retainedPhase) {
                    throw std::invalid_argument("prefix requires the preceding owned frame tail");
                }
                prefixInputState = loopSnapshot(model, pages, *radar, rasterStyle);
            };
            runtimeObserver.onPrefix = [&](const stellar7::RetainedPrefixExecution& result) {
                if (scanout) {
                    scanout->consume(result.work.timing.vgaServices, radarPixels, *retainedPalette);
                }
                std::ostringstream segment;
                printClockedContinuation(segment, result.work, synth);
                prefixSegment = segment.str();
                retainedPhase = result.work.timing.phase;
            };
            runtimeObserver.onTailEntry = [&](const stellar7::FrameTailState&, const stellar7::FrameTailGuards&) {
                if (!held.empty()) {
                    throw std::invalid_argument("tail requires released controller");
                }
            };
            runtimeObserver.onTail = [&](const stellar7::RetainedTailExecution& result) {
                if (scanout) {
                    scanout->consume(result.work.timing.vgaServices, radarPixels, *retainedPalette);
                }
                std::ostringstream segment;
                printClockedContinuation(segment, result.work, synth);
                tailSegment = segment.str();
                retainedPhase = result.work.timing.phase;
                ++emptyTailReads;
            };
            runtime.emplace(stellar7::RetainedLevel2Owners{model, assets, *frameExecutor, *loopController,
                pages, *radar, radarPixels, *activeRender.horizon, *activeRender.settings, rasterStyle,
                retainedEnemyFlag}, activeRender.copyProfile, runtimeObserver);
        }
        const auto advanceObservedFrame = [&](const stellar7::GameplayInput& input) {
            if (runtime && renderCalls + 1 < frameCount) {
                runtime->advance(input);
                return;
            }
            // The terminal diagnostic retains its existing unclocked tail policy.
            model.tickWithRenderer(input, renderer);
        };
        try {
            const auto priorEvent = nextEvent;
            applyLevel1InputEventsBeforeTick(events, count + 1U, nextEvent, held);
            if (continueLoop && loopController && priorEvent != nextEvent) {
                loopController->delivery(stellar7::FrameControllerDelivery::keyboard);
            }
            advanceObservedFrame({held.count("left") != 0, held.count("right") != 0,
                held.count("up") != 0, held.count("down") != 0, held.count("spc") != 0, false});
            if (rendered.empty()) {
                throw std::invalid_argument("world traversal did not reach ground rendering boundary");
            }
        } catch (const std::exception& error) {
            loopError = error.what();
            std::ostringstream rejected;
            rejected << "{\"supported\":false,\"owned_entry\":null,\"error\":" << std::quoted(error.what()) << '}';
            if (!continueLoop || rendered.empty()) {
                rendered = rejected.str();
            }
        }
        const auto recordChainFrame = [&]() {
            if (!traceChain || !loopError.empty() || !retainedPhase) {
                return;
            }
            const auto jsonOrNull = [](const std::string& value) { return value.empty() ? "null" : value; };
            tailState = tailSegment.empty() ? std::string{} : loopSnapshot(model, pages, *radar, rasterStyle);
            std::ostringstream frame;
            frame << "{\"entry_state\":" << jsonOrNull(renderEntry)
                  << ",\"prefix_input_state\":" << jsonOrNull(prefixInputState)
                  << ",\"prefix\":" << jsonOrNull(prefixSegment)
                  << ",\"render\":" << jsonOrNull(latestRender)
                  << ",\"tail\":" << jsonOrNull(tailSegment)
                  << ",\"tail_state\":" << jsonOrNull(tailState) << '}';
            frameChain.push_back(frame.str());
        };
        recordChainFrame();
        for (unsigned frame = 2; continueLoop && frame <= frameCount && loopError.empty()
            && retainedPhase && !tailSegment.empty(); ++frame) {
            if (!traceChain) {
                tailState = loopSnapshot(model, pages, *radar, rasterStyle);
            } else {
                prefixInputState.clear();
                prefixSegment.clear();
                tailSegment.clear();
                tailState.clear();
            }
            try {
                const auto priorEvent = nextEvent;
                applyLevel1InputEventsAfterCheckpoint(events, count + frame - 1U, nextEvent, held);
                applyLevel1InputEventsBeforeTick(events, count + frame, nextEvent, held);
                if (priorEvent != nextEvent) {
                    loopController->delivery(stellar7::FrameControllerDelivery::keyboard);
                }
                if (!held.empty() || loopController->expectedLoopTick() != model.level2LoopTick()) {
                    throw std::invalid_argument("next prefix requires retained released controller");
                }
                advanceObservedFrame({});
            } catch (const std::exception& error) {
                loopError = error.what();
            }
            recordChainFrame();
        }
        if (traceChain) {
            output << ",\"frame_chain\":{\"supported\":"
                   << (loopError.empty() && frameChain.size() == frameCount ? "true" : "false")
                   << ",\"error\":" << std::quoted(loopError) << ",\"requested_frames\":" << frameCount
                   << ",\"controller_origin\":\"restored65536\",\"empty_tail_reads\":" << emptyTailReads
                   << ",\"rejected_render\":" << (rejectedRender.empty() ? "null" : rejectedRender)
                   << ",\"frames\":[";
            for (std::size_t index = 0; index < frameChain.size(); ++index) {
                output << (index == 0 ? "" : ",") << frameChain[index];
            }
            output << "]}";
        } else if (continueLoop) {
            const auto jsonOrNull = [](const std::string& value) { return value.empty() ? "null" : value; };
            output << ",\"loop_continuation\":{\"supported\":" << (loopSupported ? "true" : "false")
                   << ",\"error\":" << std::quoted(loopError) << ",\"entry_state\":" << jsonOrNull(loopEntry)
                   << ",\"tail\":" << jsonOrNull(tailSegment) << ",\"tail_state\":" << jsonOrNull(tailState)
                   << ",\"prefix_input_state\":" << jsonOrNull(prefixInputState)
                   << ",\"prefix\":" << jsonOrNull(prefixSegment) << ",\"prefix_state\":" << jsonOrNull(prefixState)
                   << ",\"second_render\":" << jsonOrNull(secondRendered)
                   << ",\"second_render_supported\":" << (secondRenderSupported ? "true" : "false")
                   << ",\"controller_origin\":\"restored65536\",\"empty_tail_reads\":" << emptyTailReads << '}';
        }
        output << (clockedRender->kind == ClockedRenderKind::worldRadar
            ? ",\"clocked_world_radar\":" : clockedRender->kind == ClockedRenderKind::worldCockpit
            ? ",\"clocked_world_cockpit\":" : clockedRender->kind == ClockedRenderKind::worldSight
            ? ",\"clocked_world_sight\":" : clockedRender->kind == ClockedRenderKind::worldScene
            ? ",\"clocked_world_scene\":" : ",\"clocked_world_traversal\":") << rendered;
        output << ",\"retained_model_ground\":";
        printGroundState(output, model.level2Ground(), model.level2Rng());
        output << ",\"retained_model_sight\":" << (model.alternateSightPhase() ? "true" : "false");
        if (clockedRender->kind == ClockedRenderKind::worldCockpit || clockedRender->kind == ClockedRenderKind::worldRadar) {
            output << ",\"retained_model_cockpit\":{\"indicator_x\":" << model.level2CockpitIndicatorX()
                   << ",\"indicator_velocity\":" << model.level2CockpitIndicatorVelocity() << '}';
        }
        if (radar && radarHistoryError.empty()) {
            output << ",\"retained_radar\":{\"strip\":" << radar->armorStrip() << ",\"pages\":["
                   << pages.cachePage() << ',' << pages.backPage() << ',' << pages.frontPage() << ','
                   << pages.drawPage() << ',' << pages.copyPage() << "],\"actors\":[";
            for (std::size_t index = 0; index < radarAllocations.size(); ++index) {
                const auto allocation = radarAllocations[index];
                const auto& actor = radar->projection(allocation);
                output << (index == 0 ? "[" : ",[") << allocation << ',' << actor.cachedCenter.x << ','
                       << actor.cachedCenter.y << ',' << actor.cachedCenter.z << ',' << actor.projectedX
                       << ',' << actor.projectedY << ',' << actor.projectedRadius << ','
                       << static_cast<unsigned>(actor.flags) << ']';
            }
            output << "]}";
        }
    } else if (clockedRender) {
        output << (clockedRender->kind == ClockedRenderKind::worldSetup ? ",\"clocked_world_setup\":"
            : clockedRender->kind == ClockedRenderKind::horizonGround ? ",\"clocked_horizon_ground\":"
            : clockedRender->horizon ? ",\"clocked_horizon\":" : ",\"clocked_cockpit\":")
               << writeClockedRenderTrace(clock, synth, *clockedRender, assets, model);
    }
    if (scanout) {
        std::ofstream rgb(outputPath + ".scan-rgb.bin", std::ios::binary);
        if (!rgb) {
            throw std::runtime_error("cannot create native scanout artifact");
        }
        output << ",\"scanout\":{\"profile\":\"quiet-level2-v1\",\"host_present_qualified\":false"
               << ",\"terminal_boundary\":\"last-render-return\""
               << ",\"width\":" << QuietScanoutCollector::width << ",\"height\":" << QuietScanoutCollector::height
               << ",\"discarded_initial_lines\":" << scanout->discardedInitialLines()
               << ",\"pending_lines\":" << scanout->pendingLines() << ",\"completed\":[";
        std::size_t offset = 0;
        for (const auto& completed : scanout->completed()) {
            rgb.write(reinterpret_cast<const char*>(completed.rgb.data()), completed.rgb.size());
            output << (offset == 0 ? "{" : ",{") << "\"origin_ms\":" << std::setprecision(17)
                   << completed.originMilliseconds << ",\"completion_ms\":" << completed.completionMilliseconds
                   << ",\"scheduled_completion_ms\":" << completed.scheduledCompletionMilliseconds
                   << ",\"page\":" << completed.page << ",\"offset\":" << offset
                   << ",\"bytes\":" << completed.rgb.size() << '}';
            offset += completed.rgb.size();
        }
        rgb.close();
        if (!rgb) {
            throw std::runtime_error("cannot write native scanout artifact");
        }
        output << "]}";
    }
    output << "}\n";
    writeText(outputPath, output.str());
}

void writeLevel2InputScriptSoundBlasterTrace(
    const stellar7::GameplayAssets& assets,
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t count,
    const std::string& outputPath
) {
    if (count == 0 || events.empty()) {
        throw std::runtime_error(
            "Level 2 input script audio trace requires events and a positive "
            "tick count"
        );
    }
    constexpr std::size_t samplesPerFrame =
        stellar7::gameplayAudioSamplesPerFrame;

    stellar7::SoundBlasterSynth synth(
        assets.soundSequences,
        assets.soundBlasterInstrumentResource,
        assets.musicForLevel(0),
        assets.presentationMusicSequence,
        assets.musicForLevel(1), assets.musicForLevel(2), assets.logoMusicSequence
    );
    synth.startLevel2();
    const auto startupWrites = synth.registerWrites();
    synth.clearRegisterWrites();
    stellar7::PresentationClock clock(synth);
    const auto entrySamples = clock.renderEntry().size();
    const auto entryWrites = synth.registerWrites();
    synth.clearRegisterWrites();

    const auto writeList = [](
        std::ostringstream& output,
        const std::vector<stellar7::OplRegisterWrite>& writes
    ) {
        output << '[';
        for (std::size_t index = 0; index < writes.size(); ++index) {
            if (index != 0) {
                output << ',';
            }
            output
                << "{\"register\":"
                << static_cast<unsigned>(writes[index].registerNumber)
                << ",\"value\":"
                << static_cast<unsigned>(writes[index].value)
                << '}';
        }
        output << ']';
    };

    std::ostringstream output;
    output
        << "{\"format_version\":1,\"sample_rate\":"
        << stellar7::SoundBlasterSynth::sampleRate
        << ",\"sequence_rate\":"
        << stellar7::SoundBlasterSynth::sequenceRate
        << ",\"startup_writes\":";
    writeList(output, startupWrites);
    output << ",\"entry_presentation\":{\"game_tick\":0,\"steps\":" << stellar7::levelEntryFadeSteps
           << ",\"sample_count\":" << entrySamples << ",\"writes\":";
    writeList(output, entryWrites);
    output << '}';
    output << ",\"ticks\":[";

    stellar7::GameplayModel model;
    model.startLevel2();
    std::set<std::string> held;
    std::size_t nextEvent = 0;
    std::size_t level2AudioEventCursor = 0;
    std::size_t failureAudioEventCursor = 0;
    for (std::uint32_t tick = 1; tick <= count; ++tick) {
        applyLevel1InputEventsBeforeTick(events, tick, nextEvent, held);
        model.tick({
            held.count("left") != 0,
            held.count("right") != 0,
            held.count("up") != 0,
            held.count("down") != 0,
            held.count("spc") != 0,
            false,
        });
        std::vector<stellar7::Level1AudioEvent> tickAudioEvents;
        const auto& level2AudioEvents = model.level2AudioEvents();
        while (level2AudioEventCursor < level2AudioEvents.size()) {
            const auto event = level2AudioEvents[level2AudioEventCursor];
            tickAudioEvents.push_back(event);
            synth.triggerLevel1Event(event.eventId);
            ++level2AudioEventCursor;
        }
        const auto& failureAudioEvents = model.failureAudioEvents();
        while (failureAudioEventCursor < failureAudioEvents.size()) {
            const auto event = failureAudioEvents[failureAudioEventCursor];
            tickAudioEvents.push_back(event);
            synth.triggerLevel1Event(event.eventId);
            ++failureAudioEventCursor;
        }
        clock.renderFrame();
        output
            << (tick == 1 ? "" : ",")
            << "{\"tick\":" << tick << ",\"sample_count\":" << samplesPerFrame
            << ",\"elapsed_samples\":" << clock.elapsedSamples() << ",\"audio_events\":[";
        for (std::size_t index = 0; index < tickAudioEvents.size(); ++index) {
            if (index != 0) {
                output << ',';
            }
            output
                << "{\"loop_tick\":"
                << tickAudioEvents[index].loopTick
                << ",\"event_id\":"
                << tickAudioEvents[index].eventId
                << ",\"dispatch_tick\":" << tick << '}';
        }
        output << "],\"writes\":";
        writeList(output, synth.registerWrites());
        output << '}';
        synth.clearRegisterWrites();
        applyLevel1InputEventsAfterCheckpoint(events, tick, nextEvent, held);
    }
    output << "]}\n";
    writeText(outputPath, output.str());
}

void writeLevel3SoundBlasterTrace(
    const stellar7::GameplayAssets& assets,
    const std::uint32_t count,
    const std::string& outputPath,
    const std::optional<std::pair<std::uint32_t, int>>& injectedEvent,
    const bool residentSwitch = false
) {
    if (count == 0) {
        throw std::runtime_error(
            "Level 3 audio trace requires a positive tick count"
        );
    }
    constexpr std::size_t ticksPerSecond =
        stellar7::SoundBlasterSynth::sequenceRate;
    static_assert(
        stellar7::SoundBlasterSynth::sampleRate % ticksPerSecond == 0
    );
    constexpr std::size_t samplesPerTick =
        stellar7::SoundBlasterSynth::sampleRate / ticksPerSecond;

    stellar7::SoundBlasterSynth synth(
        assets.soundSequences,
        assets.soundBlasterInstrumentResource,
        assets.musicForLevel(0),
        assets.presentationMusicSequence,
        assets.musicForLevel(1),
        assets.musicForLevel(2), assets.logoMusicSequence
    );
    if (residentSwitch) {
        // Mirror the campaign handoff: keep the resident physical voices and
        // switch the sequencer descriptor without a fresh all-voice reset.
        synth.startLevel2();
        synth.clearRegisterWrites();
        synth.switchLevel3();
    } else {
        synth.startLevel3();
    }
    const auto startupWrites = synth.registerWrites();
    synth.clearRegisterWrites();

    const auto writeList = [](
        std::ostringstream& output,
        const std::vector<stellar7::OplRegisterWrite>& writes
    ) {
        output << '[';
        for (std::size_t index = 0; index < writes.size(); ++index) {
            if (index != 0) {
                output << ',';
            }
            output
                << "{\"register\":"
                << static_cast<unsigned>(writes[index].registerNumber)
                << ",\"value\":"
                << static_cast<unsigned>(writes[index].value)
                << '}';
        }
        output << ']';
    };

    std::ostringstream output;
    output
        << "{\"format_version\":1,\"sample_rate\":"
        << stellar7::SoundBlasterSynth::sampleRate
        << ",\"sequence_rate\":"
        << stellar7::SoundBlasterSynth::sequenceRate
        << ",\"resident_switch\":"
        << (residentSwitch ? "true" : "false")
        << ",\"startup_writes\":";
    writeList(output, startupWrites);
    output << ",\"ticks\":[";
    for (std::uint32_t tick = 1; tick <= count; ++tick) {
        if (injectedEvent && injectedEvent->first == tick) {
            synth.triggerLevel1Event(injectedEvent->second);
        }
        synth.render(samplesPerTick);
        output
            << (tick == 1 ? "" : ",")
            << "{\"tick\":" << tick << ",\"writes\":";
        writeList(output, synth.registerWrites());
        output << '}';
        synth.clearRegisterWrites();
    }
    output << "]}\n";
    writeText(outputPath, output.str());
}

void writeLevel3SuccessReleaseTrace(
    const stellar7::GameplayAssets& assets,
    const std::uint32_t count,
    const std::string& outputPath,
    const bool residentSwitch
) {
    if (count == 0) {
        throw std::runtime_error(
            "Level 3 success-release trace requires a positive frame count"
        );
    }
    constexpr std::size_t frameRate = 30;
    static_assert(
        stellar7::SoundBlasterSynth::sampleRate % frameRate == 0
    );
    constexpr std::size_t samplesPerFrame =
        stellar7::SoundBlasterSynth::sampleRate / frameRate;
    stellar7::SoundBlasterSynth synth(
        assets.soundSequences,
        assets.soundBlasterInstrumentResource,
        assets.musicForLevel(0),
        assets.presentationMusicSequence,
        assets.musicForLevel(1),
        assets.musicForLevel(2), assets.logoMusicSequence
    );
    if (residentSwitch) {
        synth.startLevel2();
        synth.clearRegisterWrites();
        synth.switchLevel3();
    } else {
        synth.startLevel3();
    }
    synth.clearRegisterWrites();
    for (std::uint32_t frame = 0; frame < count; ++frame) {
        synth.render(samplesPerFrame);
    }
    synth.clearRegisterWrites();
    synth.startSuccess();
    std::ostringstream output;
    output << "{\"format_version\":1,\"frames\":" << count
           << ",\"resident_switch\":"
           << (residentSwitch ? "true" : "false")
           << ",\"writes\":[";
    const auto& writes = synth.registerWrites();
    for (std::size_t index = 0; index < writes.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << "{\"register\":"
               << static_cast<unsigned>(writes[index].registerNumber)
               << ",\"value\":"
               << static_cast<unsigned>(writes[index].value) << '}';
    }
    output << "]}\n";
    writeText(outputPath, output.str());
}

void writeLevel1InputScriptModelDrawTrace(
    const stellar7::GameplayAssets& assets,
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t count,
    const std::string& outputPath,
    const std::optional<std::int16_t> level2Armor = std::nullopt,
    const bool holdLevel2Armor = false
) {
    const auto model = replayLevel1InputScriptGameplay(
        events,
        count,
        level2Armor, {}, holdLevel2Armor
    );
    writeText(
        outputPath,
        (
            model.phase() == stellar7::GameplayPhase::failureExplosion
                ? stellar7::level1FailureModelDrawTraceJson(assets, model)
                : (
                    model.phase() == stellar7::GameplayPhase::level1
                        ? stellar7::level1GameplayModelDrawTraceJson(
                            assets,
                            model
                        )
                        : stellar7::level2GameplayModelDrawTraceJson(
                            assets,
                            model
                        )
                )
        ) + "\n"
    );
}

void writeLevel1InputScriptModelDrawTraceSeries(
    const stellar7::GameplayAssets& assets,
    const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t count,
    const std::string& outputDirectory
) {
    if (count == 0 || events.empty()) {
        throw std::runtime_error(
            "input script model draw series requires events and a positive "
            "tick count"
        );
    }
    const std::filesystem::path root(outputDirectory);
    std::filesystem::create_directories(root);
    stellar7::GameplayModel model;
    std::set<std::string> held;
    std::size_t nextEvent = 0;
    for (std::uint32_t tick = 1; tick <= count; ++tick) {
        applyLevel1InputEventsBeforeTick(events, tick, nextEvent, held);
        model.tick({
            held.count("left") != 0,
            held.count("right") != 0,
            held.count("up") != 0,
            held.count("down") != 0,
            held.count("spc") != 0,
            held.count("esc") != 0,
        });
        writeText(
            (root / ("tick-" + std::to_string(tick) + ".json")).string(),
            stellar7::level1GameplayModelDrawTraceJson(assets, model) + "\n"
        );
        applyLevel1InputEventsAfterCheckpoint(events, tick, nextEvent, held);
    }
}

std::string joinedInputKeys(const std::vector<std::string>& keys) {
    std::ostringstream output;
    for (std::size_t index = 0; index < keys.size(); ++index) {
        if (index != 0) {
            output << '+';
        }
        output << keys[index];
    }
    return output.str();
}

void recordInputTransition(
    const std::uint32_t tick,
    const stellar7::Level1Input& previous,
    const stellar7::Level1Input& next,
    std::vector<RecordedInputEvent>& events
) {
    std::vector<std::string> released;
    std::vector<std::string> pressed;
    for (const auto& [name, field] : level1InputKeys) {
        if (previous.*field && !(next.*field)) {
            released.emplace_back(name);
        } else if (!(previous.*field) && next.*field) {
            pressed.emplace_back(name);
        }
    }
    if (!released.empty()) {
        events.push_back({tick, false, joinedInputKeys(released)});
    }
    if (!pressed.empty()) {
        events.push_back({tick, true, joinedInputKeys(pressed)});
    }
}

stellar7::Level1Input steeringInput(
    const stellar7::Level1State& state,
    const stellar7::Level1Entity& target,
    const bool forward,
    const bool reverse,
    const bool fire
) {
    const auto difference = static_cast<std::int16_t>(
        stellar7::fixedHeadingBetween(
            state.player().position,
            target.position
        ) - state.player().yaw
    );
    const auto desiredVelocity = std::clamp(
        static_cast<int>(difference) / 4,
        -0x200,
        0x200
    );
    const auto currentVelocity = state.player().yawVelocity;
    return {
        currentVelocity < desiredVelocity - 0x28,
        currentVelocity > desiredVelocity + 0x28,
        forward,
        reverse,
        fire,
    };
}

stellar7::Level1Input signedApproachInput(
    const stellar7::Level1State& state,
    const stellar7::Level1Entity& target
) {
    const bool movingBackward =
        state.player().forwardVelocity < 0;
    const auto targetHeading = static_cast<std::uint16_t>(
        stellar7::fixedHeadingBetween(
            state.player().position,
            target.position
        ) + (movingBackward ? 0x8000U : 0U)
    );
    const auto difference = static_cast<std::int16_t>(
        targetHeading - state.player().yaw
    );
    const auto desiredVelocity = std::clamp(
        static_cast<int>(difference) / 4,
        -0x200,
        0x200
    );
    const auto currentVelocity = state.player().yawVelocity;
    return {
        currentVelocity < desiredVelocity - 0x28,
        currentVelocity > desiredVelocity + 0x28,
        !movingBackward,
        movingBackward,
        false,
    };
}

std::string level1SuccessInputScript(
    const std::uint32_t terminalTick,
    const std::vector<RecordedInputEvent>& events,
    const std::vector<std::pair<std::string, std::uint32_t>>& checkpoints
) {
    std::ostringstream output;
    output
        << "dos-re-state-input-script-v1\n"
        << "# state_field=loop_tick\n"
        << "# controller=portable-natural-success-v1\n"
        << "# terminal=success\n"
        << "# terminal_value=" << terminalTick << '\n';
    for (const auto& [name, tick] : checkpoints) {
        output
            << "# checkpoint." << name << '=' << tick << '\n';
    }
    for (const auto& event : events) {
        output
            << event.tick << '='
            << (event.pressed ? "down." : "up.")
            << event.key << '\n';
    }
    return output.str();
}

void exportLevel1SuccessScript(
    const std::string& schedulePath,
    const std::string& tracePath
) {
    stellar7::Level1State state;
    stellar7::Level1Input held{};
    std::vector<RecordedInputEvent> events;
    std::vector<std::pair<std::string, std::uint32_t>> checkpoints;
    std::uint32_t inputBoundaryTick = 1;

    const auto advance = [
        &state,
        &held,
        &events,
        &inputBoundaryTick
    ](const stellar7::Level1Input& input) {
        recordInputTransition(inputBoundaryTick, held, input, events);
        held = input;
        state.advanceTick(input);
        ++inputBoundaryTick;
        if (state.failureState()) {
            throw std::runtime_error(
                "natural success controller reached failure"
            );
        }
    };

    for (
        std::uint32_t step = 0;
        step < 10000 && state.completionObject() == nullptr;
        ++step
    ) {
        const stellar7::Level1Entity* target = nullptr;
        for (const auto& candidate : state.entities()) {
            if (
                !candidate.active()
                || static_cast<std::int8_t>(
                    candidate.stateBytes4dTo52[0]
                ) != -1
                || std::abs(candidate.position.z)
                    > static_cast<std::int32_t>(
                        (candidate.collisionRadius + 134U) >> 1U
                    )
            ) {
                continue;
            }
            if (
                target == nullptr
                || candidate.presenterDistance < target->presenterDistance
            ) {
                target = &candidate;
            }
        }
        advance(
            target == nullptr
                ? stellar7::Level1Input{}
                : steeringInput(state, *target, true, false, true)
        );
    }
    if (state.completionObject() == nullptr) {
        throw std::runtime_error(
            "natural success controller did not reach completion object"
        );
    }
    checkpoints.emplace_back("completion-object", state.loopTick());

    for (
        std::uint32_t step = 0;
        step < 1000 && state.postWaveObject() == nullptr;
        ++step
    ) {
        advance({});
    }
    if (state.postWaveObject() == nullptr) {
        throw std::runtime_error(
            "natural success controller did not reach Type 44"
        );
    }
    checkpoints.emplace_back("type44", state.loopTick());

    for (
        std::uint32_t step = 0;
        step < 1000 && state.postWaveObject() != nullptr;
        ++step
    ) {
        const auto* target = state.postWaveObject();
        const bool moveForward = target->presenterDistance > 26000U;
        const bool moveReverse = target->presenterDistance < 22000U;
        auto input = steeringInput(
            state,
            *target,
            moveForward,
            moveReverse,
            true
        );
        if (
            !moveForward
            && !moveReverse
            && state.player().forwardVelocity != 0
        ) {
            input.left = false;
            input.right = false;
        }
        advance(input);
    }
    if (state.postWaveObject() != nullptr) {
        throw std::runtime_error(
            "natural success controller did not destroy Type 44"
        );
    }

    const auto findPickup = [&state]() -> const stellar7::Level1Entity* {
        for (const auto& candidate : state.entities()) {
            if (candidate.active() && candidate.type == 25) {
                return &candidate;
            }
        }
        return nullptr;
    };
    if (findPickup() == nullptr) {
        throw std::runtime_error(
            "natural success controller did not spawn Type 25"
        );
    }
    checkpoints.emplace_back("pickup", state.loopTick());

    for (
        std::uint32_t step = 0;
        step < 1000 && !state.successState();
        ++step
    ) {
        const auto* pickup = findPickup();
        if (pickup == nullptr) {
            throw std::runtime_error(
                "natural success controller lost Type 25 before success"
            );
        }
        advance(signedApproachInput(state, *pickup));
    }
    if (!state.successState() || state.missionActive()) {
        throw std::runtime_error(
            "natural success controller did not reach terminal success"
        );
    }
    const auto terminalTick = static_cast<std::uint32_t>(state.loopTick());
    checkpoints.emplace_back("success", terminalTick);
    // Success is entered by the deferred simulation step after the final
    // observable main-view boundary.  Releasing on `terminalTick` would be
    // coalesced with the collision-causing input and cancel it during file
    // replay.  Keep the release as script-valid unreachable cleanup at the
    // following logical value; a successful mission emits no such boundary.
    recordInputTransition(terminalTick + 1U, held, {}, events);

    writeText(
        schedulePath,
        level1SuccessInputScript(terminalTick, events, checkpoints)
    );
    writeText(
        tracePath,
        stellar7::level1StateTraceJson(
            state,
            "level1-success-controller-terminal"
        )
    );
}

stellar7::IndexedScreen loadExternalPaletteScreen(
    const std::string& resourcePath,
    const std::string& palettePath
) {
    return stellar7::applyVgaDacPalette(
        stellar7::decodeFirstIndexedScreenResource(readFile(resourcePath)),
        readFile(palettePath)
    );
}


stellar7::GameplayRunOptions nativeGameplayOptions(const std::filesystem::path& dataDirectory,
    const stellar7::HighscoreWritePolicy policy=stellar7::HighscoreWritePolicy::recovered) {
    stellar7::GameplayRunOptions options;
    options.highscoreStore = std::make_shared<stellar7::FileHighscoreStore>(dataDirectory / stellar7::highscoreFileName,policy);
    return options;
}

void traceSoundBlasterKeyOffWork() {
    constexpr std::size_t maximumCases = 1024;
    constexpr std::size_t maximumEvents = 32;
    constexpr std::array<const char*, 4> kinds{"plain", "frequency_low", "frequency_high", "carrier_level"};
    const auto readBounded = [](const std::uint64_t maximum) {
        std::uint64_t value = 0;
        if (!(std::cin >> value) || value > maximum) {
            throw std::runtime_error("invalid bounded key-off work input");
        }
        return value;
    };
    const auto readByte = [&readBounded]() {
        return static_cast<std::uint8_t>(readBounded(std::numeric_limits<std::uint8_t>::max()));
    };
    const auto count = readBounded(maximumCases);
    if (count == 0) {
        throw std::runtime_error("empty key-off work batch");
    }
    std::cout << '[';
    for (std::size_t index = 0; index < count; ++index) {
        const auto suppressed = readBounded(1);
        const auto latched = readBounded(1);
        stellar7::SoundBlasterKeyOffState state{};
        for (auto& timer : state.directTimers) {
            timer = static_cast<std::uint16_t>(readBounded(std::numeric_limits<std::uint16_t>::max()));
        }
        for (auto* group : {&state.channels, &state.notes, &state.tuning}) {
            for (auto& value : *group) {
                value = readByte();
            }
        }
        for (auto& value : state.channelDeferrals) {
            value = readByte();
        }
        const auto eventCount = readBounded(maximumEvents);
        std::vector<stellar7::SoundBlasterExplicitKeyOff> events;
        for (std::size_t eventIndex = 0; eventIndex < eventCount; ++eventIndex) {
            const auto channel = readByte();
            const auto note = readByte();
            const auto delay = readByte();
            events.push_back({channel, note, delay});
        }
        const auto work = stellar7::soundBlasterExplicitKeyOffWork(state, events,
            suppressed ? stellar7::SoundBlasterFrequencyGate::Suppressed : stellar7::SoundBlasterFrequencyGate::Open,
            latched ? stellar7::SoundBlasterLevelLatch::Latched : stellar7::SoundBlasterLevelLatch::Unlatched);
        std::cout << (index == 0 ? "{" : ",{") << "\"dispatches\":" << work.dispatches
                  << ",\"port_reads\":" << work.portReads << ",\"port_writes\":" << work.portWrites
                  << ",\"returned_wait\":" << work.returnedWait << ",\"consumed_bytes\":" << work.consumedBytes
                  << ",\"latched\":" << (latched ? "true" : "false")
                  << ",\"timer_dispatches\":" << work.timerWork.dispatches
                  << ",\"direct_timers\":[" << work.timerWork.timers[0] << ',' << work.timerWork.timers[1] << ',' << work.timerWork.timers[2]
                  << "],\"events\":[";
        for (std::size_t eventIndex = 0; eventIndex < work.events.size(); ++eventIndex) {
            const auto& event = work.events[eventIndex];
            std::cout << (eventIndex == 0 ? "{" : ",{") << "\"selected_voice\":";
            if (event.selectedVoice < 0) {
                std::cout << "null";
            } else {
                std::cout << event.selectedVoice;
            }
            std::cout << ",\"parser_dispatches\":" << event.parserDispatches
                      << ",\"handler_dispatches\":" << event.handlerDispatches
                      << ",\"scan_dispatches\":" << event.scanDispatches
                      << ",\"delta_dispatches\":" << event.deltaDispatches
                      << ",\"event_start_dispatches\":" << event.eventStartDispatches
                      << ",\"frequency_start_dispatches\":" << event.frequencyStartDispatches << '}';
        }
        std::cout << "],\"blocks\":[";
        for (std::size_t blockIndex = 0; blockIndex < work.blocks.size(); ++blockIndex) {
            const auto& block = work.blocks[blockIndex];
            std::cout << (blockIndex == 0 ? "{" : ",{") << "\"kind\":\"" << kinds[static_cast<std::size_t>(block.kind)]
                      << "\",\"dispatches\":" << block.dispatches << ",\"voice\":" << block.voice << '}';
        }
        std::cout << "]}";
    }
    std::cin >> std::ws;
    if (!std::cin.eof()) {
        throw std::runtime_error("unexpected trailing key-off work input");
    }
    std::cout << "]\n";
}

void traceSoundBlasterNoteOnWork() {
    constexpr std::size_t maximumCases = 1024;
    constexpr std::size_t maximumEvents = 32;
    constexpr std::array<const char*, 4> kinds{"plain", "frequency_low", "frequency_high", "carrier_level"};
    const auto readBounded = [](const std::uint64_t maximum) {
        std::uint64_t value = 0;
        if (!(std::cin >> value) || value > maximum) {
            throw std::runtime_error("invalid bounded note-on work input");
        }
        return value;
    };
    const auto readArray = [&readBounded](auto& values) {
        for (auto& value : values) {
            using Value = std::decay_t<decltype(value)>;
            value = static_cast<Value>(readBounded(std::numeric_limits<Value>::max()));
        }
    };
    const auto count = readBounded(maximumCases);
    if (count == 0) {
        throw std::runtime_error("empty note-on work batch");
    }
    std::cout << '[';
    for (std::size_t index = 0; index < count; ++index) {
        const auto suppressed = readBounded(1);
        const auto latched = readBounded(1);
        stellar7::SoundBlasterNoteOnState state{};
        readArray(state.directTimers);
        readArray(state.channels);
        readArray(state.sounding);
        readArray(state.tuning);
        readArray(state.channelEnabled);
        readArray(state.channelVoiceCursors);
        const auto eventCount = readBounded(maximumEvents);
        std::vector<stellar7::SoundBlasterExplicitNoteOn> events;
        for (std::size_t eventIndex = 0; eventIndex < eventCount; ++eventIndex) {
            std::array<std::uint8_t, 4> event{};
            readArray(event);
            events.push_back({event[0], event[1], event[2], event[3]});
        }
        const auto work = stellar7::soundBlasterExplicitNoteOnWork(state, events,
            suppressed ? stellar7::SoundBlasterFrequencyGate::Suppressed : stellar7::SoundBlasterFrequencyGate::Open,
            latched ? stellar7::SoundBlasterLevelLatch::Latched : stellar7::SoundBlasterLevelLatch::Unlatched);
        std::cout << (index == 0 ? "{" : ",{") << "\"dispatches\":" << work.dispatches
                  << ",\"port_reads\":" << work.portReads << ",\"port_writes\":" << work.portWrites
                  << ",\"returned_wait\":" << work.returnedWait << ",\"consumed_bytes\":" << work.consumedBytes
                  << ",\"direct_timers\":[" << work.timerWork.timers[0] << ',' << work.timerWork.timers[1] << ',' << work.timerWork.timers[2]
                  << "],\"channel_voice_cursors\":[";
        for (std::size_t channel = 0; channel < work.channelVoiceCursors.size(); ++channel) {
            std::cout << (channel == 0 ? "" : ",") << static_cast<unsigned>(work.channelVoiceCursors[channel]);
        }
        std::cout << "],\"events\":[";
        for (std::size_t eventIndex = 0; eventIndex < work.events.size(); ++eventIndex) {
            const auto& event = work.events[eventIndex];
            std::cout << (eventIndex == 0 ? "{" : ",{") << "\"selected_voice\":";
            if (event.selectedVoice < 0) {
                std::cout << "null";
            } else {
                std::cout << event.selectedVoice;
            }
            std::cout << ",\"event_start_dispatches\":" << event.eventStartDispatches
                      << ",\"frequency_start_dispatches\":" << event.frequencyStartDispatches << '}';
        }
        std::cout << "],\"blocks\":[";
        for (std::size_t blockIndex = 0; blockIndex < work.blocks.size(); ++blockIndex) {
            const auto& block = work.blocks[blockIndex];
            std::cout << (blockIndex == 0 ? "{" : ",{") << "\"kind\":\"" << kinds[static_cast<std::size_t>(block.kind)]
                      << "\",\"dispatches\":" << block.dispatches << ",\"voice\":" << block.voice << '}';
        }
        std::cout << "]}";
    }
    std::cin >> std::ws;
    if (!std::cin.eof()) {
        throw std::runtime_error("unexpected trailing note-on work input");
    }
    std::cout << "]\n";
}

CockpitWorkInput readCockpitWorkInput() {
    constexpr unsigned maximumBitmapWidth = 320;
    constexpr unsigned maximumBitmapHeight = 200;
    stellar7::CockpitWorkState state{};
    int pages = 0;
    if (!(std::cin >> state.indicatorX >> state.indicatorVelocity >> state.stage >> pages)
        || pages < 0 || pages > 1) {
        throw std::runtime_error("invalid cockpit control input");
    }
    state.pages = static_cast<stellar7::CockpitPageRelation>(pages);
    const auto readBitmap = [maximumBitmapWidth, maximumBitmapHeight]() {
        unsigned width = 0, height = 0;
        if (!(std::cin >> width >> height) || width == 0 || width > maximumBitmapWidth
            || height == 0 || height > maximumBitmapHeight) {
            throw std::runtime_error("invalid cockpit bitmap dimensions");
        }
        stellar7::IndexedBitmap bitmap{static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height), {}};
        bitmap.pixels.resize(static_cast<std::size_t>(width) * height);
        for (auto& pixel : bitmap.pixels) {
            unsigned value = 0;
            if (!(std::cin >> value) || value > std::numeric_limits<std::uint8_t>::max()) {
                throw std::runtime_error("invalid cockpit bitmap pixel");
            }
            pixel = static_cast<std::uint8_t>(value);
        }
        return bitmap;
    };
    stellar7::CockpitWorkBitmaps bitmaps{};
    bitmaps.strip = readBitmap();
    bitmaps.indicator = readBitmap();
    for (auto& bitmap : bitmaps.overlays) {
        bitmap = readBitmap();
    }
    return {state, std::move(bitmaps)};
}

void traceCockpitOperations() {
    const auto input = readCockpitWorkInput();
    std::cin >> std::ws;
    if (!std::cin.eof()) {
        throw std::runtime_error("unexpected trailing cockpit input");
    }
    const auto work = stellar7::cockpitOperations(input.state, input.bitmaps);
    std::cout << "{\"indicator_x\":" << work.state.indicatorX
              << ",\"indicator_velocity\":" << work.state.indicatorVelocity << ",\"operations\":[";
    for (std::size_t index = 0; index < work.operations.size(); ++index) {
        const auto& operation = work.operations[index];
        std::cout << (index == 0 ? "[" : ",[") << static_cast<int>(operation.kind) << ',' << operation.count << ']';
    }
    std::cout << "]}\n";
}

stellar7::HorizonWorkState readHorizonWorkInput() {
    stellar7::HorizonWorkState state{};
    int background = 0, overlay = 0;
    if (!(std::cin >> state.viewTop >> state.viewBottom >> state.cameraYaw >> state.stage
        >> state.bitmapWidth >> state.bitmapHeight >> background >> overlay)
        || background < 0 || background > 1 || overlay < 0 || overlay > 1) {
        throw std::runtime_error("invalid horizon work input");
    }
    state.background = static_cast<stellar7::HorizonBackground>(background);
    state.overlay = static_cast<stellar7::HorizonOverlay>(overlay);
    return state;
}

void traceHorizonOperations() {
    const auto state = readHorizonWorkInput();
    std::cin >> std::ws;
    if (!std::cin.eof()) {
        throw std::runtime_error("unexpected trailing horizon work input");
    }
    const auto operations = stellar7::horizonOperations(state);
    std::cout << '[';
    for (std::size_t index = 0; index < operations.size(); ++index) {
        std::cout << (index == 0 ? "[" : ",[") << static_cast<int>(operations[index].kind) << ',' << operations[index].count << ']';
    }
    std::cout << "]\n";
}

enum class GroundTrace { operations, execution };

void traceGroundOperations(const GroundTrace trace = GroundTrace::operations) {
    stellar7::GroundControlState state{};
    stellar7::Viewport viewport{};
    int clipping = 0;
    if (!(std::cin >> state.turn >> state.speed >> state.cameraHeight >> clipping) || clipping != 1) {
        throw std::runtime_error("ground work requires stationary clipped input");
    }
    for (auto& point : state.points) {
        if (!(std::cin >> point.x >> point.y)) {
            throw std::runtime_error("invalid ground point");
        }
    }
    if (!(std::cin >> viewport.left >> viewport.right >> viewport.top >> viewport.bottom)) {
        throw std::runtime_error("invalid ground viewport");
    }
    std::array<stellar7::IndexedBitmap, 8> bitmaps;
    for (auto& bitmap : bitmaps) {
        if (!(std::cin >> bitmap.width >> bitmap.height)
            || bitmap.width == 0 || bitmap.width > 320 || bitmap.height == 0 || bitmap.height > 200) {
            throw std::runtime_error("ground bitmap dimensions outside screen");
        }
        bitmap.pixels.resize(static_cast<std::size_t>(bitmap.width) * bitmap.height);
        for (auto& pixel : bitmap.pixels) {
            unsigned value = 0;
            if (!(std::cin >> value) || value > std::numeric_limits<std::uint8_t>::max()) {
                throw std::runtime_error("ground bitmap requires unsigned8 pixels");
            }
            pixel = static_cast<std::uint8_t>(value);
        }
    }
    std::optional<stellar7::StellarRng> rng;
    if (trace == GroundTrace::execution) {
        std::uint16_t indexA = 0, indexB = 0, last = 0;
        std::array<std::uint16_t, stellar7::StellarRng::wordCount> table{};
        if (!(std::cin >> indexA >> indexB >> last)
            || indexA >= table.size() * 2 || indexB >= table.size() * 2
            || (indexA & 1) != 0 || (indexB & 1) != 0) {
            throw std::runtime_error("invalid ground RNG indices");
        }
        for (auto& word : table) {
            if (!(std::cin >> word)) {
                throw std::runtime_error("invalid ground RNG table");
            }
        }
        rng.emplace(table, indexA, indexB, last, 0);
    }
    std::cin >> std::ws;
    if (!std::cin.eof()) {
        throw std::runtime_error("unexpected trailing ground input");
    }
    std::optional<stellar7::GroundExecutionWork> execution;
    if (rng.has_value()) {
        execution = stellar7::groundClippedExecution(state, *rng, bitmaps, viewport);
        std::cout << "{\"operations\":";
    }
    const auto operations = execution.has_value() ? execution->operations
        : stellar7::groundClippedOperations(state, bitmaps, viewport);
    std::cout << '[';
    for (std::size_t index = 0; index < operations.size(); ++index) {
        std::cout << (index == 0 ? "[" : ",[") << static_cast<int>(operations[index].kind)
                  << ',' << operations[index].count << ']';
    }
    std::cout << ']';
    if (execution.has_value()) {
        std::cout << ",\"points\":[";
        const auto& points = execution->ground.points();
        for (std::size_t index = 0; index < points.size(); ++index) {
            std::cout << (index == 0 ? "[" : ",[") << points[index].x << ',' << points[index].y << ']';
        }
        std::cout << "],\"visible\":[";
        for (std::size_t index = 0; index < points.size(); ++index) {
            std::cout << (index == 0 ? "" : ",") << (execution->ground.pointVisible(index) ? "true" : "false");
        }
        std::cout << "],\"rng\":{\"index_a\":" << execution->rng.indexA()
                  << ",\"index_b\":" << execution->rng.indexB() << ",\"last\":" << execution->rng.last()
                  << ",\"calls\":" << execution->rng.calls() << "},\"rng_future\":[";
        auto future = execution->rng;
        for (std::size_t index = 0; index < stellar7::StellarRng::wordCount; ++index) {
            std::cout << (index == 0 ? "" : ",") << future.next();
        }
        std::cout << "]}";
    }
    std::cout << '\n';
}

stellar7::SoundBlasterCallbackState readSoundBlasterCallbackState() {
    constexpr std::size_t maximumSequenceBytes = 65536;
    const auto readBounded = [](const std::uint64_t maximum) {
        std::uint64_t value = 0;
        if (!(std::cin >> value) || value > maximum) {
            throw std::runtime_error("invalid callback advance input");
        }
        return value;
    };
    const auto readArray = [&readBounded](auto& values) {
        for (auto& value : values) {
            using Value = std::decay_t<decltype(value)>;
            value = static_cast<Value>(readBounded(std::numeric_limits<Value>::max()));
        }
    };
    stellar7::SoundBlasterCallbackState state{};
    state.mode = static_cast<std::uint8_t>(readBounded(255));
    state.operation = static_cast<std::uint8_t>(readBounded(255));
    state.fade = static_cast<std::uint16_t>(readBounded(65535));
    state.active = readBounded(1) != 0;
    state.gate = readBounded(1) ? stellar7::SoundBlasterFrequencyGate::Suppressed : stellar7::SoundBlasterFrequencyGate::Open;
    state.latch = readBounded(1) ? stellar7::SoundBlasterLevelLatch::Latched : stellar7::SoundBlasterLevelLatch::Unlatched;
    state.wait = static_cast<std::uint16_t>(readBounded(65535));
    state.position = static_cast<std::size_t>(readBounded(maximumSequenceBytes));
    state.runningStatus = static_cast<std::uint8_t>(readBounded(255));
    const auto size = readBounded(maximumSequenceBytes);
    readArray(state.voices.directTimers);
    readArray(state.voices.channels);
    readArray(state.voices.notes);
    readArray(state.velocities);
    readArray(state.sounding);
    readArray(state.voices.tuning);
    readArray(state.voices.channelDeferrals);
    readArray(state.channelEnabled);
    readArray(state.channelVoiceCursors);
    state.sequence.resize(static_cast<std::size_t>(size));
    readArray(state.sequence);
    return state;
}

template <typename Value, std::size_t Size>
void printAudioValues(const std::array<Value, Size>& values, std::ostream& output = std::cout) {
    output << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        output << (index == 0 ? "" : ",") << static_cast<unsigned>(values[index]);
    }
    output << ']';
}

void printSoundBlasterCallbackState(const stellar7::SoundBlasterCallbackState& state,
    std::ostream& output, const SoundBlasterStateDetail detail) {
    output << "{\"position\":" << state.position << ",\"wait\":" << state.wait
              << ",\"running_status\":" << static_cast<unsigned>(state.runningStatus)
              << ",\"latched\":" << (state.latch == stellar7::SoundBlasterLevelLatch::Latched)
              << ",\"direct_timers\":";
    printAudioValues(state.voices.directTimers, output);
    output << ",\"notes\":";
    printAudioValues(state.voices.notes, output);
    output << ",\"velocities\":";
    printAudioValues(state.velocities, output);
    output << ",\"sounding\":";
    printAudioValues(state.sounding, output);
    output << ",\"cursors\":";
    printAudioValues(state.channelVoiceCursors, output);
    if (detail == SoundBlasterStateDetail::boundedWork) {
        output << ",\"mode\":" << static_cast<unsigned>(state.mode)
               << ",\"operation\":" << static_cast<unsigned>(state.operation)
               << ",\"fade\":" << state.fade << ",\"active\":" << state.active
               << ",\"suppressed\":" << (state.gate == stellar7::SoundBlasterFrequencyGate::Suppressed)
               << ",\"channels\":";
        printAudioValues(state.voices.channels, output);
        output << ",\"tuning\":";
        printAudioValues(state.voices.tuning, output);
        output << ",\"deferrals\":";
        printAudioValues(state.voices.channelDeferrals, output);
        output << ",\"enabled\":";
        printAudioValues(state.channelEnabled, output);
        output << ",\"sequence\":[";
        for (std::size_t index = 0; index < state.sequence.size(); ++index) {
            output << (index == 0 ? "" : ",") << static_cast<unsigned>(state.sequence[index]);
        }
        output << ']';
    }
    output << '}';
}

void traceSoundBlasterCallbackAdvance() {
    constexpr std::size_t maximumCases = 1024;
    std::size_t count = 0;
    if (!(std::cin >> count) || count == 0 || count > maximumCases) {
        throw std::runtime_error("invalid callback advance case count");
    }
    std::cout << '[';
    for (std::size_t index = 0; index < count; ++index) {
        const auto result = stellar7::soundBlasterCallbackAdvance(readSoundBlasterCallbackState());
        std::cout << (index == 0 ? "{" : ",{") << "\"kind\":" << static_cast<int>(result.kind)
                  << ",\"dispatches\":" << result.dispatches << ",\"state\":";
        printSoundBlasterCallbackState(result.state);
        std::cout << ",\"blocks\":[";
        for (std::size_t block = 0; block < result.blocks.size(); ++block) {
            const auto& value = result.blocks[block];
            std::cout << (block == 0 ? "[" : ",[") << static_cast<int>(value.kind) << ',' << value.dispatches << ',' << value.voice << ']';
        }
        std::cout << "]}";
    }
    std::cin >> std::ws;
    if (!std::cin.eof()) {
        throw std::runtime_error("unexpected trailing callback advance input");
    }
    std::cout << "]\n";
}

stellar7::SoundBlasterClockPhase readSoundBlasterClockPhase() {
    constexpr std::size_t maximumEvents = 8192;
    stellar7::SoundBlasterClockPhase phase{};
    int masked = 0, pending = 0, activation = 0, frameStart = 0;
    std::size_t eventCount = 0;
    if (!(std::cin >> phase.remainingCycles >> phase.deferredCycles >> masked
        >> phase.isa8DelayNanoseconds >> phase.dmaDacRate >> phase.pitPeriodMilliseconds
        >> phase.irqDelayMilliseconds >> pending >> activation >> phase.singleLinePeriodMilliseconds
        >> phase.horizontalPeriodMilliseconds >> phase.linesDone >> phase.linesTotal >> phase.verticalBlankSkip
        >> phase.minimumLatchDelayMilliseconds >> frameStart >> eventCount)
        || masked < 0 || masked > 1 || pending < 0 || pending > 1 || activation < 0 || activation > 1
        || frameStart < 0 || frameStart > 1 || eventCount > maximumEvents) {
        throw std::runtime_error("invalid OPL phase input");
    }
    phase.interrupts = masked ? stellar7::AudioClockInterrupts::masked : stellar7::AudioClockInterrupts::enabled;
    phase.irqPending = pending != 0;
    phase.irq0ActivationPossible = activation != 0;
    phase.frameStart = frameStart ? stellar7::AudioClockFrameStart::activeUnlockedVgaSurface : stellar7::AudioClockFrameStart::unsupported;
    for (std::size_t eventIndex = 0; eventIndex < eventCount; ++eventIndex) {
        stellar7::AudioClockEvent event{};
        int kind = 0;
        if (!(std::cin >> event.deadlineMilliseconds >> kind >> event.value)
            || kind < 0 || kind > static_cast<int>(stellar7::AudioClockEventKind::pendingCmos)) {
            throw std::runtime_error("invalid OPL phase event");
        }
        event.kind = static_cast<stellar7::AudioClockEventKind>(kind);
        phase.events.push_back(event);
    }
    return phase;
}

void printSoundBlasterClockStep(const stellar7::SoundBlasterClockStep& step, std::ostream& output = std::cout) {
    constexpr double cyclesPerMillisecond = 5000;
    output << "{\"dispatches\":" << step.dispatches
              << ",\"delay_cycles\":" << step.delayCycles
              << ",\"terminal_charges\":" << step.terminalCharges
              << ",\"elapsed_cycles\":" << step.elapsedCycles
              << ",\"remaining_cycles\":" << step.remainingCycles
              << ",\"deferred_cycles\":" << step.deferredCycles
              << ",\"lines_done\":" << step.linesDone
              << ",\"rollovers\":" << step.millisecondRollovers
              << ",\"bounded_queue\":[";
    for (std::size_t eventIndex = 0; eventIndex < step.boundedEvents.size(); ++eventIndex) {
        const auto& event = step.boundedEvents[eventIndex];
        const auto cycles = event.deadlineMilliseconds * cyclesPerMillisecond;
        output << (eventIndex == 0 ? "[" : ",[") << static_cast<int>(event.kind) << ',' << event.value
                  << ',' << std::floor(cycles) << ',' << std::ceil(cycles) << ']';
    }
    output << "]}";
}

void traceSoundBlasterOplPhase() {
    constexpr std::size_t maximumCases = 1024;
    std::size_t count = 0;
    if (!(std::cin >> count) || count == 0 || count > maximumCases) {
        throw std::runtime_error("invalid OPL phase case count");
    }
    std::cout << '[';
    for (std::size_t index = 0; index < count; ++index) {
        const auto phase = readSoundBlasterClockPhase();
        const auto steps = stellar7::soundBlasterOplTiming(phase);
        std::cout << (index == 0 ? "[" : ",[");
        for (std::size_t part = 0; part < steps.size(); ++part) {
            if (part != 0) {
                std::cout << ',';
            }
            printSoundBlasterClockStep(steps[part]);
        }
        std::cout << ']';
    }
    std::cin >> std::ws;
    if (!std::cin.eof()) {
        throw std::runtime_error("unexpected trailing OPL phase input");
    }
    std::cout << "]\n";
}

enum class NormalWorkPhaseInput { bounded, retainedDevices, retainedTimer, retainedCallbacks, retainedFrameCallbacks, retainedOwnedFrame };

stellar7::SoundBlasterClockPhase readRetainedNormalPhase(
    const stellar7::AudioClockVgaInterrupt vgaInterrupt, const NormalWorkPhaseInput input) {
    auto phase = readSoundBlasterClockPhase();
    phase.vgaInterrupt = vgaInterrupt;
    if (input != NormalWorkPhaseInput::bounded) {
        stellar7::AudioClockDevicePhase devices{};
        int fixed = 0, triggered = 0;
        if (!(std::cin >> devices.picTicks >> devices.pitStartMilliseconds >> devices.pitNowMilliseconds
            >> devices.pitCycleBase >> fixed >> devices.framePeriodMilliseconds >> devices.frameStartMilliseconds
            >> devices.displayEndMilliseconds >> devices.retraceStartMilliseconds >> devices.retraceEndMilliseconds
            >> devices.displayStart >> devices.realStart >> devices.memoryMask >> devices.configuredBytesSkip
            >> devices.drawnBytesSkip >> devices.pelPanning >> devices.drawnPanning
            >> devices.verticalRetraceEndRegister >> triggered)
            || fixed != 1 || triggered < 0 || triggered > 1) {
            throw std::runtime_error("invalid retained device phase input");
        }
        devices.fixedPitAndMaskedDma = true;
        devices.verticalRetraceTriggered = triggered != 0;
        phase.devices = devices;
    }
    if (input == NormalWorkPhaseInput::retainedFrameCallbacks || input == NormalWorkPhaseInput::retainedOwnedFrame) {
        stellar7::AudioClockVgaFramePhase frame{};
        int manual = 0;
        if (!(std::cin >> frame.framesPerSecond >> frame.timeBaseMilliseconds >> frame.framesSinceTimeBase >> manual)
            || manual != 0) {
            throw std::runtime_error("unsupported retained VGA frame input");
        }
        phase.devices->frame = frame;
    }
    if (input == NormalWorkPhaseInput::retainedTimer || input == NormalWorkPhaseInput::retainedCallbacks
        || input == NormalWorkPhaseInput::retainedFrameCallbacks || input == NormalWorkPhaseInput::retainedOwnedFrame) {
        stellar7::AudioClockTimerPhase timer{};
        int requested = 0, inService = 0;
        if (!(std::cin >> requested >> inService >> timer.activeMask >> timer.globalCount
            >> timer.biosCount >> timer.callbackCount >> timer.callbackReload)
            || requested < 0 || requested > 1 || inService < 0 || inService > 1) {
            throw std::runtime_error("invalid retained timer phase input");
        }
        phase.devices->irq0Requested = requested != 0;
        phase.devices->irq0InService = inService != 0;
        phase.timer = timer;
    }
    if (input == NormalWorkPhaseInput::retainedCallbacks || input == NormalWorkPhaseInput::retainedFrameCallbacks) {
        phase.callback = readSoundBlasterCallbackState();
    }
    return phase;
}

void readBiosClockState(stellar7::SoundBlasterClockPhase& phase) {
    std::array<std::uint64_t, 11> fields{};
    for (auto& field : fields) {
        if (!(std::cin >> field)) {
            throw std::runtime_error("incomplete BIOS clock state");
        }
    }
    constexpr std::array<std::uint64_t, 11> limits{
        1, std::numeric_limits<std::uint32_t>::max(), 255, 255, 255, 1, 1, 1, 255, 255, 65535};
    for (std::size_t field = 0; field < fields.size(); ++field) {
        if (fields[field] > limits[field]) {
            throw std::runtime_error("BIOS clock state out of range");
        }
    }
    phase.bios = stellar7::AudioClockBiosPhase{
        static_cast<stellar7::AudioClockBiosDomain>(fields[0]), static_cast<std::uint32_t>(fields[1]),
        static_cast<std::uint8_t>(fields[2]), static_cast<std::uint8_t>(fields[3]),
        static_cast<std::uint8_t>(fields[4]), fields[5] != 0, fields[6] != 0, fields[7] != 0,
        static_cast<std::uint8_t>(fields[8]), static_cast<std::uint8_t>(fields[9])};
    phase.timer->biosReload = static_cast<std::uint16_t>(fields[10]);
}

void readRtcClockState(stellar7::SoundBlasterClockPhase& phase) {
    constexpr std::array<std::uint64_t,12> limits{1,255,255,1,1,1,1,15,127,1,
        std::numeric_limits<std::uint64_t>::max(),std::numeric_limits<std::uint32_t>::max()};
    std::array<std::uint64_t,12> fields{};
    for (std::size_t index = 0; index < fields.size(); ++index) {
        std::string token;
        if (!(std::cin >> token) || token.empty()
            || !std::all_of(token.begin(),token.end(),[](const auto ch) { return ch >= '0' && ch <= '9'; })) {
            throw std::runtime_error("invalid RTC unsigned field");
        }
        fields[index] = std::stoull(token);
        if (fields[index] > limits[index]) {
            throw std::runtime_error("RTC state field out of range");
        }
    }
    stellar7::AudioClockRtcPhase rtc{};
    rtc.domain = static_cast<stellar7::AudioClockRtcDomain>(fields[0]);
    rtc.registerB = static_cast<std::uint8_t>(fields[1]);
    rtc.registerC = static_cast<std::uint8_t>(fields[2]);
    rtc.acknowledged = fields[3] != 0;
    rtc.locked = fields[4] != 0;
    rtc.bcd = fields[5] != 0;
    rtc.ampm = fields[6] != 0;
    rtc.divider = static_cast<std::uint8_t>(fields[7]);
    rtc.selected = static_cast<std::uint8_t>(fields[8]);
    rtc.nmi = fields[9] != 0;
    const auto delayBits = static_cast<std::uint32_t>(fields[11]);
    static_assert(sizeof(rtc.endedMilliseconds) == sizeof(fields[10]));
    static_assert(sizeof(rtc.delayMilliseconds) == sizeof(delayBits));
    std::memcpy(&rtc.endedMilliseconds,&fields[10],sizeof(rtc.endedMilliseconds));
    std::memcpy(&rtc.delayMilliseconds,&delayBits,sizeof(rtc.delayMilliseconds));
    for (auto& field : rtc.calendar) {
        std::int64_t value = 0;
        if (!(std::cin >> value) || value < 0 || value > std::numeric_limits<std::uint16_t>::max()) {
            throw std::runtime_error("invalid RTC calendar field");
        }
        field = static_cast<std::uint16_t>(value);
    }
    if (!(std::cin >> rtc.clockTime)) {
        throw std::runtime_error("invalid RTC calendar time");
    }
    phase.rtc = rtc;
}

ClockedRenderInput readClockedRenderInput(const ClockedRenderKind kind, const std::filesystem::path& dataDirectory,
    const ClockedRenderEnd end, const std::uint64_t frameCount) {
    ClockedRenderInput input{};
    input.kind = kind;
    input.end = end;
    if (frameCount < 2 || frameCount > std::numeric_limits<std::uint16_t>::max()) {
        throw std::invalid_argument("invalid retained frame count");
    }
    input.frameCount = static_cast<unsigned>(frameCount);
    if (end != ClockedRenderEnd::component) {
        if (kind != ClockedRenderKind::worldRadar) {
            throw std::invalid_argument("display continuation requires complete owned rendering");
        }
        const auto drivers = stellar7::decodeTaggedPayloads(readFile((dataDirectory / "STELLAR.RES").string()), {'V', 'G', 'A', ':'});
        if (drivers.empty()) {
            throw std::invalid_argument("display continuation requires original VGA driver");
        }
        input.copyProfile = stellar7::decodeFrameCopyProfile(drivers.front());
    }
    if (kind == ClockedRenderKind::worldCockpit || kind == ClockedRenderKind::worldRadar) {
        input.settings = stellar7::loadCockpitSettings(dataDirectory);
        if (kind == ClockedRenderKind::worldRadar
            && input.settings->options[levelIntroductionOption] != 0) {
            // Guard the projected scene-return font effect; scene execution remains unqualified.
            const auto scripts = stellar7::decodeTaggedPayloads(
                readFile((dataDirectory / "SCENEB2.RES").string()), {'T', 'T', '3', ':'});
            if (scripts.size() != 1 || scripts.front().empty()
                || scripts.front().size() > std::numeric_limits<std::uint16_t>::max()) {
                throw std::invalid_argument("unsupported Level2 introduction script");
            }
        }
    }
    if (kind != ClockedRenderKind::cockpit) {
        input.horizon = readHorizonWorkInput();
    } else {
        input.cockpit = readCockpitWorkInput();
    }
    auto phase = readRetainedNormalPhase(stellar7::AudioClockVgaInterrupt::fixedVgaNoDemand,
        NormalWorkPhaseInput::retainedOwnedFrame);
    int irqCheck = 0, delivery = 0;
    std::int64_t horizon = 0;
    if (!(std::cin >> irqCheck >> delivery >> horizon) || irqCheck < 0 || irqCheck > 1 || delivery < 0 || delivery > 2) {
        throw std::runtime_error("invalid owned rendering clock input");
    }
    phase.irqCheckRequested = irqCheck != 0;
    phase.interruptDelivery = static_cast<stellar7::AudioClockInterruptDelivery>(delivery);
    std::cin >> std::ws;
    if (!std::cin.eof()) {
        std::string version;
        if (!(std::cin >> version) || version != "bios-v1") {
            throw std::runtime_error("unsupported owned rendering BIOS input version");
        }
        readBiosClockState(phase);
        std::cin >> std::ws;
        if (!std::cin.eof()) {
            if (!(std::cin >> version) || version != "rtc-v1") {
                throw std::runtime_error("unsupported owned rendering RTC input version");
            }
            readRtcClockState(phase);
            std::cin >> std::ws;
            if (!std::cin.eof()) {
                int domain = 0;
                if (!(std::cin >> version >> domain) || version != "palette-v1" || domain != 1
                    || kind != ClockedRenderKind::worldRadar || end != ClockedRenderEnd::nextFrame) {
                    throw std::runtime_error("unsupported owned rendering palette input");
                }
                constexpr std::size_t gameplayPaletteIndex = 2;
                const auto palettes = stellar7::decodePalettes(
                    readFile((dataDirectory / "STELART.RES").string()));
                if (palettes.size() <= gameplayPaletteIndex
                    || palettes[gameplayPaletteIndex].size() != stellar7::vgaPaletteColors) {
                    throw std::runtime_error("missing source gameplay palette");
                }
                std::array<std::uint8_t, stellar7::vgaPaletteBytes> source{};
                std::size_t index = 0;
                for (const auto color : palettes[gameplayPaletteIndex]) {
                    for (const auto component : {color.red, color.green, color.blue}) {
                        source[index++] = collapseVgaDacComponent(component);
                    }
                }
                input.paletteSource = source;
                std::cin >> std::ws;
                if (!std::cin.eof()) {
                    int frontDomain = 0;
                    if (!(std::cin >> version >> frontDomain) || version != "front-v1"
                        || (frontDomain != static_cast<int>(IncomingFrontDomain::priorOwnedViewport)
                            && frontDomain != static_cast<int>(IncomingFrontDomain::beforeRaw5))) {
                        throw std::runtime_error("unsupported owned incoming-front input");
                    }
                    input.incomingFront = static_cast<IncomingFrontDomain>(frontDomain);
                    std::cin >> std::ws;
                    if (input.incomingFront == IncomingFrontDomain::beforeRaw5) {
                        int arrivalDomain = 0;
                        if (!(std::cin >> version >> arrivalDomain) || version != "arrival-v1" || arrivalDomain != 1) {
                            throw std::runtime_error("unsupported returned-render arrival input");
                        }
                        input.origin = ClockedRenderOrigin::priorReturnedRender;
                        std::cin >> std::ws;
                        if (!std::cin.eof()) {
                            int scanoutDomain = 0;
                            if (!(std::cin >> version >> scanoutDomain) || version != "scanout-v1" || scanoutDomain != 1) {
                                throw std::runtime_error("unsupported quiet scanout input");
                            }
                            input.scanout = ScanoutDomain::quietLevel2;
                            std::cin >> std::ws;
                            if (!std::cin.eof()) {
                                int keyboardDomain = 0;
                                if (!(std::cin >> version >> keyboardDomain) || version != "keyboard-v1"
                                    || keyboardDomain != 1) {
                                    throw std::runtime_error("unsupported physical keyboard input");
                                }
                                const auto readWord = []() {
                                    unsigned value = 0;
                                    if (!(std::cin >> value) || value > std::numeric_limits<std::uint16_t>::max()) {
                                        throw std::runtime_error("invalid physical keyboard word");
                                    }
                                    return static_cast<std::uint16_t>(value);
                                };
                                stellar7::KeyboardTransportState keyboard;
                                keyboard.domain = stellar7::KeyboardTransportDomain::normalVgaSet1Up;
                                keyboard.position = readWord();
                                const auto data = readWord(), control = readWord();
                                if (keyboard.position >= stellar7::keyboardBufferWords || data > 255 || control > 255) {
                                    throw std::runtime_error("invalid physical keyboard arrival ports");
                                }
                                keyboard.port60Data = static_cast<std::uint8_t>(data);
                                keyboard.port61Data = static_cast<std::uint8_t>(control);
                                for (auto& word : keyboard.buffer) { word = readWord(); }
                                for (auto& word : input.initialKeyboardSlots) { word = readWord(); }
                                phase.keyboard = keyboard;
                                std::cin >> std::ws;
                            }
                        }
                    }
                }
            }
        }
    }
    if (!std::cin.eof()) {
        throw std::runtime_error("unexpected trailing owned rendering clock input");
    }
    if (input.scanout == ScanoutDomain::quietLevel2) {
        constexpr std::uint32_t qualifiedMemoryMask = 0x1fffff;
        const auto admittedStart = [](const std::uint32_t start) { return start == 0 || start == 0x4000; };
        if (!phase.devices || !phase.devices->frame || phase.linesTotal != QuietScanoutCollector::height
            || phase.frameStart != stellar7::AudioClockFrameStart::activeUnlockedVgaSurface
            || phase.vgaInterrupt != stellar7::AudioClockVgaInterrupt::fixedVgaNoDemand
            || phase.devices->memoryMask != qualifiedMemoryMask || phase.devices->configuredBytesSkip != 0
            || phase.devices->drawnBytesSkip != 0 || phase.devices->pelPanning != 0 || phase.devices->drawnPanning != 0
            || !admittedStart(phase.devices->displayStart) || !admittedStart(phase.devices->realStart)) {
            throw std::runtime_error("unsupported quiet scanout addressing profile");
        }
    }
    input.phase = std::move(phase);
    input.horizonCycles = horizon;
    return input;
}

void printGroundState(std::ostream& output, const stellar7::Level1GroundState& state, stellar7::StellarRng rng) {
    output << "{\"points\":[";
    for (std::size_t index = 0; index < state.points().size(); ++index) {
        const auto& point = state.points()[index];
        output << (index == 0 ? "[" : ",[") << point.x << ',' << point.y << ']';
    }
    output << "],\"visible\":[";
    for (std::size_t index = 0; index < state.points().size(); ++index) {
        output << (index == 0 ? "" : ",") << (state.pointVisible(index) ? "true" : "false");
    }
    output << "],\"rng\":{\"index_a\":" << rng.indexA() << ",\"index_b\":" << rng.indexB()
           << ",\"last\":" << rng.last() << ",\"calls\":" << rng.calls() << "},\"rng_future\":[";
    for (std::size_t index = 0; index < stellar7::StellarRng::wordCount; ++index) {
        output << (index == 0 ? "" : ",") << rng.next();
    }
    output << "]}";
}

void printClockedContinuation(std::ostream& output, const stellar7::ClockedPresentationWork& work,
    const stellar7::SoundBlasterSynth& synth) {
    output << "{\"clock_phase\":";
    printBiosClockPhase(work.timing.phase, output);
    output << ",\"total\":";
    printSoundBlasterClockStep(work.timing.total, output);
    output << ",\"string_iterations\":" << work.timing.stringIterations
           << ",\"string_restarts\":" << work.timing.stringRestarts;
    output << ",\"owned_return\":";
    printSoundBlasterCallbackState(synth.callbackWorkState(), output, SoundBlasterStateDetail::boundedWork);
    output << ",\"callbacks\":[";
    for (std::size_t index = 0; index < work.timing.callbacks.size(); ++index) {
        const auto& callback = work.timing.callbacks[index];
        output << (index == 0 ? "[" : ",[") << callback.cycleOffset << ','
               << static_cast<int>(callback.advance.kind) << ',' << callback.advance.dispatches << ']';
    }
    output << "],\"opl_writes\":[";
    for (std::size_t index = 0; index < work.audio.writes.size(); ++index) {
        const auto& write = work.audio.writes[index];
        output << (index == 0 ? "[" : ",[") << write.cycleOffset << ','
               << static_cast<unsigned>(write.write.registerNumber) << ','
               << static_cast<unsigned>(write.write.value) << ']';
    }
    if (work.timing.phase.keyboard) {
        output << "],\"keyboard_irqs\":[";
        for (std::size_t index = 0; index < work.timing.keyboardIrqs.size(); ++index) {
            const auto& irq = work.timing.keyboardIrqs[index];
            output << (index == 0 ? "" : ",") << "{\"scan\":" << static_cast<unsigned>(irq.scan)
                << ",\"control_read\":" << static_cast<unsigned>(irq.controlRead) << ",\"entry\":";
            printSoundBlasterClockStep(irq.entry, output);
            output << ",\"entry_phase\":"; printBiosClockPhase(irq.entryPhase, output);
            output << ",\"pre_iret\":"; printSoundBlasterClockStep(irq.preIret, output);
            output << ",\"pre_iret_phase\":"; printBiosClockPhase(irq.preIretPhase, output);
            output << ",\"completed\":"; printSoundBlasterClockStep(irq.completed, output);
            output << ",\"completed_phase\":"; printBiosClockPhase(irq.completedPhase, output);
            output << '}';
        }
    }
    output << "],\"pcm\":[";
    for (std::size_t index = 0; index < work.audio.samples.size(); ++index) {
        output << (index == 0 ? "" : ",") << work.audio.samples[index];
    }
    output << "]}";
}

std::string writeClockedRenderTrace(stellar7::PresentationClock& clock,
    stellar7::SoundBlasterSynth& synth, const ClockedRenderInput& input,
    const stellar7::GameplayAssets& assets, const stellar7::GameplayModel& model,
    stellar7::Level2RenderState* retainedFrame, stellar7::CockpitPageState* pages,
    stellar7::RadarState* radar, stellar7::IndexedPages* pixels,
    std::optional<stellar7::SoundBlasterClockPhase>* returnedPhase,
    std::uint8_t* returnedRasterStyle, std::optional<std::uint8_t>* returnedEnemyFlag,
    const stellar7::PaletteWorkState* palette, QuietScanoutCollector* scanout,
    stellar7::RetainedFrameExecutor* executor, const ClockedRenderObservation* observation) {
    if (returnedPhase != nullptr) {
        returnedPhase->reset();
    }
    if (returnedEnemyFlag != nullptr) {
        returnedEnemyFlag->reset();
    }
    std::ostringstream owned;
    try {
        printSoundBlasterCallbackState(observation != nullptr ? observation->entry : synth.callbackWorkState(),
            owned, SoundBlasterStateDetail::boundedWork);
    } catch (const std::invalid_argument& error) {
        if (observation != nullptr) {
            throw;
        }
        std::ostringstream rejected;
        rejected << "{\"supported\":false,\"owned_entry\":null,\"error\":" << std::quoted(error.what()) << '}';
        return rejected.str();
    }
    try {
        if (input.end != ClockedRenderEnd::component && !input.phase.bios) {
            throw std::invalid_argument("display continuation requires retained BIOS context");
        }
        stellar7::ClockedCockpitWork cockpit;
        stellar7::ClockedPresentationWork rendered;
        std::optional<stellar7::GroundExecutionWork> ground;
        std::optional<stellar7::WorldViewSetupWork> worldSetup;
        std::optional<stellar7::WorldFirstCenterWork> worldCenter;
        std::optional<stellar7::WorldSceneWork> worldScene;
        std::optional<stellar7::SightWork> sight;
        std::optional<stellar7::CockpitWork> ownedCockpit;
        stellar7::CockpitWorkBitmaps cockpitBitmaps;
        std::optional<stellar7::CockpitPageState> returnedPages;
        std::optional<stellar7::RadarState> returnedRadar;
        std::optional<stellar7::RadarWork> radarWork;
        std::vector<stellar7::RadarStateActor> radarInputs;
        std::optional<stellar7::RadarPages> radarEntryPages;
        std::optional<stellar7::IndexedPages> returnedPixels;
        std::optional<stellar7::Level2FramePlan> framePlan;
        const auto incomingSightPhase = model.alternateSightPhase();
        std::array<stellar7::IndexedBitmap, 8> groundBitmaps;
        if (input.kind == ClockedRenderKind::worldRadar) {
            if (model.phase() != stellar7::GameplayPhase::level2 || retainedFrame == nullptr
                || !input.settings || !input.horizon || pages == nullptr || radar == nullptr || pixels == nullptr) {
                throw std::invalid_argument("complete Level2 frame requires retained settings and page owners");
            }
            framePlan = observation != nullptr ? observation->plan
                : stellar7::planLevel2Frame(assets, *retainedFrame, model.level2LoopTick(), model.playerArmor(),
                    *input.horizon, *input.settings, *pages, *radar, *pixels);
            if (scanout != nullptr && framePlan->radar.pageCopy) {
                throw std::invalid_argument("quiet scanout excludes front armor copy");
            }
            ground = framePlan->ground;
            worldSetup = framePlan->world;
            worldScene = framePlan->scene;
            sight = framePlan->sight;
            ownedCockpit = framePlan->cockpit;
            radarWork = framePlan->radar;
            groundBitmaps = framePlan->groundBitmaps;
            cockpitBitmaps = framePlan->cockpitBitmaps;
            radarInputs = framePlan->radarActors;
            radarEntryPages = framePlan->radarEntryPages;
            returnedPages = framePlan->pages;
            returnedRadar = framePlan->radarState;
            returnedPixels = framePlan->pixels;
            rendered = observation != nullptr ? observation->body
                : executor != nullptr ? executor->render(*framePlan)
                : clock.renderWork(input.phase, framePlan->operations, input.horizonCycles);
        } else if (input.kind == ClockedRenderKind::horizonGround || input.kind == ClockedRenderKind::worldSetup
            || input.kind == ClockedRenderKind::worldTraversal || input.kind == ClockedRenderKind::worldScene || input.kind == ClockedRenderKind::worldSight || (input.kind == ClockedRenderKind::worldCockpit)) {
            constexpr std::size_t groundGroup = 4;
            constexpr std::size_t firstGroundBitmap = 15;
            if (model.phase() != stellar7::GameplayPhase::level2 || assets.artGroups.size() <= groundGroup
                || assets.artGroups[groundGroup].size() < firstGroundBitmap + groundBitmaps.size()) {
                throw std::invalid_argument("owned horizon/ground requires active Level2 and original ground bitmaps");
            }
            for (std::size_t index = 0; index < groundBitmaps.size(); ++index) {
                groundBitmaps[index] = assets.artGroups[groundGroup][firstGroundBitmap + index];
            }
            const auto& player = model.level2Player();
            ground = stellar7::groundClippedExecution({model.level2Ground().points(), player.yawVelocity,
                player.forwardVelocity, player.position.z}, model.level2Rng(), groundBitmaps, {24, 295, 24, 145});
            auto operations = stellar7::horizonOperations(*input.horizon);
            auto measuredGround = ground->operations;
            if (input.kind == ClockedRenderKind::horizonGround) {
                // Observe84fa before ADD SP; the complete ground plan continues to84fd.
                constexpr std::uint64_t callerCleanupDispatches = 1;
                if (measuredGround.empty() || measuredGround.back().kind != stellar7::NormalWorkKind::instructions
                    || measuredGround.back().count <= callerCleanupDispatches) {
                    throw std::logic_error("ground plan lacks caller cleanup boundary");
                }
                measuredGround.back().count -= callerCleanupDispatches;
            }
            operations.insert(operations.end(), measuredGround.begin(), measuredGround.end());
            if (input.kind == ClockedRenderKind::worldSetup || input.kind == ClockedRenderKind::worldTraversal || input.kind == ClockedRenderKind::worldScene || input.kind == ClockedRenderKind::worldSight || (input.kind == ClockedRenderKind::worldCockpit)) {
                worldSetup = stellar7::worldViewSetupOperations({player.position, {0, 0, 0},
                    {player.pitch, player.roll, player.yaw}, 0x88, 7, {24, 294, 24, 145}, {256, -256, 362}});
                //84fd -> b7ac: remaining caller pushes/CALL and world prologue.
                constexpr std::uint64_t worldSetupCallerDispatches = 13;
                operations.push_back({stellar7::NormalWorkKind::instructions, worldSetupCallerDispatches});
                operations.insert(operations.end(), worldSetup->operations.begin(), worldSetup->operations.end());
            }
            if (input.kind == ClockedRenderKind::worldTraversal) {
                if (retainedFrame == nullptr || retainedFrame->order.size() != 7) {
                    throw std::invalid_argument("world traversal requires seven retained actors");
                }
                const auto& actor = retainedFrame->entities.at(retainedFrame->order.front());
                worldCenter = stellar7::worldFirstCenterOperations(worldSetup->view, {actor.flags, actor.position});
                operations.insert(operations.end(), worldCenter->operations.begin(), worldCenter->operations.end());
            }
            if (input.kind == ClockedRenderKind::worldScene || input.kind == ClockedRenderKind::worldSight || (input.kind == ClockedRenderKind::worldCockpit)) {
                if (retainedFrame == nullptr || retainedFrame->order.size() != 7) {
                    throw std::invalid_argument("world scene requires seven retained actors");
                }
                std::vector<stellar7::WorldSceneActor> actors;
                for (const auto index : retainedFrame->order) {
                    const auto& actor = retainedFrame->entities.at(index);
                    if (actor.renderProfile != stellar7::WorldActorRenderProfile::noHandles) {
                        throw std::invalid_argument("world scene requires owned no-handle actor provenance");
                    }
                    actors.push_back({{actor.flags, actor.position}, {actor.pitch, actor.roll, actor.yaw},
                        actor.type, actor.collisionRadius, 0, 0});
                }
                worldScene = stellar7::worldSceneOperations(assets, worldSetup->view, actors,
                    {assets.sharedModelRegistry[0], assets.sharedModelRegistry[1], 0, 0});
                operations.insert(operations.end(), worldScene->operations.begin(), worldScene->operations.end());
            }
            if (input.kind == ClockedRenderKind::worldSight || (input.kind == ClockedRenderKind::worldCockpit)) {
                constexpr std::uint8_t sightColor = 95;
                stellar7::SightRefreshInput refresh{{}, retainedFrame->player.yaw};
                for (const auto index : retainedFrame->order) {
                    const auto& actor = retainedFrame->entities.at(index);
                    refresh.targets.push_back({actor.type, actor.targetHeading});
                }
                sight = stellar7::sightOperations(model.level2LoopTick(), incomingSightPhase,
                    worldSetup->view.viewport, sightColor, refresh);
                operations.insert(operations.end(), sight->operations.begin(), sight->operations.end());
            }
            if ((input.kind == ClockedRenderKind::worldCockpit)) {
                if (!input.settings || pages == nullptr || retainedFrame == nullptr) {
                    throw std::invalid_argument("owned cockpit requires settings, pages and original bitmaps");
                }
                cockpitBitmaps = cockpitWorkBitmaps(assets);
                returnedPages = *pages;
                returnedPages->selectCockpitCopy();
                const auto relation = returnedPages->cachedPageDiffersFromDraw()
                    ? stellar7::CockpitPageRelation::differing : stellar7::CockpitPageRelation::matching;
                ownedCockpit = stellar7::cockpitOperations({retainedFrame->cockpitIndicatorX,
                    retainedFrame->cockpitIndicatorVelocity, input.settings->viewSize, relation}, cockpitBitmaps);
                operations.insert(operations.end(), ownedCockpit->operations.begin(), ownedCockpit->operations.end());
            }
            rendered = clock.renderWork(input.phase, operations, input.horizonCycles);
        } else if (input.horizon) {
            rendered = clock.renderHorizon(input.phase, *input.horizon, input.horizonCycles);
        } else {
            cockpit = clock.renderCockpit(input.phase, input.cockpit.state, input.cockpit.bitmaps, input.horizonCycles);
            rendered = std::move(cockpit.work);
        }
        const auto& timing = rendered.timing;
        if (scanout != nullptr) {
            if (pixels == nullptr || palette == nullptr || !returnedPages || !returnedPixels) {
                throw std::invalid_argument("quiet scanout requires retained page and palette owners");
            }
            scanout->consume(timing.vgaServices, *pixels, *palette, returnedPages->drawPage());
        }
        const auto& audio = rendered.audio;
        std::ostringstream output;
        output << std::setprecision(17) << "{\"supported\":true,\"owned_entry\":" << owned.str() << ",\"owned_return\":";
        printSoundBlasterCallbackState(synth.callbackWorkState(), output, SoundBlasterStateDetail::boundedWork);
        if (ground) {
            output << ",\"ground_entry\":";
            printGroundState(output, model.level2Ground(), model.level2Rng());
            output << ",\"ground_return\":";
            printGroundState(output, ground->ground, ground->rng);
            const auto& player = model.level2Player();
            output << ",\"ground_controls\":[" << player.yawVelocity << ',' << player.forwardVelocity << ','
                   << player.position.z << "],\"ground_bitmaps\":[";
            for (std::size_t index = 0; index < groundBitmaps.size(); ++index) {
                const auto& bitmap = groundBitmaps[index];
                output << (index == 0 ? "{" : ",{") << "\"width\":" << bitmap.width << ",\"height\":" << bitmap.height
                       << ",\"pixels\":[";
                for (std::size_t pixel = 0; pixel < bitmap.pixels.size(); ++pixel) {
                    output << (pixel == 0 ? "" : ",") << static_cast<unsigned>(bitmap.pixels[pixel]);
                }
                output << "]}";
            }
            output << ']';
        }
        if (worldSetup) {
            const auto& view = worldSetup->view;
            const auto& light = worldSetup->transformedLight;
            output << ",\"world_view\":{\"origin\":[" << view.origin.x << ',' << view.origin.y << ',' << view.origin.z
                   << "],\"matrix\":[";
            for (std::size_t index = 0; index < view.matrix.values.size(); ++index) {
                output << (index == 0 ? "" : ",") << view.matrix.values[index];
            }
            output << "],\"mode\":" << view.matrix.mode << ",\"viewport\":[" << view.viewport.left << ','
                   << view.viewport.right << ',' << view.viewport.top << ',' << view.viewport.bottom
                   << "],\"perspective_scale\":" << view.perspectiveScale << ",\"light\":["
                   << light.x << ',' << light.y << ',' << light.z << "]}";
            output << ",\"world_actors\":[";
            const auto& entities = retainedFrame != nullptr ? retainedFrame->entities : model.level2PresentationEntities();
            const auto& order = retainedFrame != nullptr ? retainedFrame->order : model.level2PresentationActiveListOrder();
            for (std::size_t ordinal = 0; ordinal < order.size(); ++ordinal) {
                const auto& actor = entities.at(order[ordinal]);
                output << (ordinal == 0 ? "{" : ",{")
                       << "\"allocation_index\":" << actor.allocationIndex
                       << ",\"flags\":" << static_cast<unsigned>(actor.flags)
                       << ",\"type\":" << actor.type << ",\"collision_radius\":" << actor.collisionRadius
                       << ",\"position\":[" << actor.position.x << ',' << actor.position.y << ',' << actor.position.z
                       << "],\"angles\":[" << actor.pitch << ',' << actor.roll << ',' << actor.yaw << "]}";
            }
            output << ']';
        }
        if (worldCenter) {
            const auto& relative = worldCenter->relativePosition;
            const auto& center = worldCenter->center;
            output << ",\"world_first_center\":{\"relative\":[" << relative.x << ',' << relative.y << ',' << relative.z
                   << "],\"center\":[" << center.x << ',' << center.y << ',' << center.z << "]}";
        }
        if (worldScene) {
            output << ",\"world_scene\":{\"registry\":[" << assets.sharedModelRegistry[0] << ','
                   << assets.sharedModelRegistry[1] << "],\"centers\":[";
            for (std::size_t index = 0; index < worldScene->centers.size(); ++index) {
                const auto center = worldScene->centers[index];
                output << (index == 0 ? "[" : ",[") << center.x << ',' << center.y << ',' << center.z << ']';
            }
            output << "],\"culled\":[";
            for (std::size_t index = 0; index < worldScene->culled.size(); ++index) {
                output << (index == 0 ? "" : ",") << (worldScene->culled[index] ? "true" : "false");
            }
            output << "],\"polygon_counts\":[";
            for (std::size_t index = 0; index < worldScene->polygonCounts.size(); ++index) {
                output << (index == 0 ? "" : ",") << worldScene->polygonCounts[index];
            }
            output << "],\"pixels\":[";
            for (std::size_t index = 0; index < worldScene->pixels.size(); ++index) {
                const auto& pixel = worldScene->pixels[index];
                output << (index == 0 ? "[" : ",[") << pixel.point.x << ',' << pixel.point.y << ','
                       << static_cast<unsigned>(pixel.color) << ']';
            }
            output << "]}";
        }
        if (sight) {
            output << ",\"sight\":{\"tick\":" << model.level2LoopTick()
                   << ",\"entry_phase\":" << (incomingSightPhase ? "true" : "false")
                   << ",\"return_phase\":" << (sight->alternatePhase ? "true" : "false")
                   << ",\"color\":" << static_cast<unsigned>(sight->color) << ",\"targets\":[";
            for (std::size_t ordinal = 0; ordinal < retainedFrame->order.size(); ++ordinal) {
                const auto& actor = retainedFrame->entities.at(retainedFrame->order[ordinal]);
                output << (ordinal == 0 ? "[" : ",[") << actor.type << ',' << actor.targetHeading << ']';
            }
            output << "],\"pixels\":[";
            for (std::size_t index = 0; index < sight->pixels.size(); ++index) {
                const auto& pixel = sight->pixels[index];
                output << (index == 0 ? "[" : ",[") << pixel.point.x << ',' << pixel.point.y << ','
                       << static_cast<unsigned>(pixel.color) << ']';
            }
            output << "]}";
        }
        if (ownedCockpit) {
            output << ",\"cockpit\":{\"entry_x\":" << retainedFrame->cockpitIndicatorX
                   << ",\"entry_velocity\":" << retainedFrame->cockpitIndicatorVelocity
                   << ",\"return_x\":" << ownedCockpit->state.indicatorX
                   << ",\"return_velocity\":" << ownedCockpit->state.indicatorVelocity
                   << ",\"view_size\":" << input.settings->viewSize
                   << ",\"cached_page\":" << returnedPages->cachePage()
                   << ",\"draw_page\":" << returnedPages->drawPage()
                   << ",\"page_relation\":\"differing\",\"physical_page_phase_qualified\":false,\"bitmaps\":[";
            const auto writeBitmap = [&output](const stellar7::IndexedBitmap& bitmap) {
                output << "{\"width\":" << bitmap.width << ",\"height\":" << bitmap.height << ",\"pixels\":[";
                for (std::size_t index = 0; index < bitmap.pixels.size(); ++index) {
                    output << (index == 0 ? "" : ",") << static_cast<unsigned>(bitmap.pixels[index]);
                }
                output << "]}";
            };
            writeBitmap(cockpitBitmaps.strip);
            output << ',';
            writeBitmap(cockpitBitmaps.indicator);
            for (const auto& bitmap : cockpitBitmaps.overlays) {
                output << ',';
                writeBitmap(bitmap);
            }
            output << "]}";
        }
        if (radarWork) {
            output << ",\"radar\":";
            printRadarWork(output, *radar, *radarEntryPages, radarInputs, *radarWork);
        }
        if (!input.horizon) {
            output << ",\"indicator_x\":" << cockpit.state.indicatorX
                   << ",\"indicator_velocity\":" << cockpit.state.indicatorVelocity;
        }
        if (timing.phase.bios) {
            output << ",\"clock_phase\":";
            printBiosClockPhase(timing.phase, output);
        }
        output << ",\"total\":";
        printSoundBlasterClockStep(timing.total, output);
        output << ",\"string_iterations\":" << timing.stringIterations << ",\"string_restarts\":" << timing.stringRestarts
               << ",\"timer\":{\"global_count\":" << timing.phase.timer->globalCount
               << ",\"bios_count\":" << timing.phase.timer->biosCount << ",\"callback_count\":" << timing.phase.timer->callbackCount
               << ",\"irq0_requested\":" << timing.phase.devices->irq0Requested
               << ",\"irq0_in_service\":" << timing.phase.devices->irq0InService << "},\"callbacks\":[";
        for (std::size_t index = 0; index < timing.callbacks.size(); ++index) {
            const auto& callback = timing.callbacks[index];
            output << (index == 0 ? "[" : ",[") << callback.cycleOffset << ',' << static_cast<int>(callback.advance.kind)
                   << ',' << callback.advance.dispatches << ']';
        }
        output << "],\"opl_writes\":[";
        for (std::size_t index = 0; index < audio.writes.size(); ++index) {
            const auto& write = audio.writes[index];
            output << (index == 0 ? "[" : ",[") << write.cycleOffset << ',' << static_cast<unsigned>(write.write.registerNumber)
                   << ',' << static_cast<unsigned>(write.write.value) << ']';
        }
        output << "],\"pcm\":[";
        for (std::size_t index = 0; index < audio.samples.size(); ++index) {
            output << (index == 0 ? "" : ",") << audio.samples[index];
        }
        output << "],\"retained_ground_applied\":" << (retainedFrame != nullptr ? "true" : "false");
        if (observation != nullptr) {
            return output.str();
        }
        auto finalPhase = timing.phase;
        if (input.end != ClockedRenderEnd::component) {
            // Initial DS1298=1; this admitted route performs no synchronization-setting mutation.
            constexpr auto synchronization = stellar7::PageFlipSynchronization::enabled;
            constexpr std::uint16_t modeXCRTCPort = 0x3d4;
            const auto copies = stellar7::frameCopyOperations(input.copyProfile);
            const auto flipped = executor != nullptr ? executor->display(*returnedPages)
                : clock.renderPageFlip(timing.phase,
                    {synchronization, returnedPages->backPage(), returnedPages->frontPage(), modeXCRTCPort},
                    input.horizonCycles);
            if (scanout != nullptr && input.scanoutEnd == ScanoutEnd::displayReturn) {
                scanout->consume(flipped.work.timing.vgaServices, *returnedPixels, *palette);
            }
            output << ",\"display_end\":{\"boundary\":\"8518->8552\",\"native_phase_qualified\":false"
                   << ",\"low_reads\":" << flipped.lowReads << ",\"high_reads\":" << flipped.highReads
                   << ",\"flip\":";
            printClockedContinuation(output, flipped.work, synth);
            const auto postFlip = executor != nullptr ? executor->postcopy(input.copyProfile)
                : clock.renderWork(flipped.work.timing.phase, copies.operations, input.horizonCycles);
            if (scanout != nullptr && input.scanoutEnd == ScanoutEnd::displayReturn) {
                scanout->consume(postFlip.timing.vgaServices, *returnedPixels, *palette);
            }
            finalPhase = postFlip.timing.phase;
            output << ",\"post_flip\":";
            printClockedContinuation(output, postFlip, synth);
            output << ",\"rectangle_effect\":\"none\",\"front_page\":" << flipped.state.frontPage
                   << ",\"back_page\":" << flipped.state.backPage << '}';
        }
        if (returnedPixels) {
            output << ",\"indexed_frame\":";
            printIndexedPage(output, *returnedPixels, returnedPages->drawPage());
            output << ",\"indexed_previous_front\":";
            printIndexedPage(output, *returnedPixels, returnedPages->frontPage());
        }
        if (palette != nullptr) {
            output << ",\"palette\":";
            printRetainedPalette(output, *palette);
        }
        output << '}';
        if (framePlan) {
            stellar7::commitLevel2Frame(*framePlan, *retainedFrame, *pages, *radar, *pixels);
            if (input.end != ClockedRenderEnd::component) {
                pages->flip();
            }
        } else if (retainedFrame != nullptr) {
            retainedFrame->ground = ground->ground;
            retainedFrame->rng = ground->rng;
            if (sight) {
                retainedFrame->alternateSightPhase = sight->alternatePhase;
                retainedFrame->sightExecution = stellar7::Level2SightExecution::rendered;
            }
            if (ownedCockpit) {
                retainedFrame->cockpitIndicatorX = ownedCockpit->state.indicatorX;
                retainedFrame->cockpitIndicatorVelocity = ownedCockpit->state.indicatorVelocity;
                retainedFrame->cockpitExecution = stellar7::Level2CockpitExecution::rendered;
                *pages = *returnedPages;
            }
            if (radarWork) {
                *pixels = std::move(*returnedPixels);
                if (radarWork->pageCopy) {
                    pages->selectArmorCopy();
                }
                *radar = *returnedRadar;
            }
            if (input.end != ClockedRenderEnd::component) {
                pages->flip();
            }
        }
        if (returnedPhase != nullptr) {
            *returnedPhase = finalPhase;
        }
        // Radar marker mode at1fdb leaves the caller's world line style untouched.
        if (returnedEnemyFlag != nullptr && radarWork) {
            *returnedEnemyFlag = radarWork->enemyFlag;
        }
        return output.str();
    } catch (const std::exception& error) {
        if (observation != nullptr) {
            throw;
        }
        std::ostringstream rejected;
        rejected << "{\"supported\":false,\"owned_entry\":" << owned.str()
                 << ",\"error\":" << std::quoted(error.what()) << '}';
        return rejected.str();
    }
}

void traceNormalWorkPhase(const stellar7::AudioClockVgaInterrupt vgaInterrupt = stellar7::AudioClockVgaInterrupt::unsupported,
    const NormalWorkPhaseInput input = NormalWorkPhaseInput::bounded) {
    constexpr std::size_t maximumCases = 1024;
    constexpr std::size_t maximumOperations = 32768;
    std::size_t count = 0;
    if (!(std::cin >> count) || count == 0 || count > maximumCases) {
        throw std::runtime_error("invalid normal work phase case count");
    }
    std::cout << '[';
    for (std::size_t index = 0; index < count; ++index) {
        auto phase = readRetainedNormalPhase(vgaInterrupt, input);
        int irqCheck = 0, delivery = 0;
        std::int64_t horizon = 0;
        std::size_t operationCount = 0;
        if (!(std::cin >> irqCheck >> delivery >> horizon >> operationCount)
            || irqCheck < 0 || irqCheck > 1 || delivery < 0 || delivery > 2
            || operationCount == 0 || operationCount > maximumOperations) {
            throw std::runtime_error("invalid normal work phase input");
        }
        phase.irqCheckRequested = irqCheck != 0;
        phase.interruptDelivery = static_cast<stellar7::AudioClockInterruptDelivery>(delivery);
        std::vector<stellar7::NormalWorkOperation> operations;
        for (std::size_t operationIndex = 0; operationIndex < operationCount; ++operationIndex) {
            int kind = 0;
            std::uint64_t workCount = 0;
            if (!(std::cin >> kind >> workCount) || kind < 0
                || kind > static_cast<int>(stellar7::NormalWorkKind::popFlagsEnabled)) {
                throw std::runtime_error("invalid normal work operation");
            }
            operations.push_back({static_cast<stellar7::NormalWorkKind>(kind), workCount});
        }
        const auto result = stellar7::normalWorkTiming(phase, operations, horizon);
        std::cout << (index == 0 ? "{\"total\":" : ",{\"total\":");
        printSoundBlasterClockStep(result.total);
        std::cout << ",\"string_iterations\":" << result.stringIterations
                  << ",\"string_restarts\":" << result.stringRestarts;
        if (result.phase.devices) {
            const auto& devices = *result.phase.devices;
            std::cout << ",\"devices\":{\"pic_ticks\":" << devices.picTicks
                      << ",\"pit_start_ms\":" << std::setprecision(17) << devices.pitStartMilliseconds
                      << ",\"pit_now_ms\":" << devices.pitNowMilliseconds
                      << ",\"pit_cycle_base\":" << devices.pitCycleBase
                      << ",\"frame_start_ms\":" << devices.frameStartMilliseconds
                      << ",\"real_start\":" << devices.realStart
                      << ",\"draw_bytes_skip\":" << devices.drawnBytesSkip
                      << ",\"draw_panning\":" << devices.drawnPanning
                      << ",\"vret_triggered\":" << devices.verticalRetraceTriggered;
            if (devices.frame) {
                std::cout << ",\"frames_since_time_base\":" << devices.frame->framesSinceTimeBase;
            }
            std::cout << '}';
        }
        if (result.phase.timer) {
            const auto& timer = *result.phase.timer;
            std::cout << ",\"timer\":{\"global_count\":" << timer.globalCount
                      << ",\"bios_count\":" << timer.biosCount
                      << ",\"callback_count\":" << timer.callbackCount
                      << ",\"irq0_requested\":" << result.phase.devices->irq0Requested
                      << ",\"irq0_in_service\":" << result.phase.devices->irq0InService << '}';
        }
        if (result.phase.callback) {
            std::cout << ",\"callback_state\":";
            printSoundBlasterCallbackState(*result.phase.callback);
            std::cout << ",\"callbacks\":[";
            for (std::size_t callback = 0; callback < result.callbacks.size(); ++callback) {
                const auto& scheduled = result.callbacks[callback];
                std::cout << (callback == 0 ? "{" : ",{") << "\"cycle_offset\":" << scheduled.cycleOffset
                          << ",\"kind\":" << static_cast<int>(scheduled.advance.kind)
                          << ",\"dispatches\":" << scheduled.advance.dispatches << '}';
            }
            std::cout << ']';
        }
        std::cout << '}';
    }
    std::cin >> std::ws;
    if (!std::cin.eof()) {
        throw std::runtime_error("unexpected trailing normal work phase input");
    }
    std::cout << "]\n";
}

void printBiosClockPhase(const stellar7::SoundBlasterClockPhase& phase, std::ostream& output) {
    const auto& devices = *phase.devices;
    const auto& frame = *devices.frame;
    const auto& timer = *phase.timer;
    const auto& bios = *phase.bios;
    output << std::setprecision(17)
              << "{\"remaining_cycles\":" << phase.remainingCycles
              << ",\"deferred_cycles\":" << phase.deferredCycles
              << ",\"interrupts\":" << static_cast<int>(phase.interrupts)
              << ",\"irq_pending\":" << phase.irqPending
              << ",\"irq_check_requested\":" << phase.irqCheckRequested
              << ",\"irq0_activation_possible\":" << phase.irq0ActivationPossible
              << ",\"delivery\":" << static_cast<int>(phase.interruptDelivery)
              << ",\"devices\":{\"pic_ticks\":" << devices.picTicks
              << ",\"pit_start_ms\":" << devices.pitStartMilliseconds
              << ",\"pit_now_ms\":" << devices.pitNowMilliseconds
              << ",\"pit_cycle_base\":" << devices.pitCycleBase
              << ",\"frame_period_ms\":" << devices.framePeriodMilliseconds
              << ",\"frame_start_ms\":" << devices.frameStartMilliseconds
              << ",\"display_end_ms\":" << devices.displayEndMilliseconds
              << ",\"retrace_start_ms\":" << devices.retraceStartMilliseconds
              << ",\"retrace_end_ms\":" << devices.retraceEndMilliseconds
              << ",\"display_start\":" << devices.displayStart
              << ",\"real_start\":" << devices.realStart
              << ",\"memory_mask\":" << devices.memoryMask
              << ",\"configured_bytes_skip\":" << devices.configuredBytesSkip
              << ",\"draw_bytes_skip\":" << devices.drawnBytesSkip
              << ",\"pel_panning\":" << devices.pelPanning
              << ",\"draw_panning\":" << devices.drawnPanning
              << ",\"vertical_retrace_end_register\":" << devices.verticalRetraceEndRegister
              << ",\"vret_triggered\":" << devices.verticalRetraceTriggered
              << ",\"irq0_requested\":" << devices.irq0Requested
              << ",\"irq0_in_service\":" << devices.irq0InService
              << ",\"frame\":{\"fps\":" << frame.framesPerSecond
              << ",\"time_base_ms\":" << frame.timeBaseMilliseconds
              << ",\"frames_since_time_base\":" << frame.framesSinceTimeBase
              << "}},\"timer\":{\"active_mask\":" << timer.activeMask
              << ",\"global_count\":" << timer.globalCount
              << ",\"bios_count\":" << timer.biosCount
              << ",\"callback_count\":" << timer.callbackCount
              << ",\"callback_reload\":" << timer.callbackReload
              << ",\"bios_reload\":" << timer.biosReload
              << "},\"bios\":{\"domain\":" << static_cast<int>(bios.domain)
              << ",\"timer_ticks\":" << bios.timerTicks
              << ",\"rollover\":" << static_cast<unsigned>(bios.rollover)
              << ",\"keyboard_state\":" << static_cast<unsigned>(bios.keyboardState)
              << ",\"keyboard_leds\":" << static_cast<unsigned>(bios.keyboardLeds)
              << ",\"keyboard_led_sync\":" << bios.keyboardLedSync
              << ",\"sync_time\":" << bios.syncTime
              << ",\"cmos_sync_pending\":" << bios.cmosSyncPending
              << ",\"floppy_timeout\":" << static_cast<unsigned>(bios.floppyTimeout)
              << ",\"motor_bits\":" << static_cast<unsigned>(bios.motorBits) << '}';
    if (phase.rtc) {
        const auto& rtc = *phase.rtc;
        std::uint64_t endedBits = 0;
        std::uint32_t delayBits = 0;
        std::memcpy(&endedBits,&rtc.endedMilliseconds,sizeof(endedBits));
        std::memcpy(&delayBits,&rtc.delayMilliseconds,sizeof(delayBits));
        std::ostringstream endedHex;
        endedHex << std::hex << std::setw(16) << std::setfill('0') << endedBits;
        output << ",\"rtc\":{\"domain\":" << static_cast<int>(rtc.domain)
            << ",\"profile\":{\"b\":" << static_cast<unsigned>(rtc.registerB)
            << ",\"c\":" << static_cast<unsigned>(rtc.registerC)
            << ",\"acknowledged\":" << rtc.acknowledged
            << ",\"ended_ms\":" << rtc.endedMilliseconds << ",\"ended_bits\":\"" << endedHex.str() << '"'
            << ",\"delay_ms\":" << rtc.delayMilliseconds << ",\"delay_bits\":" << delayBits
            << ",\"divider\":" << static_cast<unsigned>(rtc.divider)
            << ",\"selected\":" << static_cast<unsigned>(rtc.selected) << ",\"nmi\":" << rtc.nmi
            << ",\"lock\":" << rtc.locked << ",\"bcd\":" << rtc.bcd << ",\"ampm\":" << rtc.ampm
            << ",\"calendar\":[";
        for (std::size_t index = 0; index < rtc.calendar.size(); ++index) {
            output << (index == 0 ? "" : ",") << rtc.calendar[index];
        }
        output << "],\"clock_time_t\":" << rtc.clockTime << "},\"queue\":[";
        constexpr double cyclesPerMillisecond = 5000;
        for (std::size_t index = 0; index < phase.events.size(); ++index) {
            const auto& event = phase.events[index];
            const auto cycles = event.deadlineMilliseconds * cyclesPerMillisecond;
            output << (index == 0 ? "[" : ",[") << static_cast<int>(event.kind) << ',' << event.value
                << ',' << std::floor(cycles) << ',' << std::ceil(cycles) << ']';
        }
        output << "]}";
    }
    if (phase.keyboard) {
        const auto& keyboard = *phase.keyboard;
        output << ",\"keyboard\":{\"used\":" << keyboard.used << ",\"pos\":" << keyboard.position
            << ",\"p60data\":" << static_cast<unsigned>(keyboard.port60Data)
            << ",\"p60changed\":" << keyboard.port60Pending << ",\"scheduled\":" << keyboard.scheduled
            << ",\"repeat_key\":" << static_cast<unsigned>(keyboard.repeatKey) << ",\"repeat_wait\":" << keyboard.repeatWait
            << ",\"repeat_pause\":" << keyboard.repeatPause << ",\"repeat_rate\":" << keyboard.repeatRate
            << ",\"port_61_data\":" << static_cast<unsigned>(keyboard.port61Data) << ",\"buffer\":[";
        for (std::size_t i = 0; i < keyboard.buffer.size(); ++i) { output << (i == 0 ? "" : ",") << keyboard.buffer[i]; }
        output << "]},\"irq1_requested\":" << devices.irq1Requested << ",\"irq1_input\":" << devices.irq1Input
            << ",\"irq1_in_service\":" << devices.irq1InService;
    }
    output << '}';
}

void traceBiosChainPhase() {
    constexpr std::size_t maximumCases = 1024;
    constexpr std::array<const char*, 7> boundaryNames{
        "pre_int8", "post_int8", "pre_int1c", "int1c_entry", "post_int1c", "pre_eoi", "pre_iret"};
    std::size_t count = 0;
    if (!(std::cin >> count) || count == 0 || count > maximumCases) {
        throw std::runtime_error("invalid BIOS clock case count");
    }
    std::cout << '[';
    for (std::size_t index = 0; index < count; ++index) {
        auto phase = readRetainedNormalPhase(stellar7::AudioClockVgaInterrupt::fixedVgaNoDemand,
            NormalWorkPhaseInput::retainedOwnedFrame);
        readBiosClockState(phase);
        int irqCheck = 0, delivery = 0;
        if (!(std::cin >> irqCheck >> delivery) || irqCheck < 0 || irqCheck > 1 || delivery < 0 || delivery > 2) {
            throw std::runtime_error("invalid BIOS interrupt domain");
        }
        phase.irqCheckRequested = irqCheck != 0;
        phase.interruptDelivery = static_cast<stellar7::AudioClockInterruptDelivery>(delivery);
        const auto result = stellar7::biosChainTiming(phase);
        std::cout << (index == 0 ? "{\"total\":" : ",{\"total\":");
        printSoundBlasterClockStep(result.total);
        std::cout << ",\"phase\":";
        printBiosClockPhase(result.phase);
        std::cout << ",\"checkpoints\":[";
        for (std::size_t point = 0; point < result.checkpoints.size(); ++point) {
            const auto& checkpoint = result.checkpoints[point];
            std::cout << (point == 0 ? "{\"boundary\":" : ",{\"boundary\":")
                      << std::quoted(boundaryNames.at(static_cast<std::size_t>(checkpoint.boundary)))
                      << ",\"timing\":";
            printSoundBlasterClockStep(checkpoint.timing);
            std::cout << ",\"phase\":";
            printBiosClockPhase(checkpoint.phase);
            std::cout << '}';
        }
        std::cout << "]}";
    }
    std::cin >> std::ws;
    if (!std::cin.eof()) {
        throw std::runtime_error("unexpected trailing BIOS clock input");
    }
    std::cout << "]\n";
}

void traceSoundBlasterCallbackPhase() {
    constexpr std::size_t maximumCases = 1024;
    constexpr std::size_t maximumBlocks = 128;
    std::size_t count = 0;
    if (!(std::cin >> count) || count == 0 || count > maximumCases) {
        throw std::runtime_error("invalid callback phase case count");
    }
    std::cout << '[';
    for (std::size_t index = 0; index < count; ++index) {
        const auto phase = readSoundBlasterClockPhase();
        std::size_t blockCount = 0;
        int irqCheck = 0, delivery = 0, callbackKind = 0;
        auto incoming = phase;
        if (!(std::cin >> irqCheck >> delivery >> callbackKind >> blockCount)
            || irqCheck < 0 || irqCheck > 1 || delivery < 0 || delivery > 1 || callbackKind < 0
            || callbackKind > static_cast<int>(stellar7::SoundBlasterCallbackKind::explicitNoteOn)
            || blockCount == 0 || blockCount > maximumBlocks) {
            throw std::runtime_error("invalid callback phase block count");
        }
        incoming.irqCheckRequested = irqCheck != 0;
        incoming.interruptDelivery = delivery ? stellar7::AudioClockInterruptDelivery::blockedByIrq0 : stellar7::AudioClockInterruptDelivery::unsupported;
        std::vector<stellar7::SoundBlasterWorkBlock> blocks;
        for (std::size_t blockIndex = 0; blockIndex < blockCount; ++blockIndex) {
            stellar7::SoundBlasterWorkBlock block{};
            int kind = 0;
            if (!(std::cin >> kind >> block.dispatches >> block.voice)
                || kind < 0 || kind > static_cast<int>(stellar7::SoundBlasterWorkKind::carrierLevel)) {
                throw std::runtime_error("invalid callback phase work block");
            }
            block.kind = static_cast<stellar7::SoundBlasterWorkKind>(kind);
            blocks.push_back(block);
        }
        const auto result = stellar7::soundBlasterCallbackTiming(incoming, blocks, static_cast<stellar7::SoundBlasterCallbackKind>(callbackKind));
        std::cout << (index == 0 ? "{\"total\":" : ",{\"total\":");
        printSoundBlasterClockStep(result.total);
        std::cout << ",\"opl\":[";
        for (std::size_t writer = 0; writer < result.opl.size(); ++writer) {
            std::cout << (writer == 0 ? "[" : ",[");
            for (std::size_t point = 0; point < result.opl[writer].size(); ++point) {
                if (point != 0) {
                    std::cout << ',';
                }
                printSoundBlasterClockStep(result.opl[writer][point]);
            }
            std::cout << ']';
        }
        std::cout << "],\"interrupts\":[";
        for (std::size_t point = 0; point < result.interruptBoundaries.size(); ++point) {
            if (point != 0) {
                std::cout << ',';
            }
            printSoundBlasterClockStep(result.interruptBoundaries[point]);
        }
        std::cout << "]}";
    }
    std::cin >> std::ws;
    if (!std::cin.eof()) {
        throw std::runtime_error("unexpected trailing callback phase input");
    }
    std::cout << "]\n";
}


enum class NativeLifecycleDecision { decline, exhaust };

void verifyNativeLifecycle(const stellar7::GameplayAssets& assets, const std::filesystem::path& output) {
    constexpr std::uint32_t maximumTicks = 10000;
    constexpr std::size_t failureSoundCalls = 8;
    constexpr std::uint64_t reloadDraws = 16;
    constexpr std::int16_t retryArmor = 680;
    if (std::filesystem::exists(output)) {
        throw std::runtime_error("native lifecycle verification requires a fresh report");
    }
    const auto sampleCount = [](const stellar7::NativeSessionDelivery& delivery) {
        auto samples = delivery.samples.size();
        for (const auto& entry : delivery.entry) {
            samples += entry.samples.size();
        }
        return samples;
    };
    std::ostringstream out;
    out << "{\"strict_clock_qualified\":false,\"routes\":[";
    for (const auto decision : {NativeLifecycleDecision::decline, NativeLifecycleDecision::exhaust}) {
        stellar7::NativeGameplaySession session(assets, stellar7::GameplayModel{});
        const auto initialized = session.initialize();
        if (initialized.firstSample != 0 || session.elapsedSamples() != sampleCount(initialized)) {
            throw std::runtime_error("native lifecycle lost its initialization sample cursor");
        }
        const auto initialBudget = session.model().remainingContinues();
        std::size_t decisionStep = 0;
        std::size_t retries = 0;
        std::vector<std::size_t> failureCalls;
        std::vector<std::uint64_t> retryDraws;
        std::vector<std::size_t> retryFrames;
        auto previous = session.model().phase();
        while (session.model().phase() != stellar7::GameplayPhase::campaignEnded && session.model().ticks() < maximumTicks) {
            const auto phase = session.model().phase();
            stellar7::GameplayInput input;
            if (phase == stellar7::GameplayPhase::continuePrompt) {
                if (previous != phase) {
                    failureCalls.push_back(session.model().failureAudioEvents().size());
                    decisionStep = 0;
                }
                // Neutral, select/release, confirm/release: decisions use live phase, not fitted ticks.
                if (decision == NativeLifecycleDecision::exhaust) {
                    input.forward = decisionStep == 1;
                    input.confirm = decisionStep == 3;
                    if (decisionStep > 4) {
                        throw std::runtime_error("native Continue decision did not complete");
                    }
                } else {
                    input.confirm = decisionStep == 1;
                    if (decisionStep > 2) {
                        throw std::runtime_error("native decline did not complete");
                    }
                }
                ++decisionStep;
            }
            const auto beforeSample = session.elapsedSamples();
            const auto beforeRng = session.model().failureRng().calls();
            const auto delivery = session.advance(input);
            const auto deliveredSamples = sampleCount(delivery);
            if (delivery.firstSample != beforeSample || session.elapsedSamples() != beforeSample + deliveredSamples) {
                throw std::runtime_error("native lifecycle lost its sample cursor");
            }
            const auto current = session.model().phase();
            if (phase == stellar7::GameplayPhase::continuePrompt && current == stellar7::GameplayPhase::level1RetryHandoff) {
                ++retries;
                retryDraws.push_back(session.model().level1State().rng().calls() - beforeRng);
                retryFrames.push_back(0);
                if (session.model().playerArmor() != retryArmor || session.model().score() != 0) {
                    throw std::runtime_error("native retry did not restore source armor/score");
                }
            }
            if (current == stellar7::GameplayPhase::level1 && !retryFrames.empty()) {
                ++retryFrames.back();
            }
            if (current == stellar7::GameplayPhase::campaignEnded && phase != stellar7::GameplayPhase::continuePrompt) {
                failureCalls.push_back(session.model().failureAudioEvents().size());
            }
            previous = phase;
        }
        const auto ended = session.model().phase() == stellar7::GameplayPhase::campaignEnded;
        const auto expectedRetries = decision == NativeLifecycleDecision::decline ? 0U : initialBudget;
        if (!ended || retries != expectedRetries || failureCalls.size() != expectedRetries + 1
            || std::any_of(failureCalls.begin(), failureCalls.end(), [failureSoundCalls](const auto calls) { return calls != failureSoundCalls; })
            || std::any_of(retryDraws.begin(), retryDraws.end(), [reloadDraws](const auto draws) { return draws != reloadDraws; })
            || std::any_of(retryFrames.begin(), retryFrames.end(), [](const auto frames) { return frames < 2; })) {
            throw std::runtime_error("native lifecycle did not reach its complete route: "
                + std::string(decision == NativeLifecycleDecision::decline ? "decline" : "exhaustion")
                + " ticks=" + std::to_string(session.model().ticks()) + " ended=" + std::to_string(ended)
                + " retries=" + std::to_string(retries) + " failures=" + std::to_string(failureCalls.size())
                + " first_calls=" + std::to_string(failureCalls.empty() ? 0 : failureCalls.front())
                + " first_draws=" + std::to_string(retryDraws.empty() ? 0 : retryDraws.front())
                + " first_retry_frames=" + std::to_string(retryFrames.empty() ? 0 : retryFrames.front()));
        }
        out << (decision == NativeLifecycleDecision::decline ? "" : ",") << "{\"route\":"
            << std::quoted(decision == NativeLifecycleDecision::decline ? "decline" : "exhaustion")
            << ",\"ended\":true,\"sample_cursor_continuous\":true,\"ticks\":" << session.model().ticks()
            << ",\"sample_end\":" << session.elapsedSamples() << ",\"retries\":" << retries
            << ",\"remaining_continues\":" << session.model().remainingContinues();
        const auto writeCounts = [&](const char* name, const auto& values) {
            out << ',' << std::quoted(name) << ":[";
            for (std::size_t index = 0; index < values.size(); ++index) {
                out << (index == 0 ? "" : ",") << values[index];
            }
            out << ']';
        };
        writeCounts("failure_model_events", failureCalls);
        writeCounts("retry_rng_draws", retryDraws);
        writeCounts("returned_retry_level1_frames", retryFrames);
        out << '}';
    }
    out << "]}\n";
    writeText(output.string(), out.str());
}

enum class NativeTracePages { delivered, physicalBack };

enum class NativeTraceBoundary { display, completedTail };

void traceNativeSession(const stellar7::GameplayAssets& assets, const std::vector<ParsedLevel1InputEvent>& events,
    const std::uint32_t count, const std::filesystem::path& directory,
    const stellar7::NativeStartupAssets* startupAssets = nullptr,
    const NativeTracePages pages = NativeTracePages::delivered, const std::uint32_t firstArtifactFrame = 1,
    const NativeTraceBoundary boundary = NativeTraceBoundary::display) {
    constexpr std::uint32_t maximumTraceFrames = 10000;
    if (count == 0 || count > maximumTraceFrames || firstArtifactFrame == 0 || firstArtifactFrame > count
        || std::filesystem::exists(directory)) {
        throw std::runtime_error("native session trace requires 1..10000 frames and a fresh output directory");
    }
    std::filesystem::create_directories(directory);
    std::unique_ptr<stellar7::NativeAudioOwner> startupAudio;
    std::vector<std::int16_t> audio;
    std::ostringstream startupRecord;
    if (startupAssets) {
        stellar7::NativeStartupSequence startup(assets, *startupAssets);
        startup.start();
        startup.acceptSkipInput();
        const auto appendStartup = [&]() {
            const auto samples = startup.advance(1024);
            audio.insert(audio.end(), samples.begin(), samples.end());
            if (audio.size() > stellar7::SoundBlasterSynth::sampleRate * 120) {
                throw std::runtime_error("native startup trace exceeded its diagnostic safety bound");
            }
        };
        while (startup.phase() != stellar7::NativeStartupPhase::menu) { appendStartup(); }
        const auto menuSample = startup.elapsedSamples();
        writeBytes((directory / "startup-menu.indexed.bin").string(), startup.frame().pixels);
        writeBytes((directory / "startup-menu.back.indexed.bin").string(), startup.menuPagePixels(stellar7::NativeMenuPage::back));
        writeBytes((directory / "startup-menu.cache.indexed.bin").string(), startup.menuPagePixels(stellar7::NativeMenuPage::cache));
        std::vector<std::uint8_t> menuPalette;
        for (const auto& color : startup.frame().palette) {
            menuPalette.insert(menuPalette.end(), {color.red, color.green, color.blue});
        }
        writeBytes((directory / "startup-menu.palette.bin").string(), menuPalette);
        startup.requestBegin();
        while (startup.phase() != stellar7::NativeStartupPhase::ready) { appendStartup(); }
        startupAudio = startup.releaseAudio();
        startupRecord << ",\"startup\":{\"route\":\"accepted-early-skip/default-difficulty0/immediate-Begin\","
            << "\"original_input_and_cpu_timing_qualified\":false,\"physical_startup_page_order_qualified\":false,"
            << "\"logo_steps\":" << startup.logoSteps() << ",\"menu_sample\":" << menuSample
            << ",\"menu_indexed\":\"startup-menu.indexed.bin\",\"menu_back_indexed\":\"startup-menu.back.indexed.bin\""
            << ",\"menu_cache_indexed\":\"startup-menu.cache.indexed.bin\",\"menu_palette\":\"startup-menu.palette.bin\""
            << ",\"menu_page_segments\":{\"front\":" << startup.menuPageSegment(stellar7::NativeMenuPage::front)
            << ",\"back\":" << startup.menuPageSegment(stellar7::NativeMenuPage::back)
            << ",\"cache\":" << startup.menuPageSegment(stellar7::NativeMenuPage::cache) << '}'
            << ",\"gameplay_sample\":" << startupAudio->elapsedSamples()
            << ",\"opl_writes_at_adoption\":" << startupAudio->synth().registerWrites().size()
            << ",\"driver_at_adoption\":";
        writeSoundBlasterDriverState(startupRecord, startupAudio->synth().driverState());
        startupRecord << '}';
    }
    stellar7::NativeGameplaySession session(assets, stellar7::GameplayModel{},
        stellar7::NativeSessionStart::level1, std::move(startupAudio));
    const auto appendDelivery = [&](const stellar7::NativeSessionDelivery& delivery) {
        for (const auto& entry : delivery.entry) {
            audio.insert(audio.end(), entry.samples.begin(), entry.samples.end());
        }
        audio.insert(audio.end(), delivery.samples.begin(), delivery.samples.end());
    };
    appendDelivery(session.initialize());
    std::set<std::string> held;
    std::size_t nextEvent = 0;
    std::ostringstream out;
    const auto writeOpl = [&](const std::size_t first) {
        const auto& writes = session.synth().registerWrites();
        if (first > writes.size()) {
            throw std::logic_error("native OPL trace lost its write history");
        }
        out << '[';
        for (auto index = first; index < writes.size(); ++index) {
            out << (index == first ? "" : ",") << '['
                << static_cast<unsigned>(writes[index].registerNumber) << ','
                << static_cast<unsigned>(writes[index].value) << ']';
        }
        out << ']';
    };
    out << "{\"clock_domain\":\"legacy-presentation-unqualified\",\"strict_clock_qualified\":false,\"initial_opl_writes\":";
    writeOpl(0);
    auto oplCursor = session.synth().registerWrites().size();
    out << startupRecord.str();
    if (boundary == NativeTraceBoundary::completedTail) {
        out << ",\"frame_boundary\":\"post-display-tail\"";
    }
    if (firstArtifactFrame != 1) {
        out << ",\"first_artifact_frame\":" << firstArtifactFrame;
    }
    out << ",\"frames\":[";
    for (std::uint32_t tick = 1; tick <= count; ++tick) {
        applyLevel1InputEventsBeforeTick(events, tick, nextEvent, held);
        auto input = gameplayInputFromHeld(held);
        input.confirm = held.count("enter") != 0;
        const auto pageBefore = session.pageObservation();
        const std::array<int, 2> cockpitBefore{session.model().level2CockpitIndicatorX(),
            session.model().level2CockpitIndicatorVelocity()};
        if (boundary == NativeTraceBoundary::completedTail
            && (input.left || input.right || input.forward || input.reverse || input.fire || input.exit || input.confirm)) {
            throw std::invalid_argument("complete Level1 frame requires neutral input");
        }
        const auto delivery = boundary == NativeTraceBoundary::completedTail
            ? session.advanceLevel1Frame() : session.advance(input);
        appendDelivery(delivery);
        const auto prefix = "frame-" + std::to_string(tick);
        const bool saveArtifacts = tick >= firstArtifactFrame;
        const auto indexed = saveArtifacts ? prefix + ".indexed.bin" : "";
        const auto palettePath = saveArtifacts ? prefix + ".palette.bin" : "";
        std::string level2State;
        const auto phase = session.model().phase();
        if (saveArtifacts && (phase == stellar7::GameplayPhase::level2Handoff || phase == stellar7::GameplayPhase::level2
            || phase == stellar7::GameplayPhase::level2SuccessPresentation
            || phase == stellar7::GameplayPhase::level5LoaderReturn
            || phase == stellar7::GameplayPhase::level5
            || phase == stellar7::GameplayPhase::level5SuccessReturn
            || phase == stellar7::GameplayPhase::level2SuccessReturn
            || phase == stellar7::GameplayPhase::level3LoaderReturn
            || phase == stellar7::GameplayPhase::level3PreloopReturn
            || phase == stellar7::GameplayPhase::level3Native
            || phase == stellar7::GameplayPhase::endingEntry
            || phase == stellar7::GameplayPhase::endingPresentation
            || phase == stellar7::GameplayPhase::endingTeardown
            || phase == stellar7::GameplayPhase::endingReturn
            || phase == stellar7::GameplayPhase::nativeMenu
            || phase == stellar7::GameplayPhase::highscoreNameEntry || phase == stellar7::GameplayPhase::highscoreHall
            || (pages == NativeTracePages::physicalBack && session.model().failureFromLevel2()
                && (phase == stellar7::GameplayPhase::failurePreExplosionBlackout
                    || phase == stellar7::GameplayPhase::failureExplosion
                    || phase == stellar7::GameplayPhase::continuePrompt)))) {
            level2State = prefix + ".level2.json";
            writeLevel2ModelState(session.model(), (directory / level2State).string());
        }
        if (saveArtifacts) {
            writeBytes((directory / indexed).string(), session.frame().pixels);
        }
        std::string retainedBack;
        if (saveArtifacts && pages == NativeTracePages::physicalBack) {
            if (const auto back = session.retainedPagePixels(stellar7::NativePageRole::back)) {
                retainedBack = prefix + ".back.indexed.bin";
                writeBytes((directory / retainedBack).string(), *back);
            }
        }
        std::vector<std::uint8_t> palette;
        for (const auto& color : session.frame().palette) {
            palette.insert(palette.end(), {color.red, color.green, color.blue});
        }
        if (saveArtifacts) {
            writeBytes((directory / palettePath).string(), palette);
        }
        out << (tick == 1 ? "" : ",") << "{\"tick\":" << session.model().ticks()
            << ",\"phase\":" << static_cast<unsigned>(session.model().phase())
            << ",\"phase_name\":" << std::quoted(gameplayPhaseName(session.model().phase()))
            << ",\"presentation_frame\":" << session.model().presentationFrame()
            << ",\"level1_success_pending\":" << (session.model().pendingLevel1SuccessContact() ? "true" : "false")
            << ",\"remaining_continues\":" << session.model().remainingContinues()
            << ",\"failure_rng_calls\":" << session.model().failureRng().calls()
            << ",\"cockpit_before_frame\":[" << cockpitBefore[0] << ',' << cockpitBefore[1] << ']'
            << ",\"cockpit_indicator\":[" << session.model().level2CockpitIndicatorX()
            << ',' << session.model().level2CockpitIndicatorVelocity() << ']'
            << ",\"first_sample\":" << delivery.firstSample << ",\"sample_end\":" << session.elapsedSamples()
            << ",\"entry_frames\":" << delivery.entry.size() << ",\"indexed\":" << std::quoted(indexed)
            << ",\"palette\":" << std::quoted(palettePath) << ",\"clock_source\":"
            << std::quoted(session.synth().driverState().clockSource == stellar7::SoundBlasterClockSource::samples
                ? "samples" : "normalWork") << ",\"driver\":";
        writeSoundBlasterDriverState(out, session.synth().driverState());
        out << ",\"opl_write_begin\":" << oplCursor << ",\"opl_writes\":";
        writeOpl(oplCursor);
        oplCursor = session.synth().registerWrites().size();
        out << ",\"opl_write_end\":" << oplCursor;
        if (phase == stellar7::GameplayPhase::successPresentation || phase == stellar7::GameplayPhase::level2Handoff) {
            auto rng = session.model().level1State().rng();
            out << ",\"source_rng_future\":[";
            for (std::size_t index = 0; index < stellar7::StellarRng::wordCount; ++index) {
                out << (index == 0 ? "" : ",") << rng.next();
            }
            out << ']';
        }
        if (pages == NativeTracePages::physicalBack && phase == stellar7::GameplayPhase::failureExplosion) {
            auto rng = session.model().failureRng();
            out << ",\"destruction_rng_future\":[";
            for (std::size_t index = 0; index < stellar7::StellarRng::wordCount; ++index) {
                out << (index == 0 ? "" : ",") << rng.next();
            }
            out << ']';
        }
        const auto pageAfter = session.pageObservation();
        if (phase == stellar7::GameplayPhase::level5LoaderReturn) {
            out << ",\"loader_player_radius\":" << session.model().level2Player().collisionRadius
                << ",\"loader_animation\":[";
            const auto& actors = session.model().level2Entities();
            for (std::size_t index = 0; index < actors.size(); ++index) {
                const auto& actor = actors[index];
                out << (index == 0 ? "[" : ",[") << static_cast<unsigned>(actor.modelAnimationVariant) << ','
                    << static_cast<unsigned>(actor.presentationModelAnimationVariant) << ','
                    << actor.animatedModelPresenterEnabled << ']';
            }
            out << ']';
        }
        if (session.model().campaignLevel() == 4
            && (phase == stellar7::GameplayPhase::level5 || session.model().failureFromLevel2())) {
            const auto& sourceTick = stellar7::GameplayAnalysisAccess::failureSourceTick(session.model());
            if (sourceTick) {
                out << ",\"campaign_source_tick\":" << *sourceTick;
            }
            out << ",\"campaign_animation\":[";
            auto first = true;
            for (const auto& actor : session.model().level2Entities()) {
                if (actor.type != 11) {
                    continue;
                }
                out << (first ? "[" : ",[") << static_cast<unsigned>(actor.modelAnimationVariant) << ','
                    << static_cast<unsigned>(actor.presentationModelAnimationVariant) << ','
                    << static_cast<unsigned>(actor.stateBytes4dTo52[3]) << ']';
                first = false;
            }
            out << ']';
        }
        const bool initialLevel2Pages = (phase == stellar7::GameplayPhase::level2Handoff
            || phase == stellar7::GameplayPhase::level2) && session.model().level2LoopTick() < 3;
        if (saveArtifacts && pages == NativeTracePages::physicalBack
            && (tick == firstArtifactFrame || (phase == stellar7::GameplayPhase::level1 && session.model().playerArmor() < 0)
                || phase == stellar7::GameplayPhase::failurePreExplosionBlackout
                || phase == stellar7::GameplayPhase::failureExplosion
                || phase == stellar7::GameplayPhase::failurePostExplosionBlackout
                || phase == stellar7::GameplayPhase::level1RetryHandoff
                || phase == stellar7::GameplayPhase::successPresentation
                || phase == stellar7::GameplayPhase::continuePrompt
                || phase == stellar7::GameplayPhase::level2SuccessPresentation
                || phase == stellar7::GameplayPhase::level2SuccessReturn
                || phase == stellar7::GameplayPhase::level3LoaderReturn
                || phase == stellar7::GameplayPhase::level3PreloopReturn
                || phase == stellar7::GameplayPhase::level3Native
                || phase == stellar7::GameplayPhase::level5LoaderReturn
                || phase == stellar7::GameplayPhase::highscoreNameEntry || phase == stellar7::GameplayPhase::highscoreHall || initialLevel2Pages)) {
            const auto cache = session.retainedPagePixels(stellar7::NativePageRole::cache);
            out << ",\"retained_cache_indexed\":";
            if (cache) {
                const auto path = prefix + ".cache.indexed.bin";
                writeBytes((directory / path).string(), *cache);
                out << std::quoted(path);
            } else {
                out << "null";
            }
            out << ",\"retained_page_selections\":[" << pageAfter.copy << ',' << pageAfter.draw << ']';
        }
        if (pages == NativeTracePages::physicalBack && phase == stellar7::GameplayPhase::continuePrompt) {
            out << ",\"modal_page_selections\":[" << pageAfter.copy << ',' << pageAfter.draw << ']'
                << ",\"continue_choice\":" << static_cast<unsigned>(session.model().continueChoice())
                << ",\"continue_pressed_choice\":";
            if (session.model().pressedContinueChoice()) {
                out << static_cast<unsigned>(*session.model().pressedContinueChoice());
            } else {
                out << "null";
            }
            auto rng = session.model().failureRng();
            out << ",\"modal_rng_future\":[";
            for (std::size_t index = 0; index < stellar7::StellarRng::wordCount; ++index) {
                out << (index == 0 ? "" : ",") << rng.next();
            }
            out << ']';
        }
        if (pages == NativeTracePages::physicalBack) {
            out << ",\"retained_back_indexed\":";
            if (retainedBack.empty()) {
                out << "null";
            } else {
                out << std::quoted(retainedBack);
            }
        }
        out << ",\"page_projection\":" << std::quoted(pageAfter.projection == stellar7::NativePageProjection::retainedNativeLevel1
                ? "retained-native-level1" : pageAfter.projection == stellar7::NativePageProjection::retainedStationaryLevel2
                ? "retained-stationary-level2" : pageAfter.projection == stellar7::NativePageProjection::retainedSteeringLevel2
                    ? "retained-steering-level2" : pageAfter.projection == stellar7::NativePageProjection::retainedMovingLevel2
                    ? "retained-moving-level2" : pageAfter.projection == stellar7::NativePageProjection::retainedLevel2FailureRestore
                    ? "retained-level2-failure-restore" : pageAfter.projection == stellar7::NativePageProjection::retainedLevel2Destruction
                    ? "retained-level2-destruction" : pageAfter.projection == stellar7::NativePageProjection::retainedLevel2DestructionClear
                    ? "retained-level2-destruction-clear" : pageAfter.projection == stellar7::NativePageProjection::retainedLevel2Continue
                    ? "retained-level2-continue" : pageAfter.projection == stellar7::NativePageProjection::retainedLevel2Success
                    ? "retained-level2-success" : pageAfter.projection == stellar7::NativePageProjection::retainedLevel5Loader
                    ? "retained-level5-loader" : pageAfter.projection == stellar7::NativePageProjection::retainedLevel5
                    ? "retained-level5" : pageAfter.projection == stellar7::NativePageProjection::retainedHighscoreEntry
                    ? "retained-highscore-entry" : pageAfter.projection == stellar7::NativePageProjection::retainedHighscoreHall
                    ? "retained-highscore-hall" : pageAfter.projection == stellar7::NativePageProjection::retainedLevel1FailureRestore
                    ? "retained-level1-failure-restore" : pageAfter.projection == stellar7::NativePageProjection::retainedLevel1Destruction
                    ? "retained-level1-destruction" : pageAfter.projection == stellar7::NativePageProjection::retainedLevel1DestructionClear
                    ? "retained-level1-destruction-clear" : pageAfter.projection == stellar7::NativePageProjection::retainedLevel1Continue
                    ? "retained-level1-continue" : pageAfter.projection == stellar7::NativePageProjection::retainedLevel1Loader
                    ? "retained-level1-loader" : pageAfter.projection == stellar7::NativePageProjection::retainedLevel1Success
                    ? "retained-level1-success" : pageAfter.projection == stellar7::NativePageProjection::retainedLevel2SuccessReturn
                    ? "retained-level2-success-return" : pageAfter.projection == stellar7::NativePageProjection::retainedLevel3Loader
                    ? "retained-level3-loader" : pageAfter.projection == stellar7::NativePageProjection::retainedLevel3Preloop
                    ? "retained-level3-preloop" : pageAfter.projection == stellar7::NativePageProjection::retainedLevel3
                    ? "retained-level3" : "clean-unqualified")
            << ",\"page_before_frame\":[" << pageBefore.cache << ',' << pageBefore.back << ',' << pageBefore.front << ']'
            << ",\"page_after_frame\":[" << pageAfter.cache << ',' << pageAfter.back << ',' << pageAfter.front << ']';
        if (const auto* editor = session.highscoreNameEntry()) {
            const auto caret = editor->caret();
            out << ",\"highscore_name\":" << std::quoted(std::string(editor->name()))
                << ",\"highscore_caret\":[" << caret.countdown << ',' << caret.nextColor << ']';
        }
        out << ",\"level1_radar_objects\":[";
        if (session.model().phase() == stellar7::GameplayPhase::level1) {
            const auto objects = session.model().level1State().radarPresentationObjects();
            for (std::size_t index = 0; index < objects.size(); ++index) {
                const auto& object = objects[index];
                out << (index == 0 ? "" : ",") << '[' << static_cast<unsigned>(object.kind) << ','
                    << object.slot << ',' << object.type << ',' << static_cast<unsigned>(object.flags) << ','
                    << object.radius << ',' << object.position.x << ',' << object.position.y << ','
                    << object.position.z << ',' << static_cast<int>(object.category) << ']';
            }
        }
        out << ']';
        out << ",\"allocation_revision\":" << session.model().level2AllocationRevision()
            << ",\"presentation_pool_slots\":[";
        const auto& presented = session.model().level2PresentationEntities();
        for (std::size_t index = 0; index < presented.size(); ++index) {
            out << (index == 0 ? "" : ",");
            const auto slot = session.model().level2PresentationPoolSlot(index);
            if (slot) {
                out << *slot;
            } else {
                out << "null";
            }
        }
        out << "],\"presentation_flags\":[";
        for (std::size_t index = 0; index < presented.size(); ++index) {
            out << (index == 0 ? "" : ",") << static_cast<unsigned>(presented[index].flags);
        }
        out << "],\"radar_projections\":[";
        for (std::size_t index = 0; index < presented.size(); ++index) {
            out << (index == 0 ? "" : ",");
            const auto projection = session.radarObservation(index);
            if (!projection) {
                out << "null";
                continue;
            }
            out << '[' << static_cast<unsigned>(projection->flags) << ',' << projection->projectedX
                << ',' << projection->projectedY << ',' << projection->projectedRadius << ','
                << projection->cachedCenter.x << ',' << projection->cachedCenter.y << ',' << projection->cachedCenter.z << ']';
        }
        out << ']';
        if (!delivery.entry.empty()) {
            out << ",\"cockpit_initialization\":[";
            const auto& states = session.model().lastCockpitInitialization().states;
            for (std::size_t index = 0; index < states.size(); ++index) {
                out << (index == 0 ? "" : ",") << '[' << states[index].x << ',' << states[index].velocity << ']';
            }
            out << ']';
        }
        out << ",\"level2_state\":";
        if (level2State.empty()) {
            out << "null";
        } else {
            out << std::quoted(level2State);
        }
        out << ",\"state\":" << stellar7::level1StateTraceJson(session.model().level1State(), "native-session")
            << ",\"audio_events\":[";
        for (std::size_t index = 0; index < delivery.events.size(); ++index) {
            out << (index == 0 ? "" : ",") << delivery.events[index].eventId;
        }
        out << "]}";
        applyLevel1InputEventsAfterCheckpoint(events, tick, nextEvent, held);
        if (session.model().phase() == stellar7::GameplayPhase::campaignEnded) {
            break;
        }
    }
    out << "]}\n";
    writeText((directory / "session.json").string(), out.str());
    writeWave((directory / "audio.wav").string(), audio);
}


}  // namespace

int main(int argc,char** argv) {
    try {
        const bool helpRequested=argc==2&&std::string(argv[1])=="--help";
        if(!helpRequested) {
            if(const auto result=stellar7::runNativePlayerCommand(argc,argv,runInstalledPlayer)) {return *result;}
        }
        if (argc == 2 && std::string(argv[1]) == "--trace-sbl-callback-phase") {
            traceSoundBlasterCallbackPhase();
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--trace-normal-work-phase") {
            traceNormalWorkPhase();
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--trace-normal-vga-work-phase") {
            traceNormalWorkPhase(stellar7::AudioClockVgaInterrupt::fixedVgaNoDemand);
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--trace-normal-device-work-phase") {
            traceNormalWorkPhase(stellar7::AudioClockVgaInterrupt::fixedVgaNoDemand, NormalWorkPhaseInput::retainedDevices);
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--trace-normal-timer-work-phase") {
            traceNormalWorkPhase(stellar7::AudioClockVgaInterrupt::fixedVgaNoDemand, NormalWorkPhaseInput::retainedTimer);
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--trace-normal-callback-work-phase") {
            traceNormalWorkPhase(stellar7::AudioClockVgaInterrupt::fixedVgaNoDemand, NormalWorkPhaseInput::retainedCallbacks);
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--trace-normal-frame-work-phase") {
            traceNormalWorkPhase(stellar7::AudioClockVgaInterrupt::fixedVgaNoDemand, NormalWorkPhaseInput::retainedFrameCallbacks);
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--trace-bios-chain-phase") {
            traceBiosChainPhase();
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--trace-sbl-keyoff-work") {
            traceSoundBlasterKeyOffWork();
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--trace-sbl-noteon-work") {
            traceSoundBlasterNoteOnWork();
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--trace-sbl-callback-advance") {
            traceSoundBlasterCallbackAdvance();
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--trace-cockpit-operations") {
            traceCockpitOperations();
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--trace-horizon-operations") {
            traceHorizonOperations();
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--trace-ground-operations") {
            traceGroundOperations();
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--trace-ground-execution") {
            traceGroundOperations(GroundTrace::execution);
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--trace-sbl-opl-phase") {
            traceSoundBlasterOplPhase();
            return 0;
        }
        if (argc == 4 && std::string(argv[1]) == "--trace-sbl-direct-expiry") {
            writeDirectExpiryTrace(loadGameplayAssets(argv[2], argv[3]));
            return 0;
        }
        if (argc == 7 && std::string(argv[1]) == "--trace-sbl-direct-timer-work") {
            const std::string gate(argv[2]);
            const std::string latch(argv[3]);
            if (gate != "open" && gate != "suppressed") {
                throw std::runtime_error("frequency gate must be open or suppressed");
            }
            if (latch != "unlatched" && latch != "latched") {
                throw std::runtime_error("level latch must be unlatched or latched");
            }
            std::array<std::uint16_t, 3> timers{};
            for (std::size_t index = 0; index < timers.size(); ++index) {
                const auto argument = index + 4;
                std::size_t consumed = 0;
                const auto value = std::stoull(argv[argument], &consumed, 0);
                if (argv[argument][consumed] != '\0' || value > std::numeric_limits<std::uint16_t>::max()) {
                    throw std::runtime_error("direct timer outside unsigned16 width");
                }
                timers[index] = static_cast<std::uint16_t>(value);
            }
            const auto work = stellar7::soundBlasterDirectTimerWork(timers,
                gate == "open" ? stellar7::SoundBlasterFrequencyGate::Open : stellar7::SoundBlasterFrequencyGate::Suppressed,
                latch == "latched" ? stellar7::SoundBlasterLevelLatch::Latched : stellar7::SoundBlasterLevelLatch::Unlatched);
            std::cout << "{\"base_dispatches\":" << work.dispatches << ",\"port_reads\":" << work.portReads
                      << ",\"port_writes\":" << work.portWrites << ",\"expired_mask\":" << static_cast<unsigned>(work.expiredMask)
                      << ",\"direct_timers\":[" << work.timers[0] << ',' << work.timers[1] << ',' << work.timers[2]
                      << "],\"scope\":\"Operation8 direct-timer helper including RET, not caller CALL; open gate requires untuned voices; hardware/PIC delays excluded\"}\n";
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--trace-sbl-opl-work") {
            const auto work = stellar7::soundBlasterOplWork();
            std::cout << "{\"base_dispatches\":" << work.dispatches
                      << ",\"address_dispatches\":" << work.addressDispatches
                      << ",\"data_dispatches\":" << work.dataDispatches
                      << ",\"port_reads\":" << work.portReads
                      << ",\"port_writes\":" << work.portWrites
                      << ",\"scope\":\"Resident OPL writer includes RET, excludes caller CALL; hardware delays and scheduler charges require CPU-budget phase\"}\n";
            return 0;
        }
        if (argc == 5 && std::string(argv[1]) == "--trace-sbl-wait-work") {
            std::array<std::uint16_t, 3> timers{};
            for (std::size_t index = 0; index < timers.size(); ++index) {
                const auto argument = index + 2;
                std::size_t consumed = 0;
                const auto value = std::stoull(argv[argument], &consumed, 0);
                if (argv[argument][consumed] != '\0' || value > std::numeric_limits<std::uint16_t>::max()) {
                    throw std::runtime_error("direct timer outside unsigned16 width");
                }
                timers[index] = static_cast<std::uint16_t>(value);
            }
            const auto dispatches = stellar7::soundBlasterWaitDispatches(timers);
            std::cout << "{\"base_dispatches\":" << dispatches
                      << ",\"scope\":\"Active unended mode0 sequence, positive wait, no fade/expired direct timers; includes callback RETF, excludes IRQ CALLF and device/scheduler work\"}\n";
            return 0;
        }
        if (argc >= 2 && std::string(argv[1]) == "--trace-solid-polygon") {
            if (argc < 14 || argc % 2 != 0) {
                throw std::runtime_error("solid polygon requires modes, four bounds and at least three coordinate pairs");
            }
            const std::string scale(argv[2]);
            const std::string clipping(argv[3]);
            if (scale != "native" && scale != "double") {
                throw std::runtime_error("solid polygon scale must be native or double");
            }
            if (clipping != "enabled" && clipping != "disabled") {
                throw std::runtime_error("solid polygon clipping must be enabled or disabled");
            }
            const auto coordinate = [&argv](const int index) {
                std::size_t consumed = 0;
                const auto value = std::stoll(argv[index], &consumed, 0);
                if (argv[index][consumed] != '\0' || value < std::numeric_limits<std::int16_t>::min()
                    || value > std::numeric_limits<std::int16_t>::max()) {
                    throw std::runtime_error("solid polygon coordinate outside signed16 width");
                }
                return static_cast<std::int16_t>(value);
            };
            const stellar7::Viewport viewport{coordinate(4), coordinate(5), coordinate(6), coordinate(7)};
            std::vector<stellar7::ProjectedPoint> points;
            for (int index = 8; index < argc; index += 2) {
                points.push_back({coordinate(index), coordinate(index + 1)});
            }
            const auto solid = stellar7::solidPolygon(points, viewport,
                scale == "native" ? stellar7::PolygonScale::Native : stellar7::PolygonScale::Double,
                clipping == "enabled" ? stellar7::PolygonClipMode::Enabled : stellar7::PolygonClipMode::Disabled);
            std::cout << "{\"base_dispatches\":" << solid.work.dispatches
                      << ",\"string_iterations\":" << solid.work.stringIterations
                      << ",\"port_instructions\":" << solid.work.portInstructions
                      << ",\"prefix_dispatches\":" << solid.prefixWork.dispatches
                      << ",\"prefix_string_iterations\":" << solid.prefixWork.stringIterations
                      << ",\"suffix_dispatches\":" << solid.suffixWork.dispatches
                      << ",\"suffix_string_iterations\":" << solid.suffixWork.stringIterations
                      << ",\"span_start_y\":" << solid.polygon.firstY << ",\"spans\":[";
            for (std::size_t index = 0; index < solid.polygon.rows.size(); ++index) {
                if (index != 0) {
                    std::cout << ',';
                }
                std::cout << '[' << solid.polygon.rows[index].left << ',' << solid.polygon.rows[index].right << ']';
            }
            std::cout << "],\"normalized_points\":[";
            for (std::size_t index = 0; index < solid.polygon.normalizedPoints.size(); ++index) {
                if (index != 0) {
                    std::cout << ',';
                }
                std::cout << '[' << solid.polygon.normalizedPoints[index].x << ',' << solid.polygon.normalizedPoints[index].y << ']';
            }
            std::cout << "],\"returned_viewport\":[" << solid.returnedViewport.left << ',' << solid.returnedViewport.right << ','
                      << solid.returnedViewport.top << ',' << solid.returnedViewport.bottom
                      << "],\"scope\":\"Noncollapsed uniform solid call includes caller CALLF and RETF; excludes IRQs, REP restarts, alternating-color, line/wireframe and device-clock charges\"}\n";
            return 0;
        }
        if (argc >= 2 && std::string(argv[1]) == "--trace-polygon-input") {
            if (argc < 14 || argc % 2 != 0) {
                throw std::runtime_error("polygon input requires modes, four bounds and at least three coordinate pairs");
            }
            const std::string scale(argv[2]);
            const std::string colors(argv[3]);
            if (scale != "native" && scale != "double") {
                throw std::runtime_error("polygon scale must be native or double");
            }
            if (colors != "same" && colors != "different") {
                throw std::runtime_error("polygon colors must be same or different");
            }
            const auto coordinate = [&argv](const int index) {
                std::size_t consumed = 0;
                const auto value = std::stoll(argv[index], &consumed, 0);
                if (argv[index][consumed] != '\0' || value < std::numeric_limits<std::int16_t>::min()
                    || value > std::numeric_limits<std::int16_t>::max()) {
                    throw std::runtime_error("polygon input coordinate outside signed16 width");
                }
                return static_cast<std::int16_t>(value);
            };
            const stellar7::Viewport viewport{coordinate(4), coordinate(5), coordinate(6), coordinate(7)};
            std::vector<stellar7::ProjectedPoint> points;
            for (int index = 8; index < argc; index += 2) {
                points.push_back({coordinate(index), coordinate(index + 1)});
            }
            const auto input = stellar7::preparePolygon(points, viewport,
                scale == "native" ? stellar7::PolygonScale::Native : stellar7::PolygonScale::Double,
                colors == "same" ? stellar7::PolygonColorRelation::Same : stellar7::PolygonColorRelation::Different);
            const auto writePoints = [](const std::vector<stellar7::ProjectedPoint>& values) {
                std::cout << '[';
                for (std::size_t index = 0; index < values.size(); ++index) {
                    if (index != 0) {
                        std::cout << ',';
                    }
                    std::cout << '[' << values[index].x << ',' << values[index].y << ']';
                }
                std::cout << ']';
            };
            std::cout << "{\"base_dispatches\":" << input.work.dispatches
                      << ",\"string_iterations\":" << input.work.stringIterations << ",\"points\":";
            writePoints(input.points);
            std::cout << ",\"outline_points\":";
            writePoints(input.outlinePoints);
            std::cout << ",\"viewport\":[" << input.viewport.left << ',' << input.viewport.right << ','
                      << input.viewport.top << ',' << input.viewport.bottom
                      << "],\"scope\":\"Filled3467->3533 setup plus caller CALLF; excludes clipping, IRQs, REP restarts and scratch-alias paths\"}\n";
            return 0;
        }
        if (argc >= 7 && argc % 2 == 1 && std::string(argv[1]) == "--trace-polygon-clip") {
            const std::string mode(argv[2]);
            if (mode != "enabled" && mode != "disabled") {
                throw std::runtime_error("polygon clipping mode must be enabled or disabled");
            }
            const auto coordinate = [&argv](const int index) {
                std::size_t consumed = 0;
                const auto value = std::stoll(argv[index], &consumed, 0);
                if (argv[index][consumed] != '\0' || value < std::numeric_limits<std::int16_t>::min()
                    || value > std::numeric_limits<std::int16_t>::max()) {
                    throw std::runtime_error("polygon clip coordinate outside signed16 width");
                }
                return static_cast<std::int16_t>(value);
            };
            const stellar7::Viewport viewport{coordinate(3), coordinate(4), coordinate(5), coordinate(6)};
            std::vector<stellar7::ProjectedPoint> points;
            for (int index = 7; index < argc; index += 2) {
                points.push_back({coordinate(index), coordinate(index + 1)});
            }
            const auto clip = stellar7::clipPolygon(points, viewport,
                mode == "enabled" ? stellar7::PolygonClipMode::Enabled : stellar7::PolygonClipMode::Disabled);
            std::cout << "{\"base_dispatches\":" << clip.work.dispatches
                      << ",\"string_iterations\":" << clip.work.stringIterations << ",\"points\":[";
            for (std::size_t index = 0; index < clip.points.size(); ++index) {
                if (index != 0) {
                    std::cout << ',';
                }
                std::cout << '[' << clip.points[index].x << ',' << clip.points[index].y << ']';
            }
            std::cout << "],\"scope\":\"Optional3533->353f clip includes CALLF/RETF; excludes IRQs/REP restarts and flags/device parity\"}\n";
            return 0;
        }
        if (argc >= 8 && argc % 2 == 0 && std::string(argv[1]) == "--trace-polygon-spans") {
            std::vector<stellar7::ProjectedPoint> points;
            const auto coordinate = [&argv](const int index) {
                std::size_t consumed = 0;
                const auto value = std::stoll(argv[index], &consumed, 0);
                if (argv[index][consumed] != '\0' || value < std::numeric_limits<std::int16_t>::min()
                    || value > std::numeric_limits<std::int16_t>::max()) {
                    throw std::runtime_error("polygon coordinate outside signed16 width");
                }
                return static_cast<std::int16_t>(value);
            };
            for (int index = 2; index < argc; index += 2) {
                points.push_back({coordinate(index), coordinate(index + 1)});
            }
            const auto polygon = stellar7::rasterPolygon(points);
            std::cout << "{\"span_start_y\":" << polygon.firstY << ",\"spans\":[";
            for (std::size_t index = 0; index < polygon.rows.size(); ++index) {
                if (index != 0) {
                    std::cout << ',';
                }
                std::cout << '[' << polygon.rows[index].left << ',' << polygon.rows[index].right << ']';
            }
            std::cout << "],\"normalized_points\":[";
            for (std::size_t index = 0; index < polygon.normalizedPoints.size(); ++index) {
                if (index != 0) {
                    std::cout << ',';
                }
                std::cout << '[' << polygon.normalizedPoints[index].x << ',' << polygon.normalizedPoints[index].y << ']';
            }
            std::cout << "],\"edge_dispatches\":" << polygon.edgeWork.dispatches
                      << ",\"edge_string_iterations\":" << polygon.edgeWork.stringIterations
                      << ",\"preparation_dispatches\":" << polygon.preparationWork.dispatches
                      << ",\"preparation_string_iterations\":" << polygon.preparationWork.stringIterations
                      << ",\"scope\":\"Post-clipping filled preparation3549->3874 includes edge work; excludes clipping, line/collapsed paths, device call, IRQs and REP restarts\"}\n";
            return 0;
        }
        if (argc == 7 && std::string(argv[1]) == "--trace-polygon-edge") {
            const std::string side(argv[2]);
            if (side != "left" && side != "right") {
                throw std::runtime_error("polygon edge side must be left or right");
            }
            const auto coordinate = [&argv](const int index) {
                std::size_t consumed = 0;
                const auto value = std::stoll(argv[index], &consumed, 0);
                if (argv[index][consumed] != '\0' || value < std::numeric_limits<std::int16_t>::min()
                    || value > std::numeric_limits<std::int16_t>::max()) {
                    throw std::runtime_error("polygon edge coordinate outside signed16 width");
                }
                return static_cast<std::int16_t>(value);
            };
            const auto edge = stellar7::rasterEdge({coordinate(3), coordinate(4)}, {coordinate(5), coordinate(6)},
                side == "left" ? stellar7::PolygonEdgeSide::Left : stellar7::PolygonEdgeSide::Right);
            std::cout << "{\"base_dispatches\":" << edge.work.dispatches << ",\"string_iterations\":" << edge.work.stringIterations
                      << ",\"samples\":[";
            for (std::size_t index = 0; index < edge.samples.size(); ++index) {
                if (index != 0) {
                    std::cout << ',';
                }
                std::cout << '[' << edge.samples[index].y << ',' << edge.samples[index].x << ']';
            }
            std::cout << "],\"scope\":\"Post-clipping edge dispatcher only; excludes IRQs, outer polygon and scheduler charges\"}\n";
            return 0;
        }
        if (argc == 3 && std::string(argv[1]) == "--trace-edge-writer-work") {
            std::size_t consumed = 0;
            const auto rows = std::stoll(argv[2], &consumed, 0);
            if (argv[2][consumed] != '\0' || rows < 0 || rows > std::numeric_limits<std::uint16_t>::max()) {
                throw std::runtime_error("edge row count outside unsigned16 width");
            }
            const auto work = stellar7::edgeWriterWork(static_cast<std::uint16_t>(rows));
            std::cout << "{\"base_dispatches\":" << work.dispatches
                      << ",\"return_dispatches\":1,\"string_iterations\":" << work.stringIterations
                      << ",\"port_instructions\":0,\"scope\":\"Writer only; excludes caller, IRQs and scheduler charges\"}\n";
            return 0;
        }
        if (argc == 5 && std::string(argv[1]) == "--trace-timer-irq-work") {
            const auto mask = [&argv](const int index) {
                std::size_t consumed = 0;
                const auto value = std::stoll(argv[index], &consumed, 0);
                if (argv[index][consumed] != '\0' || value < 0 || value > std::numeric_limits<std::uint16_t>::max()) {
                    throw std::runtime_error("timer slot mask outside unsigned16 width");
                }
                return static_cast<std::uint16_t>(value);
            };
            const std::string exit(argv[4]);
            if (exit != "iret" && exit != "bios") {
                throw std::runtime_error("timer exit must be iret or bios");
            }
            const auto path = exit == "iret" ? stellar7::TimerIrqExit::localIret : stellar7::TimerIrqExit::biosChain;
            std::cout << "{\"base_dispatches\":" << stellar7::timerIrqWrapperDispatches(mask(2), mask(3), path)
                      << ",\"scope\":\"Wrapper only; excludes callback/BIOS bodies, I/O delays, hardware entry and scheduler charges\"}\n";
            return 0;
        }
        if (argc == 3 && std::string(argv[1]) == "--trace-cockpit-control-work") {
            std::size_t consumed = 0;
            const auto x = std::stoll(argv[2], &consumed, 0);
            if (argv[2][consumed] != '\0' || x < std::numeric_limits<std::int16_t>::min()
                || x > std::numeric_limits<std::int16_t>::max()) {
                throw std::runtime_error("cockpit indicator outside signed16 width");
            }
            std::cout << "{\"base_dispatches\":" << stellar7::cockpitControlDispatches(static_cast<std::int16_t>(x))
                      << ",\"scope\":\"Stage>70, differing pages; excludes nine callees, IRQs and scheduler charges\"}\n";
            return 0;
        }
        if ((argc == 5 && (std::string(argv[1]) == "--trace-planar-copy-work"
                || std::string(argv[1]) == "--trace-planar-copy-operations"))
            || (argc == 6 && (std::string(argv[1]) == "--trace-planar-bitmap-work"
                || std::string(argv[1]) == "--trace-planar-bitmap-operations"))) {
            const auto coordinate = [&argv](const int index) {
                std::size_t consumed = 0;
                const auto value = std::stoll(argv[index], &consumed, 0);
                if (argv[index][consumed] != '\0' || value < 0 || value > std::numeric_limits<std::int16_t>::max()) {
                    throw std::runtime_error("planar argument outside nonnegative signed16 width");
                }
                return static_cast<std::uint16_t>(value);
            };
            stellar7::SpanWork work{};
            if (argc == 5) {
                if (std::string(argv[1]) == "--trace-planar-copy-operations") {
                    const auto operations = stellar7::planarCopyOperations(coordinate(2), coordinate(3), coordinate(4));
                    std::cout << '[';
                    for (std::size_t index = 0; index < operations.size(); ++index) {
                        std::cout << (index == 0 ? "[" : ",[") << static_cast<int>(operations[index].kind)
                                  << ',' << operations[index].count << ']';
                    }
                    std::cout << "]\n";
                    return 0;
                }
                work = stellar7::planarCopyWork(coordinate(2), coordinate(3), coordinate(4));
            } else {
                stellar7::IndexedBitmap bitmap{coordinate(2), coordinate(3), {}};
                if (bitmap.width > 320 || bitmap.height > 200) {
                    throw std::runtime_error("planar bitmap dimensions outside screen");
                }
                bitmap.pixels.resize(static_cast<std::size_t>(bitmap.width) * bitmap.height);
                for (auto& pixel : bitmap.pixels) {
                    unsigned value;
                    if (!(std::cin >> value) || value > std::numeric_limits<std::uint8_t>::max()) {
                        throw std::runtime_error("planar bitmap requires unsigned8 pixels on stdin");
                    }
                    pixel = static_cast<std::uint8_t>(value);
                }
                const std::string mode(argv[5]);
                if (mode != "transparent" && mode != "opaque" && mode != "general-transparent") {
                    throw std::runtime_error("planar blit mode must be transparent, opaque or general-transparent");
                }
                const auto path = mode == "opaque" ? stellar7::PlanarBlitMode::opaque
                    : mode == "transparent" ? stellar7::PlanarBlitMode::transparent : stellar7::PlanarBlitMode::generalTransparent;
                work = stellar7::planarBitmapWork(bitmap, static_cast<std::int16_t>(coordinate(4)),
                    path);
                if (std::string(argv[1]) == "--trace-planar-bitmap-operations") {
                    const auto operations = stellar7::planarBitmapOperations(bitmap, static_cast<std::int16_t>(coordinate(4)), path);
                    std::cout << '[';
                    for (std::size_t index = 0; index < operations.size(); ++index) {
                        std::cout << (index == 0 ? "[" : ",[") << static_cast<int>(operations[index].kind)
                                  << ',' << operations[index].count << ']';
                    }
                    std::cout << "]\n";
                    return 0;
                }
            }
            std::cout << "{\"base_dispatches\":" << work.dispatches
                      << ",\"string_iterations\":" << work.stringIterations
                      << ",\"port_instructions\":" << work.portInstructions
                      << ",\"scope\":\"Resident body through RETF; excludes caller, REP restarts, IRQs and I/O delays\"}\n";
            return 0;
        }
        if (argc >= 4 && argc % 2 == 0 && std::string(argv[1]) == "--trace-solid-span-work") {
            std::vector<stellar7::RasterSpan> rows;
            const auto coordinate = [&argv](const int index) {
                std::size_t consumed = 0;
                const auto value = std::stoll(argv[index], &consumed, 0);
                if (argv[index][consumed] != '\0' || value < std::numeric_limits<std::int16_t>::min()
                    || value > std::numeric_limits<std::int16_t>::max()) {
                    throw std::runtime_error("span coordinate outside signed16 width");
                }
                return static_cast<std::int16_t>(value);
            };
            for (int index = 2; index < argc; index += 2) {
                rows.push_back({coordinate(index), coordinate(index + 1)});
            }
            const auto work = stellar7::solidSpanWork(rows);
            std::cout << "{\"base_dispatches\":" << work.dispatches
                      << ",\"string_iterations\":" << work.stringIterations
                      << ",\"port_instructions\":" << work.portInstructions
                      << ",\"caller_dispatches\":1,\"scope\":\"Solid-color path only; excludes REP restarts, IRQs and I/O delays\"}\n";
            return 0;
        }
        if (argc == 4 && std::string(argv[1]) == "--trace-vertex-cache-work") {
            const std::string cache(argv[2]);
            if (cache != "cached" && cache != "uncached") {
                throw std::runtime_error("vertex cache state must be cached or uncached");
            }
            std::size_t consumed = 0;
            const auto mode = std::stoll(argv[3], &consumed, 0);
            if (argv[3][consumed] != '\0' || mode < 0 || mode > std::numeric_limits<std::uint16_t>::max()) {
                throw std::runtime_error("vertex-work mode outside its integer width");
            }
            const auto state = cache == "cached" ? stellar7::VertexCacheState::Populated : stellar7::VertexCacheState::Empty;
            std::cout << "{\"base_dispatches\":" << stellar7::vertexCacheDispatches(state, static_cast<std::uint16_t>(mode))
                      << ",\"caller_dispatches\":1,\"scope\":\"Normal-core base work only; no scheduler, IRQ or device timing\"}\n";
            return 0;
        }
        if (argc == 15 && std::string(argv[1]) == "--trace-vector-transform-work") {
            const auto integer = [&argv](const int index, const std::int64_t minimum, const std::int64_t maximum) {
                std::size_t consumed = 0;
                const auto value = std::stoll(argv[index], &consumed, 0);
                if (argv[index][consumed] != '\0' || value < minimum || value > maximum) {
                    throw std::runtime_error("vector-work argument outside its integer width");
                }
                return value;
            };
            stellar7::FixedMatrixQ14 matrix{};
            matrix.mode = static_cast<std::uint16_t>(integer(2, 0, std::numeric_limits<std::uint16_t>::max()));
            const stellar7::FixedPosition relative{
                static_cast<std::int32_t>(integer(3, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max())),
                static_cast<std::int32_t>(integer(4, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max())),
                static_cast<std::int32_t>(integer(5, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()))
            };
            for (std::size_t index = 0; index < matrix.values.size(); ++index) {
                matrix.values[index] = static_cast<std::int16_t>(integer(
                    static_cast<int>(index) + 6,
                    std::numeric_limits<std::int16_t>::min(), std::numeric_limits<std::int16_t>::max()
                ));
            }
            std::cout << "{\"base_dispatches\":" << stellar7::vectorTransformDispatches(relative, matrix)
                      << ",\"caller_dispatches\":1,\"scope\":\"Normal-core base work only; no scheduler, IRQ or device timing\"}\n";
            return 0;
        }
        if (
            argc == 5
            && std::string(argv[1]) == "--render-level1-audio-event"
        ) {
            const std::filesystem::path root(argv[2]);
            stellar7::PcSpeakerSynth synth(
                stellar7::decodeTaggedPayloads(
                    readFile((root / "COCKPIT.RES").string()),
                    std::array<char, 4>{'S', 'N', 'G', ':'}
                )
            );
            synth.triggerLevel1Event(std::stoi(argv[3]));
            writeWave(
                argv[4],
                synth.render(stellar7::PcSpeakerSynth::sampleRate * 2)
            );
            std::cout << "wrote " << argv[4] << '\n';
            return 0;
        }
        if (
            argc == 4
            && std::string(argv[1]) == "--render-pc-speaker-music"
        ) {
            const std::filesystem::path root(argv[2]);
            stellar7::PcSpeakerSynth synth(
                stellar7::decodeTaggedPayloads(
                    readFile((root / "COCKPIT.RES").string()),
                    std::array<char, 4>{'S', 'N', 'G', ':'}
                )
            );
            synth.startMusic();
            writeWave(
                argv[3],
                synth.render(stellar7::PcSpeakerSynth::sampleRate * 10)
            );
            std::cout << "wrote " << argv[3] << '\n';
            return 0;
        }
        if (
            argc == 4
            && std::string(argv[1]) == "--render-sound-blaster-music"
        ) {
            const std::filesystem::path root(argv[2]);
            auto instrumentResources = stellar7::decodeTaggedPayloads(
                readFile((root / "STELLAR.RES").string()),
                std::array<char, 4>{'0', '0', '3', ':'}
            );
            if (instrumentResources.empty()) {
                throw std::runtime_error(
                    "STELLAR.RES lacks the Sound Blaster instrument bank"
                );
            }
            stellar7::SoundBlasterSynth synth(
                stellar7::decodeTaggedPayloads(
                    readFile((root / "COCKPIT.RES").string()),
                    std::array<char, 4>{'S', 'N', 'G', ':'}
                ),
                std::move(instrumentResources.front()),
                [&root]() {
                    auto music = stellar7::decodeTaggedPayloads(
                        readFile((root / "LEVEL1.RES").string()),
                        std::array<char, 4>{'S', 'N', 'G', ':'}
                    );
                    return music.empty()
                        ? std::vector<std::uint8_t>{}
                        : std::move(music.front());
                }()
            );
            synth.startMusic();
            writeWave(
                argv[3],
                synth.render(stellar7::SoundBlasterSynth::sampleRate * 10)
            );
            std::cout << "wrote " << argv[3] << '\n';
            return 0;
        }
        if (
            argc == 4
            && std::string(argv[1]) == "--render-sound-blaster-success"
        ) {
            const std::filesystem::path root(argv[2]);
            auto instrumentResources = stellar7::decodeTaggedPayloads(
                readFile((root / "STELLAR.RES").string()),
                std::array<char, 4>{'0', '0', '3', ':'}
            );
            if (instrumentResources.empty()) {
                throw std::runtime_error(
                    "STELLAR.RES lacks the Sound Blaster instrument bank"
                );
            }
            auto presentationResources = stellar7::decodeTaggedPayloads(
                readFile((root / "STELLAR.RES").string()),
                std::array<char, 4>{'S', 'N', 'G', ':'}
            );
            auto levelMusicResources = stellar7::decodeTaggedPayloads(
                readFile((root / "LEVEL1.RES").string()),
                std::array<char, 4>{'S', 'N', 'G', ':'}
            );
            stellar7::SoundBlasterSynth synth(
                stellar7::decodeTaggedPayloads(
                    readFile((root / "COCKPIT.RES").string()),
                    std::array<char, 4>{'S', 'N', 'G', ':'}
                ),
                std::move(instrumentResources.front()),
                levelMusicResources.empty()
                    ? std::vector<std::uint8_t>{}
                    : std::move(levelMusicResources.front()),
                presentationResources.size() < 2
                    ? std::vector<std::uint8_t>{}
                    : std::move(presentationResources[1])
            );
            synth.startLevel1();
            synth.startSuccess();
            writeWave(
                argv[3],
                synth.render(stellar7::SoundBlasterSynth::sampleRate * 6)
            );
            std::cout << "wrote " << argv[3] << '\n';
            return 0;
        }
        if (
            argc == 4
            && std::string(argv[1])
                == "--trace-sound-blaster-success-writes"
        ) {
            const std::filesystem::path root(argv[2]);
            auto instrumentResources = stellar7::decodeTaggedPayloads(
                readFile((root / "STELLAR.RES").string()),
                std::array<char, 4>{'0', '0', '3', ':'}
            );
            if (instrumentResources.empty()) {
                throw std::runtime_error(
                    "STELLAR.RES lacks the Sound Blaster instrument bank"
                );
            }
            auto presentationResources = stellar7::decodeTaggedPayloads(
                readFile((root / "STELLAR.RES").string()),
                std::array<char, 4>{'S', 'N', 'G', ':'}
            );
            auto levelMusicResources = stellar7::decodeTaggedPayloads(
                readFile((root / "LEVEL1.RES").string()),
                std::array<char, 4>{'S', 'N', 'G', ':'}
            );
            stellar7::SoundBlasterSynth synth(
                stellar7::decodeTaggedPayloads(
                    readFile((root / "COCKPIT.RES").string()),
                    std::array<char, 4>{'S', 'N', 'G', ':'}
                ),
                std::move(instrumentResources.front()),
                levelMusicResources.empty()
                    ? std::vector<std::uint8_t>{}
                    : std::move(levelMusicResources.front()),
                presentationResources.size() < 2
                    ? std::vector<std::uint8_t>{}
                    : std::move(presentationResources[1])
            );
            synth.startLevel1();
            synth.startSuccess();
            synth.render(stellar7::SoundBlasterSynth::sampleRate * 6);
            std::vector<std::uint8_t> pairs;
            pairs.reserve(synth.registerWrites().size() * 2);
            for (const auto write : synth.registerWrites()) {
                pairs.push_back(write.registerNumber);
                pairs.push_back(write.value);
            }
            writeBytes(argv[3], pairs);
            std::cout << "wrote " << argv[3] << '\n';
            return 0;
        }
        if (argc == 5 && std::string(argv[1]) == "--trace-sound-blaster-success-lifetime") {
            const auto assets = loadGameplayAssets(argv[2], argv[3]);
            stellar7::SoundBlasterSynth synth(
                assets.soundSequences, assets.soundBlasterInstrumentResource,
                assets.musicForLevel(0), assets.presentationMusicSequence,
                assets.musicForLevel(1), assets.musicForLevel(2), assets.logoMusicSequence
            );
            synth.startLevel2();
            synth.startSuccess();
            synth.clearRegisterWrites();
            std::ostringstream timedWrites;
            bool firstWrite = true;
            std::size_t samples = 0;
            // Single-sample stepping identifies the FC/automatic-shutdown
            // boundary without a frame-size-dependent rounding assumption.
            while (synth.successActive() && samples < stellar7::SoundBlasterSynth::sampleRate * 10) {
                synth.render(1);
                for (const auto write : synth.registerWrites()) {
                    timedWrites << (firstWrite ? "" : ",")
                        << "{\"sample\":" << samples
                        << ",\"register\":" << static_cast<unsigned>(write.registerNumber)
                        << ",\"value\":" << static_cast<unsigned>(write.value) << '}';
                    firstWrite = false;
                }
                synth.clearRegisterWrites();
                ++samples;
            }
            if (synth.successActive()) {
                throw std::runtime_error("success sequence did not finish within ten seconds");
            }
            writeText(argv[4], "{\"sample_rate\":"
                + std::to_string(stellar7::SoundBlasterSynth::sampleRate)
                + ",\"samples_through_end\":" + std::to_string(samples)
                + ",\"interactive_frames\":" + std::to_string((samples + 1454) / 1455)
                + ",\"scope\":\"Isolated cue after direct Level2 bootstrap; startup and explicit release writes excluded\""
                + ",\"writes\":[" + timedWrites.str() + "]"
                + "}\n");
            return 0;
        }
        if (argc == 4 && std::string(argv[1]) == "--decode-first-screen") {
            const auto resource = readFile(argv[2]);
            writePpm(argv[3], stellar7::decodeFirstScreenResource(resource));
            std::cout << "wrote " << argv[3] << '\n';
            return 0;
        }
        if (
            argc == 5
            && std::string(argv[1]) == "--decode-screen-with-palette"
        ) {
            writePpm(
                argv[4],
                loadExternalPaletteScreen(argv[2], argv[3])
            );
            std::cout << "wrote " << argv[4] << '\n';
            return 0;
        }
        if (
            argc == 5
            && std::string(argv[1]) == "--render-gameplay-frame"
        ) {
            const auto assets = loadGameplayAssets(argv[2], argv[3]);
            stellar7::GameplayModel model;
            model.tick({false, false, false, false, true});
            writePpm(argv[4], stellar7::renderGameplay(assets, model));
            std::cout << "wrote " << argv[4] << '\n';
            return 0;
        }
        if (
            argc == 5
            && std::string(argv[1]) == "--render-gameplay-indexed"
        ) {
            const auto assets = loadGameplayAssets(argv[2], argv[3]);
            writeBytes(
                argv[4],
                stellar7::renderGameplayIndexed(
                    assets,
                    stellar7::GameplayModel{}
                ).pixels
            );
            std::cout << "wrote " << argv[4] << '\n';
            return 0;
        }
        if (
            argc == 6
            && std::string(argv[1]) == "--render-level3-indexed"
        ) {
            const auto assets = loadGameplayAssets(argv[2], argv[3]);
            stellar7::GameplayModel model;
            model.startLevel3();
            const auto ticks = static_cast<std::uint32_t>(
                std::stoul(argv[4])
            );
            for (std::uint32_t tick = 0; tick < ticks; ++tick) {
                model.tick({});
            }
            writeBytes(
                argv[5],
                stellar7::renderGameplayIndexed(assets, model).pixels
            );
            std::cout << "wrote " << argv[5] << '\n';
            return 0;
        }
        if (
            (argc == 6 || argc == 7)
            && std::string(argv[1])
                == "--render-level3-native-guardian-indexed"
        ) {
            auto assets = loadGameplayAssets(
                argv[2],
                argv[3],
                argc == 7 ? argv[6] : ""
            );
            assets.useUnprovenLevel3GuardianSurface = true;
            stellar7::GameplayModel model;
            stellar7::GameplayAnalysisAccess::loadLevel3NativeCheckpoint(
                model,
                readFile(argv[4])
            );
            writeBytes(
                argv[5],
                stellar7::renderGameplayIndexed(assets, model).pixels
            );
            std::cout << "wrote " << argv[5] << '\n';
            return 0;
        }
        if (
            (argc == 8 || argc == 9)
            && std::string(argv[1])
                == "--render-level3-native-input-script-indexed"
        ) {
            auto assets = loadGameplayAssets(
                argv[2],
                argv[3],
                argc == 9 ? argv[8] : ""
            );
            assets.useUnprovenLevel3GuardianSurface = true;
            assets.useUnprovenLevel3SuccessSurfaces = true;
            renderLevel3NativeInputScriptIndexed(
                assets,
                readFile(argv[4]),
                loadLevel1InputScript(argv[5]),
                static_cast<std::uint32_t>(std::stoul(argv[6])),
                argv[7]
            );
            std::cout << "wrote " << argv[7] << '\n';
            return 0;
        }
        if (argc == 8 && std::string(argv[1])
                == "--render-level3-native-input-script-clean-indexed") {
            const auto assets = loadGameplayAssets(argv[2], argv[3]);
            renderLevel3NativeInputScriptIndexed(
                assets, readFile(argv[4]), loadLevel1InputScript(argv[5]),
                static_cast<std::uint32_t>(std::stoul(argv[6])), argv[7]
            );
            return 0;
        }
        if (
            (argc == 8 || argc == 9)
            && std::string(argv[1])
                == "--trace-level3-native-input-script-model-draws"
        ) {
            const auto assets = loadGameplayAssets(
                argv[2],
                argv[3],
                argc == 9 ? argv[8] : ""
            );
            const auto model = replayLevel3NativeInputScript(
                readFile(argv[4]),
                loadLevel1InputScript(argv[5]),
                static_cast<std::uint32_t>(std::stoul(argv[6]))
            );
            writeText(
                argv[7],
                stellar7::level2GameplayModelDrawTraceJson(assets, model)
                    + "\n"
            );
            std::cout << "wrote " << argv[7] << '\n';
            return 0;
        }
        if (
            (argc == 7 || argc == 8)
                && std::string(argv[1])
                == "--render-level3-native-guardian-history-indexed"
        ) {
            auto assets = loadGameplayAssets(
                argv[2],
                argv[3],
                argc == 8 ? argv[7] : ""
            );
            assets.useUnprovenLevel3GuardianSurface = true;
            stellar7::GameplayModel model;
            stellar7::GameplayAnalysisAccess::loadLevel3NativeCheckpoint(
                model,
                readFile(argv[4])
            );
            stellar7::IndexedScreen previous{
                readFile(argv[5]),
                assets.cockpit.palette,
            };
            writeBytes(
                argv[6],
                stellar7::renderGameplayIndexedWithHistory(
                    assets,
                    model,
                    previous
                ).pixels
            );
            std::cout << "wrote " << argv[6] << '\n';
            return 0;
        }
        if (
            (argc == 7 || argc == 8)
                && std::string(argv[1])
                == "--render-level3-native-guardian-page-history-indexed"
        ) {
            auto assets = loadGameplayAssets(
                argv[2],
                argv[3],
                argc == 8 ? argv[7] : ""
            );
            assets.useUnprovenLevel3GuardianSurface = true;
            stellar7::GameplayModel model;
            stellar7::GameplayAnalysisAccess::loadLevel3NativeCheckpoint(
                model,
                readFile(argv[4])
            );
            stellar7::IndexedScreen previous{
                readFile(argv[5]),
                assets.cockpit.palette,
            };
            writeBytes(
                argv[6],
                stellar7::renderGameplayIndexedWithPageHistory(
                    assets,
                    model,
                    previous
                ).pixels
            );
            std::cout << "wrote " << argv[6] << '\n';
            return 0;
        }
        if (
            (argc == 6 || argc == 7)
                && std::string(argv[1])
                == "--trace-level3-native-guardian-model-draws"
        ) {
            const auto assets = loadGameplayAssets(
                argv[2],
                argv[3],
                argc == 7 ? argv[6] : ""
            );
            stellar7::GameplayModel model;
            stellar7::GameplayAnalysisAccess::loadLevel3NativeCheckpoint(
                model,
                readFile(argv[4])
            );
            writeText(
                argv[5],
                stellar7::level2GameplayModelDrawTraceJson(assets, model)
                    + "\n"
            );
            std::cout << "wrote " << argv[5] << '\n';
            return 0;
        }
        if (
            (argc == 6 || argc == 7)
                && std::string(argv[1])
                == "--trace-level3-native-guardian-radar"
        ) {
            const auto assets = loadGameplayAssets(
                argv[2],
                argv[3],
                argc == 7 ? argv[6] : ""
            );
            (void)assets;
            stellar7::GameplayModel model;
            stellar7::GameplayAnalysisAccess::loadLevel3NativeCheckpoint(
                model,
                readFile(argv[4])
            );
            writeText(
                argv[5],
                stellar7::level2RadarContactTraceJson(model)
                    + "\n"
            );
            std::cout << "wrote " << argv[5] << '\n';
            return 0;
        }
        if (
            (argc == 6 || argc == 7)
                && std::string(argv[1]) == "--render-level3-success-indexed"
        ) {
            auto assets = loadGameplayAssets(
                argv[2],
                argv[3],
                argc == 7 ? argv[6] : ""
            );
            assets.useUnprovenLevel3SuccessSurfaces = true;
            stellar7::GameplayModel model;
            stellar7::GameplayAnalysisAccess::prepareLevel3Success(
                model,
                static_cast<unsigned>(std::stoul(argv[4]))
            );
            writeBytes(
                argv[5],
                stellar7::renderGameplayIndexed(assets, model).pixels
            );
            std::cout << "wrote " << argv[5] << '\n';
            return 0;
        }
        if (
            (argc == 6 || argc == 7)
            && std::string(argv[1])
                == "--render-level3-contact-success-indexed"
        ) {
            auto assets = loadGameplayAssets(
                argv[2],
                argv[3],
                argc == 7 ? argv[6] : ""
            );
            assets.useUnprovenLevel3SuccessSurfaces = true;
            stellar7::GameplayModel model;
            stellar7::GameplayAnalysisAccess::prepareLevel3PickupSuccess(
                model,
                static_cast<unsigned>(std::stoul(argv[4]))
            );
            writeBytes(
                argv[5],
                stellar7::renderGameplayIndexed(assets, model).pixels
            );
            std::cout << "wrote " << argv[5] << '\n';
            return 0;
        }
        if (
            argc == 5
            && std::string(argv[1])
                == "--render-level1-failure-prompt-indexed"
        ) {
            const auto assets = loadGameplayAssets(argv[2], argv[3]);
            stellar7::GameplayModel model;
            for (
                unsigned step = 0;
                step < 2000
                    && model.phase() != stellar7::GameplayPhase::continuePrompt;
                ++step
            ) {
                model.tick({});
            }
            if (model.phase() != stellar7::GameplayPhase::continuePrompt) {
                throw std::runtime_error(
                    "natural idle failure did not reach the Continue prompt"
                );
            }
            writeBytes(
                argv[4],
                stellar7::renderGameplayIndexed(assets, model).pixels
            );
            std::cout << "wrote " << argv[4] << '\n';
            return 0;
        }
        if (
            argc == 6
            && std::string(argv[1])
                == "--trace-level1-failure-model-draws"
        ) {
            const auto assets = loadGameplayAssets(argv[2], argv[3]);
            const auto targetFrame = static_cast<unsigned>(
                std::stoul(argv[4])
            );
            if (
                targetFrame
                >= stellar7::GameplayModel::failureExplosionFrames
            ) {
                throw std::runtime_error(
                    "failure frame must be in the range 0 through 44"
                );
            }
            stellar7::GameplayModel model;
            for (
                unsigned step = 0;
                step < 2000
                    && model.phase()
                        != stellar7::GameplayPhase::failureExplosion;
                ++step
            ) {
                model.tick({});
            }
            if (
                model.phase()
                != stellar7::GameplayPhase::failureExplosion
            ) {
                throw std::runtime_error(
                    "natural idle failure did not reach its presenter"
                );
            }
            while (model.presentationFrame() < targetFrame) {
                model.tick({});
            }
            writeText(
                argv[5],
                stellar7::level1FailureModelDrawTraceJson(assets, model)
                    + "\n"
            );
            std::cout << "wrote " << argv[5] << '\n';
            return 0;
        }
        if (
            argc == 6
            && std::string(argv[1])
                == "--render-level1-failure-frame-indexed"
        ) {
            const auto assets = loadGameplayAssets(argv[2], argv[3]);
            const auto targetFrame = static_cast<unsigned>(
                std::stoul(argv[4])
            );
            if (
                targetFrame
                >= stellar7::GameplayModel::failureExplosionFrames
            ) {
                throw std::runtime_error(
                    "failure frame must be in the range 0 through 44"
                );
            }
            stellar7::GameplayModel model;
            for (
                unsigned step = 0;
                step < 2000
                    && model.phase()
                        != stellar7::GameplayPhase::failureExplosion;
                ++step
            ) {
                model.tick({});
            }
            if (
                model.phase()
                != stellar7::GameplayPhase::failureExplosion
            ) {
                throw std::runtime_error(
                    "natural idle failure did not reach its presenter"
                );
            }
            while (model.presentationFrame() < targetFrame) {
                model.tick({});
            }
            writeBytes(
                argv[5],
                stellar7::renderGameplayIndexed(assets, model).pixels
            );
            std::cout << "wrote " << argv[5] << '\n';
            return 0;
        }
        if (
            argc == 7
            && std::string(argv[1])
                == "--render-level1-input-script-indexed"
        ) {
            writeLevel1InputScriptIndexedFrame(
                loadGameplayAssets(argv[2], argv[3]),
                loadLevel1InputScript(argv[4]),
                static_cast<std::uint32_t>(std::stoul(argv[5])),
                argv[6]
            );
            std::cout << "wrote " << argv[6] << '\n';
            return 0;
        }
        if (
            argc == 8
            && (std::string(argv[1])
                == "--trace-gameplay-input-script-model-draws-armored"
                || std::string(argv[1])
                == "--trace-gameplay-input-script-model-draws-armor-held")
        ) {
            const auto parsedArmor = std::stol(argv[5]);
            if (
                parsedArmor < std::numeric_limits<std::int16_t>::min()
                || parsedArmor > std::numeric_limits<std::int16_t>::max()
            ) {
                throw std::runtime_error("Level 2 armor is out of range");
            }
            writeLevel1InputScriptModelDrawTrace(
                loadGameplayAssets(argv[2], argv[3]),
                loadLevel1InputScript(argv[4]),
                static_cast<std::uint32_t>(std::stoul(argv[6])),
                argv[7],
                static_cast<std::int16_t>(parsedArmor),
                std::string(argv[1])
                    == "--trace-gameplay-input-script-model-draws-armor-held"
            );
            std::cout << "wrote " << argv[7] << '\n';
            return 0;
        }
        if (
            argc == 8
            && (std::string(argv[1])
                == "--render-level2-input-script-indexed-armored"
                || std::string(argv[1])
                == "--render-level2-input-script-indexed-armor-held")
        ) {
            writeLevel1InputScriptIndexedFrame(
                loadGameplayAssets(argv[2], argv[3]),
                loadLevel1InputScript(argv[4]),
                static_cast<std::uint32_t>(std::stoul(argv[6])),
                argv[7],
                static_cast<std::int16_t>(std::stoi(argv[5])),
                std::string(argv[1])
                    == "--render-level2-input-script-indexed-armor-held"
            );
            std::cout << "wrote " << argv[7] << '\n';
            return 0;
        }
        if (argc == 8 && std::string(argv[1])
            == "--render-level2-input-script-display-armor-held") {
            const auto armor = std::stol(argv[5]);
            if (armor < -32768 || armor > 32767) {
                throw std::runtime_error("Level 2 armor is out of range");
            }
            const auto model = replayLevel1InputScriptGameplay(
                loadLevel1InputScript(argv[4]),
                static_cast<std::uint32_t>(std::stoul(argv[6])),
                static_cast<std::int16_t>(armor), {}, true);
            const auto frame = stellar7::renderGameplayIndexed(
                loadGameplayAssets(argv[2], argv[3]), model);
            writeBytes(std::string(argv[7]) + ".bin", frame.pixels);
            std::vector<std::uint8_t> dac;
            for (const auto color : frame.palette) {
                dac.push_back(collapseVgaDacComponent(color.red));
                dac.push_back(collapseVgaDacComponent(color.green));
                dac.push_back(collapseVgaDacComponent(color.blue));
            }
            writeBytes(std::string(argv[7]) + ".dac", dac);
            return 0;
        }
        if (
            argc == 14
            && std::string(argv[1])
                == "--render-level2-controlled-actor-indexed"
        ) {
            const auto assets = loadGameplayAssets(argv[2], argv[3]);
            const auto model = replayLevel2ControlledActorWarp(
                loadLevel1InputScript(argv[4]),
                static_cast<std::uint32_t>(std::stoul(argv[5])),
                static_cast<std::uint16_t>(std::stoul(argv[6])),
                static_cast<std::size_t>(std::stoull(argv[7])),
                {
                    static_cast<std::int32_t>(std::stol(argv[8])),
                    static_cast<std::int32_t>(std::stol(argv[9])),
                    static_cast<std::int32_t>(std::stol(argv[10])),
                },
                static_cast<std::uint8_t>(std::stoul(argv[11])),
                static_cast<std::int16_t>(std::stoi(argv[12]))
            );
            writeBytes(
                argv[13],
                stellar7::renderGameplayIndexed(assets, model).pixels
            );
            std::cout << "wrote " << argv[13] << '\n';
            return 0;
        }
        if (
            argc == 7
            && std::string(argv[1])
                == "--render-level1-input-script-rgb"
        ) {
            writeLevel1InputScriptRgbFrame(
                loadGameplayAssets(argv[2], argv[3]),
                loadLevel1InputScript(argv[4]),
                static_cast<std::uint32_t>(std::stoul(argv[5])),
                argv[6]
            );
            std::cout << "wrote " << argv[6] << '\n';
            return 0;
        }
        if (
            argc == 7
            && std::string(argv[1])
                == "--render-level1-input-script-frame"
        ) {
            const auto assets = loadGameplayAssets(argv[2], argv[3]);
            const auto model = replayLevel1InputScriptGameplay(
                loadLevel1InputScript(argv[4]),
                static_cast<std::uint32_t>(std::stoul(argv[5]))
            );
            writePpm(argv[6], stellar7::renderGameplay(assets, model));
            std::cout << "wrote " << argv[6] << '\n';
            return 0;
        }
        if (
            argc == 8
            && std::string(argv[1])
                == "--render-level1-input-script-rgb-sequence"
        ) {
            writeLevel1InputScriptRgbSequence(
                loadGameplayAssets(argv[2], argv[3]),
                loadLevel1InputScript(argv[4]),
                static_cast<std::uint32_t>(std::stoul(argv[5])),
                argv[6],
                argv[7]
            );
            std::cout
                << "wrote " << argv[6] << '\n'
                << "wrote " << argv[7] << '\n';
            return 0;
        }
        if (
            argc == 7
            && std::string(argv[1])
                == "--render-level1-input-script-indexed-sequence"
        ) {
            writeLevel1InputScriptIndexedSequence(
                loadGameplayAssets(argv[2], argv[3]),
                loadLevel1InputScript(argv[4]),
                static_cast<std::uint32_t>(std::stoul(argv[5])),
                argv[6]
            );
            std::cout << "wrote " << argv[6] << '\n';
            return 0;
        }
        if (
            argc == 7
            && std::string(argv[1])
                == "--render-level1-input-script-dac-sequence"
        ) {
            writeLevel1InputScriptDacSequence(
                loadGameplayAssets(argv[2], argv[3]),
                loadLevel1InputScript(argv[4]),
                static_cast<std::uint32_t>(std::stoul(argv[5])),
                argv[6]
            );
            std::cout << "wrote " << argv[6] << '\n';
            return 0;
        }
        if (
            argc == 7
            && std::string(argv[1])
                == "--render-level1-input-script-sound-blaster"
        ) {
            writeLevel1InputScriptSoundBlaster(
                loadGameplayAssets(argv[2], argv[3]),
                loadLevel1InputScript(argv[4]),
                static_cast<std::uint32_t>(std::stoul(argv[5])),
                argv[6]
            );
            std::cout << "wrote " << argv[6] << '\n';
            return 0;
        }
        if (
            argc == 8
            && std::string(argv[1])
                == "--render-level1-input-script-sound-blaster-armored"
        ) {
            const auto parsedArmor = std::stol(argv[3]);
            if (
                parsedArmor < std::numeric_limits<std::int16_t>::min()
                || parsedArmor > std::numeric_limits<std::int16_t>::max()
            ) {
                throw std::runtime_error("Level 2 armor is out of range");
            }
            writeLevel1InputScriptSoundBlaster(
                loadGameplayAssets(argv[2], argv[4]),
                loadLevel1InputScript(argv[5]),
                static_cast<std::uint32_t>(std::stoul(argv[6])),
                argv[7],
                static_cast<std::int16_t>(parsedArmor)
            );
            std::cout << "wrote " << argv[7] << '\n';
            return 0;
        }
        if (
            argc == 7
            && std::string(argv[1])
                == "--render-level2-input-script-sound-blaster"
        ) {
            writeLevel2InputScriptSoundBlaster(
                loadGameplayAssets(argv[2], argv[3]),
                loadLevel1InputScript(argv[4]),
                static_cast<std::uint32_t>(std::stoul(argv[5])),
                argv[6]
            );
            std::cout << "wrote " << argv[6] << '\n';
            return 0;
        }
        if ((argc == 5 && std::string(argv[1]) == "--play-controlled-level2") ||
            (argc == 6 && std::string(argv[1]) == "--smoke-controlled-level2") ||
            (argc == 5 && std::string(argv[1]) == "--play-controlled-level2-input") ||
            (argc == 6 && std::string(argv[1]) == "--smoke-controlled-level2-input")) {
            const auto hostInput = std::string(argv[1]) == "--play-controlled-level2-input"
                || std::string(argv[1]) == "--smoke-controlled-level2-input";
            const auto frames = std::stoul(argv[4]);
            if (frames == 0 || frames > controlledSessionFullFrames) {
                throw std::invalid_argument("controlled playback requires 1..64 admitted full frames");
            }
            const auto input = readClockedRenderInput(ClockedRenderKind::worldRadar, argv[2],
                ClockedRenderEnd::nextFrame, controlledSessionFullFrames + 1);
            auto request = controlledBootstrapRequest(loadLevel1InputScript(argv[3]), input, static_cast<unsigned>(frames));
            const auto visibility = argc == 6 ? stellar7::ControlledFrontendVisibility::hidden
                : stellar7::ControlledFrontendVisibility::shown;
            const auto result = [&]() {
                if (hostInput) {
                    return stellar7::runControlledInput(loadGameplayAssets(argv[2]), std::move(request),
                        static_cast<unsigned>(frames), visibility);
                }
                stellar7::ControlledLevel2Session session(loadGameplayAssets(argv[2]), std::move(request));
                return stellar7::runControlledSession(session, static_cast<unsigned>(frames), visibility);
            }();
            if (argc == 6) {
                std::ofstream out(argv[5], std::ios::binary);
                out << "{\"cancelled\":" << (result.cancelled ? "true" : "false")
                    << ",\"frames\":" << result.frames << ",\"samples_submitted\":" << result.samplesSubmitted
                    << ",\"samples_played\":" << result.samplesPlayed << ",\"scans_presented\":" << result.scansPresented
                    << ",\"gdi_blits\":" << result.gdiBlits;
                if (hostInput) {
                    out << ",\"keyboard_returns\":" << result.keyboardReturns << ",\"keyboard_edges\":" << result.keyboardEdges
                        << ",\"keyboard_events\":[";
                    for (std::size_t i = 0; i < result.keyboardEvents.size(); ++i) {
                        const auto& event = result.keyboardEvents[i];
                        out << (i == 0 ? "" : ",") << '[' << event.loopTick << ',' << static_cast<unsigned>(event.edge) << ']';
                    }
                    out << ']';
                }
                out << "}\n";
                if (!out) { throw std::runtime_error("cannot write controlled frontend receipt"); }
            }
            return result.cancelled ? 2 : 0;
        }
        if (argc == 7 && (std::string(argv[1]) == "--verify-controlled-level2-input-session"
            || std::string(argv[1]) == "--trace-controlled-level2-input-session")) {
            const auto frames = std::stoul(argv[5]);
            if (frames == 0 || frames > controlledSessionTraceFrames) {
                throw std::invalid_argument("controlled input requires 1..80 bounded frames");
            }
            const auto input = readClockedRenderInput(ClockedRenderKind::worldRadar, argv[2],
                ClockedRenderEnd::nextFrame, controlledSessionTraceFrames + 1);
            writeControlledSessionTrace(loadGameplayAssets(argv[2]), loadLevel1InputScript(argv[3]), input,
                static_cast<unsigned>(frames), argv[6], std::string(argv[1]) == "--verify-controlled-level2-input-session"
                    ? ControlledSessionCheck::deliveryAndFailure : ControlledSessionCheck::trace,
                stellar7::ControlledArrivalInput::hostForward, loadReturnKeys(argv[4]));
            return 0;
        }
        if (argc == 6 && (std::string(argv[1]) == "--trace-controlled-level2-session"
            || std::string(argv[1]) == "--verify-controlled-level2-session"
            || std::string(argv[1]) == "--trace-controlled-level2-forward-session"
            || std::string(argv[1]) == "--verify-controlled-level2-forward-session"
            || std::string(argv[1]) == "--trace-controlled-level2-keyboard-session"
            || std::string(argv[1]) == "--verify-controlled-level2-keyboard-session")) {
            const auto command = std::string(argv[1]);
            const auto forward = command == "--trace-controlled-level2-forward-session"
                || command == "--verify-controlled-level2-forward-session";
            const auto physical = command == "--trace-controlled-level2-keyboard-session"
                || command == "--verify-controlled-level2-keyboard-session";
            constexpr unsigned controlledForwardFrames = 2;
            const auto frames = std::stoul(argv[4]);
            if (frames == 0 || frames > controlledSessionFullFrames || ((forward || physical) && frames > controlledForwardFrames)) {
                throw std::invalid_argument("controlled session requires 1..64 released or 1..2 resident-forward frames");
            }
            const auto clocked = readClockedRenderInput(ClockedRenderKind::worldRadar, argv[2],
                ClockedRenderEnd::nextFrame, controlledSessionFullFrames + 1);
            writeControlledSessionTrace(loadGameplayAssets(argv[2]), loadLevel1InputScript(argv[3]),
                clocked, static_cast<unsigned>(frames), argv[5],
                command == "--verify-controlled-level2-session" || command == "--verify-controlled-level2-forward-session"
                    || command == "--verify-controlled-level2-keyboard-session"
                    ? ControlledSessionCheck::deliveryAndFailure : ControlledSessionCheck::trace,
                physical ? stellar7::ControlledArrivalInput::physicalForwardPair
                    : forward ? stellar7::ControlledArrivalInput::residentForward : stellar7::ControlledArrivalInput::released);
            return 0;
        }
        if (
            argc == 7
            && std::string(argv[1])
                == "--trace-level1-input-script-sound-blaster-writes"
        ) {
            writeLevel1InputScriptSoundBlasterTrace(
                loadGameplayAssets(argv[2], argv[3]),
                loadLevel1InputScript(argv[4]),
                static_cast<std::uint32_t>(std::stoul(argv[5])),
                argv[6]
            );
            std::cout << "wrote " << argv[6] << '\n';
            return 0;
        }
        if (
            (argc == 8 || argc == 9 || (argc == 10 && std::string(argv[1]) == "--trace-campaign-clocked-world-loop"))
            && (std::string(argv[1])
                == "--trace-level1-input-script-sound-blaster-writes-armored"
                || std::string(argv[1])
                == "--trace-level1-input-script-sound-blaster-writes-armor-held"
                || std::string(argv[1]) == "--trace-campaign-sound-blaster-work-state"
                || std::string(argv[1]) == "--trace-campaign-clocked-cockpit"
                || std::string(argv[1]) == "--trace-campaign-clocked-horizon"
                || std::string(argv[1]) == "--trace-campaign-clocked-horizon-ground"
                || std::string(argv[1]) == "--trace-campaign-clocked-world-setup"
                    || std::string(argv[1]) == "--trace-campaign-clocked-world-traversal"
                    || std::string(argv[1]) == "--trace-campaign-clocked-world-scene"
                    || std::string(argv[1]) == "--trace-campaign-clocked-world-sight"
                    || std::string(argv[1]) == "--trace-campaign-clocked-world-cockpit"
                    || (std::string(argv[1]) == "--trace-campaign-clocked-world-radar"
                        || std::string(argv[1]) == "--trace-campaign-clocked-world-display"
                        || std::string(argv[1]) == "--trace-campaign-clocked-world-loop"))
        ) {
            const auto parsedArmor = std::stol(argv[3]);
            if (
                parsedArmor < std::numeric_limits<std::int16_t>::min()
                || parsedArmor > std::numeric_limits<std::int16_t>::max()
            ) {
                throw std::runtime_error("Level 2 armor is out of range");
            }
            writeLevel1InputScriptSoundBlasterTrace(
                loadGameplayAssets(argv[2], argv[4]),
                loadLevel1InputScript(argv[5]),
                static_cast<std::uint32_t>(std::stoul(argv[6])),
                argv[7],
                static_cast<std::int16_t>(parsedArmor),
                std::string(argv[1]) != "--trace-level1-input-script-sound-blaster-writes-armored",
                argc >= 9 ? std::optional<std::size_t>{std::stoull(argv[8])} : std::nullopt,
                (std::string(argv[1]) == "--trace-campaign-sound-blaster-work-state"
                    || std::string(argv[1]) == "--trace-campaign-clocked-cockpit"
                    || std::string(argv[1]) == "--trace-campaign-clocked-horizon"
                    || std::string(argv[1]) == "--trace-campaign-clocked-horizon-ground"
                    || std::string(argv[1]) == "--trace-campaign-clocked-world-setup"
                    || std::string(argv[1]) == "--trace-campaign-clocked-world-traversal"
                    || std::string(argv[1]) == "--trace-campaign-clocked-world-scene"
                    || std::string(argv[1]) == "--trace-campaign-clocked-world-sight"
                    || std::string(argv[1]) == "--trace-campaign-clocked-world-cockpit"
                    || (std::string(argv[1]) == "--trace-campaign-clocked-world-radar"
                        || std::string(argv[1]) == "--trace-campaign-clocked-world-display"
                        || std::string(argv[1]) == "--trace-campaign-clocked-world-loop"))
                    ? SoundBlasterCampaignTrace::ownedWorkState : SoundBlasterCampaignTrace::legacy,
                (std::string(argv[1]) == "--trace-campaign-clocked-cockpit"
                    || std::string(argv[1]) == "--trace-campaign-clocked-horizon"
                    || std::string(argv[1]) == "--trace-campaign-clocked-horizon-ground"
                    || std::string(argv[1]) == "--trace-campaign-clocked-world-setup"
                    || std::string(argv[1]) == "--trace-campaign-clocked-world-traversal"
                    || std::string(argv[1]) == "--trace-campaign-clocked-world-scene"
                    || std::string(argv[1]) == "--trace-campaign-clocked-world-sight"
                    || std::string(argv[1]) == "--trace-campaign-clocked-world-cockpit"
                    || (std::string(argv[1]) == "--trace-campaign-clocked-world-radar"
                        || std::string(argv[1]) == "--trace-campaign-clocked-world-display"
                        || std::string(argv[1]) == "--trace-campaign-clocked-world-loop"))
                    ? std::optional<ClockedRenderInput>{readClockedRenderInput(
                        (std::string(argv[1]) == "--trace-campaign-clocked-world-radar"
                        || std::string(argv[1]) == "--trace-campaign-clocked-world-display"
                        || std::string(argv[1]) == "--trace-campaign-clocked-world-loop")
                            ? ClockedRenderKind::worldRadar : std::string(argv[1]) == "--trace-campaign-clocked-world-cockpit"
                            ? ClockedRenderKind::worldCockpit : std::string(argv[1]) == "--trace-campaign-clocked-world-sight"
                            ? ClockedRenderKind::worldSight : std::string(argv[1]) == "--trace-campaign-clocked-world-scene"
                            ? ClockedRenderKind::worldScene : std::string(argv[1]) == "--trace-campaign-clocked-world-traversal"
                            ? ClockedRenderKind::worldTraversal : std::string(argv[1]) == "--trace-campaign-clocked-world-setup"
                            ? ClockedRenderKind::worldSetup : std::string(argv[1]) == "--trace-campaign-clocked-horizon-ground"
                            ? ClockedRenderKind::horizonGround : std::string(argv[1]) == "--trace-campaign-clocked-horizon"
                            ? ClockedRenderKind::horizon : ClockedRenderKind::cockpit, argv[2],
                        std::string(argv[1]) == "--trace-campaign-clocked-world-loop"
                            ? ClockedRenderEnd::nextFrame : std::string(argv[1]) == "--trace-campaign-clocked-world-display"
                            ? ClockedRenderEnd::display : ClockedRenderEnd::component,
                        argc == 10 ? std::stoull(argv[9]) : 2U)} : std::nullopt
            );
            std::cout << "wrote " << argv[7] << '\n';
            return 0;
        }
        if (
            argc == 7
            && std::string(argv[1])
                == "--trace-level2-input-script-sound-blaster-writes"
        ) {
            writeLevel2InputScriptSoundBlasterTrace(
                loadGameplayAssets(argv[2], argv[3]),
                loadLevel1InputScript(argv[4]),
                static_cast<std::uint32_t>(std::stoul(argv[5])),
                argv[6]
            );
            std::cout << "wrote " << argv[6] << '\n';
            return 0;
        }
        if (
            (argc == 6 || argc == 8)
            && std::string(argv[1])
                == "--trace-level3-sound-blaster-writes"
        ) {
            const auto injectedEvent = argc == 8
                ? std::optional<std::pair<std::uint32_t, int>>{
                    {
                        static_cast<std::uint32_t>(std::stoul(argv[6])),
                        std::stoi(argv[7]),
                    }
                }
                : std::nullopt;
            writeLevel3SoundBlasterTrace(
                loadGameplayAssets(argv[2], argv[3]),
                static_cast<std::uint32_t>(std::stoul(argv[4])),
                argv[5],
                injectedEvent,
                false
            );
            std::cout << "wrote " << argv[5] << '\n';
            return 0;
        }
        if (
            (argc == 6 || argc == 8)
            && std::string(argv[1])
                == "--trace-level3-resident-sound-blaster-writes"
        ) {
            const auto injectedEvent = argc == 8
                ? std::optional<std::pair<std::uint32_t, int>>{
                    {
                        static_cast<std::uint32_t>(std::stoul(argv[6])),
                        std::stoi(argv[7]),
                    }
                }
                : std::nullopt;
            writeLevel3SoundBlasterTrace(
                loadGameplayAssets(argv[2], argv[3]),
                static_cast<std::uint32_t>(std::stoul(argv[4])),
                argv[5],
                injectedEvent,
                true
            );
            std::cout << "wrote " << argv[5] << '\n';
            return 0;
        }
        if (
            argc == 6
            && std::string(argv[1])
                == "--trace-level3-success-release-writes"
        ) {
            writeLevel3SuccessReleaseTrace(
                loadGameplayAssets(argv[2], argv[3]),
                static_cast<std::uint32_t>(std::stoul(argv[4])),
                argv[5],
                false
            );
            std::cout << "wrote " << argv[5] << '\n';
            return 0;
        }
        if (
            argc == 6
            && std::string(argv[1])
                == "--trace-level2-to-level3-success-release-writes"
        ) {
            writeLevel3SuccessReleaseTrace(
                loadGameplayAssets(argv[2], argv[3]),
                static_cast<std::uint32_t>(std::stoul(argv[4])),
                argv[5],
                true
            );
            std::cout << "wrote " << argv[5] << '\n';
            return 0;
        }
        if (argc == 5 && std::string(argv[1]) == "--trace-page-flip-clock") {
            const auto phase = std::stoull(argv[2]);
            const std::string mode = argv[3];
            if (mode != "sync" && mode != "unsync") {
                throw std::runtime_error("page flip mode must be sync or unsync");
            }
            const auto synchronization = mode == "sync" ? stellar7::FrameSynchronization::verticalRetrace
                : stellar7::FrameSynchronization::disabled;
            std::ostringstream output;
            output << "{\"format_version\":1,\"initial_video_phase_ns\":" << phase
                   << ",\"synchronization\":\"" << mode << "\",\"elapsed_ns\":"
                   << stellar7::pageFlipNanoseconds(phase, synchronization)
                   << ",\"scope\":\"Page-flip base budget only; excludes preceding work and IRQ service\"}\n";
            writeText(argv[4], output.str());
            return 0;
        }
        if (argc == 4 && std::string(argv[1]) == "--trace-level-entry-clock") {
            const auto phase = std::stoull(argv[2]);
            const auto schedule = stellar7::levelEntrySchedule(phase);
            std::ostringstream output;
            output << "{\"format_version\":1,\"initial_video_phase_ns\":" << phase
                   << ",\"scope\":\"Base instruction budget only; excludes IRQ/service overhead\",\"steps\":[";
            for (std::size_t index = 0; index < schedule.size(); ++index) {
                const auto& step = schedule[index];
                output << (index == 0 ? "" : ",") << "{\"step\":" << index
                       << ",\"upload_ns\":" << step.uploadNanoseconds
                       << ",\"release_ns\":" << step.releaseNanoseconds
                       << ",\"elapsed_ns\":" << step.elapsedNanoseconds
                       << ",\"before_samples\":" << step.beforeUploadSamples
                       << ",\"masked_samples\":" << step.maskedSamples
                       << ",\"after_samples\":" << step.afterUploadSamples << '}';
            }
            output << "]}\n";
            writeText(argv[3], output.str());
            return 0;
        }
        if (argc == 5 && std::string(argv[1]) == "--trace-level-entry-fade") {
            const auto assets = loadGameplayAssets(argv[2], argv[3]);
            const stellar7::GameplayModel model;
            const auto frozen = stellar7::renderGameplayIndexed(assets, model);
            std::ostringstream output;
            output << "{\"format_version\":1,\"phases\":[";
            for (unsigned step = 0; step < stellar7::levelEntryFadeSteps; ++step) {
                const auto frame = stellar7::renderLevelEntryIndexed(frozen, assets.cockpitDacPalette, step);
                output << (step == 0 ? "" : ",") << "{\"game_tick\":" << model.ticks()
                       << ",\"intensity\":" << step * stellar7::levelEntryFadeIntensityStep << ",\"dac\":[";
                for (std::size_t index = 0; index < frame.palette.size(); ++index) {
                    const auto color = frame.palette[index];
                    output << (index == 0 ? "" : ",") << (color.red >> 2) << ','
                           << (color.green >> 2) << ',' << (color.blue >> 2);
                }
                output << "]}";
            }
            output << "]}\n";
            writeText(argv[4], output.str());
            return 0;
        }
        if (argc == 7 && std::string(argv[1]) == "--trace-music-callbacks") {
            writeMusicCallbackTrace(
                loadGameplayAssets(argv[2], argv[3]),
                static_cast<unsigned>(std::stoul(argv[4])),
                static_cast<std::uint32_t>(std::stoul(argv[5])), argv[6]
            );
            return 0;
        }
        if (argc == 7 && std::string(argv[1]) == "--trace-level1-settled-music-success-writes") {
            const auto assets = loadGameplayAssets(argv[2], argv[3]);
            stellar7::SoundBlasterSynth synth(
                assets.soundSequences, assets.soundBlasterInstrumentResource,
                assets.musicForLevel(0), assets.presentationMusicSequence
            );
            synth.startLevel1();
            synth.render(stellar7::SoundBlasterSynth::sampleRate * std::stoul(argv[4]));
            synth.clearRegisterWrites();
            synth.startSuccess();
            synth.render(stellar7::SoundBlasterSynth::sampleRate * std::stoul(argv[5]));
            synth.stopSuccess();
            std::vector<std::uint8_t> pairs;
            for (const auto write : synth.registerWrites()) {
                pairs.push_back(write.registerNumber);
                pairs.push_back(write.value);
            }
            writeBytes(argv[6], pairs);
            return 0;
        }
        if (argc == 7 && std::string(argv[1]) == "--trace-level1-idle-sound-blaster-success-writes") {
            writeLevel1InputScriptSoundBlasterSuccessWrites(
                loadGameplayAssets(argv[2], argv[3]), {},
                static_cast<std::uint32_t>(std::stoul(argv[4])),
                static_cast<std::uint32_t>(std::stoul(argv[5])), argv[6]
            );
            return 0;
        }
        if (
            (argc == 7 || argc == 8)
            && std::string(argv[1])
                == "--trace-level1-input-script-sound-blaster-success-writes"
        ) {
            const std::uint32_t successSeconds = argc == 8
                ? static_cast<std::uint32_t>(std::stoul(argv[6]))
                : 6;
            const char* outputPath = argc == 8 ? argv[7] : argv[6];
            writeLevel1InputScriptSoundBlasterSuccessWrites(
                loadGameplayAssets(argv[2], argv[3]),
                loadLevel1InputScript(argv[4]),
                static_cast<std::uint32_t>(std::stoul(argv[5])),
                successSeconds,
                outputPath
            );
            std::cout << "wrote " << outputPath << '\n';
            return 0;
        }
        if (
            argc == 7
            && (
                std::string(argv[1])
                    == "--trace-level1-input-script-model-draws"
                || std::string(argv[1])
                    == "--trace-gameplay-input-script-model-draws"
            )
        ) {
            writeLevel1InputScriptModelDrawTrace(
                loadGameplayAssets(argv[2], argv[3]),
                loadLevel1InputScript(argv[4]),
                static_cast<std::uint32_t>(std::stoul(argv[5])),
                argv[6]
            );
            std::cout << "wrote " << argv[6] << '\n';
            return 0;
        }
        if (
            argc == 7
            && std::string(argv[1])
                == "--trace-level1-input-script-model-draws-series"
        ) {
            writeLevel1InputScriptModelDrawTraceSeries(
                loadGameplayAssets(argv[2], argv[3]),
                loadLevel1InputScript(argv[4]),
                static_cast<std::uint32_t>(std::stoul(argv[5])),
                argv[6]
            );
            std::cout << "wrote trace series to " << argv[6] << '\n';
            return 0;
        }
        if (argc == 3 && std::string(argv[1]) == "--trace-campaign-assets") {
            const auto assets = loadGameplayAssets(argv[2]);
            std::cout << "{\"levels\":[";
            for (std::size_t index = 0; index < assets.campaignLevels.size(); ++index) {
                const auto& level = assets.campaignLevels[index];
                std::cout << (index == 0 ? "{" : ",{") << "\"resource\":" << std::quoted(level.resourceName)
                          << ",\"models\":" << level.models.size() << ",\"bitmap_groups\":" << level.bitmapGroups.size()
                          << ",\"music_bytes\":" << level.musicSequence.size() << '}';
            }
            std::cout << "]}\n";
            return 0;
        }
        if (argc == 4 && std::string(argv[1]) == "--verify-native-lifecycle") {
            verifyNativeLifecycle(loadGameplayAssets(argv[2]), argv[3]);
            return 0;
        }
        if (argc == 6 && (std::string(argv[1]) == "--smoke-native-menu"
            || std::string(argv[1]) == "--smoke-native-entry")) {
            std::size_t consumed = 0;
            const auto count = std::stoull(argv[4], &consumed);
            const auto maximumFrames = std::string(argv[1]) == "--smoke-native-entry"
                ? stellar7::maximumNativeEntrySmokeFrames : stellar7::maximumGameplaySmokeFrames;
            if (argv[4][consumed] != '\0' || count == 0 || count > maximumFrames) {
                throw std::invalid_argument("native frontend smoke requires 1.." + std::to_string(maximumFrames) + " frames");
            }
            const std::filesystem::path directory(argv[5]);
            if (std::filesystem::exists(directory)) {
                throw std::invalid_argument("native frontend smoke requires a fresh output directory");
            }
            const auto events = std::filesystem::is_regular_file(argv[3])
                ? loadLevel1InputScript(argv[3], InputScriptDomain::campaign)
                : parseLevel1InputEventSpec(argv[3]);
            std::vector<stellar7::GameplayInput> inputs;
            std::set<std::string> held;
            std::size_t nextEvent = 0;
            for (unsigned tick = 1; tick <= count; ++tick) {
                applyLevel1InputEventsBeforeTick(events, tick, nextEvent, held);
                auto input = gameplayInputFromHeld(held);
                input.confirm = held.count("enter") != 0;
                inputs.push_back(input);
            }
            unsigned beginCallbacks = 0;
            stellar7::GameplaySmokeResult result;
            if (std::string(argv[1]) == "--smoke-native-entry") {
                const auto startup = stellar7::decodeNativeStartupAssets(
                    readFile((std::filesystem::path(argv[2]) / "STELART.RES").string()));
                result = stellar7::runNativeEntrySmoke(loadGameplayAssets(argv[2]), startup, inputs);
                beginCallbacks = result.startupBeginRequests;
            } else {
                stellar7::runMenu(std::cin, std::cout, [&]() {
                    ++beginCallbacks;
                    result = stellar7::runGameplaySmoke(loadGameplayAssets(argv[2]), inputs);
                    return 0;
                });
            }
            if (beginCallbacks != 1) {
                throw std::runtime_error("native frontend smoke requires menu Begin");
            }
            std::filesystem::create_directories(directory);
            writeBytes((directory / "final.indexed.bin").string(), result.finalFrame.pixels);
            std::vector<std::uint8_t> palette;
            for (const auto color : result.finalFrame.palette) {
                palette.insert(palette.end(), {color.red, color.green, color.blue});
            }
            writeBytes((directory / "final.palette.bin").string(), palette);
            std::ofstream report(directory / "smoke.json");
            report << "{\"clock_domain\":\"legacy-presentation-unqualified\",\"strict_clock_qualified\":false"
                << ",\"physical_keyboard_tested\":false,\"begin_callbacks\":" << beginCallbacks
                << ",\"cancelled\":" << (result.cancelled ? "true" : "false")
                << ",\"frames\":" << result.frames << ",\"retained_level1_frames\":" << result.retainedLevel1Frames
                << ",\"gdi_blits\":" << result.gdiBlits << ",\"generated_samples\":" << result.generatedSamples
                << ",\"queued_samples\":" << result.queuedSamples << ",\"completed_samples\":" << result.completedSamples
                << ",\"audio_events\":" << result.audioEvents << ",\"opl_writes\":" << result.oplWrites
                << ",\"phases\":[";
            for (std::size_t index = 0; index < result.phases.size(); ++index) {
                const auto& phase = result.phases[index];
                const auto prefix = "phase-" + std::to_string(phase.frame);
                writeBytes((directory / (prefix + ".indexed.bin")).string(), phase.indexed.pixels);
                std::vector<std::uint8_t> phasePalette;
                for (const auto& color : phase.indexed.palette) {
                    phasePalette.insert(phasePalette.end(), {color.red, color.green, color.blue});
                }
                writeBytes((directory / (prefix + ".palette.bin")).string(), phasePalette);
                report << (index == 0 ? "" : ",") << "{\"frame\":" << phase.frame
                    << ",\"phase\":" << std::quoted(gameplayPhaseName(phase.phase))
                    << ",\"sample_end\":" << phase.sampleEnd << ",\"opl_write_end\":" << phase.oplWriteEnd
                    << ",\"gdi_blits\":" << phase.gdiBlits
                    << ",\"indexed\":" << std::quoted(prefix + ".indexed.bin")
                    << ",\"palette\":" << std::quoted(prefix + ".palette.bin") << '}';
            }
            report << "]}\n";
            if (!report) { throw std::runtime_error("cannot write native frontend smoke receipt"); }
            return result.cancelled || result.frames != count ? 1 : 0;
        }
        if ((argc == 5 || argc == 6 || ((argc == 7 || argc == 8)
            && std::string(argv[1]) == "--trace-native-startup-level1-frames"))
            && (std::string(argv[1]) == "--trace-native-startup-logo-work"
            || std::string(argv[1]) == "--trace-native-startup-logo-countdown"
            || std::string(argv[1]) == "--trace-native-startup-logo-event"
            || std::string(argv[1]) == "--trace-native-startup-logo-animation"
            || std::string(argv[1]) == "--trace-native-startup-logo-complete"
            || std::string(argv[1]) == "--trace-native-startup-logo-released"
            || std::string(argv[1]) == "--trace-native-startup-intro-skipped"
            || std::string(argv[1]) == "--trace-native-startup-menu-song"
            || std::string(argv[1]) == "--trace-native-startup-menu-resources"
            || std::string(argv[1]) == "--trace-native-startup-credits"
            || std::string(argv[1]) == "--trace-native-startup-menu-entry"
            || std::string(argv[1]) == "--trace-native-startup-adoption"
            || std::string(argv[1]) == "--trace-native-startup-level1-loader"
            || std::string(argv[1]) == "--trace-native-startup-level1-frames"
            || std::string(argv[1]) == "--trace-native-startup-begin")) {
            const bool level1Frames = std::string(argv[1]) == "--trace-native-startup-level1-frames";
            const bool level1Loader = level1Frames || std::string(argv[1]) == "--trace-native-startup-level1-loader";
            const bool adoption = level1Loader || std::string(argv[1]) == "--trace-native-startup-adoption";
            const bool begin = adoption || std::string(argv[1]) == "--trace-native-startup-begin";
            const bool menuEntry = begin || std::string(argv[1]) == "--trace-native-startup-menu-entry";
            const bool credits = menuEntry || std::string(argv[1]) == "--trace-native-startup-credits";
            const bool menuResources = credits || std::string(argv[1]) == "--trace-native-startup-menu-resources";
            const bool menuSong = menuResources || std::string(argv[1]) == "--trace-native-startup-menu-song";
            const bool introSkip = menuSong || std::string(argv[1]) == "--trace-native-startup-intro-skipped";
            const bool released = introSkip || std::string(argv[1]) == "--trace-native-startup-logo-released";
            const bool soundEnd = released || std::string(argv[1]) == "--trace-native-startup-logo-complete";
            const bool animation = soundEnd || std::string(argv[1]) == "--trace-native-startup-logo-animation";
            const bool directEvent = animation || std::string(argv[1]) == "--trace-native-startup-logo-event";
            const bool completeCountdown = directEvent || std::string(argv[1]) == "--trace-native-startup-logo-countdown";
            std::size_t consumed = 0;
            const auto requested = std::stoul(argv[3], &consumed);
            if (argv[3][consumed] != '\0' || requested == 0 || requested > 1000000) {
                throw std::invalid_argument("invalid source countdown poll budget");
            }
            const auto packetPolls = argc >= 6 ? std::stoul(argv[5], &consumed) : 10000;
            if (packetPolls == 0 || packetPolls > 10000 || (argc >= 6 && argv[5][consumed] != '\0')) {
                throw std::invalid_argument("invalid source countdown packet size");
            }
            const auto level1FrameCount = argc >= 7 ? std::stoul(argv[6],&consumed) : 2;
            if (level1FrameCount == 0 || level1FrameCount > std::numeric_limits<std::uint16_t>::max()
                || (argc >= 7 && argv[6][consumed] != '\0')) {
                throw std::invalid_argument("invalid retained Level1 frame count");
            }
            const auto level1Input = argc == 8 ? loadLevel1InputScript(argv[7]) : std::vector<ParsedLevel1InputEvent>{};
            const std::filesystem::path directory(argv[4]);
            if (completeCountdown && std::filesystem::exists(directory)) {
                throw std::invalid_argument("complete countdown trace requires a fresh output directory");
            }
            const auto assets = loadGameplayAssets(argv[2]);
            const auto resources = stellar7::decodeNativeStartupAssets(
                readFile((std::filesystem::path(argv[2]) / "STELART.RES").string()));
            stellar7::NativeStartupSequence startup(assets, resources);
            startup.start(stellar7::NativeStartupExecution::recoveredContext);
            startup.initializeSettings(stellar7::StartupCapabilityProfile::noMouseOrJoystick);
            startup.initializeResources(stellar7::NativeStartupDeviceProfile::portableOplNoDspProbe);
            startup.initializePresentation();
            startup.initializeFonts();
            const auto& fonts = *startup.recoveredFonts();
            auto pcm = fonts.samples;
            std::vector<stellar7::ClockedPresentationWork> packets;
            std::vector<std::int64_t> origins;
            const auto& context = *startup.recoveredContext();
            const auto& presentation = *startup.recoveredPresentation();
            std::int64_t cycles = context.installer.timing.total.elapsedCycles + context.work.timing.total.elapsedCycles
                + startup.recoveredSettings()->work.timing.total.elapsedCycles
                + startup.recoveredResources()->work.timing.total.elapsedCycles;
            for (const auto& work : presentation.work) { cycles += work.timing.total.elapsedCycles; }
            for (const auto& palette : presentation.palettes) { cycles += palette.work.timing.total.elapsedCycles; }
            for (const auto& work : fonts.work) { cycles += work.timing.total.elapsedCycles; }
            std::size_t polls = 0;
            while (polls < requested && startup.phase() == stellar7::NativeStartupPhase::logoCountdownWork) {
                auto work = startup.pollLogoCountdown(std::min<std::size_t>(packetPolls, requested - polls));
                polls += work.timing.countdownPolls;
                pcm.insert(pcm.end(), work.audio.samples.begin(), work.audio.samples.end());
                origins.push_back(cycles);
                cycles += work.timing.total.elapsedCycles;
                packets.push_back(std::move(work));
            }
            if (completeCountdown && startup.phase() != stellar7::NativeStartupPhase::awaitingLogoFrames) {
                throw std::invalid_argument("countdown did not return within guest-poll budget");
            }
            std::optional<stellar7::ClockedPresentationWork> eventWork;
            if (directEvent) {
                eventWork = startup.playLogoEvent();
                pcm.insert(pcm.end(), eventWork->audio.samples.begin(), eventWork->audio.samples.end());
            }
            if (animation) {
                startup.animateLogo(packetPolls);
                const auto& samples = startup.recoveredLogo()->samples;
                pcm.insert(pcm.end(), samples.begin(), samples.end());
            }
            if (soundEnd) {
                startup.finishLogoSound(packetPolls);
                const auto& samples = startup.recoveredLogoEnd()->samples;
                pcm.insert(pcm.end(), samples.begin(), samples.end());
            }
            if (released) {
                startup.releaseLogo();
                const auto& samples = startup.recoveredLogoRelease()->samples;
                pcm.insert(pcm.end(), samples.begin(), samples.end());
            }
            if (introSkip) {
                startup.skipIntro(stellar7::NativeIntroKey::escape);
                const auto& intro = *startup.recoveredIntro();
                pcm.insert(pcm.end(), intro.samples.begin(), intro.samples.end());
            }
            if (menuSong) {
                startup.beginMenuSong();
                const auto& samples = startup.recoveredMenuSong()->samples;
                pcm.insert(pcm.end(), samples.begin(), samples.end());
            }
            if (menuResources) {
                startup.initializeMenuResources();
                const auto& run = *startup.recoveredMenuResources();
                pcm.insert(pcm.end(), run.samples.begin(), run.samples.end());
            }
            if (credits) {
                startup.skipCredits(stellar7::NativeIntroKey::escape,packetPolls);
                const auto& run = *startup.recoveredCredits();
                pcm.insert(pcm.end(),run.samples.begin(),run.samples.end());
            }
            bool menuSampleFallback = false;
            if (menuEntry) {
                startup.enterMenu(stellar7::NativeMenuEntryProfile::keyboardOnly,packetPolls);
                const auto& run = *startup.recoveredMenuEntry();
                pcm.insert(pcm.end(),run.samples.begin(),run.samples.end());
                try { startup.advance(1); menuSampleFallback = true; }
                catch (const std::logic_error&) { }
                if (menuSampleFallback) { throw std::logic_error("recovered menu admitted a sample fallback"); }
            }
            bool beginSampleFallback = false;
            bool beginAdoption = false;
            bool earlyAdoptionRejected = false;
            if (begin) {
                if (adoption) {
                    try { startup.releaseGameplay(); }
                    catch (const std::logic_error&) { earlyAdoptionRejected = true; }
                    if (!earlyAdoptionRejected) { throw std::logic_error("early gameplay handoff admitted"); }
                }
                startup.beginGame(stellar7::NativeMenuBeginProfile::enterPressRelease,packetPolls);
                const auto& run = *startup.recoveredBegin();
                pcm.insert(pcm.end(),run.samples.begin(),run.samples.end());
                try { startup.advance(1); beginSampleFallback = true; }
                catch (const std::logic_error&) { }
                try { startup.releaseAudio(); beginAdoption = true; }
                catch (const std::logic_error&) { }
                if (beginSampleFallback || beginAdoption) {
                    throw std::logic_error("recovered Begin admitted an unsupported gameplay-clock fallback");
                }
            }
            std::filesystem::create_directories(directory);
            if (completeCountdown) {
                writeBytes((directory / "front.indexed").string(), startup.menuPagePixels(stellar7::NativeMenuPage::front));
                writeBytes((directory / "back.indexed").string(), startup.menuPagePixels(stellar7::NativeMenuPage::back));
                writeBytes((directory / "cache.indexed").string(), startup.menuPagePixels(stellar7::NativeMenuPage::cache));
                writeBytes((directory / "presentation.dac").string(), std::vector<std::uint8_t>(
                    presentation.palette.dac.colors.begin(), presentation.palette.dac.colors.end()));
            }
            writeWave((directory / "logo-work.wav").string(), pcm);
            if (introSkip) {
                const auto& palette = startup.recoveredIntro()->palette;
                writeBytes((directory / "intro.dac").string(), std::vector<std::uint8_t>(
                    palette.dac.colors.begin(), palette.dac.colors.end()));
                writeBytes((directory / "intro.source").string(), std::vector<std::uint8_t>(
                    palette.source.begin(), palette.source.end()));
            }
            std::ofstream report(directory / "logo-work.json");
            report << "{\"original_state_imports\":0,\"original_dos_elapsed_qualified\":false"
                << ",\"guest_polls\":" << polls << ",\"first_sample\":" << fonts.firstSample
                << ",\"samples\":" << startup.elapsedSamples() << ",\"global_count\":" << startup.countdown()
                << ",\"font_heights\":[" << static_cast<unsigned>(fonts.firstFont.height) << ','
                << static_cast<unsigned>(fonts.secondFont.height) << "],\"font_glyph_counts\":["
                << fonts.firstFont.glyphs.size() << ',' << fonts.secondFont.glyphs.size()
                << "],\"memory_probe_bytes\":" << fonts.memoryProbeBytes
                << ",\"memory_probe_released\":" << (fonts.probeReleased ? "true" : "false")
                << ",\"first_program_callback\":";
            enum class CallbackRecord { first, retainedSeries, retainedSeriesWithFade };
            const auto printCallback = [&](const auto& callback, const std::int64_t cycleOrigin, const CallbackRecord detail) {
                    const auto& state = callback.advance.state;
                    report << '{';
                    if (detail == CallbackRecord::retainedSeriesWithFade) {
                        report << "\"fade\":" << state.fade << ",\"callback_cycle\":" << cycleOrigin+callback.cycleOffset << ',';
                    }
                    report << "\"position\":" << state.position << ",\"wait\":" << state.wait
                        << ",\"status\":" << static_cast<unsigned>(state.runningStatus)
                        << ",\"dispatches\":" << callback.advance.dispatches << ",\"blocks\":[";
                    for (std::size_t index = 0; index < callback.advance.blocks.size(); ++index) {
                        const auto& block = callback.advance.blocks[index];
                        report << (index == 0 ? "[" : ",[") << static_cast<unsigned>(block.kind) << ','
                            << block.dispatches << ',' << block.voice << ',' << static_cast<unsigned>(block.registerNumber)
                            << ',' << static_cast<unsigned>(block.value) << ']';
                    }
                    report << "],\"opl_writes\":[";
                    std::size_t writer = 0;
                    for (const auto& block : callback.advance.blocks) {
                        if (block.kind != stellar7::SoundBlasterWorkKind::registerWrite) { continue; }
                        report << (writer == 0 ? "[" : ",[") << cycleOrigin + callback.timing.oplEffects[writer].elapsedCycles << ','
                            << static_cast<unsigned>(block.registerNumber) << ',' << static_cast<unsigned>(block.value) << ']';
                        ++writer;
                    }
                    const auto array = [&](const auto& values) {
                        for (std::size_t index = 0; index < values.size(); ++index) {
                            report << (index == 0 ? "" : ",") << static_cast<unsigned>(values[index]);
                        }
                    };
                    if (!state.program) {
                        report << "],\"program\":null,\"kind\":" << static_cast<unsigned>(callback.advance.kind)
                            << ",\"channel_enabled\":["; array(state.channelEnabled);
                        report << "],\"tuning\":["; array(state.voices.tuning);
                        report << "],\"velocities\":["; array(state.velocities);
                        report << "],\"sounding\":["; array(state.sounding);
                        report << "],\"owners\":["; array(state.voices.channels);
                        report << "],\"notes\":["; array(state.voices.notes);
                        report << "],\"cursors\":["; array(state.channelVoiceCursors);
                        report << "],\"deferrals\":["; array(state.voices.channelDeferrals);
                        report << "],\"direct_timers\":["; array(state.voices.directTimers);
                        report << "]}";
                        return;
                    }
                    const auto& program = *state.program;
                    report << "],\"channel_programs\":["; array(program.channelPrograms);
                    report << "],\"channel_enabled\":["; array(state.channelEnabled);
                    report << "],\"tuning\":["; array(state.voices.tuning);
                    report << "],\"velocities\":["; array(state.velocities);
                    report << "],\"sounding\":["; array(state.sounding);
                    report << "],\"program_voices\":[";
                    for (std::size_t index = 0; index < program.voices.size(); ++index) {
                        const auto& voice = program.voices[index];
                        report << (index == 0 ? "[" : ",[") << static_cast<unsigned>(voice.program) << ','
                            << static_cast<unsigned>(voice.tuningMagnitude) << ',' << voice.operatorScale << ','
                            << voice.operatorFallback << ',' << voice.carrierScale << ',' << voice.carrierFallback
                            << ',' << voice.operatorDisabled << ']';
                    }
                    report << "],\"operators\":[";
                    for (std::size_t index = 0; index < program.operators.size(); ++index) {
                        report << (index == 0 ? "[" : ",["); array(program.operators[index]); report << ']';
                    }
                    report << "],\"handle_status\":" << program.handleStatus << ",\"repeat_count\":" << program.repeatCount
                        << ",\"handle_volume\":" << program.handleVolume << ",\"accumulator\":" << program.accumulator
                        << ",\"fade_divider\":" << program.fadeDivider
                        << ",\"amplitude_depth\":" << static_cast<unsigned>(program.amplitudeDepth)
                        << ",\"vibrato_depth\":" << static_cast<unsigned>(program.vibratoDepth)
                        << ",\"note_select\":" << static_cast<unsigned>(program.noteSelect)
                        << ",\"waveform_mask\":" << program.waveformMask;
                    if (detail != CallbackRecord::first) {
                        report << ",\"kind\":" << static_cast<unsigned>(callback.advance.kind)
                            << ",\"loop_marker\":" << program.loopMarker << ",\"owners\":[";
                        array(state.voices.channels);
                        report << "],\"notes\":["; array(state.voices.notes);
                        report << "],\"cursors\":["; array(state.channelVoiceCursors);
                        report << "],\"deferrals\":["; array(state.voices.channelDeferrals);
                        report << ']';
                    }
                    report << '}';
            };
            bool emitted = false;
            for (std::size_t packetIndex = 0; packetIndex < packets.size(); ++packetIndex) {
                for (const auto& callback : packets[packetIndex].timing.callbacks) {
                    if (callback.advance.kind != stellar7::SoundBlasterCallbackKind::programControl || emitted) { continue; }
                    emitted = true;
                    printCallback(callback, origins[packetIndex], CallbackRecord::first);
                }
            }
            if (!emitted) { report << "null"; }
            if (completeCountdown) {
                report << ",\"countdown_returned\":true,\"callback_series\":[";
                std::size_t callbackIndex = 0;
                for (std::size_t packetIndex = 0; packetIndex < packets.size(); ++packetIndex) {
                    for (const auto& callback : packets[packetIndex].timing.callbacks) {
                        if (callbackIndex++ != 0) { report << ','; }
                        printCallback(callback, origins[packetIndex], CallbackRecord::retainedSeries);
                    }
                }
                report << ']';
            }
            report << ",\"clock_phase\":";
            printBiosClockPhase(begin ? startup.recoveredBegin()->finalPhase
                : menuEntry ? startup.recoveredMenuEntry()->finalPhase
                : credits ? startup.recoveredCredits()->finalPhase
                : menuResources ? startup.recoveredMenuResources()->finalPhase
                : menuSong ? startup.recoveredMenuSong()->finalPhase
                : introSkip ? startup.recoveredIntro()->finalPhase
                : released ? startup.recoveredLogoRelease()->finalPhase
                : soundEnd ? startup.recoveredLogoEnd()->finalPhase : animation ? startup.recoveredLogo()->finalPhase
                : eventWork ? eventWork->timing.phase : packets.back().timing.phase, report);
            if (eventWork) {
                const auto& state = *eventWork->timing.phase.callback;
                report << ",\"logo_event\":{\"id\":26,\"dispatches\":" << eventWork->timing.total.dispatches
                    << ",\"elapsed_cycles\":" << eventWork->timing.total.elapsedCycles
                    << ",\"callbacks\":" << eventWork->timing.callbacks.size()
                    << ",\"direct_timers\":[" << state.voices.directTimers[0] << ',' << state.voices.directTimers[1]
                    << ',' << state.voices.directTimers[2] << "],\"velocity0\":" << static_cast<unsigned>(state.velocities[0])
                    << ",\"note6\":" << static_cast<unsigned>(state.voices.notes[6])
                    << ",\"sounding6\":" << static_cast<unsigned>(state.sounding[6]) << ",\"opl_writes\":[";
                for (std::size_t index = 0; index < eventWork->audio.writes.size(); ++index) {
                    const auto& write = eventWork->audio.writes[index];
                    report << (index == 0 ? "[" : ",[") << write.cycleOffset << ','
                        << static_cast<unsigned>(write.write.registerNumber) << ',' << static_cast<unsigned>(write.write.value) << ']';
                }
                report << "]}";
            }
            if (soundEnd) {
                const auto& end = *startup.recoveredLogoEnd();
                const auto& state = *end.finalPhase.callback;
                const auto& program = *state.program;
                report << ",\"logo_end\":{\"source_end\":\"625D\",\"repeat\":" << program.repeatCount
                    << ",\"status\":" << program.handleStatus << ",\"sequence_position\":" << state.position
                    << ",\"guest_polls\":" << end.guestPolls << ",\"sample_end\":" << end.firstSample + end.samples.size()
                    << ",\"returned\":" << (end.finalPhase.sequenceSelection->logoSoundReturned ? "true" : "false")
                    << ",\"sounding\":[";
                for (std::size_t index = 0; index < state.sounding.size(); ++index) {
                    report << (index == 0 ? "" : ",") << static_cast<unsigned>(state.sounding[index]);
                }
                report << "],\"callback_series\":[";
                auto origin = cycles + eventWork->timing.total.elapsedCycles;
                for (const auto& work : startup.recoveredLogo()->work) { origin += work.timing.total.elapsedCycles; }
                bool first = true;
                for (const auto& work : end.work) {
                    for (const auto& callback : work.timing.callbacks) {
                        if (!first) { report << ','; } first = false;
                        printCallback(callback, origin, CallbackRecord::retainedSeries);
                    }
                    origin += work.timing.total.elapsedCycles;
                }
                report << "]}";
            }
            if (released) {
                const auto& release = *startup.recoveredLogoRelease();
                const auto& state = *release.finalPhase.callback;
                report << ",\"logo_release\":{\"source_end\":\"626D\",\"metadata_released\":"
                    << (release.metadataReleased ? "true" : "false") << ",\"original_heap_timing_qualified\":false"
                    << ",\"current_selected\":"
                    << (state.domain == stellar7::SoundBlasterCallbackDomain::sequence ? "true" : "false")
                    << ",\"no_handle_dispatches\":" << stellar7::soundBlasterCallbackAdvance(state).dispatches
                    << ",\"velocity0\":" << static_cast<unsigned>(state.velocities[0])
                    << ",\"direct_timers\":[" << state.voices.directTimers[0] << ',' << state.voices.directTimers[1]
                    << ',' << state.voices.directTimers[2] << "],\"sample_end\":" << release.firstSample + release.samples.size()
                    << ",\"opl_writes\":[";
                bool first = true;
                std::int64_t origin = 0;
                for (const auto& work : release.work) {
                    for (const auto& write : work.audio.writes) {
                        report << (first ? "[" : ",[") << origin + write.cycleOffset << ','
                            << static_cast<unsigned>(write.write.registerNumber) << ','
                            << static_cast<unsigned>(write.write.value) << ']';
                        first = false;
                    }
                    origin += work.timing.total.elapsedCycles;
                }
                report << "]}";
            }
            if (introSkip) {
                const auto& intro = *startup.recoveredIntro();
                std::vector<const stellar7::ClockedPresentationWork*> work{&intro.query};
                for (const auto& step : intro.blackout.steps) {
                    work.push_back(&step.before); work.push_back(&step.palette.work);
                }
                work.push_back(&intro.blackout.clear);
                for (const auto& tail : intro.paletteSelection) { work.push_back(&tail); }
                std::size_t callbacks = 0, queryDispatches = 0;
                for (const auto& operation : intro.input.query) { queryDispatches += operation.count; }
                for (const auto* slice : work) {
                    callbacks += slice->timing.callbacks.size();
                    if (!slice->audio.writes.empty() || std::any_of(slice->timing.callbacks.begin(), slice->timing.callbacks.end(),
                        [](const auto& callback) { return callback.advance.kind != stellar7::SoundBlasterCallbackKind::noHandle; })) {
                        throw std::logic_error("intro skip differs from the admitted no-handle audio domain");
                    }
                }
                report << ",\"intro_skip\":{\"source_end\":\"637F\",\"source_key\":" << intro.input.sourceKey
                    << ",\"query_dispatches\":" << queryDispatches
                    << ",\"original_keyboard_delivery_qualified\":false,\"keyboard_provider_untimed\":true"
                    << ",\"palette_provider_untimed\":true,\"no_handle_callbacks\":" << callbacks
                    << ",\"velocity0\":" << static_cast<unsigned>(intro.finalPhase.callback->velocities[0])
                    << ",\"sample_end\":" << intro.firstSample + intro.samples.size() << ",\"opl_writes\":[],\"intensities\":[";
                for (std::size_t i = 0; i < intro.blackout.steps.size(); ++i) {
                    report << (i == 0 ? "" : ",") << static_cast<unsigned>(intro.blackout.steps[i].intensity);
                }
                report << "],\"callback_series\":[";
                auto origin = cycles + eventWork->timing.total.elapsedCycles;
                for (const auto& slice : startup.recoveredLogo()->work) { origin += slice.timing.total.elapsedCycles; }
                for (const auto& slice : startup.recoveredLogoEnd()->work) { origin += slice.timing.total.elapsedCycles; }
                for (const auto& slice : startup.recoveredLogoRelease()->work) { origin += slice.timing.total.elapsedCycles; }
                bool first = true;
                for (const auto* slice : work) {
                    for (const auto& callback : slice->timing.callbacks) {
                        if (!first) { report << ','; } first = false;
                        printCallback(callback, origin, CallbackRecord::retainedSeries);
                    }
                    origin += slice->timing.total.elapsedCycles;
                }
                report << "]}";
            }
            if (menuSong) {
                const auto& song = *startup.recoveredMenuSong();
                report << ",\"menu_song\":{\"source_end\":\"40A5\",\"startup_opened_resource\":"
                    << (startup.recoveredResources()->resources.state.openedStartupResource ? "true" : "false")
                    << ",\"selected_song\":1,\"loaded_songs\":"
                    << std::count(song.after.sequenceAllocations.begin(), song.after.sequenceAllocations.end(), true)
                    << ",\"old_allocation_retained\":" << (song.after.sequenceAllocations[0] ? "true" : "false")
                    << ",\"old_position_before\":" << song.before.sequencePositions[0]
                    << ",\"old_position_after\":" << song.after.sequencePositions[0]
                    << ",\"old_selected\":" << (song.after.sequenceSelections[0] ? "true" : "false")
                    << ",\"new_selected\":" << (song.after.sequenceSelections[1] ? "true" : "false")
                    << ",\"global_count_written\":2000,\"global_count\":" << song.finalPhase.timer->globalCount
                    << ",\"sample_end\":" << song.firstSample + song.samples.size()
                    << ",\"running_status\":" << static_cast<unsigned>(song.finalPhase.callback->runningStatus)
                    << ",\"velocity0\":" << static_cast<unsigned>(song.finalPhase.callback->velocities[0])
                    << ",\"sequence_position\":" << song.finalPhase.callback->position << '}';
            }
            if (menuResources) {
                const auto& run = *startup.recoveredMenuResources();
                writeBytes((directory / "menu.dac").string(), std::vector<std::uint8_t>(
                    run.palette.dac.colors.begin(), run.palette.dac.colors.end()));
                writeBytes((directory / "menu.source").string(), std::vector<std::uint8_t>(
                    run.palette.source.begin(), run.palette.source.end()));
                std::vector<const stellar7::ClockedPresentationWork*> work;
                for (const auto& slice : run.before) { work.push_back(&slice); }
                for (const auto& step : run.blackout.steps) { work.push_back(&step.before); work.push_back(&step.palette.work); }
                work.push_back(&run.blackout.clear);
                for (const auto& slice : run.after) { work.push_back(&slice); }
                std::size_t callbacks = 0;
                for (const auto* slice : work) { callbacks += slice->timing.callbacks.size(); }
                report << ",\"menu_resources\":{\"source_end\":\"41BE\",\"selected_font\":0,\"bitmap_groups\":3"
                    << ",\"selected_bitmap_group\":3,\"bitmap_counts\":[" << run.bitmapCounts[0] << ','
                    << run.bitmapCounts[1] << ',' << run.bitmapCounts[2] << "],\"color_tables\":2,\"random_seed\":["
                    << run.randomSeed[0] << ',' << run.randomSeed[1] << "],\"configuration\":[" << run.configuration.viewSize;
                for (const auto value : run.configuration.options) { report << ',' << value; }
                report << "],\"streams_closed\":" << (run.streamsClosed ? "true" : "false")
                    << ",\"palette_index\":2,\"selected_song\":1,\"old_allocation_retained\":"
                    << (run.driverAfter.sequenceAllocations[0] ? "true" : "false")
                    << ",\"old_position_before\":" << run.driverBefore.sequencePositions[0]
                    << ",\"old_position_after\":" << run.driverAfter.sequencePositions[0]
                    << ",\"sequence_position\":" << run.finalPhase.callback->position
                    << ",\"sample_end\":" << run.firstSample + run.samples.size() << ",\"callbacks\":" << callbacks
                    << ",\"cache_cursor\":" << run.cache.nextCursor
                    << ",\"bitmap_resource\":\"STELART.RES\",\"native_providers_untimed\":true"
                    << ",\"callback_cycle_origin\":\"menu-resource-entry\",\"callback_series\":[";
                std::int64_t origin = 0;
                bool first = true;
                for (const auto* slice : work) {
                    for (const auto& callback : slice->timing.callbacks) {
                        if (!first) { report << ','; } first = false;
                        printCallback(callback, origin, CallbackRecord::retainedSeries);
                    }
                    origin += slice->timing.total.elapsedCycles;
                }
                report << "]}";
            }
            if (credits) {
                const auto& run = *startup.recoveredCredits();
                writeBytes((directory / "credits.displayed.indexed").string(),run.displayedPage);
                writeBytes((directory / "credits.dac").string(),std::vector<std::uint8_t>(
                    run.palette.dac.colors.begin(),run.palette.dac.colors.end()));
                writeBytes((directory / "credits.source").string(),std::vector<std::uint8_t>(
                    run.palette.source.begin(),run.palette.source.end()));
                std::size_t callbacks = 0;
                for (const auto& work : run.work) { callbacks += work.timing.callbacks.size(); }
                report << ",\"credits\":{\"source_end\":\"0147\",\"first_sample\":" << run.firstSample
                    << ",\"sample_end\":" << run.firstSample + run.samples.size()
                    << ",\"leader_dots\":" << run.drawing.leaderDots
                    << ",\"drawing_writes\":" << run.drawing.effects.size()
                    << ",\"palette_uploads\":" << run.paletteUploads.size()
                    << ",\"native_key\":" << run.sourceKey << ",\"countdown_read\":" << run.countdownRead
                    << ",\"countdown_written\":" << (run.finalPhase.sequenceSelection->creditsCountdownWritten ? "true" : "false")
                    << ",\"input_returned\":" << (run.finalPhase.sequenceSelection->creditsInputReturned ? "true" : "false")
                    << ",\"selected_song\":1,\"selected_font\":" << static_cast<unsigned>(run.selectedFont)
                    << ",\"draw_page\":" << run.drawPage << ",\"copy_page\":" << run.copyPage
                    << ",\"surface\":[" << run.surface.viewport.left << ',' << run.surface.viewport.right
                    << ',' << run.surface.viewport.top << ',' << run.surface.viewport.bottom << ','
                    << (run.surface.clipping == stellar7::PolygonClipMode::Enabled ? 1 : 0) << ','
                    << static_cast<unsigned>(run.surface.rasterStyle) << ',' << static_cast<unsigned>(run.surface.lineColor)
                    << ',' << static_cast<unsigned>(run.textColor) << ']'
                    << ",\"old_allocation_retained\":" << (run.driverAfter.sequenceAllocations[0] ? "true" : "false")
                    << ",\"old_position_before\":" << run.driverBefore.sequencePositions[0]
                    << ",\"old_position_after\":" << run.driverAfter.sequencePositions[0]
                    << ",\"native_providers_untimed\":true,\"original_scanout_qualified\":false"
                    << ",\"pixel_effect_boundary\":\"operation-return\",\"callbacks\":" << callbacks
                    << ",\"callback_cycle_origin\":\"credits-entry\",\"callback_series\":[";
                std::int64_t origin = 0;
                bool first = true;
                for (const auto& work : run.work) {
                    for (const auto& callback : work.timing.callbacks) {
                        if (!first) { report << ','; } first = false;
                        printCallback(callback,origin,CallbackRecord::retainedSeries);
                    }
                    origin += work.timing.total.elapsedCycles;
                }
                report << "]}";
            }
            if (menuEntry) {
                const auto& run = *startup.recoveredMenuEntry();
                writeBytes((directory / "menu-entry.dac").string(),std::vector<std::uint8_t>(
                    run.palette.dac.colors.begin(),run.palette.dac.colors.end()));
                writeBytes((directory / "menu-entry.transfer").string(),run.transfer);
                writeBytes((directory / "menu-entry.scratch").string(),run.scratch);
                std::size_t callbacks = 0;
                for (const auto& work : run.work) { callbacks += work.timing.callbacks.size(); }
                report << ",\"menu_entry\":{\"source_end\":\"160F\",\"first_sample\":" << run.firstSample
                    << ",\"sample_end\":" << run.firstSample + run.samples.size()
                    << ",\"selected_song\":1,\"widget_count\":" << run.drawing.widgetCount
                    << ",\"selection\":" << startup.menu().selection()
                    << ",\"pointer\":[" << run.pointer.x << ',' << run.pointer.y << ']'
                    << ",\"old_rectangle\":[" << run.oldRectangle.x << ',' << run.oldRectangle.y << ','
                    << run.oldRectangle.width << ',' << run.oldRectangle.height << ']'
                    << ",\"held_queries\":[28,57,79,80,81,71,72,73,75,77],\"neutral_return\":0"
                    << ",\"countdown_written\":" << (run.finalPhase.sequenceSelection->menuEntryCountdownWritten ? "true" : "false")
                    << ",\"countdown\":" << run.finalPhase.timer->globalCount
                    << ",\"draw_page\":" << startup.menuPageSegment(stellar7::NativeMenuPage::back)
                    << ",\"copy_page\":" << startup.menuPageSegment(stellar7::NativeMenuPage::back)
                    << ",\"palette_uploads\":" << run.paletteUploads.size()
                    << ",\"native_preconstruction_pointer_untimed\":true,\"native_allocation_prefix_read\":false"
                    << ",\"sample_fallback_admitted\":" << (menuSampleFallback ? "true" : "false")
                    << ",\"native_input_providers_untimed\":true,\"original_dos_elapsed_qualified\":false"
                    << ",\"callbacks\":" << callbacks << ",\"callback_cycle_origin\":\"menu-entry\",\"callback_series\":[";
                std::int64_t origin = 0;
                bool first = true;
                for (const auto& work : run.work) {
                    for (const auto& callback : work.timing.callbacks) {
                        if (!first) { report << ','; } first = false;
                        printCallback(callback,origin,CallbackRecord::retainedSeries);
                    }
                    origin += work.timing.total.elapsedCycles;
                }
                report << "]}";
            }
            if (begin) {
                const auto& run = *startup.recoveredBegin();
                writeBytes((directory / "begin.dac").string(),std::vector<std::uint8_t>(
                    run.palette.dac.colors.begin(),run.palette.dac.colors.end()));
                writeBytes((directory / "begin.source").string(),std::vector<std::uint8_t>(
                    run.palette.source.begin(),run.palette.source.end()));
                std::size_t callbacks = 0;
                for (const auto& work : run.work) { callbacks += work.timing.callbacks.size(); }
                report << ",\"begin\":{\"source_end\":\"0180\",\"first_sample\":" << run.firstSample
                    << ",\"sample_end\":" << run.firstSample + run.samples.size() << ",\"polls\":[";
                for (std::size_t index = 0; index < run.polls.size(); ++index) {
                    const auto& poll = run.polls[index];
                    const auto stem = index == 0 ? "begin-press" : "begin-release";
                    writeBytes((directory / (std::string(stem)+".front.indexed")).string(),poll.front);
                    writeBytes((directory / (std::string(stem)+".back.indexed")).string(),poll.back);
                    writeBytes((directory / (std::string(stem)+".transfer")).string(),poll.transfer);
                    report << (index == 0 ? "{" : ",{") << "\"active_widget\":" << poll.activeWidget
                        << ",\"activation\":" << poll.activation << ",\"result\":" << poll.result
                        << ",\"sample_end\":" << poll.sampleEnd << '}';
                }
                report << "],\"fade_requests\":" << static_cast<unsigned>(run.finalPhase.sequenceSelection->beginFadeRequests)
                    << ",\"palette_uploads\":" << run.paletteUploads.size()
                    << ",\"difficulty\":[";
                for (std::size_t i = 0; i < run.difficulty.size(); ++i) { report << (i == 0 ? "" : ",") << int{run.difficulty[i]}; }
                report << "],\"countdown\":" << run.finalPhase.timer->globalCount
                    << ",\"countdown_written\":" << (run.finalPhase.sequenceSelection->beginCountdownWritten ? "true" : "false")
                    << ",\"countdown_returned\":" << (run.finalPhase.sequenceSelection->beginCountdownReturned ? "true" : "false")
                    << ",\"guest_polls\":" << run.guestPolls
                    << ",\"current_cleared\":" << (run.finalPhase.sequenceSelection->beginCurrentCleared ? "true" : "false")
                    << ",\"sequence_selected\":" << (run.driverAfter.sequenceSelected ? "true" : "false")
                    << ",\"sequence_allocated\":" << (run.driverAfter.sequenceAllocated ? "true" : "false")
                    << ",\"remaining_fade\":" << run.driverAfter.fade
                    << ",\"sample_fallback_admitted\":" << (beginSampleFallback ? "true" : "false")
                    << ",\"gameplay_adoption_admitted\":" << (beginAdoption ? "true" : "false")
                    << ",\"native_input_providers_untimed\":true,\"original_dos_elapsed_qualified\":false"
                    << ",\"callbacks\":" << callbacks << ",\"callback_cycle_origin\":\"begin-entry\",\"callback_series\":[";
                std::int64_t origin = 0;
                bool first = true;
                for (const auto& work : run.work) {
                    for (const auto& callback : work.timing.callbacks) {
                        if (!first) { report << ','; } first = false;
                        printCallback(callback,origin,CallbackRecord::retainedSeriesWithFade);
                    }
                    origin += work.timing.total.elapsedCycles;
                }
                report << "],\"foreground\":[";
                origin = 0; first = true;
                for (const auto& work : run.work) {
                    for (const auto& effect : work.timing.soundBlasterEffects) {
                        if (!first) { report << ','; } first = false;
                        report << '[' << static_cast<unsigned>(effect.kind) << ','
                            << origin+effect.cycleOffset << ',' << effect.value << ']';
                    }
                    origin += work.timing.total.elapsedCycles;
                }
                report << "]}";
            }
            if (animation) {
                const auto& logo = *startup.recoveredLogo();
                report << ",\"logo_animation\":{\"source_end\":\"6241\",\"original_scanout_qualified\":false"
                    << ",\"pixel_effect_boundary\":\"operation-return\",\"cache_provider_untimed\":true,\"frames\":[";
                std::size_t frameWork = 0, frameCallbacks = 0;
                for (std::size_t index = 0; index < logo.frames.size(); ++index) {
                    const auto& frame = logo.frames[index];
                    while (frameWork < logo.work.size()) {
                        const auto& work = logo.work[frameWork++];
                        frameCallbacks += work.timing.callbacks.size();
                        if (work.timing.phase.sequenceSelection->logoFramesCompleted == index + 1) { break; }
                    }
                    const auto& audio = *frame.phase.callback;
                    const auto stem = "logo-frame-" + std::to_string(index);
                    for (std::size_t page = 0; page < frame.pages.size(); ++page) {
                        writeBytes((directory / (stem + ".page" + std::to_string(page) + ".indexed")).string(), frame.pages[page]);
                    }
                    report << (index == 0 ? "{" : ",{") << "\"ordinal\":" << index << ",\"index\":" << frame.index
                        << ",\"sample_end\":" << frame.sampleEnd << ",\"global_count\":" << frame.phase.timer->globalCount
                        << ",\"callbacks_completed\":" << frameCallbacks << ",\"direct_timers\":["
                        << audio.voices.directTimers[0] << ',' << audio.voices.directTimers[1] << ',' << audio.voices.directTimers[2]
                        << "],\"sequence_position\":" << audio.position << ",\"sequence_wait\":" << audio.wait
                        << ",\"running_status\":" << static_cast<unsigned>(audio.runningStatus)
                        << ",\"velocity0\":" << static_cast<unsigned>(audio.velocities[0])
                        << ",\"note6\":" << static_cast<unsigned>(audio.voices.notes[6])
                        << ",\"sounding6\":" << static_cast<unsigned>(audio.sounding[6])
                        << ",\"pages\":[" << std::quoted(stem + ".page0.indexed") << ','
                        << std::quoted(stem + ".page1.indexed") << ',' << std::quoted(stem + ".page2.indexed")
                        << "],\"clock_phase\":";
                    printBiosClockPhase(frame.phase, report);
                    report << '}';
                }
                report << "],\"callback_series\":[";
                auto origin = cycles + eventWork->timing.total.elapsedCycles;
                bool first = true;
                for (const auto& work : logo.work) {
                    for (const auto& callback : work.timing.callbacks) {
                        if (!first) { report << ','; } first = false;
                        printCallback(callback, origin, CallbackRecord::retainedSeries);
                    }
                    origin += work.timing.total.elapsedCycles;
                }
                report << "]}";
            }
            if (adoption) {
                const auto beforeSamples = startup.elapsedSamples();
                auto handoff = startup.releaseGameplay();
                const auto* synth = &handoff.synth();
                const auto* phase = &handoff.clockPhase();
                const auto writes = synth->registerWrites().size();
                std::ostringstream beforePhase;
                printBiosClockPhase(*phase,beforePhase);
                bool wrongResourcesRejected = false;
                try {
                    stellar7::GameplayAssets otherAssets;
                    auto invalid = std::make_unique<stellar7::NativeGameplaySession>(otherAssets,std::move(handoff));
                } catch (const std::invalid_argument&) { wrongResourcesRejected = true; }
                auto session = std::make_unique<stellar7::NativeGameplaySession>(assets,std::move(handoff));
                std::ostringstream afterPhase;
                printBiosClockPhase(*session->recoveredClockPhase(),afterPhase);
                bool repeatAdoptionRejected = false;
                try { auto invalid = std::make_unique<stellar7::NativeGameplaySession>(assets,std::move(handoff)); }
                catch (const std::invalid_argument&) { repeatAdoptionRejected = true; }
                bool repeatReleaseRejected = false;
                try { startup.releaseGameplay(); }
                catch (const std::logic_error&) { repeatReleaseRejected = true; }
                bool sampleEntryRejected = false;
                try { session->initialize(); }
                catch (const std::logic_error&) { sampleEntryRejected = true; }
                bool prematureFrameRejected = false;
                try { session->advanceLevel1Frame(); }
                catch (const std::logic_error&) { prematureFrameRejected = true; }
                const auto pages = session->pageObservation();
                for (const auto role : {stellar7::NativePageRole::front,stellar7::NativePageRole::back,stellar7::NativePageRole::cache}) {
                    const auto name = role == stellar7::NativePageRole::front ? "front" : role == stellar7::NativePageRole::back ? "back" : "cache";
                    writeBytes((directory/(std::string("adoption.")+name+".indexed")).string(),*session->retainedPagePixels(role));
                }
                report << ",\"adoption\":{\"source_entry\":\"0180\",\"same_synth\":" << (&session->synth()==synth ? "true" : "false")
                    << ",\"same_clock_phase_storage\":" << (session->recoveredClockPhase()==phase ? "true" : "false")
                    << ",\"clock_phase_equal\":" << (beforePhase.str()==afterPhase.str() ? "true" : "false")
                    << ",\"first_sample\":" << beforeSamples << ",\"sample_end\":" << session->elapsedSamples()
                    << ",\"opl_before\":" << writes << ",\"opl_after\":" << session->synth().registerWrites().size()
                    << ",\"early_rejected\":" << (earlyAdoptionRejected ? "true" : "false")
                    << ",\"wrong_resources_rejected\":" << (wrongResourcesRejected ? "true" : "false")
                    << ",\"repeat_adoption_rejected\":" << (repeatAdoptionRejected ? "true" : "false")
                    << ",\"repeat_release_rejected\":" << (repeatReleaseRejected ? "true" : "false")
                    << ",\"sample_entry_rejected\":" << (sampleEntryRejected ? "true" : "false")
                    << ",\"premature_frame_rejected\":" << (prematureFrameRejected ? "true" : "false")
                    << ",\"front\":" << pages.front << ",\"back\":" << pages.back << ",\"cache\":" << pages.cache
                    << ",\"draw\":" << pages.draw << ",\"copy\":" << pages.copy
                    << ",\"model_ticks\":" << session->model().ticks() << ",\"rng_calls\":" << session->model().level1State().rng().calls()
                    << ",\"cockpit_recorded_loader_draws\":" << unsigned{session->model().lastCockpitInitialization().displaySteps}
                    << ",\"loader_executed\":false,\"frames_executed\":0}";
                if (level1Loader) {
                    auto copyProfile = stellar7::FrameCopyProfile::unknown;
                    if (level1Frames) {
                        const auto drivers = stellar7::decodeTaggedPayloads(readFile(
                            (std::filesystem::path(argv[2])/"STELLAR.RES").string()), {'V','G','A',':'});
                        if (drivers.empty()) { throw std::invalid_argument("Level1 display requires its original VGA driver"); }
                        copyProfile = stellar7::decodeFrameCopyProfile(drivers.front());
                    }
                    const auto exportBoundary = [&](const std::string& name, const std::uint64_t firstSample) {
                        const auto roles = session->pageObservation();
                        for (const auto role : {stellar7::NativePageRole::front,stellar7::NativePageRole::back,stellar7::NativePageRole::cache}) {
                            const auto label = role == stellar7::NativePageRole::front ? "front" : role == stellar7::NativePageRole::back ? "back" : "cache";
                            writeBytes((directory/(name+"."+label+".indexed")).string(),*session->retainedPagePixels(role));
                        }
                        std::vector<std::uint8_t> rgb;
                        for (const auto color : session->frame().palette) { rgb.insert(rgb.end(),{color.red,color.green,color.blue}); }
                        writeBytes((directory/(name+".rgb")).string(),rgb);
                        report << "{\"first_sample\":" << firstSample << ",\"sample_end\":" << session->elapsedSamples()
                            << ",\"same_synth\":" << (&session->synth()==synth ? "true" : "false")
                            << ",\"same_clock_phase_storage\":" << (session->recoveredClockPhase()==phase ? "true" : "false")
                            << ",\"model_ticks\":" << session->model().ticks() << ",\"loop_tick\":" << session->model().level1State().loopTick()
                            << ",\"front\":" << roles.front << ",\"back\":" << roles.back << ",\"cache\":" << roles.cache
                            << ",\"draw\":" << roles.draw << ",\"copy\":" << roles.copy << ",\"ground\":";
                        const auto& state = session->model().level1State();
                        printGroundState(report,state.ground(),state.rng());
                        report << ",\"actors\":[";
                        for (std::size_t index = 0; index < state.entities().size(); ++index) {
                            if (index != 0) { report << ','; }
                            printLoopEntity(report,state.entities()[index],state.entities()[index].allocationIndex);
                        }
                        report << "],\"player\":";
                        printLoopEntity(report,state.player(),std::nullopt);
                        if (std::any_of(state.projectiles().begin(),state.projectiles().end(),
                            [](const auto& shot) { return shot.active(); })) {
                            report << ",\"fire_state\":" << stellar7::level1StateTraceJson(state,"completed-frame")
                                << ",\"projectile_cache\":[";
                            for (std::size_t slot = 0; slot < state.projectiles().size(); ++slot) {
                                const auto& shot = state.projectiles()[slot];
                                report << (slot ? "," : "") << '[' << shot.presenterDistance << ',' << shot.targetHeading << ']';
                            }
                            report << "],\"mixed_order\":[";
                            const auto objects = state.radarPresentationObjects();
                            for (std::size_t index = 0; index < objects.size(); ++index) {
                                report << (index ? "," : "") << '[' << static_cast<unsigned>(objects[index].kind)
                                    << ',' << objects[index].slot << ']';
                            }
                            report << ']';
                        }
                        report << ",\"active_order\":[";
                        for (std::size_t index = 0; index < state.activeListOrder().size(); ++index) {
                            report << (index ? "," : "") << state.activeListOrder()[index];
                        }
                        report << "],\"phase\":";
                        printBiosClockPhase(*session->recoveredClockPhase(),report);
                        report << ",\"callback\":";
                        printSoundBlasterCallbackState(session->synth().callbackWorkState(),report,SoundBlasterStateDetail::boundedWork);
                    };
                    const auto exportWork = [&](const auto& work) {
                        std::int64_t elapsed = 0;
                        std::uint64_t dispatches = 0;
                        report << ",\"opl_events\":[";
                        bool comma = false;
                        for (const auto& packet : work) {
                            for (const auto& write : packet.audio.writes) {
                                report << (comma ? "," : "") << '[' << elapsed+write.cycleOffset << ','
                                    << unsigned{write.write.registerNumber} << ',' << unsigned{write.write.value} << ']';
                                comma = true;
                            }
                            elapsed += packet.timing.total.elapsedCycles;
                            dispatches += packet.timing.total.dispatches;
                        }
                        report << "],\"elapsed_cycles\":" << elapsed << ",\"dispatches\":" << dispatches;
                    };
                    const auto delivery = session->initializeRecoveredLevel1(packetPolls);
                    writeWave((directory/"level1-loader.wav").string(),delivery.samples);
                    bool repeatLoaderRejected = false;
                    try { session->initializeRecoveredLevel1(packetPolls); }
                    catch (const std::invalid_argument&) { repeatLoaderRejected = true; }
                    report << ",\"level1_loader\":";
                    exportBoundary("level1-loader",delivery.firstSample);
                    exportWork(session->recoveredLevel1Loader()->work);
                    report << ",\"repeat_rejected\":" << (repeatLoaderRejected ? "true" : "false")
                        << ",\"native_provider_elapsed_qualified\":false}";
                    if (level1Frames) {
                        report << ",\"level1_frames\":[";
                        std::size_t nextInput = 0;
                        std::set<std::string> heldInput;
                        for (unsigned frame = 0; frame < level1FrameCount; ++frame) {
                            applyLevel1InputEventsBeforeTick(level1Input,frame+1,nextInput,heldInput);
                            const auto delivered = [&] {
                                try {
                                    return session->advanceRecoveredLevel1Frame(copyProfile,packetPolls,
                                        gameplayInputFromHeld(heldInput));
                                } catch (const std::invalid_argument& error) {
                                    throw std::invalid_argument("recovered Level1 frame " + std::to_string(frame+1)
                                        + ": " + error.what());
                                }
                            }();
                            const auto label = std::string("level1-frame-")+std::to_string(frame+1);
                            writeWave((directory/(label+".wav")).string(),delivered.samples);
                            if (frame != 0) { report << ','; }
                            exportBoundary(label,delivered.firstSample);
                            const auto& run = *session->recoveredLevel1Frame();
                            exportWork(run.work);
                            report << ",\"first_sample_frame\":" << run.firstSample
                                << ",\"prefix_rng_calls\":" << run.prefixRngCalls << ",\"ground_rng_calls\":" << run.groundRngCalls
                                << ",\"palette_uploads\":" << run.palettes.size() << '}';
                        }
                        report << ']';
                        const auto end = session->elapsedSamples();
                        bool unsupportedInputRejected = false;
                        stellar7::GameplayInput unsupportedInput;
                        unsupportedInput.confirm = true;
                        try { session->advanceRecoveredLevel1Frame(copyProfile,packetPolls,unsupportedInput); }
                        catch (const std::invalid_argument&) { unsupportedInputRejected = true; }
                        report << ",\"unsupported_input_rejected\":" << (unsupportedInputRejected && session->elapsedSamples()==end ? "true" : "false");
                    }
                }
            }
            report << "}\n";
            if (!report) { throw std::runtime_error("cannot write native logo work receipt"); }
            return 0;
        }
        if (argc == 4 && (std::string(argv[1]) == "--trace-native-startup-context"
            || std::string(argv[1]) == "--trace-native-startup-settings"
            || std::string(argv[1]) == "--trace-native-startup-resources"
            || std::string(argv[1]) == "--trace-native-startup-presentation")) {
            const auto assets = loadGameplayAssets(argv[2]);
            const auto resources = stellar7::decodeNativeStartupAssets(
                readFile((std::filesystem::path(argv[2]) / "STELART.RES").string()));
            stellar7::NativeStartupSequence startup(assets, resources);
            startup.start(stellar7::NativeStartupExecution::recoveredContext);
            const auto& run = *startup.recoveredContext();
            const std::filesystem::path directory(argv[3]);
            std::filesystem::create_directories(directory);
            auto pcm = run.installer.audio.samples;
            pcm.insert(pcm.end(), run.work.audio.samples.begin(), run.work.audio.samples.end());
            writeWave((directory / "audio.wav").string(), pcm);
            std::ofstream report(directory / "context.json");
            report << "{\"clock_domain\":\"native-recovered-context\",\"boundary\":\"0824:41BB\""
                << ",\"awaiting_settings\":true,\"natural_startup_qualified\":false,\"gameplay_qualified\":false"
                << ",\"original_state_imports\":0,\"logo_steps\":" << startup.logoSteps()
                << ",\"samples\":" << startup.elapsedSamples() << ",\"matrix_mode\":" << run.context.state.matrix.mode
                << ",\"matrix\":[";
            for (std::size_t index = 0; index < run.context.state.matrix.values.size(); ++index) {
                report << (index == 0 ? "" : ",") << run.context.state.matrix.values[index];
            }
            const auto& light = run.context.state.light;
            report << "],\"light\":[" << light.x << ',' << light.y << ',' << light.z << "],\"effects\":[";
            for (std::size_t index = 0; index < run.context.effects.size(); ++index) {
                const auto& effect = run.context.effects[index];
                report << (index == 0 ? "[" : ",[") << static_cast<unsigned>(effect.output) << ','
                    << effect.index << ',' << effect.value << ',' << effect.sourcePc << ',' << effect.afterDispatch << ']';
            }
            report << "],\"operations\":[";
            for (std::size_t index = 0; index < run.context.operations.size(); ++index) {
                report << (index == 0 ? "" : ",") << run.context.operations[index].count;
            }
            const auto writeWork = [&](const stellar7::ClockedPresentationWork& work) {
                report << "{\"total\":";
                printSoundBlasterClockStep(work.timing.total, report);
                report << ",\"clock_phase\":";
                printBiosClockPhase(work.timing.phase, report);
                report << ",\"timer_install_stage\":" << static_cast<unsigned>(work.timing.phase.timerInstall->stage)
                    << ",\"first_sample\":" << work.audio.firstSample << ",\"samples\":" << work.audio.samples.size()
                    << ",\"callbacks\":" << work.timing.callbacks.size() << ",\"opl_writes\":" << work.audio.writes.size() << '}';
            };
            report << "],\"installer\":";
            writeWork(run.installer);
            report << ",\"context\":";
            writeWork(run.work);
            report << "}\n";
            if (!report) {
                throw std::runtime_error("cannot write native startup context receipt");
            }
            if (std::string(argv[1]) != "--trace-native-startup-context") {
                startup.initializeSettings(stellar7::StartupCapabilityProfile::noMouseOrJoystick);
                const auto& completed = *startup.recoveredSettings();
                const auto& settings = completed.settings;
                writeWave((directory / "settings.wav").string(), completed.work.audio.samples);
                std::ofstream settingsReport(directory / "settings.json");
                settingsReport << "{\"boundary\":\"0824:4059\",\"awaiting_resource_initialization\":true"
                    << ",\"host_io_advances_virtual_time\":false,\"mouse_capability\":\"absent-native\""
                    << ",\"original_state_imports\":0,\"original_dos_elapsed_qualified\":false,\"gameplay_qualified\":false"
                    << ",\"samples\":" << startup.elapsedSamples() << ",\"configuration\":[" << settings.configuration.viewSize;
                for (const auto option : settings.configuration.options) {
                    settingsReport << ',' << option;
                }
                settingsReport << "],\"loaded_header\":[";
                const auto& header = settings.loadedTable.header();
                for (std::size_t index = 0; index < header.size(); ++index) {
                    settingsReport << (index == 0 ? "" : ",") << static_cast<unsigned>(header[index]);
                }
                settingsReport << "],\"view_accumulator\":" << settings.viewAccumulator << ",\"view_descriptor\":[";
                for (std::size_t index = 0; index < settings.viewDescriptor.words.size(); ++index) {
                    settingsReport << (index == 0 ? "" : ",") << settings.viewDescriptor.words[index];
                }
                settingsReport << "],\"effects\":[";
                for (std::size_t index = 0; index < settings.effects.size(); ++index) {
                    const auto& effect = settings.effects[index];
                    settingsReport << (index == 0 ? "[" : ",[") << static_cast<unsigned>(effect.kind) << ','
                        << effect.value << ',' << effect.sourcePc << ',' << effect.afterDispatch << ']';
                }
                settingsReport << "],\"host_services\":[";
                for (std::size_t index = 0; index < settings.hostServices.size(); ++index) {
                    const auto& service = settings.hostServices[index];
                    settingsReport << (index == 0 ? "[" : ",[") << static_cast<unsigned>(service.kind) << ','
                        << service.afterDispatch << ']';
                }
                settingsReport << "],\"operations\":[";
                for (std::size_t index = 0; index < settings.operations.size(); ++index) {
                    settingsReport << (index == 0 ? "" : ",") << settings.operations[index].count;
                }
                settingsReport << "],\"work\":{\"total\":";
                printSoundBlasterClockStep(completed.work.timing.total, settingsReport);
                settingsReport << ",\"clock_phase\":";
                printBiosClockPhase(completed.work.timing.phase, settingsReport);
                settingsReport << ",\"first_sample\":" << completed.work.audio.firstSample
                    << ",\"samples\":" << completed.work.audio.samples.size()
                    << ",\"callbacks\":" << completed.work.timing.callbacks.size()
                    << ",\"opl_writes\":" << completed.work.audio.writes.size() << "}}\n";
                if (!settingsReport) {
                    throw std::runtime_error("cannot write native startup settings receipt");
                }
                if (std::string(argv[1]) == "--trace-native-startup-resources"
                    || std::string(argv[1]) == "--trace-native-startup-presentation") {
                    startup.initializeResources(stellar7::NativeStartupDeviceProfile::portableOplNoDspProbe);
                    const auto& initialized = *startup.recoveredResources();
                    const auto& work = initialized.work;
                    const auto& phase = work.timing.phase;
                    writeWave((directory / "resources.wav").string(), work.audio.samples);
                    std::ofstream resourceReport(directory / "resources.json");
                    resourceReport << "{\"boundary\":\"0824:6114\",\"awaiting_presentation_resources\":true"
                        << ",\"native_probe_policy\":\"portable-opl-no-dsp-probe\",\"original_state_imports\":0"
                        << ",\"original_dos_elapsed_qualified\":false,\"gameplay_qualified\":false"
                        << ",\"initial_opl_writes\":" << run.work.timing.phase.soundBlasterInitialization->oplWrites
                        << ",\"driver_enabled\":" << (phase.soundBlasterInitialization->driverEnabled ? "true" : "false")
                        << ",\"song_allocated\":" << (phase.callback->domain == stellar7::SoundBlasterCallbackDomain::noHandle ? "false" : "true")
                        << ",\"first_sample\":" << work.audio.firstSample << ",\"samples\":" << startup.elapsedSamples()
                        << ",\"timer\":{\"active_mask\":" << phase.timer->activeMask << ",\"reload\":" << phase.timer->callbackReload
                        << ",\"count\":" << phase.timer->callbackCount << ",\"global_count\":" << phase.timer->globalCount
                        << ",\"pointer\":[" << phase.timerInstall->callbackSegment << ',' << phase.timerInstall->callbackOffset << "]}"
                        << ",\"total\":";
                    printSoundBlasterClockStep(work.timing.total, resourceReport);
                    resourceReport << ",\"clock_phase\":";
                    printBiosClockPhase(phase, resourceReport);
                    resourceReport << ",\"opl_writes\":[";
                    for (std::size_t index = 0; index < work.audio.writes.size(); ++index) {
                        const auto& write = work.audio.writes[index];
                        resourceReport << (index == 0 ? "[" : ",[") << write.cycleOffset << ','
                            << static_cast<unsigned>(write.write.registerNumber) << ',' << static_cast<unsigned>(write.write.value) << ']';
                    }
                    resourceReport << "],\"bank\":[";
                    const auto& driver = initialized.resources.driverInitialization;
                    for (std::size_t index = 0; index < driver.state.bank.size(); ++index) {
                        resourceReport << (index == 0 ? "" : ",") << static_cast<unsigned>(driver.state.bank[index]);
                    }
                    resourceReport << "],\"operator_records\":[";
                    for (std::size_t index = 0; index < driver.state.operators.size(); ++index) {
                        resourceReport << (index == 0 ? "[" : ",[");
                        const auto& patch = driver.state.operators[index];
                        for (std::size_t byte = 0; byte < patch.size(); ++byte) {
                            resourceReport << (byte == 0 ? "" : ",") << static_cast<unsigned>(patch[byte]);
                        }
                        resourceReport << ']';
                    }
                    std::uint64_t driverDispatches = 0;
                    for (const auto& operation : driver.operations) {
                        driverDispatches += operation.kind == stellar7::NormalWorkKind::soundBlasterOplWrite
                            ? stellar7::soundBlasterOplWork().dispatches : operation.count;
                    }
                    resourceReport << "],\"driver_source_dispatches\":" << driverDispatches << ",\"driver_source_effects\":[";
                    for (std::size_t index = 0; index < driver.effects.size(); ++index) {
                        const auto& effect = driver.effects[index];
                        resourceReport << (index == 0 ? "[" : ",[") << static_cast<unsigned>(effect.kind) << ','
                            << effect.index << ',' << effect.value << ',' << effect.sourcePc << ',' << effect.afterDispatch << ']';
                    }
                    resourceReport << "],\"source_effects\":" << driver.effects.size()
                        << ",\"native_resource_services\":" << initialized.resources.hostServices.size()
                        << ",\"native_probe_services\":" << driver.hostServices.size()
                        << ",\"transfer_buffer_bytes\":" << initialized.transferBuffer.size()
                        << ",\"scratch_buffer_bytes\":" << initialized.scratchBuffer.size() << "}\n";
                    if (!resourceReport) {
                        throw std::runtime_error("cannot write native startup resource receipt");
                    }
                    if (std::string(argv[1]) == "--trace-native-startup-presentation") {
                        startup.initializePresentation();
                        const auto& presentation = *startup.recoveredPresentation();
                        writeWave((directory / "presentation.wav").string(), presentation.samples);
                        for (const auto page : {stellar7::NativeMenuPage::front, stellar7::NativeMenuPage::back,
                            stellar7::NativeMenuPage::cache}) {
                            const auto name = page == stellar7::NativeMenuPage::front ? "front"
                                : page == stellar7::NativeMenuPage::back ? "back" : "cache";
                            const auto pixels = startup.menuPagePixels(page);
                            std::ofstream indexed(directory / (std::string(name) + ".indexed"), std::ios::binary);
                            indexed.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
                            if (!indexed) { throw std::runtime_error("cannot write native startup page"); }
                        }
                        std::ofstream dac(directory / "presentation.dac", std::ios::binary);
                        dac.write(reinterpret_cast<const char*>(presentation.palette.dac.colors.data()), stellar7::vgaPaletteBytes);
                        if (!dac) { throw std::runtime_error("cannot write native startup palette"); }
                        std::size_t noHandle = 0;
                        std::size_t oplWrites = 0;
                        std::size_t commits = 0;
                        const auto countWork = [&](const auto& part) {
                            oplWrites += part.audio.writes.size();
                            for (const auto& callback : part.timing.callbacks) {
                                noHandle += callback.advance.kind == stellar7::SoundBlasterCallbackKind::noHandle;
                            }
                        };
                        for (const auto& part : presentation.work) { countWork(part); }
                        for (const auto& part : presentation.palettes) {
                            countWork(part.work);
                            commits += part.commits.size();
                        }
                        std::ofstream output(directory / "presentation.json");
                        output << "{\"boundary\":\"0824:61c1\",\"original_state_imports\":0"
                            << ",\"original_dos_elapsed_qualified\":false,\"gameplay_qualified\":false"
                            << ",\"native_graphics_provider\":\"mode-x-black-unclipped-zero-initial-dac\""
                            << ",\"loaded_songs\":" << presentation.songs.size()
                            << ",\"song_selected\":" << (presentation.finalPhase.sequenceSelection->published ? "true" : "false")
                            << ",\"selected_song\":" << *presentation.selectedHead
                            << ",\"first_sample\":" << presentation.firstSample << ",\"samples\":" << startup.elapsedSamples()
                            << ",\"draw_page\":\"cache\",\"opl_writes\":" << oplWrites
                            << ",\"no_handle_callbacks\":" << noHandle << ",\"palette_commits\":" << commits
                            << ",\"global_count\":" << startup.countdown()
                            << ",\"global_count_writes\":[240],\"palette_intensities\":[";
                        std::size_t paletteIndex = 0;
                        for (const auto& segment : presentation.presentation.segments) {
                            if (segment.kind == stellar7::StartupPresentationKind::palette) {
                                output << (paletteIndex++ == 0 ? "" : ",") << static_cast<unsigned>(segment.request.intensity);
                            }
                        }
                        const auto driverSource = [&](const auto& body) {
                            std::uint64_t dispatches = 0;
                            for (const auto& operation : body.operations) { dispatches += operation.count; }
                            output << "{\"dispatches\":" << dispatches << ",\"effects\":[";
                            for (std::size_t index = 0; index < body.effects.size(); ++index) {
                                const auto& effect = body.effects[index];
                                output << (index == 0 ? "[" : ",[") << static_cast<unsigned>(effect.kind) << ','
                                    << effect.index << ',' << effect.value << ',' << effect.sourcePc << ',' << effect.afterDispatch << ']';
                            }
                            output << "]}";
                        };
                        output << "],\"direct_reset\":";
                        driverSource(presentation.directReset);
                        output << ",\"sequence_initialization\":";
                        driverSource(presentation.sequenceInitialization);
                        output << ",\"clock_phase\":";
                        printBiosClockPhase(presentation.finalPhase, output);
                        output << "}\n";
                        if (!output) { throw std::runtime_error("cannot write native presentation receipt"); }
                    }
                }
            }
            return 0;
        }
        if ((argc == 6 || argc == 7)
            && (std::string(argv[1]) == "--trace-native-session"
            || std::string(argv[1]) == "--trace-native-complete-frames"
            || std::string(argv[1]) == "--trace-native-entry"
            || std::string(argv[1]) == "--trace-native-entry-pages")) {
            std::size_t consumed = 0;
            const auto count = std::stoul(argv[4], &consumed);
            if (argv[4][consumed] != '\0') {
                throw std::runtime_error("invalid native session frame count");
            }
            const auto events = std::filesystem::is_regular_file(argv[3])
                ? loadLevel1InputScript(argv[3], InputScriptDomain::campaign)
                : parseLevel1InputEventSpec(argv[3]);
            const auto complete = std::string(argv[1]) == "--trace-native-complete-frames";
            const auto startup = std::string(argv[1]) != "--trace-native-session"
                ? std::optional<stellar7::NativeStartupAssets>{stellar7::decodeNativeStartupAssets(
                    readFile((std::filesystem::path(argv[2]) / "STELART.RES").string()))}
                : std::nullopt;
            const auto firstArtifact = argc == 7 ? std::stoul(argv[6], &consumed) : 1;
            if (argc == 7 && (argv[6][consumed] != '\0' || firstArtifact == 0 || firstArtifact > count)) {
                throw std::runtime_error("invalid native trace first artifact frame");
            }
            traceNativeSession(loadGameplayAssets(argv[2]), events, count, argv[5], startup ? &*startup : nullptr,
                std::string(argv[1]) == "--trace-native-entry-pages" ? NativeTracePages::physicalBack : NativeTracePages::delivered,
                static_cast<std::uint32_t>(firstArtifact),
                complete ? NativeTraceBoundary::completedTail : NativeTraceBoundary::display);
            return 0;
        }
        if (argc == 3 && (std::string(argv[1]) == "--native-entry-skip"
            || std::string(argv[1]) == "--playable-campaign")) {
            const auto startup = stellar7::decodeNativeStartupAssets(
                readFile((std::filesystem::path(argv[2]) / "STELART.RES").string()));
            const auto drivers = stellar7::decodeTaggedPayloads(
                readFile((std::filesystem::path(argv[2]) / "STELLAR.RES").string()), {'V','G','A',':'});
            if (drivers.empty()) { throw std::runtime_error("missing native frame-copy driver"); }
            const auto copyProfile = stellar7::decodeFrameCopyProfile(drivers.front());
            if (std::string(argv[1]) == "--playable-campaign") {
                auto assets=loadGameplayAssets(argv[2]);
                const std::filesystem::path dataDirectory=argv[2];
                assets.storyScenes=std::make_shared<const stellar7::StorySceneAssets>(stellar7::decodeStorySceneAssets(
                    readFile((dataDirectory/"SCENEB1.RES").string()),readFile((dataDirectory/"SCENEA.RES").string()),
                    {readFile((dataDirectory/"SCENEB2.RES").string()),readFile((dataDirectory/"SCENEB3.RES").string()),
                        readFile((dataDirectory/"SCENEB5.RES").string())},
                    readFile((dataDirectory/"SCENEC.RES").string()),readFile((dataDirectory/"VOICE.RES").string()),
                    readFile((dataDirectory/"DRAXON.RES").string()),readFile((dataDirectory/"SCENEE.RES").string())));
                assets.attractVoiceAvailable=true; //The actual VOICE resource was read and validated above.
                auto options=nativeGameplayOptions(dataDirectory,stellar7::HighscoreWritePolicy::nativeStagedReplace);
                options.menuFeatures=stellar7::NativeMenuFeatures::releaseMenus;
                options.nativeGameRate=stellar7::defaultNativeGameRate;
                return stellar7::runSdlPlayableNativeEntry(std::move(assets),startup,copyProfile,std::move(options));
            }
            return stellar7::runRecoveredNativeEntry(loadGameplayAssets(argv[2]), startup, copyProfile,
                nativeGameplayOptions(argv[2]));
        }
        if ((argc == 3 || argc == 4) && std::string(argv[1]) == "--menu") {
            return stellar7::runMenu(std::cin, std::cout, [&]() {
                return stellar7::runGameplay(loadGameplayAssets(argv[2], argc == 4 ? argv[3] : ""), nativeGameplayOptions(argv[2]));
            });
        }
        if ((argc == 3 || argc == 4) && std::string(argv[1]) == "--play") {
            return stellar7::runGameplay(loadGameplayAssets(argv[2], argc == 4 ? argv[3] : ""), nativeGameplayOptions(argv[2]));
        }
        if ((argc == 3 || argc == 4) && std::string(argv[1]) == "--play-level2") {
            return stellar7::runGameplay(
                loadGameplayAssets(argv[2], argc == 4 ? argv[3] : ""),
                {"", true}
            );
        }
        if (
            (argc == 3 || argc == 4 || argc == 5)
            && std::string(argv[1]) == "--play-level3"
        ) {
            return stellar7::runGameplay(
                loadGameplayAssets(
                    argv[2],
                    argc >= 4 ? argv[3] : "",
                    argc == 5 ? argv[4] : ""
                ),
                {"", false, true}
            );
        }
        if (
            (argc == 5 || argc == 6)
            && std::string(argv[1]) == "--play-level3-record"
        ) {
            const int result = stellar7::runGameplay(
                loadGameplayAssets(
                    argv[2],
                    argv[3],
                    argc == 6 ? argv[5] : ""
                ),
                {argv[4], false, true}
            );
            if (result == 0) {
                std::cout << "wrote " << argv[4] << '\n';
            }
            return result;
        }
        if (
            (argc == 6 || argc == 7)
            && std::string(argv[1]) == "--play-level3-guardian-record"
        ) {
            stellar7::GameplayModel model;
            stellar7::loadLevel3NativeCheckpoint(model, readFile(argv[4]));
            const int result = stellar7::runGameplay(
                loadGameplayAssets(
                    argv[2],
                    argv[3],
                    argc == 7 ? argv[6] : ""
                ),
                std::move(model),
                {argv[5], false, true}
            );
            if (result == 0) {
                std::cout << "wrote " << argv[5] << '\n';
            }
            return result;
        }
        if (
            (argc == 5 || argc == 6)
            && std::string(argv[1]) == "--play-record"
        ) {
            const int result = stellar7::runGameplay(
                loadGameplayAssets(
                    argv[2],
                    argv[3],
                    argc == 6 ? argv[5] : ""
                ),
                {argv[4]}
            );
            if (result == 0) {
                std::cout << "wrote " << argv[4] << '\n';
            }
            return result;
        }
        if (
            argc == 4
            && std::string(argv[1]) == "--export-level1-success-script"
        ) {
            exportLevel1SuccessScript(argv[2], argv[3]);
            std::cout
                << "wrote " << argv[2] << '\n'
                << "wrote " << argv[3] << '\n';
            return 0;
        }
        if (
            argc == 3
            && std::string(argv[1]) == "--trace-level1-entry"
        ) {
            writeText(
                argv[2],
                stellar7::level1StateTraceJson(
                    stellar7::Level1State{},
                    "level1-entry-pre-integration"
                )
            );
            std::cout << "wrote " << argv[2] << '\n';
            return 0;
        }
        if (
            argc == 3
            && std::string(argv[1]) == "--trace-level1-first-idle-tick"
        ) {
            stellar7::Level1State state;
            state.advanceIdleTick();
            writeText(
                argv[2],
                stellar7::level1StateTraceJson(
                    state,
                    "level1-first-idle-tick-post-main-render"
                )
            );
            std::cout << "wrote " << argv[2] << '\n';
            return 0;
        }
        if (
            argc == 4
            && std::string(argv[1]) == "--trace-level1-idle-ticks"
        ) {
            const auto count = std::stoul(argv[2]);
            if (count == 0) {
                throw std::runtime_error("idle tick count must be positive");
            }
            stellar7::Level1State state;
            for (std::uint32_t tick = 0; tick < count; ++tick) {
                state.advanceIdleTick();
            }
            writeText(
                argv[3],
                stellar7::level1StateTraceJson(
                    state,
                    "level1-idle-ticks-post-main-render"
                )
            );
            std::cout << "wrote " << argv[3] << '\n';
            return 0;
        }
        if (
            argc == 7
            && std::string(argv[1]) == "--trace-level1-held-input"
        ) {
            const std::string keySpec = argv[2];
            const auto pressTick = std::stoul(argv[3]);
            const auto releaseTick = std::stoul(argv[4]);
            const auto count = std::stoul(argv[5]);
            if (
                count == 0
                || pressTick == 0
                || releaseTick <= pressTick
            ) {
                throw std::runtime_error(
                    "held input requires "
                    "0 < press tick < release tick"
                );
            }
            std::vector<std::string> keys;
            std::size_t start = 0;
            while (start <= keySpec.size()) {
                const auto separator = keySpec.find('+', start);
                const auto key = keySpec.substr(
                    start,
                    separator == std::string::npos
                        ? std::string::npos
                        : separator - start
                );
                if (
                    key.empty()
                    || (
                        key != "left"
                        && key != "right"
                        && key != "up"
                        && key != "down"
                        && key != "spc"
                    )
                    || std::find(keys.begin(), keys.end(), key) != keys.end()
                ) {
                    throw std::runtime_error(
                        "unsupported held input key specification"
                    );
                }
                keys.push_back(key);
                if (separator == std::string::npos) {
                    break;
                }
                start = separator + 1;
            }
            const auto hasKey = [&keys](const std::string& key) {
                return std::find(keys.begin(), keys.end(), key) != keys.end();
            };
            if (
                (hasKey("left") && hasKey("right"))
                || (hasKey("up") && hasKey("down"))
            ) {
                throw std::runtime_error(
                    "held input cannot contain opposing directions"
                );
            }
            stellar7::Level1State state;
            for (std::uint32_t tick = 1; tick <= count; ++tick) {
                const bool active = tick >= pressTick && tick < releaseTick;
                stellar7::Level1Input input{};
                input.left = active && hasKey("left");
                input.right = active && hasKey("right");
                input.forward = active && hasKey("up");
                input.reverse = active && hasKey("down");
                input.fire = active && hasKey("spc");
                state.advanceTick(input);
            }
            writeText(
                argv[6],
                stellar7::level1StateTraceJson(
                    state,
                    "level1-held-input-post-main-render"
                )
            );
            std::cout << "wrote " << argv[6] << '\n';
            return 0;
        }
        if (
            argc == 5
            && std::string(argv[1])
                == "--trace-level1-input-script-file-series"
        ) {
            writeLevel1InputScriptTraceSeries(
                loadLevel1InputScript(argv[2]),
                parseLevel1TraceTicks(argv[3]),
                argv[4],
                "level1-input-script-post-main-render"
            );
            std::cout << "wrote trace series to " << argv[4] << '\n';
            return 0;
        }
        if (
            argc == 5
            && std::string(argv[1])
                == "--trace-gameplay-input-script-phases"
        ) {
            writeGameplayInputScriptPhases(
                loadLevel1InputScript(argv[2], InputScriptDomain::campaign),
                static_cast<std::uint32_t>(std::stoul(argv[3])),
                argv[4]
            );
            std::cout << "wrote " << argv[4] << '\n';
            return 0;
        }
        if (
            argc == 5
            && std::string(argv[1])
                == "--trace-level1-input-script-phase"
        ) {
            writeLevel1InputScriptGameplayPhase(
                loadLevel1InputScript(argv[2]),
                static_cast<std::uint32_t>(std::stoul(argv[3])),
                argv[4]
            );
            std::cout << "wrote " << argv[4] << '\n';
            return 0;
        }
        if (
            argc == 5
            && std::string(argv[1])
                == "--trace-level3-native-guardian-forward-fire"
        ) {
            writeLevel3NativeGuardianForwardFireProbe(
                readFile(argv[2]),
                static_cast<std::uint32_t>(std::stoul(argv[3])),
                argv[4]
            );
            std::cout << "wrote " << argv[4] << '\n';
            return 0;
        }
        if (
            argc == 6
            && std::string(argv[1])
                == "--trace-level3-native-input-script-state"
        ) {
            writeLevel3NativeInputScriptState(
                readFile(argv[2]),
                loadLevel1InputScript(argv[3]),
                static_cast<std::uint32_t>(std::stoul(argv[4])),
                argv[5]
            );
            std::cout << "wrote " << argv[5] << '\n';
            return 0;
        }
        if (
            argc == 8
            && std::string(argv[1])
                == "--probe-level3-native-input-constant"
        ) {
            writeLevel3NativeConstantInputProbe(
                readFile(argv[2]),
                loadLevel1InputScript(argv[3]),
                static_cast<std::uint32_t>(std::stoul(argv[4])),
                static_cast<std::uint32_t>(std::stoul(argv[5])),
                argv[6],
                argv[7]
            );
            std::cout << "wrote " << argv[7] << '\n';
            return 0;
        }
        if (argc == 6 && (std::string(argv[1])
                == "--trace-level2-input-script-state-series-armored"
                || std::string(argv[1])
                == "--trace-level2-input-script-state-series-armor-held")) {
            const auto armor = std::stol(argv[3]);
            if (armor < -32768 || armor > 32767) {
                throw std::runtime_error("Level 2 armor is out of range");
            }
            writeAcceptanceStateSeries(
                loadLevel1InputScript(argv[2]), parseLevel1TraceTicks(argv[4]),
                argv[5], static_cast<std::int16_t>(armor), {},
                std::string(argv[1])
                    == "--trace-level2-input-script-state-series-armor-held"
            );
            return 0;
        }
        if (argc == 6 && std::string(argv[1])
                == "--trace-level3-native-input-script-state-series") {
            writeAcceptanceStateSeries(
                loadLevel1InputScript(argv[3]), parseLevel1TraceTicks(argv[4]),
                argv[5], std::nullopt, readFile(argv[2])
            );
            return 0;
        }
        if (
            argc == 5
            && std::string(argv[1])
                == "--trace-level2-input-script-state"
        ) {
            writeLevel2InputScriptState(
                loadLevel1InputScript(argv[2]),
                static_cast<std::uint32_t>(std::stoul(argv[3])),
                argv[4]
            );
            std::cout << "wrote " << argv[4] << '\n';
            return 0;
        }
        if (
            argc == 6
            && std::string(argv[1])
                == "--trace-level2-input-script-state-armored"
        ) {
            const auto parsedArmor = std::stol(argv[3]);
            if (
                parsedArmor < std::numeric_limits<std::int16_t>::min()
                || parsedArmor > std::numeric_limits<std::int16_t>::max()
            ) {
                throw std::runtime_error("Level 2 armor is out of range");
            }
            writeLevel2InputScriptState(
                loadLevel1InputScript(argv[2]),
                static_cast<std::uint32_t>(std::stoul(argv[4])),
                argv[5],
                static_cast<std::int16_t>(parsedArmor)
            );
            std::cout << "wrote " << argv[5] << '\n';
            return 0;
        }
        if (
            argc == 8
            && std::string(argv[1])
                == "--export-level2-success-controller"
        ) {
            const auto parsedArmor = std::stol(argv[3]);
            if (
                parsedArmor < std::numeric_limits<std::int16_t>::min()
                || parsedArmor > std::numeric_limits<std::int16_t>::max()
            ) {
                throw std::runtime_error("Level 2 armor is out of range");
            }
            exportLevel2SuccessfulControllerRoute(
                loadLevel1InputScript(argv[2]),
                static_cast<std::int16_t>(parsedArmor),
                static_cast<std::uint32_t>(std::stoul(argv[4])),
                static_cast<std::uint32_t>(std::stoul(argv[5])),
                argv[6],
                argv[7]
            );
            std::cout << "wrote " << argv[6] << " and " << argv[7]
                << '\n';
            return 0;
        }
        if (
            argc == 8
            && std::string(argv[1])
                == "--export-level2-survival-controller"
        ) {
            const auto parsedArmor = std::stol(argv[3]);
            if (
                parsedArmor < std::numeric_limits<std::int16_t>::min()
                || parsedArmor > std::numeric_limits<std::int16_t>::max()
            ) {
                throw std::runtime_error("Level 2 armor is out of range");
            }
            exportLevel2SuccessfulControllerRoute(
                loadLevel1InputScript(argv[2]),
                static_cast<std::int16_t>(parsedArmor),
                static_cast<std::uint32_t>(std::stoul(argv[4])),
                static_cast<std::uint32_t>(std::stoul(argv[5])),
                argv[6],
                argv[7],
                true
            );
            std::cout << "wrote " << argv[6] << " and " << argv[7]
                << '\n';
            return 0;
        }
        if (
            argc == 5
            && std::string(argv[1])
                == "--export-level3-success-controller"
        ) {
            exportLevel3SuccessfulControllerRoute(
                static_cast<std::uint32_t>(std::stoul(argv[2])),
                argv[3],
                argv[4]
            );
            std::cout << "wrote " << argv[3] << " and " << argv[4]
                << '\n';
            return 0;
        }
        if (
            argc == 8
            && std::string(argv[1])
                == "--export-level3-native-input-script-controller"
        ) {
            exportLevel3NativeInputScriptControllerRoute(
                readFile(argv[2]),
                loadLevel1InputScript(argv[3]),
                static_cast<std::uint32_t>(std::stoul(argv[4])),
                static_cast<std::uint32_t>(std::stoul(argv[5])),
                argv[6],
                argv[7]
            );
            std::cout << "wrote " << argv[6] << " and " << argv[7]
                << '\n';
            return 0;
        }
        if (
            argc == 5
            && std::string(argv[1]) == "--trace-level1-input-script-file"
        ) {
            const auto count = std::stoul(argv[3]);
            writeLevel1InputScriptTrace(
                loadLevel1InputScript(argv[2]),
                static_cast<std::uint32_t>(count),
                argv[4],
                "level1-input-script-post-main-render"
            );
            std::cout << "wrote " << argv[4] << '\n';
            return 0;
        }
        if (
            argc == 5
            && std::string(argv[1]) == "--trace-level1-input-script"
        ) {
            struct InputEvent {
                std::uint32_t tick;
                bool pressed;
                std::vector<std::string> keys;
            };
            const std::string eventSpec = argv[2];
            const auto count = std::stoul(argv[3]);
            if (count == 0 || eventSpec.empty()) {
                throw std::runtime_error(
                    "input script requires events and a positive tick count"
                );
            }
            const auto parseKeys = [](const std::string& keySpec) {
                std::vector<std::string> keys;
                std::size_t start = 0;
                while (start <= keySpec.size()) {
                    const auto separator = keySpec.find('+', start);
                    const auto key = keySpec.substr(
                        start,
                        separator == std::string::npos
                            ? std::string::npos
                            : separator - start
                    );
                    if (
                        key.empty()
                        || (
                            key != "left"
                            && key != "right"
                            && key != "up"
                            && key != "down"
                            && key != "spc"
                        )
                        || std::find(keys.begin(), keys.end(), key)
                            != keys.end()
                    ) {
                        throw std::runtime_error(
                            "unsupported input script key specification"
                        );
                    }
                    keys.push_back(key);
                    if (separator == std::string::npos) {
                        break;
                    }
                    start = separator + 1;
                }
                return keys;
            };
            std::vector<InputEvent> events;
            std::set<std::string> validationHeld;
            std::size_t eventStart = 0;
            while (eventStart <= eventSpec.size()) {
                const auto eventEnd = eventSpec.find('~', eventStart);
                const auto eventText = eventSpec.substr(
                    eventStart,
                    eventEnd == std::string::npos
                        ? std::string::npos
                        : eventEnd - eventStart
                );
                const auto assignment = eventText.find('=');
                const auto separator = eventText.find(
                    '.',
                    assignment == std::string::npos
                        ? 0
                        : assignment + 1
                );
                if (
                    assignment == std::string::npos
                    || separator == std::string::npos
                ) {
                    throw std::runtime_error(
                        "invalid input script event"
                    );
                }
                const auto tick = std::stoul(
                    eventText.substr(0, assignment)
                );
                const auto operation = eventText.substr(
                    assignment + 1,
                    separator - assignment - 1
                );
                if (
                    tick == 0
                    || (
                        !events.empty()
                        && tick < events.back().tick
                    )
                    || (operation != "down" && operation != "up")
                ) {
                    throw std::runtime_error(
                        "invalid input script event"
                    );
                }
                auto keys = parseKeys(eventText.substr(separator + 1));
                const bool pressed = operation == "down";
                for (const auto& key : keys) {
                    if (
                        (pressed && validationHeld.count(key) != 0)
                        || (!pressed && validationHeld.count(key) == 0)
                    ) {
                        throw std::runtime_error(
                            "input script key state is inconsistent"
                        );
                    }
                    if (pressed) {
                        validationHeld.insert(key);
                    } else {
                        validationHeld.erase(key);
                    }
                }
                if (
                    (
                        validationHeld.count("left") != 0
                        && validationHeld.count("right") != 0
                    )
                    || (
                        validationHeld.count("up") != 0
                        && validationHeld.count("down") != 0
                    )
                ) {
                    throw std::runtime_error(
                        "input script cannot hold opposing directions"
                    );
                }
                events.push_back({
                    static_cast<std::uint32_t>(tick),
                    pressed,
                    std::move(keys),
                });
                if (eventEnd == std::string::npos) {
                    break;
                }
                eventStart = eventEnd + 1;
            }
            if (!validationHeld.empty()) {
                throw std::runtime_error(
                    "input script must release every held key"
                );
            }

            stellar7::Level1State state;
            std::set<std::string> held;
            std::size_t nextEvent = 0;
            for (std::uint32_t tick = 1; tick <= count; ++tick) {
                while (
                    nextEvent < events.size()
                    && events[nextEvent].tick == tick
                ) {
                    for (const auto& key : events[nextEvent].keys) {
                        if (events[nextEvent].pressed) {
                            held.insert(key);
                        } else {
                            held.erase(key);
                        }
                    }
                    ++nextEvent;
                }
                state.advanceTick({
                    held.count("left") != 0,
                    held.count("right") != 0,
                    held.count("up") != 0,
                    held.count("down") != 0,
                    held.count("spc") != 0,
                });
            }
            writeText(
                argv[4],
                stellar7::level1StateTraceJson(
                    state,
                    "level1-input-script-post-main-render"
                )
            );
            std::cout << "wrote " << argv[4] << '\n';
            return 0;
        }
        if (argc != 1) {
            auto& usage=helpRequested?std::cout:std::cerr;
            usage
                << "usage:\n"
                << "  stellar7-tools --trace-vector-transform-work MODE X Y Z M00 M10 M20 M01 M11 M21 M02 M12 M22\n"
                << "  stellar7-tools --trace-vertex-cache-work cached|uncached MODE\n"
                << "  stellar7-tools --trace-polygon-spans X0 Y0 X1 Y1 X2 Y2 [...]\n"
                << "  stellar7-tools --trace-sbl-direct-timer-work open|suppressed unlatched|latched TIMER6 TIMER7 TIMER8\n"
                << "  stellar7-tools --trace-sbl-direct-expiry DATA_DIRECTORY PALETTE.bin\n"
                << "  stellar7-tools --trace-sbl-opl-work\n"
                << "  stellar7-tools --trace-sbl-opl-phase < PHASE_CASES.txt\n"
                << "  stellar7-tools --trace-sbl-callback-phase < CALLBACK_PHASE_CASES.txt\n"
                << "  stellar7-tools --trace-sbl-keyoff-work < CALLBACK_CASES.txt\n"
                << "  stellar7-tools --trace-sbl-noteon-work < CALLBACK_CASES.txt\n"
                << "  stellar7-tools --trace-sbl-wait-work TIMER6 TIMER7 TIMER8\n"
                << "  stellar7-tools --trace-solid-polygon native|double enabled|disabled LEFT RIGHT TOP BOTTOM X0 Y0 [...]\n"
                << "  stellar7-tools --trace-polygon-input native|double same|different LEFT RIGHT TOP BOTTOM X0 Y0 [...]\n"
                << "  stellar7-tools --trace-polygon-clip enabled|disabled LEFT RIGHT TOP BOTTOM [X0 Y0 ...]\n"
                << "  stellar7-tools --trace-polygon-edge left|right X0 Y0 X1 Y1\n"
                << "  stellar7-tools --trace-edge-writer-work ROWS\n"
                << "  stellar7-tools --trace-timer-irq-work ACTIVE_MASK EXPIRED_MASK iret|bios\n"
                << "  stellar7-tools --trace-solid-span-work LEFT RIGHT [LEFT RIGHT ...]\n"
                << "  stellar7-tools\n"
                << "  stellar7-tools --trace-native-session DATA_DIRECTORY EVENTS_OR_SCRIPT FRAMES OUT_DIRECTORY [FIRST_ARTIFACT_FRAME]\n"
                << "  stellar7-tools --trace-native-complete-frames DATA_DIRECTORY NEUTRAL_EVENTS_OR_SCRIPT FRAMES OUT_DIRECTORY [FIRST_ARTIFACT_FRAME]\n"
                << "  stellar7-tools --trace-native-startup-context DATA_DIRECTORY OUT_DIRECTORY\n"
                << "  stellar7-tools --trace-native-startup-settings DATA_DIRECTORY OUT_DIRECTORY\n"
                << "  stellar7-tools --trace-native-startup-resources DATA_DIRECTORY OUT_DIRECTORY\n"
                << "  stellar7-tools --trace-native-startup-presentation DATA_DIRECTORY OUT_DIRECTORY\n"
                << "  stellar7-tools --trace-native-startup-logo-work DATA_DIRECTORY GUEST_POLLS OUT_DIRECTORY [PACKET_POLLS]\n"
                << "  stellar7-tools --trace-native-startup-logo-event DATA_DIRECTORY GUEST_POLLS OUT_DIRECTORY [PACKET_POLLS]\n"
                << "  stellar7-tools --trace-native-startup-logo-animation DATA_DIRECTORY GUEST_POLLS OUT_DIRECTORY [PACKET_POLLS]\n"
                << "  stellar7-tools --trace-native-startup-logo-complete DATA_DIRECTORY GUEST_POLLS OUT_DIRECTORY [PACKET_POLLS]\n"
                << "  stellar7-tools --trace-native-entry DATA_DIRECTORY EVENTS_OR_SCRIPT FRAMES OUT_DIRECTORY [FIRST_ARTIFACT_FRAME]\n"
                << "  stellar7-tools --trace-native-entry-pages DATA_DIRECTORY EVENTS_OR_SCRIPT FRAMES OUT_DIRECTORY [FIRST_ARTIFACT_FRAME]\n"
                << "  stellar7-tools --native-entry-skip DATA_DIRECTORY\n"
                << "  stellar7-tools --playable-campaign DATA_DIRECTORY\n"
                << "  stellar7-tools --smoke-native-entry DATA_DIRECTORY EVENTS_OR_SCRIPT FRAMES OUT_DIRECTORY\n"
                << "  stellar7-tools --verify-native-lifecycle DATA_DIRECTORY OUT.json\n"
                << "  stellar7-tools --smoke-native-menu DATA_DIRECTORY EVENTS_OR_SCRIPT FRAMES OUT_DIRECTORY < menu-input\n"
                << "  stellar7-tools --menu DATA_DIRECTORY [cockpit-palette.bin]\n"
                << "  stellar7-tools --play DATA_DIRECTORY [cockpit-palette.bin]\n"
                << "  stellar7-tools --play-controlled-level2 DATA_DIRECTORY SCRIPT FRAMES < qualified-raw4-request\n"
                << "  stellar7-tools --play-controlled-level2-input DATA_DIRECTORY SCRIPT FRAMES < qualified-keyboard-raw4-request\n"
                << "  stellar7-tools --verify-controlled-level2-input-session DATA_DIRECTORY SCRIPT RETURN_KEYS FRAMES OUTPUT < qualified-keyboard-raw4-request\n"
                << "  stellar7-tools --smoke-controlled-level2 DATA_DIRECTORY SCRIPT FRAMES OUTPUT < qualified-raw4-request\n"
                << "  stellar7-tools --play-level2 DATA_DIRECTORY "
                   "[cockpit-palette.bin]\n"
                << "  stellar7-tools --play-level3 DATA_DIRECTORY "
                   "[cockpit-palette.bin [SUCCESS_SEQUENCE_DIR]]\n"
                << "  stellar7-tools --play-level3-record DATA_DIRECTORY "
                   "cockpit-palette.bin OUT.input.script "
                   "[SUCCESS_SEQUENCE_DIR]\n"
                << "  stellar7-tools --play-level3-guardian-record DATA_DIRECTORY "
                   "cockpit-palette.bin DS.bin OUT.input.script "
                   "[SUCCESS_SEQUENCE_DIR]\n"
                << "  stellar7-tools --play-record DATA_DIRECTORY "
                   "cockpit-palette.bin OUT.input.script "
                   "[SUCCESS_SEQUENCE_DIR]\n"
                << "  stellar7-tools --render-level1-audio-event "
                   "DATA_DIRECTORY EVENT_ID OUT.wav\n"
                << "  stellar7-tools --render-pc-speaker-music "
                   "DATA_DIRECTORY OUT.wav\n"
                << "  stellar7-tools --render-sound-blaster-music "
                   "DATA_DIRECTORY OUT.wav\n"
                << "  stellar7-tools --render-sound-blaster-success "
                   "DATA_DIRECTORY OUT.wav\n"
                << "  stellar7-tools --trace-sound-blaster-success-writes "
                   "DATA_DIRECTORY OUT.bin\n"
                << "  stellar7-tools --trace-sound-blaster-success-lifetime "
                   "DATA_DIRECTORY PALETTE.bin OUT.json\n"
                << "  stellar7-tools --trace-level1-entry OUT.json\n"
                << "  stellar7-tools --trace-level1-first-idle-tick OUT.json\n"
                << "  stellar7-tools --trace-level1-idle-ticks COUNT OUT.json\n"
                << "  stellar7-tools --trace-level1-held-input "
                   "KEY PRESS_TICK RELEASE_TICK COUNT OUT.json\n"
                << "  stellar7-tools --trace-level1-input-script "
                   "EVENTS COUNT OUT.json\n"
                << "  stellar7-tools --trace-level1-input-script-file "
                   "INPUT.script COUNT OUT.json\n"
                << "  stellar7-tools --trace-level1-input-script-file-series "
                   "INPUT.script TICK[,TICK...] OUT_DIRECTORY\n"
                << "  stellar7-tools --trace-level1-input-script-phase "
                   "INPUT.script COUNT OUT.json\n"
                << "  stellar7-tools --trace-level2-input-script-state "
                   "INPUT.script COUNT OUT.json\n"
                << "  stellar7-tools --trace-level2-input-script-state-series-armored "
                   "INPUT.script ARMOR TICKS OUT_DIRECTORY\n"
                << "  stellar7-tools --trace-level2-input-script-state-series-armor-held "
                   "INPUT.script ARMOR TICKS OUT_DIRECTORY (diagnostic per-boundary override)\n"
                << "  stellar7-tools --trace-level3-native-input-script-state-series "
                   "DS.bin INPUT.script TICKS OUT_DIRECTORY\n"
                << "  stellar7-tools --render-level3-native-input-script-clean-indexed "
                   "DATA_DIRECTORY PALETTE.bin DS.bin INPUT.script TICK OUT.bin\n"
                << "  stellar7-tools --trace-level3-native-guardian-forward-fire "
                   "DS.bin COUNT OUT.json\n"
                << "  stellar7-tools --trace-level3-native-input-script-state "
                   "DS.bin INPUT.script COUNT OUT.json\n"
                << "  stellar7-tools --trace-level2-input-script-state-armored "
                   "INPUT.script ARMOR COUNT OUT.json\n"
                << "  stellar7-tools --export-level2-success-controller "
                   "INPUT.script ARMOR CONTROL_START MAXIMUM_TICK "
                   "OUT.script OUT.json\n"
                << "  stellar7-tools --export-level2-survival-controller "
                   "INPUT.script ARMOR CONTROL_START MAXIMUM_TICK "
                   "OUT.script OUT.json\n"
                << "  stellar7-tools --export-level3-success-controller "
                   "MAXIMUM_LOOP OUT.script OUT.json\n"
                << "  stellar7-tools --export-level1-success-script "
                   "OUT.schedule.json OUT.trace.json\n"
                << "  stellar7-tools --decode-first-screen STELART.RES OUT.ppm\n"
                << "  stellar7-tools --decode-screen-with-palette "
                   "COCKPIT.RES PALETTE.bin OUT.ppm\n"
                << "  stellar7-tools --render-gameplay-frame "
                   "DATA_DIRECTORY PALETTE.bin OUT.ppm\n"
                << "  stellar7-tools --render-gameplay-indexed "
                   "DATA_DIRECTORY PALETTE.bin OUT.bin\n"
                << "  stellar7-tools --render-level3-indexed "
                   "DATA_DIRECTORY PALETTE.bin TICKS OUT.bin\n"
                << "  stellar7-tools --render-level3-native-guardian-history-indexed "
                   "DATA_DIRECTORY PALETTE.bin DS.bin PREVIOUS.bin OUT.bin "
                   "[SUCCESS_SEQUENCE_DIR]\n"
                << "  stellar7-tools --render-level3-native-input-script-indexed "
                   "DATA_DIRECTORY PALETTE.bin DS.bin INPUT.script "
                   "TICK OUT.bin [SUCCESS_SEQUENCE_DIR]\n"
                << "  stellar7-tools --trace-level3-native-input-script-model-draws "
                   "DATA_DIRECTORY PALETTE.bin DS.bin INPUT.script "
                   "TICK OUT.json [SUCCESS_SEQUENCE_DIR]\n"
                << "  stellar7-tools --render-level3-native-guardian-page-history-indexed "
                   "DATA_DIRECTORY PALETTE.bin DS.bin PREVIOUS.bin OUT.bin "
                   "[SUCCESS_SEQUENCE_DIR]\n"
                << "  stellar7-tools --trace-level3-native-guardian-radar "
                   "DATA_DIRECTORY PALETTE.bin DS.bin OUT.json "
                   "[SUCCESS_SEQUENCE_DIR]\n"
                << "  stellar7-tools --render-level3-success-indexed "
                   "DATA_DIRECTORY PALETTE.bin FRAME OUT.bin "
                   "[SUCCESS_SEQUENCE_DIR]\n"
                << "  stellar7-tools --render-level3-contact-success-indexed "
                   "DATA_DIRECTORY PALETTE.bin FRAME OUT.bin "
                   "[SUCCESS_SEQUENCE_DIR]\n"
                << "  stellar7-tools --render-level1-failure-prompt-indexed "
                   "DATA_DIRECTORY PALETTE.bin OUT.bin\n"
                << "  stellar7-tools --render-level1-failure-frame-indexed "
                   "DATA_DIRECTORY PALETTE.bin FRAME OUT.bin\n"
                << "  stellar7-tools --trace-level1-failure-model-draws "
                   "DATA_DIRECTORY PALETTE.bin FRAME OUT.json\n"
                << "  stellar7-tools --render-level1-input-script-indexed "
                   "DATA_DIRECTORY PALETTE.bin INPUT.script "
                   "COUNT OUT.bin\n"
                << "  stellar7-tools --render-level2-input-script-indexed-armored "
                   "DATA_DIRECTORY PALETTE.bin INPUT.script ARMOR "
                   "COUNT OUT.bin\n"
                << "  stellar7-tools --render-level2-input-script-indexed-armor-held "
                   "DATA_DIRECTORY PALETTE.bin INPUT.script ARMOR "
                   "COUNT OUT.bin\n"
                << "  stellar7-tools --render-level2-input-script-display-armor-held "
                   "DATA_DIRECTORY PALETTE.bin INPUT.script ARMOR "
                   "COUNT OUT_PREFIX\n"
                << "  stellar7-tools --trace-gameplay-input-script-model-draws-armor-held "
                   "DATA_DIRECTORY PALETTE.bin INPUT.script ARMOR "
                   "COUNT OUT.json\n"
                << "  stellar7-tools --render-level1-input-script-rgb "
                   "DATA_DIRECTORY PALETTE.bin INPUT.script "
                   "COUNT OUT.rgb\n"
                << "  stellar7-tools --render-level1-input-script-frame "
                   "DATA_DIRECTORY PALETTE.bin INPUT.script "
                   "COUNT OUT.ppm\n"
                << "  stellar7-tools --render-level2-controlled-actor-indexed "
                   "DATA_DIRECTORY PALETTE.bin INPUT.script COUNT "
                   "WARP_LOOP ALLOCATION X Y Z HEALTH ARMOR OUT.bin\n"
                << "  stellar7-tools --render-level1-input-script-indexed-sequence "
                   "DATA_DIRECTORY PALETTE.bin INPUT.script COUNT "
                   "[SUCCESS_SECONDS] OUT.bin\n"
                << "  stellar7-tools --render-level1-input-script-rgb-sequence "
                   "DATA_DIRECTORY PALETTE.bin INPUT.script COUNT "
                   "OUT.rgb OUT.manifest.json\n"
                << "  stellar7-tools --render-level1-input-script-sound-blaster "
                   "DATA_DIRECTORY PALETTE.bin INPUT.script COUNT OUT.wav\n"
                << "  stellar7-tools --render-level1-input-script-sound-blaster-armored "
                   "DATA_DIRECTORY ARMOR PALETTE.bin INPUT.script COUNT OUT.wav\n"
                << "  stellar7-tools --render-level2-input-script-sound-blaster "
                   "DATA_DIRECTORY PALETTE.bin INPUT.script COUNT OUT.wav\n"
                << "  stellar7-tools --trace-level1-input-script-sound-blaster-writes "
                   "DATA_DIRECTORY PALETTE.bin INPUT.script COUNT OUT.json\n"
                << "  stellar7-tools --trace-level1-input-script-sound-blaster-writes-armored "
                   "DATA_DIRECTORY ARMOR PALETTE.bin INPUT.script COUNT OUT.json [PRESENTATION_CALLBACKS]\n"
                << "  stellar7-tools --trace-level1-input-script-sound-blaster-writes-armor-held "
                   "DATA_DIRECTORY ARMOR PALETTE.bin INPUT.script COUNT OUT.json [PRESENTATION_CALLBACKS]\n"
                << "  stellar7-tools --trace-level2-input-script-sound-blaster-writes "
                   "DATA_DIRECTORY PALETTE.bin INPUT.script COUNT OUT.json\n"
                << "  stellar7-tools --trace-level3-sound-blaster-writes "
                   "DATA_DIRECTORY PALETTE.bin COUNT OUT.json "
                   "[EVENT_TICK EVENT_ID]\n"
                << "  stellar7-tools --trace-level3-resident-sound-blaster-writes "
                   "DATA_DIRECTORY PALETTE.bin COUNT OUT.json "
                   "[EVENT_TICK EVENT_ID]\n"
                << "  stellar7-tools --trace-level3-success-release-writes "
                   "DATA_DIRECTORY PALETTE.bin FRAME_COUNT OUT.json\n"
                << "  stellar7-tools --trace-level2-to-level3-success-release-writes "
                   "DATA_DIRECTORY PALETTE.bin FRAME_COUNT OUT.json\n"
                << "  stellar7-tools "
                   "--trace-level1-input-script-sound-blaster-success-writes "
                   "DATA_DIRECTORY PALETTE.bin INPUT.script COUNT [SUCCESS_SECONDS] OUT.bin\n"
                << "  stellar7-tools --trace-level1-idle-sound-blaster-success-writes "
                   "DATA_DIRECTORY PALETTE.bin COUNT SUCCESS_SECONDS OUT.bin\n"
                << "  stellar7-tools --trace-level1-settled-music-success-writes "
                   "DATA_DIRECTORY PALETTE.bin MUSIC_SECONDS SUCCESS_SECONDS OUT.bin\n"
                << "  stellar7-tools --trace-level-entry-fade DATA_DIRECTORY PALETTE.bin OUT.json\n"
                << "  stellar7-tools --trace-level-entry-clock VIDEO_PHASE_NS OUT.json\n"
                << "  stellar7-tools --trace-page-flip-clock VIDEO_PHASE_NS sync|unsync OUT.json\n"
                << "  stellar7-tools --trace-music-callbacks "
                   "DATA_DIRECTORY PALETTE.bin LEVEL(0=presentation) CALLBACK_COUNT OUT.json\n"
                << "  stellar7-tools --trace-level1-input-script-model-draws "
                   "DATA_DIRECTORY PALETTE.bin INPUT.script "
                   "COUNT OUT.json\n"
                << "  stellar7-tools --trace-level1-input-script-model-draws-series "
                   "DATA_DIRECTORY PALETTE.bin INPUT.script COUNT OUT_DIRECTORY\n";
            return helpRequested?0:2;
        }
        return stellar7::runMenu(std::cin, std::cout);
    } catch (const std::exception& error) {
        std::cerr << "stellar7: " << error.what() << '\n';
        return 1;
    }
}
