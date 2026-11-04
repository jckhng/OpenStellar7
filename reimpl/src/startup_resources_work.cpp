#include "stellar7/startup_resources_work.hpp"

#include "stellar7/timer_registration_work.hpp"

#include <stdexcept>

namespace stellar7 {
namespace {

constexpr std::uint32_t gameBase = 0x08240;
constexpr std::uint32_t wrapperBase = 0x1ebc0;
constexpr std::uint16_t driverSelector = 4;
constexpr std::uint16_t bankSelector = 3;
constexpr std::uint16_t transferDimension = 32;
constexpr std::uint16_t scratchBytes = 500;
constexpr std::uint16_t drawCallbackOffset = 0x5ac8;
constexpr std::uint16_t callbackPeriod = 4;

}  // namespace

StartupResourcesWork startupResourcesOperations(const StartupSettingsWork& settings,
    const std::vector<std::uint8_t>& instrumentResource, const StartupResourceMusicProfile music) {
    if ((music != StartupResourceMusicProfile::sourceSelected
            && music != StartupResourceMusicProfile::playableUnpublished)
        || (music == StartupResourceMusicProfile::playableUnpublished
            ? settings.configuration.options[2] != 0 : settings.configuration.options[2] == 0)) {
        throw std::invalid_argument("unsupported startup sound-stop branch");
    }
    // Validate the complete bank/device profile before returning any source work.
    StartupResourcesWork result{};
    result.driverInitialization = soundBlasterInitializationOperations(instrumentResource);
    result.state.driverSelector = driverSelector;
    const auto plain = [&](const std::uint64_t count) {
        result.operations.push_back({NormalWorkKind::instructions, count});
    };
    const auto host = [&](const StartupResourcesHostServiceKind kind, const std::uint32_t pc) {
        result.hostServices.push_back({kind, pc, result.operations.size()});
    };
    const auto effect = [&](const StartupResourcesEffectKind kind,
        const StartupResourceReference reference, const std::uint16_t value,
        const std::uint32_t pc) {
        result.effects.push_back({kind, reference, value, pc, result.operations.size()});
    };

    plain(5); //4059,405E,406C,4071,40A2: normal-memory/option2 route.
    if (music == StartupResourceMusicProfile::playableUnpublished) {
        plain(3); //4060 PUSH485A,4064 CALL033E,4069 cleanup.
        //033E clears B96/B94 before the driver is loaded; native provider has no assigned elapsed cost.
        host(StartupResourcesHostServiceKind::stopUninitializedMusic, wrapperBase + 0x033e);
    }
    plain(11); //6070..6083, including CALL062A.
    host(StartupResourcesHostServiceKind::artRegistry, gameBase + 0x6083);
    plain(6); //6088/608B/608E,60A4/60A7/60AC; PAL lookup succeeds, absent old cache.
    result.state.openedStartupResource = false; //60A4 retains DI0 for the6070 return branch.
    plain(2); //60BA/60BE.
    host(StartupResourcesHostServiceKind::paletteCache, gameBase + 0x60be);
    plain(2); //60C3/60C6.
    result.state.paletteCache = StartupResourceReference::artPalette;
    effect(StartupResourcesEffectKind::paletteCache, result.state.paletteCache, 0, gameBase + 0x60c6);
    plain(3); //60C9/60CC/60CF: existing native graphics roles.
    result.state.graphicsPagesSelected = true;
    effect(StartupResourcesEffectKind::graphicsPages, StartupResourceReference::none, 0, gameBase + 0x60cf);
    plain(4); //60D2/60D7/60D9/60DD; fresh graphics resource.
    host(StartupResourcesHostServiceKind::graphicsResource, gameBase + 0x60dd);
    plain(2); //60E2/60E5.
    result.state.graphics = StartupResourceReference::startupGraphics;
    effect(StartupResourcesEffectKind::graphicsResource, result.state.graphics, 0, gameBase + 0x60e5);
    plain(1); //60E8 CALL5B5C.
    plain(6); //5B5C..5B64 CALL22A9, transfer size query.
    host(StartupResourcesHostServiceKind::transferBufferSize, gameBase + 0x5b64);
    plain(5); //5B69..5B70 CALL22C1.
    host(StartupResourcesHostServiceKind::allocateTransferBuffer, gameBase + 0x5b70);
    plain(3); //5B75/5B78/5B7B.
    result.state.transferBuffer = StartupResourceReference::transferBuffer;
    result.state.transferWidth = transferDimension;
    result.state.transferHeight = transferDimension;
    effect(StartupResourcesEffectKind::transferBuffer, result.state.transferBuffer, 0, gameBase + 0x5b7b);
    plain(3); //5B7F/5B83/5B85: retain metadata identity, not its raw guest pointer.
    result.state.graphicsMetadata = StartupResourceReference::startupGraphicsMetadata;
    effect(StartupResourcesEffectKind::graphicsMetadata, result.state.graphicsMetadata, 0, gameBase + 0x5b85);
    plain(3); //5B88/5B8B/5B8C CALLB6BE.
    host(StartupResourcesHostServiceKind::allocateScratchBuffer, gameBase + 0x5b8c);
    plain(2); //5B91/5B94.
    result.state.scratchBuffer = StartupResourceReference::scratchBuffer;
    result.state.scratchBytes = scratchBytes;
    effect(StartupResourcesEffectKind::scratchBuffer, result.state.scratchBuffer, scratchBytes, gameBase + 0x5b94);
    plain(1); //5B97 source draw callback identity.
    result.state.drawCallbackOffset = drawCallbackOffset;
    effect(StartupResourcesEffectKind::drawCallback, StartupResourceReference::none,
        drawCallbackOffset, gameBase + 0x5b97);
    plain(1); //5B9D RET.
    plain(5); //60EB/60EF/60F2/60F3/60F4 CALL0006.

    plain(11); //0006..001A, selector4 tag construction via B71A.
    host(StartupResourcesHostServiceKind::driverTag, wrapperBase + 0x001a);
    plain(6); //001F..002A CALL0228, whole native driver resource provider.
    host(StartupResourcesHostServiceKind::loadDriver, wrapperBase + 0x002a);
    plain(3); //002D/0030/0033.
    result.state.driver = StartupResourceReference::soundBlasterDriver;
    effect(StartupResourcesEffectKind::driverEntry, result.state.driver, driverSelector, wrapperBase + 0x0033);
    plain(6); //0037..0042 CALL0478 operation0.
    plain(11); //0478..048B, loaded-driver path.
    plain(12); //Driver0000 JMP,17C8..17DA dispatcher CALL.
    plain(3); //SBL2E60 MOVAX3,MOVCX9,RET.
    plain(9); //Driver17DC..17E4.
    plain(5); //048F..0493.
    plain(8); //0047..0056 CALL01BE (0055 PUSHCS).

    plain(6); //01BE..01C9, format bank selector3.
    result.state.bankSelector = bankSelector;
    effect(StartupResourcesEffectKind::bankSelector, StartupResourceReference::instrumentBank,
        bankSelector, wrapperBase + 0x01c9);
    plain(5); //01CC..01D4 CALL0228.
    host(StartupResourcesHostServiceKind::loadInstrumentBank, wrapperBase + 0x01d4);
    plain(10); //01D7..01F0, bank present, descriptor setup and CALL02B2.
    plain(22); //02B2..02E5 source payload-address calculation, including RET.
    plain(3); //01F3/01F6/01F9 descriptor payload reference.
    result.state.instrumentPayloadOffset = static_cast<std::size_t>(instrumentResource[1]) + 2;
    effect(StartupResourcesEffectKind::instrumentDescriptor, StartupResourceReference::instrumentPayload,
        static_cast<std::uint16_t>(result.state.instrumentPayloadOffset), wrapperBase + 0x01f9);
    plain(5); //01FC..0204 CALL0478 operation2.
    plain(11); //0478..048B.
    plain(12); //Driver0000 JMP,17C8..17DA.
    result.driverFirstOperation = result.operations.size();
    result.operations.insert(result.operations.end(), result.driverInitialization.operations.begin(),
        result.driverInitialization.operations.end());
    plain(9); //Driver17DC..17E4.
    plain(5); //048F..0493.
    plain(4); //0209/020C/020F/0212 CALL231B.
    host(StartupResourcesHostServiceKind::releaseInstrumentBank, wrapperBase + 0x0212);
    plain(5); //0217..0220 success RETF.
    plain(7); //0059,0068,006B,006E,0071,0073,0076.
    result.operations.push_back({NormalWorkKind::soundBlasterDriverEnable, 1, 1}); //0078
    result.state.driverEnabled = true;
    effect(StartupResourcesEffectKind::driverEnabled, result.state.driver, 1, wrapperBase + 0x0078);
    plain(6); //007D..0084 RETF.
    plain(3); //60F9/60FC/60FE success branch.
    plain(7); //6100..610C arguments and CALL26B1.
    const auto registration = timerRegistrationOperations();
    const auto registrationStart = result.operations.size();
    result.operations.insert(result.operations.end(), registration.begin(), registration.end());
    result.state.callbackRegistered = true;
    for (std::size_t index = 0; index < registration.size(); ++index) {
        if (registration[index].kind == NormalWorkKind::timerCallbackActivate) {
            result.effects.push_back({StartupResourcesEffectKind::callbackRegistered,
                StartupResourceReference::none, callbackPeriod, 0x17640 + 0x26ed,
                registrationStart + index + 1});
            break;
        }
    }
    plain(1); //6111 cleanup; stop before6114.
    return result;
}

}  // namespace stellar7
