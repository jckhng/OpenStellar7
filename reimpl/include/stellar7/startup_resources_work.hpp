#pragma once

#include "stellar7/driver_initialization_work.hpp"
#include "stellar7/startup_settings_work.hpp"

namespace stellar7 {

enum class StartupResourceMusicProfile { sourceSelected, playableUnpublished };

enum class StartupResourceReference { none, artPalette, startupGraphics,
    startupGraphicsMetadata, transferBuffer, scratchBuffer, soundBlasterDriver,
    instrumentBank, instrumentPayload };
enum class StartupResourcesEffectKind { paletteCache, graphicsPages, graphicsResource,
    transferBuffer, graphicsMetadata, scratchBuffer, drawCallback, driverEntry,
    bankSelector, instrumentDescriptor, driverEnabled, callbackRegistered };
enum class StartupResourcesHostServiceKind { artRegistry, paletteCache, graphicsResource,
    transferBufferSize, allocateTransferBuffer, allocateScratchBuffer, driverTag,
    loadDriver, loadInstrumentBank, releaseInstrumentBank, stopUninitializedMusic };

struct StartupResourcesEffect {
    StartupResourcesEffectKind kind;
    StartupResourceReference reference;
    std::uint16_t value;
    std::uint32_t sourcePc;
    // Number of completed operations, preserving expanded OPL writer boundaries.
    std::size_t afterOperation;
};

struct StartupResourcesHostService {
    StartupResourcesHostServiceKind kind;
    std::uint32_t sourcePc;
    std::size_t afterOperation;
};

struct StartupResourcesState {
    StartupResourceReference paletteCache = StartupResourceReference::none;
    StartupResourceReference graphics = StartupResourceReference::none;
    StartupResourceReference graphicsMetadata = StartupResourceReference::none;
    StartupResourceReference transferBuffer = StartupResourceReference::none;
    StartupResourceReference scratchBuffer = StartupResourceReference::none;
    StartupResourceReference driver = StartupResourceReference::none;
    std::size_t instrumentPayloadOffset = 0;
    std::uint16_t transferWidth = 0;
    std::uint16_t transferHeight = 0;
    std::uint16_t scratchBytes = 0;
    std::uint16_t drawCallbackOffset = 0;
    std::uint16_t driverSelector = 0;
    std::uint16_t bankSelector = 0;
    bool graphicsPagesSelected = false;
    bool driverEnabled = false;
    bool callbackRegistered = false;
    bool openedStartupResource = false;
};

struct StartupResourcesWork {
    StartupResourcesState state;
    std::vector<NormalWorkOperation> operations;
    std::vector<StartupResourcesEffect> effects;
    std::vector<StartupResourcesHostService> hostServices;
    SoundBlasterInitializationWork driverInitialization;
    // Child metadata is relative to this first operation in the combined program.
    std::size_t driverFirstOperation = 0;
};

// Source4059..6114, fresh native graphics/cache and selector4 success profile.
// Resource presence is admitted by the caller's decoded providers, not guest pointers.
StartupResourcesWork startupResourcesOperations(const StartupSettingsWork& settings,
    const std::vector<std::uint8_t>& instrumentResource,
    StartupResourceMusicProfile music = StartupResourceMusicProfile::sourceSelected);

}  // namespace stellar7
