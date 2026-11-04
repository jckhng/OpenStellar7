#include "stellar7/recovered_frontend.hpp"

#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::size_t maximumWorkPacketBudget = 10000;

void append(NativeSessionDelivery& delivery, const std::uint64_t firstSample,
    const std::vector<std::int16_t>& samples) {
    if (firstSample != delivery.firstSample + delivery.samples.size()) {
        throw std::logic_error("recovered frontend source audio is not contiguous");
    }
    delivery.samples.insert(delivery.samples.end(), samples.begin(), samples.end());
}

void append(NativeSessionDelivery& delivery, const ClockedPresentationWork& work) {
    append(delivery, work.audio.firstSample, work.audio.samples);
}

template<class Run>
void appendRun(NativeSessionDelivery& delivery, const Run& run) {
    append(delivery, run.firstSample, run.samples);
}
} // namespace

RecoveredFrontend::RecoveredFrontend(const GameplayAssets& assets, const NativeStartupAssets& startup,
    const FrameCopyProfile copyProfile, const std::size_t workPacketBudget,
    std::shared_ptr<HighscoreStore> highscoreStore, const CampaignExecution execution,const NativePointerCapability pointerCapability,const NativeMenuFeatures menuFeatures,const NativeJoystickSample joystick)
    : assets_(assets), highscoreStore_(std::move(highscoreStore)), copyProfile_(copyProfile), workPacketBudget_(workPacketBudget),
      execution_(execution),pointerCapability_(pointerCapability),menuFeatures_(menuFeatures),initialJoystick_(joystick) {
    if (!validNativeMenuFeatures(menuFeatures) || (execution!=CampaignExecution::playable && menuFeatures!=NativeMenuFeatures::legacy)) {
        throw std::invalid_argument("menu features require an explicit playable frontend");
    }
    if(joystick.connected&&!hasNativeMenuFeature(menuFeatures,NativeMenuFeatures::joystickControls)) {throw std::invalid_argument("joystick requires native capability");}
    if (copyProfile != FrameCopyProfile::modeXNoop || workPacketBudget == 0
        || workPacketBudget > maximumWorkPacketBudget
        || (execution != CampaignExecution::sourceWork && execution != CampaignExecution::playable)
        || (pointerCapability!=NativePointerCapability::unavailable && pointerCapability!=NativePointerCapability::portablePointer)
        || (pointerCapability!=NativePointerCapability::unavailable && execution!=CampaignExecution::playable)) {
        throw std::invalid_argument("recovered frontend requires a validated copy profile and work budget");
    }
    startup_ = std::make_unique<NativeStartupSequence>(assets, startup,LogoCompletion::sequenceEnd,highscoreStore_);
}

NativeSessionDelivery RecoveredFrontend::advanceStartup(const NativeMenuInput input, const NativeDeliverySink& sink) {
    if (session_) { throw std::logic_error("recovered frontend startup is already adopted"); }
    if(execution_==CampaignExecution::sourceWork && input.sourceKey!=0) {
        throw std::invalid_argument("source startup rejects native presentation key packets");
    }
    if(input.calibrateJoystick&&(!hasNativeMenuFeature(menuFeatures_,NativeMenuFeatures::joystickControls)
        ||execution_!=CampaignExecution::playable||startup_->phase()!=NativeStartupPhase::awaitingMenuDecision)) {
        throw std::invalid_argument("calibration request requires its playable menu capability");
    }
    if(input.sourceKey!=0&&(!hasNativeMenuFeature(menuFeatures_,NativeMenuFeatures::automaticAttract)
        ||(!startup_->attractProgress()&&!startup_->creditsActive()
            && !(startup_->titleActive() && startup_->phase()==NativeStartupPhase::awaitingMenuResources)))) {
        throw std::invalid_argument("source key requires an active native attract consumer");
    }
    if(input.pointer&&pointerCapability_!=NativePointerCapability::portablePointer) {
        throw std::invalid_argument("pointer packet requires its explicit portable host capability");
    }
    if (startup_->phase() != NativeStartupPhase::awaitingMenuDecision && !startup_->storyActive() && !startup_->titleActive() && !startup_->creditsActive()
        && (input.up || input.down || input.confirm || input.left || input.right || input.cancel || input.pointer)) {
        throw std::invalid_argument("menu input requires the recovered menu boundary");
    }
    if(input.joystick) {
        if(!hasNativeMenuFeature(menuFeatures_,NativeMenuFeatures::joystickControls)) {throw std::invalid_argument("joystick packet requires native capability");}
        if(startup_->phase()==NativeStartupPhase::idle) {initialJoystick_=*input.joystick;}
        else {startup_->setJoystickSample(*input.joystick);}
    }
    NativeSessionDelivery delivery;
    delivery.firstSample = elapsedSamples();
    switch (startup_->phase()) {
    case NativeStartupPhase::idle:
        startup_->start(execution_ == CampaignExecution::playable && assets_.cockpitSettings
            && assets_.cockpitSettings->options[2] == 0
                ? NativeStartupExecution::playableMusicOff : execution_ == CampaignExecution::playable
                    ? NativeStartupExecution::playableContext : NativeStartupExecution::recoveredContext, pointerCapability_,menuFeatures_,initialJoystick_);
        append(delivery, startup_->recoveredContext()->installer);
        append(delivery, startup_->recoveredContext()->work);
        break;
    case NativeStartupPhase::awaitingSettings:
        startup_->initializeSettings(pointerCapability_ == NativePointerCapability::portablePointer
            ? StartupCapabilityProfile::portablePointer : StartupCapabilityProfile::noMouseOrJoystick);
        append(delivery, startup_->recoveredSettings()->work);
        break;
    case NativeStartupPhase::awaitingResourceInitialization:
        startup_->initializeResources(NativeStartupDeviceProfile::portableOplNoDspProbe);
        append(delivery, startup_->recoveredResources()->work);
        break;
    case NativeStartupPhase::awaitingPresentationResources:
        startup_->initializePresentation();
        appendRun(delivery, *startup_->recoveredPresentation());
        break;
    case NativeStartupPhase::awaitingFontResources:
        startup_->initializeFonts();
        appendRun(delivery, *startup_->recoveredFonts());
        break;
    case NativeStartupPhase::logoCountdownWork:
        append(delivery, startup_->pollLogoCountdown(workPacketBudget_));
        break;
    case NativeStartupPhase::awaitingLogoFrames:
        append(delivery, startup_->playLogoEvent(execution_ == CampaignExecution::playable
            && assets_.cockpitSettings && assets_.cockpitSettings->options[3] == 0
                ? StartupLogoEventProfile::playableSoundsDisabled : StartupLogoEventProfile::sourceSound));
        break;
    case NativeStartupPhase::awaitingLogoAnimation:
        startup_->animateLogo(workPacketBudget_);
        appendRun(delivery, *startup_->recoveredLogo());
        for (const auto& frame : startup_->recoveredLogo()->frames) {
            delivery.presentations.push_back({frame.sampleEnd,{frame.pages[0],startup_->frame().palette}});
        }
        break;
    case NativeStartupPhase::awaitingLogoSoundEnd:
        startup_->finishLogoSound(workPacketBudget_,sink);
        appendRun(delivery, *startup_->recoveredLogoEnd());
        break;
    case NativeStartupPhase::awaitingLogoRelease:
        startup_->releaseLogo();
        appendRun(delivery, *startup_->recoveredLogoRelease());
        break;
    case NativeStartupPhase::awaitingIntroInput:
        if(hasNativeMenuFeature(menuFeatures_,NativeMenuFeatures::storyPanels)) {
            delivery=startup_->pollStoryIntro(input,workPacketBudget_,sink);
        } else {
            startup_->skipIntro(NativeIntroKey::escape);appendRun(delivery,*startup_->recoveredIntro());
        }
        break;
    case NativeStartupPhase::awaitingMenuMusic:
        startup_->beginMenuSong();
        appendRun(delivery, *startup_->recoveredMenuSong());
        break;
    case NativeStartupPhase::awaitingMenuResources:
        if(startup_->titleActive()) { delivery=startup_->pollIntroTitle(input,workPacketBudget_,sink); }
        else {
            startup_->initializeMenuResources();
            appendRun(delivery, *startup_->recoveredMenuResources());
        }
        break;
    case NativeStartupPhase::awaitingMenuConstruction:
        if (startup_->creditsActive()) { delivery=startup_->pollPlayableCredits(input,workPacketBudget_,sink); }
        else {
            startup_->skipCredits(NativeIntroKey::escape, workPacketBudget_);
            appendRun(delivery, *startup_->recoveredCredits());
        }
        break;
    case NativeStartupPhase::awaitingMenuEntry:
        startup_->enterMenu(NativeMenuEntryProfile::keyboardOnly, workPacketBudget_);
        appendRun(delivery, *startup_->recoveredMenuEntry());
        if(pointerCapability_==NativePointerCapability::portablePointer
            &&startup_->recoveredMenuResources()->configuration.options[0]!=0) {
            delivery.pointerPosition=startup_->recoveredMenuEntry()->pointer;
        }
        break;
    case NativeStartupPhase::awaitingMenuDecision:
        if(execution_==CampaignExecution::playable) {
            delivery=startup_->pollPlayableMenu(input,workPacketBudget_,sink);
        } else {
            startup_->pollRecoveredMenu(input, workPacketBudget_, sink);
            appendRun(delivery, *startup_->recoveredMenuInput());
        }
        break;
    case NativeStartupPhase::awaitingGameplayLoader:
        session_ = std::make_unique<NativeGameplaySession>(assets_, startup_->releaseGameplay(), highscoreStore_);
        delivery = session_->initializeRecoveredLevel1(workPacketBudget_);
        break;
    default:
        throw std::logic_error("unsupported recovered frontend startup boundary");
    }
    if (delivery.firstSample + delivery.samples.size() != elapsedSamples()) {
        throw std::logic_error("recovered frontend lost source audio");
    }
    if (!frame().pixels.empty()) { delivery.presentations.push_back({elapsedSamples(),frame()}); }
    return delivery;
}

NativeSessionDelivery RecoveredFrontend::advanceGameplay(const GameplayInput& input,
    const ContinueKeyboardPacket& continueInput,const NameEntryInput& text,const NativeDeliverySink& sink) {
    if(input.pointer&&pointerCapability_!=NativePointerCapability::portablePointer) {
        throw std::invalid_argument("pointer packet requires its explicit portable host capability");
    }
    if (!session_) { throw std::logic_error("recovered frontend requires its completed startup"); }
    if(input.analog||input.joystickAction!=GameplaySystemAction::none) {throw std::invalid_argument("interpreted joystick input belongs to its retained controller");}
    if(input.joystick&&!hasNativeMenuFeature(menuFeatures_,NativeMenuFeatures::joystickControls)) {throw std::invalid_argument("joystick packet requires native capability");}
    if (execution_ == CampaignExecution::sourceWork && (input.systemAction!=GameplaySystemAction::none||input.sourceKey!=0)) {
        throw std::invalid_argument("source gameplay rejects queued system actions");
    }
    const auto phase = session_->model().phase();
    if (execution_ == CampaignExecution::playable
        && (phase == GameplayPhase::level1 || phase == GameplayPhase::level2 || phase == GameplayPhase::level2SuccessPresentation
            || phase == GameplayPhase::level2SuccessReturn || phase == GameplayPhase::level3LoaderReturn
            || phase == GameplayPhase::level3PreloopReturn || phase == GameplayPhase::level3Native
            || phase == GameplayPhase::playableLoaderReturn || phase == GameplayPhase::playablePreloopReturn
            || phase == GameplayPhase::playable
            || (session_->playableCampaign() && (phase==GameplayPhase::failurePreExplosionBlackout
                || phase==GameplayPhase::failureExplosion || phase==GameplayPhase::failurePostExplosionBlackout
                || phase==GameplayPhase::continuePrompt || phase==GameplayPhase::highscoreNameEntry
                || phase==GameplayPhase::highscoreHall || phase==GameplayPhase::campaignEnded
                || phase==GameplayPhase::endingEntry || phase==GameplayPhase::endingPresentation
                || phase==GameplayPhase::endingTeardown || phase==GameplayPhase::endingReturn
                || phase==GameplayPhase::nativeMenu)))) {
        return session_->advancePlayableCampaign(input,workPacketBudget_,text,sink);
    }
    NativeSessionDelivery delivery;
    switch (session_->model().phase()) {
    case GameplayPhase::level1:
        delivery = session_->model().recoveredSuccessContact()
            ? session_->advanceRecoveredSuccess(workPacketBudget_)
            : session_->advanceRecoveredLevel1Frame(copyProfile_, workPacketBudget_, input);
        break;
    case GameplayPhase::successPresentation:
        if (!session_->recoveredSuccess() || !session_->recoveredSuccess()->finished) {
            delivery = session_->advanceRecoveredSuccess(workPacketBudget_);
        } else if (session_->model().level1State().missionActive()) {
            delivery = session_->resumeRecoveredLevel1Success(copyProfile_,workPacketBudget_);
        } else {
            delivery = session_->advanceRecoveredLevel2Transition(workPacketBudget_,input);
        }
        break;
    case GameplayPhase::level2PreloopReturn:
    case GameplayPhase::level2:
        delivery = session_->advanceRecoveredLevel2Frame(copyProfile_,workPacketBudget_,input);
        break;
    case GameplayPhase::failurePreExplosionBlackout:
    case GameplayPhase::failureExplosion:
        delivery = session_->advanceRecoveredFailure(copyProfile_, workPacketBudget_);
        break;
    case GameplayPhase::failurePostExplosionBlackout:
        return advanceContinue({}); //0401 entry consumes no keyboard packet.
    case GameplayPhase::continuePrompt:
        return session_->model().recoveredContinueDecision() == ContinueDecision::yes
            ? advanceRetry() : advanceContinue(continueInput);
    default:
        throw std::invalid_argument("unsupported recovered gameplay transition");
    }
    delivery.presentations.push_back({elapsedSamples(),frame()});
    return delivery;
}

const IndexedScreen& RecoveredFrontend::frame() const noexcept {
    return session_ ? session_->frame() : startup_->frame();
}

NativeSessionDelivery RecoveredFrontend::advanceContinue(const ContinueKeyboardPacket& input) {
    if (!session_) { throw std::logic_error("recovered Continue requires the retained gameplay session"); }
    return session_->advanceRecoveredContinue(input,workPacketBudget_);
}

NativeSessionDelivery RecoveredFrontend::advanceRetry() {
    if (!session_) { throw std::logic_error("recovered retry requires the retained gameplay session"); }
    return session_->model().failureFromLevel2()
        ? session_->resumeRecoveredLevel2Retry(workPacketBudget_)
        : session_->resumeRecoveredLevel1Retry(workPacketBudget_);
}

std::uint64_t RecoveredFrontend::elapsedSamples() const noexcept {
    return session_ ? session_->elapsedSamples() : startup_->elapsedSamples();
}

} // namespace stellar7
