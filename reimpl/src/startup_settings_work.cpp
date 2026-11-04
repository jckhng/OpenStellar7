#include "stellar7/startup_settings_work.hpp"

#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::uint32_t gameBase = 0x08240;
constexpr std::int16_t maximumSetting = 82;
constexpr std::int16_t minimumViewWidth = 64;
constexpr std::int16_t maximumViewWidth = 255;
constexpr std::uint16_t viewNumeratorScale = 256;
constexpr std::uint16_t tableByteMask = 0xffef;
constexpr unsigned tableOffsetShift = 4;
constexpr std::size_t viewWidthWord = 3;
constexpr std::size_t viewScaleWord = 4;
// Immutable original DS1534; BX addresses bytes, not word indices.
constexpr std::array<std::int16_t, 9> viewScaleTable{{9, 12, 17, 28, 32, 42, 52, 90, 128}};
}

static StartupSettingsWork settingsOperations(const StartupContextState& context,
    const CockpitSettings& configuration, const HighscoreTable& table,
    const StartupCapabilityProfile profile,const std::optional<StartupJoystickPresence> joystick) {
    if ((profile != StartupCapabilityProfile::noMouseOrJoystick && profile != StartupCapabilityProfile::portablePointer)
        || configuration.viewSize < 0 || configuration.viewSize > maximumSetting
        || (!joystick && configuration.options[1]!=0)
        || (joystick && *joystick!=StartupJoystickPresence::absent && *joystick!=StartupJoystickPresence::present)) {
        throw std::invalid_argument("unsupported native startup settings domain");
    }
    const auto& header = table.header();
    const auto decoded = decodeCockpitSettings(std::vector<std::uint8_t>(header.begin(), header.end()));
    if (decoded.viewSize != configuration.viewSize || decoded.options != configuration.options) {
        throw std::invalid_argument("startup settings differ from loaded table header");
    }

    StartupSettingsWork result{configuration, table, context.viewDescriptor, 0, {}, {}, {}};
    result.capability=profile;result.nativeJoystick=joystick;
    std::uint64_t dispatches = 0;
    const auto plain = [&](const std::uint64_t count) {
        result.operations.push_back({NormalWorkKind::instructions, count});
        dispatches += count;
    };
    const auto effect = [&](const StartupSettingsEffectKind kind, const std::int16_t value,
        const std::uint16_t sourcePc) {
        result.effects.push_back({kind, value, gameBase + sourcePc, dispatches});
    };
    const auto host = [&](const StartupSettingsHostServiceKind kind) {
        result.hostServices.push_back({kind, dispatches});
    };

    plain(3); //41BB,4056,8B38; whole319A storage release is native/untimed.
    host(StartupSettingsHostServiceKind::releaseStorage);
    plain(1); //8B3B; whole30D6 file/heap/header/table load is native/untimed.
    host(StartupSettingsHostServiceKind::loadSettings);
    plain(1); //3108 CALL8CC4 remains source-owned inside the replaced loader.
    plain(1); //8CC4 clears only the mouse-active byte.
    effect(StartupSettingsEffectKind::mouseActive, 0, 0x8cc4);
    plain(2); //8CC9 CMP,8CCE JZ.
    if (configuration.options[0] != 0) {
        plain(1); //8CD0 CALLF4EFE; the capability query body is native/untimed.
        if(profile==StartupCapabilityProfile::portablePointer) {
            //Declared native host capability, distinct from an original INT33 handshake receipt.
            host(StartupSettingsHostServiceKind::portablePointerQuery);
            plain(2); //8CD5 OR/8CD7 nonzero branch; no8CD9 clear.
            plain(3); //8CDE/8CE2 pushes and8CE6 CALL5B9E.
            host(StartupSettingsHostServiceKind::portablePointerPosition); //Whole native position provider, unmeasured.
            plain(1); //8CE9 cleanup.
        } else {
            host(StartupSettingsHostServiceKind::mouseAbsentQuery);
            plain(3); //8CD5 OR,8CD7 JNZ,8CD9 option store after AX=0.
            result.configuration.options[0] = 0;
            effect(StartupSettingsEffectKind::mouseOption, 0, 0x8cd9);
        }
    }
    plain(1); //8CDC or8CEC RET.
    plain(1); //310B CALL7D64.
    plain(2); //7D64 CMP,7D69 JZ.
    if(configuration.options[1]!=0) {
        plain(1);host(StartupSettingsHostServiceKind::portableJoystickQuery); //2494 provider is native/untimed.
        plain(2); //7D70 AND1,7D73 store12B2.
        result.configuration.options[1]=*joystick==StartupJoystickPresence::present ? 1 : 0;
        effect(StartupSettingsEffectKind::joystickOption,result.configuration.options[1],0x7d73);
        effect(StartupSettingsEffectKind::joystickActive,result.configuration.options[1],0x7d6b);
    } else {
        plain(1);effect(StartupSettingsEffectKind::joystickActive,0,0x7d78);
    }
    plain(1); //7D76/7D7D RET.

    plain(10); //8B3E range checks and8B70..8B7C quotient store.
    result.viewAccumulator = static_cast<std::int16_t>(
        configuration.viewSize * viewNumeratorScale / maximumSetting);
    effect(StartupSettingsEffectKind::viewAccumulator, result.viewAccumulator, 0x8b7c);
    plain(2); //8B7F PUSH,8B80 CALL8AFC.
    plain(6); //8AFC..8B07 prologue and upper clamp branch.
    auto width = result.viewAccumulator;
    if (width > maximumViewWidth) {
        plain(1); //8B09 MOVSI,255.
        width = maximumViewWidth;
    }
    plain(7); //8B0C..8B1D; table PUSH precedes the lower width clamp.
    const auto byteOffset = (static_cast<std::uint16_t>(width + 1) & tableByteMask) >> tableOffsetShift;
    const auto scale = viewScaleTable[byteOffset / sizeof(std::int16_t)];
    if (width < minimumViewWidth) {
        plain(1); //8B1F MOVSI,64.
        width = minimumViewWidth;
    }
    plain(4); //8B22..8B26 arguments and CALLFC1FC.
    plain(6); //C1FC..C204 descriptor0 argument and CALLBE46.
    plain(19); //BE46 valid static descriptor0/count1; skips the error call.
    plain(4); //C207..C20F writes only descriptor+6.
    result.viewDescriptor.words[viewWidthWord] = width;
    effect(StartupSettingsEffectKind::viewWidth, width, 0xc20f);
    plain(2); //C212,C215 writes only descriptor+8.
    result.viewDescriptor.words[viewScaleWord] = scale;
    effect(StartupSettingsEffectKind::viewScale, scale, 0xc215);
    plain(4); //C218..C21C far return.
    plain(5); //8B2B..8B32 cleanup and near return.
    plain(2); //8B83,8B86; next sourcePC4059 is outside this transaction.
    return result;
}

StartupSettingsWork startupSettingsOperations(const StartupContextState& context,
    const CockpitSettings& configuration,const HighscoreTable& table,const StartupCapabilityProfile profile) {
    return settingsOperations(context,configuration,table,profile,std::nullopt);
}

StartupSettingsWork playableJoystickSettingsOperations(const StartupContextState& context,
    const CockpitSettings& configuration,const HighscoreTable& table,const StartupCapabilityProfile profile,
    const StartupJoystickPresence joystick) {
    return settingsOperations(context,configuration,table,profile,joystick);
}

}  // namespace stellar7

