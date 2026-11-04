#include "stellar7/native_session.hpp"
#include "level1_reward_model_bank.hpp"
#include "playable_pointer.hpp"
#include "stellar7/ui_text_work.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::int64_t workHorizon=120000;
constexpr std::int64_t flipHorizon=2*workHorizon;
constexpr std::size_t maximumPacketBudget=10000;
constexpr std::array<NativeSessionStart,7> starts{NativeSessionStart::level1,NativeSessionStart::level2,
    NativeSessionStart::level3,NativeSessionStart::level4,NativeSessionStart::level5,
    NativeSessionStart::level6,NativeSessionStart::level7};
}

NativeSessionDelivery NativeGameplaySession::advancePlayableLifecycle(const GameplayInput& input,
    const NameEntryInput& text,const std::size_t workPacketBudget) {
    const bool endingScore=model_.playableHighscoreCaller()==PlayableHighscoreCaller::endingTitle
        && (model_.phase()==GameplayPhase::highscoreNameEntry || model_.phase()==GameplayPhase::highscoreHall);
    if (!playableCampaign_ || !audio_->workPhase_ || !recoveredStartupPalette_ || !pixels_ || !radar_
        || (!campaignResources_ && !endingScore) || !assets_.cockpitSettings || workPacketBudget==0
        || workPacketBudget>maximumPacketBudget) {
        throw std::invalid_argument("playable lifecycle requires its retained campaign/device owners");
    }
    NativeSessionDelivery delivery;
    delivery.firstSample=clock_.elapsedSamples();
    auto phase=*audio_->workPhase_;
    auto palette=*recoveredStartupPalette_;
    const auto append=[&](ClockedPresentationWork work) {
        if (work.audio.firstSample!=delivery.firstSample+delivery.samples.size()) {
            throw std::logic_error("playable lifecycle lost its retained PCM epoch");
        }
        phase=work.timing.phase;
        delivery.samples.insert(delivery.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
    };
    const auto publish=[&]() {
        frame_.pixels=*retainedPagePixels(NativePageRole::front);
        frame_.palette=expandVgaDacPalette(std::vector<std::uint8_t>(palette.dac.colors.begin(),palette.dac.colors.end()));
        delivery.presentations.push_back({delivery.firstSample+delivery.samples.size(),frame_});
    };
    const auto upload=[&](const std::uint8_t intensity) {
        auto work=clock_.renderPalette(phase,palette,{0,vgaPaletteColors,0,intensity},flipHorizon);
        palette=work.state;append(std::move(work.work));publish();
    };
    const auto blackout=[&]() {
        const auto incoming=retainedPagePixels(NativePageRole::front);
        auto work=clock_.renderBlackout(phase,{palette,{}},pages_,*pixels_,flipHorizon,
            BlackoutPageLayout::recoveredStartup);
        for (auto& step:work.steps) {
            append(std::move(step.before));palette=step.palette.state;
            append(std::move(step.palette.work));
            frame_.pixels=*incoming;
            frame_.palette=expandVgaDacPalette(std::vector<std::uint8_t>(palette.dac.colors.begin(),palette.dac.colors.end()));
            delivery.presentations.push_back({delivery.firstSample+delivery.samples.size(),frame_});
        }
        append(std::move(work.clear));palette=work.state.palette;publish();
    };
    const auto flip=[&]() {
        auto work=clock_.renderPageFlip(phase,{PageFlipSynchronization::enabled,pages_.frontPage(),pages_.backPage(),
            0x3d4,PageFlipLayout::recoveredStartup,PageFlipWaitDomain::retainedProgram},flipHorizon);
        append(std::move(work.work));publish();
    };
    const auto commit=[&]() {
        audio_->workPhase_=phase;*recoveredStartupPalette_=palette;
        if(audio_->playablePointerEnabled()) {
            const auto current=model_.phase();
            if(current==GameplayPhase::continuePrompt) {
                delivery.pointerPosition=modalPointer_.value_or(ProjectedPoint{161,
                    static_cast<std::int16_t>(model_.continueChoice()==ContinueChoice::yes?74:118)});
            } else if(current==GameplayPhase::highscoreNameEntry) {
                delivery.pointerPosition=modalPointer_.value_or(ProjectedPoint{161,139});
            } else if(current==GameplayPhase::highscoreHall) {
                delivery.pointerPosition=modalPointer_.value_or(ProjectedPoint{164,173});
            }
        }
    };
    const auto retry=[&]() {
        if(schedulePlayableStory(model_.campaignLevel(),true)) {return;}
        commit();
        auto child=reloadPlayableRetry(workPacketBudget);
        if(child.firstSample!=delivery.firstSample+delivery.samples.size()) {throw std::logic_error("Story retry lost PCM prefix");}
        delivery.samples.insert(delivery.samples.end(),child.samples.begin(),child.samples.end());
        delivery.presentations.insert(delivery.presentations.end(),child.presentations.begin(),child.presentations.end());
        phase=*audio_->workPhase_;palette=*recoveredStartupPalette_;
    };
    const auto finishScore=[&]() {
        const auto caller=model_.playableHighscoreCaller();
        if (!caller) {throw std::logic_error("playable score return lost its parent");}
        model_.finishPlayableHighscore();
        if (*caller==PlayableHighscoreCaller::retry) {retry();}
    };
    const auto previous=model_.phase();
    const bool pointer=audio_->pointerCapability_==NativePointerCapability::portablePointer;
    const bool pointerCancel=pointer&&input.pointer&&input.pointer->phase==NativePointerPhase::cancel;
    if(pointer&&!pointerCancel&&audio_->playablePointerEnabled()&&input.pointer
        && (previous==GameplayPhase::continuePrompt || previous==GameplayPhase::highscoreNameEntry
            || previous==GameplayPhase::highscoreHall)) {
        modalPointer_=sourcePointerPosition(input.pointer->position);
    }
    const bool pointerDown=!pointerCancel&&(input.confirm||input.fire||(audio_->playablePointerEnabled()&&input.pointer
        &&(input.pointer->primary||input.pointer->secondary)));
    if (previous==GameplayPhase::failurePreExplosionBlackout || previous==GameplayPhase::failureExplosion) {
        if (!text.characters.empty()) {
            throw std::invalid_argument("destruction does not consume name input");
        }
        const auto entering=previous==GameplayPhase::failurePreExplosionBlackout;
        if (entering) {blackout();failureCursor_=0;}
        model_.advancePlayableFailure();
        const auto& pending=model_.failureAudioEvents();
        if (failureCursor_>pending.size()) {throw std::logic_error("playable destruction lost its event cursor");}
        while (failureCursor_<pending.size()) {
            const auto event=pending[failureCursor_++];
            if (audio_->playableSoundsEnabled()) { phase.callback=synth_.triggerPlayableEvent(event.eventId); }
            delivery.events.push_back(event);
        }
        failurePages_=true;
        if (model_.phase()==GameplayPhase::failurePostExplosionBlackout) {
            blackout();
        } else {
            renderFailurePages();flip();
            if (entering) {
                for (std::uint8_t intensity=0;intensity<64;intensity+=4) {upload(intensity);}
            }
        }
        commit();return delivery;
    }
    if (previous==GameplayPhase::failurePostExplosionBlackout) {
        model_.advancePlayableFailure();
        if (model_.phase()==GameplayPhase::continuePrompt) {
            continuePages_=false;renderContinuePages();upload(vgaComponentMaximum);
        }
        commit();return delivery;
    }
    if (previous==GameplayPhase::continuePrompt) {
        if (!pointerCancel&&!text.characters.empty()) {throw std::invalid_argument("Continue does not consume name input");}
        std::optional<ContinuePointerInput> pointerInput;
        if(pointer&&(pointerCancel||(!input.forward&&!input.reverse&&!input.left&&!input.right))) {
            const auto position=modalPointer_.value_or(ProjectedPoint{161,
                static_cast<std::int16_t>(model_.continueChoice()==ContinueChoice::yes?74:118)});
            const auto hit=pointerWidgetHit(continuePointerHitBoxes(),position);
            pointerInput=ContinuePointerInput{hit?std::optional{*hit==0?ContinueChoice::yes:ContinueChoice::no}:std::nullopt,
                pointerCancel?ContinuePointerButton::cancel
                    :pointerDown?ContinuePointerButton::held:ContinuePointerButton::released};
        } else if(pointer) {modalPointer_.reset();}
        const auto decision=model_.pollPlayableContinue(input,pointerInput);
        if (decision==ContinueDecision::yes) {
            if (!highscoreTable_) {throw std::invalid_argument("playable Continue requires its owned highscore table");}
            if (highscoreTable_->rank(model_.score())) {
                model_.beginPlayableHighscoreNameEntry();nameEntry_.emplace(nameCaret_);
                nameEntryPages_=false;nameOkPressed_=false;incomingNameOkPressed_=false;
                pendingNameInput_={};renderNameEntryPages();upload(vgaComponentMaximum);
            } else {retry();}
        } else if (decision==ContinueDecision::pending) {
            renderContinuePages();flip();
        }
        commit();return delivery;
    }
    if (previous==GameplayPhase::highscoreNameEntry) {
        if (!nameEntry_ || !highscoreTable_) {throw std::logic_error("playable score editor lost its owner");}
        const auto mouse=pointer ? pointerWidgetPoll(nameOkPressed_?std::optional<std::size_t>{0}:std::nullopt,
            namePointerHitBoxes(),modalPointer_.value_or(ProjectedPoint{161,139}),
            pointerCancel?PointerButton::cancel:pointerDown?PointerButton::held:PointerButton::released) : PointerWidgetPoll{};
        if (pointer ? mouse.activated.has_value() : nameOkPressed_ && !input.confirm) {
            pendingNameInput_={};incomingNameOkPressed_=false;renderNameEntryPages(NameEntryPoll::accepted);
            if (nameEntry_->name().empty()) {
                finishScore();commit();return delivery;
            }
            auto candidate=*highscoreTable_;
            const auto mark=model_.playableHighscoreCaller()==PlayableHighscoreCaller::endingTitle
                ? HighscoreNameMark::flagged : HighscoreNameMark::ordinary;
            if (!candidate.insert(model_.score(),nameEntry_->name(),mark)) {
                throw std::logic_error("playable score editor lost its admitted rank");
            }
            candidate.setConfiguration(audio_->menuPreferencesApplied_
                ? audio_->menuResources_->configuration : *assets_.cockpitSettings);
            bool saveFailed=false;
            if (highscoreStore_) {
                const auto saved=highscoreStore_->saveAndReload(candidate);
                if (saved.status==HighscoreSaveStatus::saved&&saved.reloaded) {
                    candidate=*saved.reloaded;
                } else {saveFailed=true;}
            }
            highscoreTable_=std::move(candidate);model_.beginPlayableHighscoreHall();
            hallPages_=false;hallContinuePressed_=false;incomingHallContinuePressed_=false;
            renderHallPages();
            if(saveFailed) {
                constexpr std::string_view message="SCORE NOT SAVED";
                constexpr std::int16_t screenWidth=320,noticeY=155;
                constexpr std::uint8_t noticeColor=239;
                const auto width=uiTextWidthOperations(assets_.titleFont,UiFontLayout::fixed,message).width;
                const ProjectedPoint origin{static_cast<std::int16_t>((screenWidth-width)/2),noticeY};
                for(const auto page:{pages_.backPage(),pages_.frontPage()}) {
                    pixels_->apply(uiTextOperations(assets_.titleFont,UiFontLayout::fixed,message,origin,noticeColor,
                        page,{0,319,0,199}).effects);
                }
            }
            upload(vgaComponentMaximum);
        } else {
            pendingNameInput_=pointerCancel?NameEntryInput{}:text;incomingNameOkPressed_=pointer?mouse.pressed.has_value():input.confirm;
            renderNameEntryPages();flip();
        }
        commit();return delivery;
    }
    if (previous==GameplayPhase::highscoreHall) {
        const auto mouse=pointer ? pointerWidgetPoll(hallContinuePressed_?std::optional<std::size_t>{0}:std::nullopt,
            scoresPointerHitBoxes(),modalPointer_.value_or(ProjectedPoint{164,173}),
            pointerCancel?PointerButton::cancel:pointerDown?PointerButton::held:PointerButton::released) : PointerWidgetPoll{};
        if ((!pointerCancel&&input.exit) || (pointer ? mouse.activated.has_value() : hallContinuePressed_ && !input.confirm)) {
            finishScore();
        } else {
            incomingHallContinuePressed_=pointer?mouse.pressed.has_value():input.confirm;renderHallPages();flip();
        }
        commit();return delivery;
    }
    throw std::invalid_argument("unsupported playable lifecycle boundary");
}

} //namespace stellar7
