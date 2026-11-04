#include "stellar7/bomb7_audio_work.hpp"
#include "stellar7/native_session.hpp"
#include "playable_detail.hpp"

#include <algorithm>
#include <stdexcept>

namespace stellar7 {
std::optional<SoundBlasterBomb7CallerRow> SoundBlasterBomb7CallerRow::fromMenuSong(
    const std::vector<std::uint8_t>& payload) {
    constexpr std::size_t menuPayloadBytes = 16155;
    if (payload.size() != menuPayloadBytes || payload[0] != 0x84 || payload[1] != 0 || payload[2] != 0) {
        return std::nullopt;
    }
    std::array<std::uint8_t, bomb7CallerRowBytes> values{};
    std::copy_n(payload.begin() + bomb7MenuPayloadOffset, values.size(), values.begin());
    return SoundBlasterBomb7CallerRow(values);
}

bool NativeAudioOwner::playableStoryProfile() const {
    if (!playablePreferences_ || !hasNativeMenuFeature(menuFeatures_,NativeMenuFeatures::storyPanels)
        || !settings_ || !menuResources_) {return false;}
    const auto legal=[](const std::int16_t value) {return value==0 || value==255;};
    return legal(settings_->settings.configuration.options[5]) && legal(menuResources_->configuration.options[5]);
}
bool NativeAudioOwner::playableStoryEnabled() const {
    if (!playableStoryProfile()) {throw std::invalid_argument("Story requires its admitted current canonical settings");}
    return menuResources_->configuration.options[5]==255;
}

bool NativeAudioOwner::playableSoundsEnabled() const {
    // Component sessions have no editable menu and retain their fixed enabled profile.
    if (!menuResources_) { return true; }
    if (menuResources_->configuration.options[3] != 0
        && menuResources_->configuration.options[3] != 1) {
        throw std::invalid_argument("playable sound effects require their current boolean setting");
    }
    return menuResources_->configuration.options[3] != 0;
}

bool NativeAudioOwner::playableMusicEnabled() const {
    constexpr std::size_t musicOption = 2;
    if (!menuResources_ && !settings_) { return true; }
    const auto& configuration = menuResources_ ? menuResources_->configuration : settings_->settings.configuration;
    const auto value = configuration.options[musicOption];
    if (value != 0 && value != 1) {
        throw std::invalid_argument("playable music requires its current boolean setting");
    }
    return value != 0;
}

PlayableMusicPublication NativeAudioOwner::playableMusicPublication() const {
    return playableMusicEnabled() ? PlayableMusicPublication::current : PlayableMusicPublication::suspended;
}

void NativeAudioOwner::synchronizePlayableMusic(SoundBlasterClockPhase& phase) {
    const bool disabled = !playableMusicEnabled();
    if (phase.playableMusicOff == disabled) { return; }
    phase.callback = synth_.setPlayableMusicPublication(playableMusicPublication());
    phase.playableMusicOff = disabled;
    if (!phase.playableCampaign) { phase.playableStartup = true; }
}

bool NativeAudioOwner::playablePointerEnabled() const {
    if (pointerCapability_ != NativePointerCapability::portablePointer) { return false; }
    if (!menuResources_ && !settings_) { return false; }
    const auto& configuration = menuResources_ ? menuResources_->configuration : settings_->settings.configuration;
    const auto value = configuration.options[0];
    if (value != 0 && value != 1) { throw std::invalid_argument("native mouse requires a boolean canonical setting"); }
    return value != 0;
}

bool NativeAudioOwner::playablePointerProfile() const {
    if (!playablePreferences_ || pointerCapability_ != NativePointerCapability::portablePointer
        || !settings_ || !menuResources_ || settings_->settings.capability != StartupCapabilityProfile::portablePointer
        || menuResources_->pointerCapability != StartupCapabilityProfile::portablePointer) { return false; }
    const auto initial = settings_->settings.configuration.options[0];
    const auto current = menuResources_->configuration.options[0];
    return (initial == 0 || initial == 1) && (current == 0 || current == 1);
}

CampaignDifficulty NativeAudioOwner::playableDifficulty() const {
    constexpr std::size_t difficultyOption = 4;
    if (!menuResources_) { return CampaignDifficulty::easy; }
    const auto value = menuResources_->configuration.options[difficultyOption];
    if (value < 0 || value > static_cast<std::int16_t>(CampaignDifficulty::hard)
        || (!playablePreferences_ && value != 0)) {
        throw std::invalid_argument("difficulty requires its current admitted playable setting");
    }
    return static_cast<CampaignDifficulty>(value);
}

bool NativeAudioOwner::playableDifficultyProfile() const {
    constexpr std::size_t difficultyOption = 4;
    if (!playablePreferences_ || !settings_ || !menuResources_) { return false; }
    const auto original = settings_->settings.configuration.options[difficultyOption];
    const auto current = menuResources_->configuration.options[difficultyOption];
    constexpr auto maximum = static_cast<std::int16_t>(CampaignDifficulty::hard);
    return original >= 0 && original <= maximum && current >= 0 && current <= maximum;
}

bool NativeAudioOwner::playableDetailProfile() const noexcept {
    return playablePreferences_ && settings_ && menuResources_ && menuResources_->streamsClosed
        && (hasNativeMenuFeature(menuFeatures_,NativeMenuFeatures::fullGraphicDetail)
            ? nativeFullDetailSettingSupported(settings_->settings.configuration.viewSize)
                && nativeFullDetailSettingSupported(menuResources_->configuration.viewSize)
            : nativeDetailSettingSupported(settings_->settings.configuration.viewSize)
                && nativeDetailSettingSupported(menuResources_->configuration.viewSize));
}

std::int16_t NativeAudioOwner::playableDetail() const {
    if (!playableDetailProfile()) {
        throw std::invalid_argument("detail requires its current admitted native setting");
    }
    return menuResources_->configuration.viewSize;
}

bool NativeAudioOwner::playableMusicProfile() const {
    constexpr std::array<std::int16_t,6> expected{0,0,1,1,0,255};
    if (!settings_ || !resources_ || !presentation_ || !logo_ || !logoEnd_ || !logoRelease_
        || !menuSong_ || !menuResources_ || !menuResources_->streamsClosed
        || !logo_->finalPhase.sequenceSelection || !logoRelease_->finalPhase.sequenceSelection
        || !menuSong_->finalPhase.sequenceSelection || menuSong_->song.next || menuSong_->song.repeat != 0xffff
        || menuSong_->song.payload != menuSong_->finalPhase.sequenceSelection->prepared.sequence) {
        return false;
    }
    const auto& original = settings_->settings.configuration;
    const auto& current = menuResources_->configuration;
    if ((original.options[2] != 0 && original.options[2] != 1)
        || (current.options[2] != 0 && current.options[2] != 1)
        || (original.options[3] != 0 && original.options[3] != 1)
        || (current.options[3] != 0 && current.options[3] != 1)) {
        return false;
    }
    const bool cold = startupMusicProfile_ == StartupMusicProfile::playableUnpublished;
    if (original.options[2] != (cold ? 0 : 1)
        || presentation_->audioProfile != startupMusicProfile_ || menuSong_->audioProfile != startupMusicProfile_) {
        return false;
    }
    const auto& released = *logoRelease_->finalPhase.sequenceSelection;
    if (cold ? (presentation_->selectedHead || !released.logoTimerFallback || !released.logoEmptyCurrentReturned)
        : (!presentation_->selectedHead || !released.logoCurrentCleared)) {
        return false;
    }
    auto loadedOptions = original.options;
    auto currentOptions = current.options;
    loadedOptions[2] = 1; currentOptions[2] = 1;
    loadedOptions[3] = 1; currentOptions[3] = 1;
    if (playableDifficultyProfile()) { loadedOptions[4] = 0; currentOptions[4] = 0; }
    if (playableStoryProfile()) { loadedOptions[5] = 255; currentOptions[5] = 255; }
    if (playablePointerProfile()) { loadedOptions[0] = 0; currentOptions[0] = 0; }
    if (playableJoystickProfile()) { loadedOptions[1]=0;currentOptions[1]=0; }
    const auto& logo = *logo_->finalPhase.sequenceSelection;
    return loadedOptions == expected && currentOptions == expected
        && ((original.viewSize >= 81 && original.viewSize <= 82 && current.viewSize >= 81 && current.viewSize <= 82)
            || playableDetailProfile())
        && logo.logoEventPlayed && logo.logoFramesCompleted == 17
        && logo.logoEventSuppressed == (original.options[3] == 0);
}

bool NativeAudioOwner::playableSoundsProfile() const {
    constexpr std::size_t soundsOption = 3;
    constexpr std::array<std::int16_t, 6> expected{0, 0, 1, 1, 0, 255};
    if (!settings_ || !logo_ || !menuResources_ || !logo_->finalPhase.sequenceSelection) {
        return false;
    }
    const auto& original = settings_->settings.configuration;
    const auto& current = menuResources_->configuration;
    if ((original.options[soundsOption] != 0 && original.options[soundsOption] != 1)
        || (current.options[soundsOption] != 0 && current.options[soundsOption] != 1)) {
        return false;
    }
    auto originalOptions = original.options;
    auto currentOptions = current.options;
    originalOptions[soundsOption] = 1;
    currentOptions[soundsOption] = 1;
    if (playableDifficultyProfile()) { originalOptions[4] = 0; currentOptions[4] = 0; }
    if (playableStoryProfile()) { originalOptions[5] = 255; currentOptions[5] = 255; }
    if (playablePointerProfile()) { originalOptions[0] = 0; currentOptions[0] = 0; }
    if (playableJoystickProfile()) { originalOptions[1]=0;currentOptions[1]=0; }
    if (playableMusicProfile()) { originalOptions[2] = 1; currentOptions[2] = 1; }
    const auto& logo = *logo_->finalPhase.sequenceSelection;
    //B3E6(event26) is either a direct driver call or its allocation-free early
    //return. It never changes the later native0086 song/menu-cache generation.
    return originalOptions == expected && currentOptions == expected
        && ((original.viewSize >= 81 && original.viewSize <= 82
            && current.viewSize >= 81 && current.viewSize <= 82) || playableDetailProfile())
        && logo.logoEventPlayed && logo.logoFramesCompleted == 17
        && logo.logoEventSuppressed == (original.options[soundsOption] == 0);
}

std::optional<SoundBlasterBomb7CallerRow> NativeAudioOwner::bomb7CallerRow() const {
    constexpr std::array<std::int16_t, 6> effectiveOptions{0, 0, 1, 1, 0, 255};
    constexpr std::int16_t sourceView = 81;
    constexpr std::int16_t fullView = 82;
    if (!adopted_ || !context_ || !settings_ || !resources_ || !fonts_ || !menuSong_ || !begin_
        || !menuResources_ || !menuResources_->streamsClosed || !workPhase_ || !workPhase_->callback
        || !workPhase_->callback->program || menuSong_->song.next || menuSong_->song.repeat != 0xffff
        || (menuResources_->configuration.options != effectiveOptions && !playableSoundsProfile() && !playableMusicProfile())
        || (menuResources_->configuration.viewSize != sourceView && menuResources_->configuration.viewSize != fullView
            && !playableDetailProfile())) {
        return std::nullopt;
    }
    //Only the retained independent0086 menu allocation can mint this generation.
    return SoundBlasterBomb7CallerRow::fromMenuSong(menuSong_->song.payload);
}

void NativeGameplaySession::validatePlayableBombEvent() const {
    if (!audio_->workPhase_ || !audio_->workPhase_->callback || !audio_->workPhase_->callback->program) {
        throw std::invalid_argument("Bomb requires its retained work-clock program");
    }
    const auto& row = audio_->workPhase_->callback->program->bomb7CallerRow;
    if (row) {
        synth_.validatePlayableBombEvent(*row);
        return;
    }
    //Pure pre-adoption preview: do not publish a row/flag or consume an input.
    const auto owned = audio_->bomb7CallerRow();
    if (!owned) {
        throw std::invalid_argument("Bomb requires its retained menu-song audio owner");
    }
    synth_.validatePlayableBombEvent(*owned);
}
} // namespace stellar7
