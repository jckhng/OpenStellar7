#include "stellar7/native_session.hpp"
#include "playable_startup_provider.hpp"

#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::uint16_t recoveredFront = 0xa000;
constexpr std::uint16_t recoveredCache = 0xa400;
constexpr std::uint16_t recoveredBack = 0xa800;

void requireKnownPages(const IndexedPages& pages) {
    for (const auto page : {recoveredFront,recoveredBack,recoveredCache}) {
        for (std::int16_t y = 0; y < 200; ++y) {
            for (std::int16_t x = 0; x < 320; ++x) {
                if (!pages.known(page,{x,y})) {
                    throw std::invalid_argument("startup adoption requires retained known page contents");
                }
            }
        }
    }
}
}

NativeStartupGameplayHandoff::NativeStartupGameplayHandoff(const GameplayAssets& assets,
    std::unique_ptr<NativeAudioOwner> audio, IndexedPages pages, CockpitPageState roles,
    PaletteWorkState palette)
    : assets_(&assets), audio_(std::move(audio)), pages_(std::move(pages)),
      roles_(roles), palette_(std::move(palette)) {}

const SoundBlasterSynth& NativeStartupGameplayHandoff::synth() const {
    if (!audio_) { throw std::logic_error("startup handoff was already consumed"); }
    return audio_->synth_;
}

const SoundBlasterClockPhase& NativeStartupGameplayHandoff::clockPhase() const {
    if (!audio_ || !audio_->workPhase_) { throw std::logic_error("startup handoff has no recovered clock"); }
    return *audio_->workPhase_;
}

std::uint64_t NativeStartupGameplayHandoff::elapsedSamples() const {
    if (!audio_) { throw std::logic_error("startup handoff was already consumed"); }
    return audio_->elapsedSamples();
}

NativeStartupGameplayHandoff NativeStartupSequence::releaseGameplay() {
    if (phase_ != NativeStartupPhase::awaitingGameplayLoader || !audio_ || !audio_->workPhase_
        || !audio_->begin_ || !audio_->workPhase_->sequenceSelection
        || (!audio_->workPhase_->sequenceSelection->beginCurrentCleared
            && !playableStartupBeginReturned(*audio_->workPhase_)) || audio_->adopted_) {
        throw std::logic_error("gameplay handoff requires completed retained Begin0180");
    }
    requireKnownPages(pages_);
    auto roles = storyRoles_.value_or(CockpitPageState::fromRecoveredMenu());
    roles.selectFront(); //9D4C retained DRAW=FRONT; COPY remains the menu BACK.
    if(storyRoles_) {roles.selectBackCopy();} //The actual menu/Begin pointer provider last selected BACK for copies.
    auto palette = audio_->begin_->palette;
    NativeStartupGameplayHandoff result(game_,std::move(audio_),std::move(pages_),roles,std::move(palette));
    phase_ = NativeStartupPhase::adopted;
    return result;
}

std::unique_ptr<NativeAudioOwner> NativeGameplaySession::adoptAudio(const GameplayAssets& assets,
    NativeStartupGameplayHandoff& handoff) {
    const bool storyRoles=handoff.audio_ && (hasNativeMenuFeature(handoff.audio_->menuFeatures_,NativeMenuFeatures::storyPanels)
        || handoff.audio_->attractPagesTransferred_)
        && handoff.roles_.frontPage()==recoveredBack && handoff.roles_.backPage()==recoveredFront;
    if (handoff.assets_ != &assets || !handoff.audio_ || handoff.audio_->adopted_
        || !handoff.audio_->begin_ || !handoff.audio_->workPhase_
        || !handoff.audio_->workPhase_->sequenceSelection
        || (!handoff.audio_->workPhase_->sequenceSelection->beginCurrentCleared
            && !playableStartupBeginReturned(*handoff.audio_->workPhase_))
        || !handoff.audio_->workPhase_->timer || handoff.audio_->workPhase_->timer->globalCount != 0
        || !handoff.audio_->workPhase_->callback
        || (handoff.audio_->workPhase_->callback->domain != SoundBlasterCallbackDomain::noHandle
            && !playableStartupBeginReturned(*handoff.audio_->workPhase_))
        || (!storyRoles && (handoff.roles_.frontPage() != recoveredFront || handoff.roles_.backPage() != recoveredBack))
        || handoff.roles_.cachePage() != recoveredCache || handoff.roles_.drawPage() != handoff.roles_.frontPage()
        || handoff.roles_.copyPage() != handoff.roles_.backPage()) {
        throw std::invalid_argument("startup gameplay adoption requires its original owners, resources and roles");
    }
    const auto driver = handoff.audio_->synth_.driverState();
    if (driver.sequenceSelected || !driver.sequenceAllocated) {
        throw std::invalid_argument("startup gameplay adoption lost retained song allocation/current clear");
    }
    requireKnownPages(handoff.pages_);
    return std::move(handoff.audio_);
}

NativeGameplaySession::NativeGameplaySession(const GameplayAssets& assets,
    NativeStartupGameplayHandoff&& handoff, std::shared_ptr<HighscoreStore> highscoreStore)
    : assets_(assets), model_(handoff.audio_ && hasNativeMenuFeature(handoff.audio_->menuFeatures_,NativeMenuFeatures::fullGraphicDetail)
          ? GameplayModel::fromNativeStartup(handoff.audio_->playableDetail())
          : GameplayModel::fromNativeStartup()), audio_(adoptAudio(assets,handoff)),
      recoveredStartupPalette_(std::move(handoff.palette_)), synth_(audio_->synth_), clock_(audio_->clock_),
      pages_(handoff.roles_), pixels_(std::move(handoff.pages_)), highscoreTable_(audio_->currentScores_),
      highscoreStore_(std::move(highscoreStore)) {
    audio_->adopted_ = true;
    if (audio_->playablePreferences_) { model_.initializePlayableDifficulty(audio_->playableDifficulty()); }
    level1Pages_ = true;
    frame_.pixels = *retainedPagePixels(NativePageRole::front);
    const auto& colors = recoveredStartupPalette_->dac.colors;
    frame_.palette = expandVgaDacPalette(std::vector<std::uint8_t>(colors.begin(),colors.end()));
}

const SoundBlasterClockPhase* NativeGameplaySession::recoveredClockPhase() const noexcept {
    return audio_->workPhase_ ? &*audio_->workPhase_ : nullptr;
}

} // namespace stellar7
