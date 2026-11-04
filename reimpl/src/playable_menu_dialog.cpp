#include "playable_menu_dialog.hpp"
#include "playable_menu_pages.hpp"
#include "briefing_strip.hpp"
#include "playable_pointer.hpp"

#include <algorithm>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::array<std::array<std::size_t,4>,6> briefingLinks{{
    {{5,2,0,1}},{{1,1,0,1}},{{0,3,2,2}},{{2,4,3,3}},{{3,5,4,4}},{{4,0,5,5}}}};
constexpr std::array<ProjectedPoint,6> briefingPointers{{{67,172},{248,172},{32,46},{32,64},{32,82},{32,100}}};

constexpr std::int16_t detailPointerOriginX=119; //8152: widgetX118+1.
constexpr std::int16_t detailFocusY=46; //8468: widgetY43+height6/2.
constexpr std::int16_t detailEditY=47; //8152 fixes pointerY=widgetY+4 while editing.
constexpr int detailKeyStep=2;
constexpr std::int16_t detailJoystickDownThreshold=40; //1A02..1A15 changes widget0's DOWN link.
constexpr std::int16_t inGameDetailYOffset=-28;
constexpr std::string_view detailBoundaryNotice="DETAIL 51-81 ONLY";
constexpr std::string_view saveFailureNotice="SAVE FAILED - RETRY EXIT/ESC UNDO";
constexpr std::string_view liveSaveFailureNotice="SAVE FAILED - RETRY PLAY/ESC UNDO";

ProjectedPoint detailFocus(const CockpitSettings& configuration) {
    return {static_cast<std::int16_t>(detailPointerOriginX+configuration.viewSize),detailFocusY};
}

PreferenceAvailability availablePreferences(const NativePointerCapability capability,
    const NativeDetailCapability detailCapability,const NativeMenuFeatures menuFeatures,const PlayableJoystick* joystick) {
    auto result=fixedPlayablePreferences();
    if(hasNativeMenuFeature(menuFeatures,NativeMenuFeatures::joystickControls)&&joystick) {
        result[static_cast<std::size_t>(PreferenceControl::joystick)]={};
    }
    if(hasNativeMenuFeature(menuFeatures,NativeMenuFeatures::storyPanels)) {result[static_cast<std::size_t>(PreferenceControl::story)]={};}
    if((detailCapability==NativeDetailCapability::bounded51To81||detailCapability==NativeDetailCapability::fullRange)) {result[static_cast<std::size_t>(PreferenceControl::detail)]={};}
    if(capability==NativePointerCapability::portablePointer) {result[static_cast<std::size_t>(PreferenceControl::mouse)]={};}
    result[static_cast<std::size_t>(PreferenceControl::sounds)]={};
    result[static_cast<std::size_t>(PreferenceControl::music)]={};
    result[static_cast<std::size_t>(PreferenceControl::difficulty)]={};
    return result;
}
}

PlayableMenuDialog::PlayableMenuDialog(PlayableMenuPage page,const GameplayAssets& assets,
    const NativePointerCapability capability,const NativeDetailCapability detailCapability,const NativeMenuFeatures menuFeatures,const PreferenceLayout layout,const PlayableModelDetail sharedDetail,PlayableJoystick* joystick)
    :page_(page),layout_(layout),pointerCapability_(capability),detailCapability_(detailCapability),menuFeatures_(menuFeatures),joystick_(joystick) {
    if(layout==PreferenceLayout::inGame&&!hasNativeMenuFeature(menuFeatures,NativeMenuFeatures::fullGraphicDetail)) {
        throw std::invalid_argument("live preferences require explicit full detail menu features");
    }
    if(!validNativeMenuFeatures(menuFeatures)) {throw std::invalid_argument("invalid native menu features");}
    if(detailCapability!=NativeDetailCapability::unavailable&&detailCapability!=NativeDetailCapability::bounded51To81&&detailCapability!=NativeDetailCapability::fullRange) {
        throw std::invalid_argument("invalid native detail capability");
    }
    if(page==PlayableMenuPage::briefing) {
        text_=parseBriefingText(assets.briefingText);viewer_.emplace(assets,sharedDetail);viewer_->select(0,assets);selection_=1;
    } else if(page==PlayableMenuPage::preferences) {
        selection_=static_cast<std::size_t>(PreferenceControl::exit);
    } else if(page!=PlayableMenuPage::scores) {
        throw std::invalid_argument("modal menu requires a recovered submenu");
    }
}

bool PlayableMenuDialog::advanceDetail(const NativeMenuInput& input,CockpitSettings& configuration) {
    if(page_!=PlayableMenuPage::preferences||(detailCapability_!=NativeDetailCapability::bounded51To81&&detailCapability_!=NativeDetailCapability::fullRange)) {return false;}
    if(!(detailCapability_==NativeDetailCapability::fullRange ? nativeFullDetailSettingSupported(configuration.viewSize)
        : nativeDetailSettingSupported(configuration.viewSize))) {
        throw std::invalid_argument("native detail editor requires settings51..82");
    }
    const bool raw=pointerCapability_==NativePointerCapability::portablePointer&&configuration.options[0]!=0&&input.pointer;
    const auto point=raw?sourcePointerPosition(input.pointer->position):pointerPosition();
    const auto hit=pointerWidgetHit(preferenceBoxes(),point);
    const bool horizontal=!input.up&&!input.down&&(input.left||input.right);
    const bool mouseDown=raw&&(input.pointer->primary||input.pointer->secondary);
    const bool held=input.confirm||mouseDown||input.up||input.down||input.left||input.right;
    if(detailEditing_) {
        if(!held) {detailEditing_=false;pressed_.reset();armed_=true;return true;}
    } else {
        const bool selected=selection_==static_cast<std::size_t>(PreferenceControl::detail);
        const bool confirmDetail=input.confirm&&!input.up&&!input.down&&!input.left&&!input.right
            &&(pointerCapability_==NativePointerCapability::portablePointer?hit==0:selected);
        const bool mouseDetail=mouseDown&&hit==0&&!input.up&&!input.down&&!input.left&&!input.right;
        if(!(horizontal&&selected)&&!confirmDetail&&!mouseDetail) {return false;}
        detailEditing_=true;pressed_=static_cast<std::size_t>(PreferenceControl::detail);
    }
    const int requested=horizontal?configuration.viewSize+(input.right?detailKeyStep:0)-(input.left?detailKeyStep:0)
        :point.x-detailPointerOriginX;
    const auto value=static_cast<std::int16_t>(std::clamp(requested,0,static_cast<int>(nativeDetailInteractiveMaximum)));
    pointerPosition_=ProjectedPoint{static_cast<std::int16_t>(detailPointerOriginX+value),static_cast<std::int16_t>(detailEditY+(layout_==PreferenceLayout::inGame?inGameDetailYOffset:0))};
    if(detailCapability_==NativeDetailCapability::bounded51To81&&value<nativeDetailMinimum) {notice_=detailBoundaryNotice;return true;}
    configuration.viewSize=value;notice_={};
    return true;
}

void PlayableMenuDialog::cancelInteraction() noexcept {
    detailEditing_=false;pressed_.reset();
}

MenuDialogResult PlayableMenuDialog::advance(NativeMenuInput input,CockpitSettings& configuration,
    HighscoreTable& scores,HighscoreStore* store,const JoystickCounterSample* joystickSample) {
    if(page_==PlayableMenuPage::preferences&&!initialConfiguration_) {
        initialConfiguration_=configuration;
        if(joystick_) {initialJoystick_=*joystick_;}
    }
    if(pointerCapability_==NativePointerCapability::portablePointer&&input.pointer
        &&input.pointer->phase==NativePointerPhase::cancel) {
        const auto& boxes=page_==PlayableMenuPage::scores?scoresPointerHitBoxes()
            :page_==PlayableMenuPage::briefing?briefingPointerHitBoxes():preferenceBoxes();
        detailEditing_=false;
        pressed_=pointerWidgetPoll(pressed_,boxes,pointerPosition(),PointerButton::cancel).pressed;
        return MenuDialogResult::pending;
    }
    if(input.cancel&&backAvailable()) {
        if(saveFailed_) {
            configuration=*initialConfiguration_;
            if(joystick_&&initialJoystick_) {*joystick_=*initialJoystick_;}
        }
        return MenuDialogResult::closed;
    }
    if(input.cancel&&layout_==PreferenceLayout::inGame) {return savePreferences(configuration,scores,store);}
    if(advanceDetail(input,configuration)) {return MenuDialogResult::pending;}
    std::optional<PreferenceDirection> direction;
    if(input.up) {direction=PreferenceDirection::up;}
    else if(input.down) {direction=PreferenceDirection::down;}
    else if(input.left) {direction=PreferenceDirection::left;}
    else if(input.right) {direction=PreferenceDirection::right;}
    if(direction&&armed_) {
        if(page_==PlayableMenuPage::briefing) {
            selection_=briefingLinks.at(selection_).at(static_cast<std::size_t>(*direction));
        } else if(page_==PlayableMenuPage::preferences) {
            auto next=preferenceNeighbor(static_cast<PreferenceControl>(selection_),*direction,layout_);
            if(layout_==PreferenceLayout::title&&detailCapability_==NativeDetailCapability::fullRange
                &&selection_==static_cast<std::size_t>(PreferenceControl::detail)&&*direction==PreferenceDirection::down) {
                next=configuration.viewSize>detailJoystickDownThreshold ? PreferenceControl::joystick : PreferenceControl::mouse;
            }
            selection_=static_cast<std::size_t>(next);
        }
        pressed_.reset();pointerPosition_.reset();armed_=false;
        if(page_==PlayableMenuPage::preferences&&(detailCapability_==NativeDetailCapability::bounded51To81||detailCapability_==NativeDetailCapability::fullRange)
            &&selection_==static_cast<std::size_t>(PreferenceControl::detail)) {pointerPosition_=detailFocus(configuration);
            if(layout_==PreferenceLayout::inGame) {pointerPosition_->y+=inGameDetailYOffset;}}
        if(page_==PlayableMenuPage::preferences) {notice_=availablePreferences(pointerCapability_,detailCapability_,menuFeatures_,joystick_).at(selection_);}
        return MenuDialogResult::pending;
    }
    if(!direction) {armed_=true;}
    std::optional<std::size_t> activated;
    const bool pointer=pointerCapability_==NativePointerCapability::portablePointer && !direction;
    if(pointer) {
        const bool enabled=configuration.options[0]!=0;
        if(enabled&&input.pointer) {pointerPosition_=sourcePointerPosition(input.pointer->position);}
        const auto& boxes=page_==PlayableMenuPage::scores ? scoresPointerHitBoxes()
            : page_==PlayableMenuPage::briefing ? briefingPointerHitBoxes() : preferenceBoxes();
        const auto down=input.confirm || (enabled&&input.pointer&&(input.pointer->primary||input.pointer->secondary));
        const auto poll=pointerWidgetPoll(pressed_,boxes,pointerPosition(),
            down ? PointerButton::held : PointerButton::released);
        pressed_=poll.pressed;activated=poll.activated;
        if(!activated) {return MenuDialogResult::pending;}
    } else {
        if(input.confirm&&!pressed_) {pressed_=selection_;}
        if(input.confirm||!pressed_) {return MenuDialogResult::pending;}
        activated=pressed_;pressed_.reset();
    }
    const auto selected=*activated;
    if(page_==PlayableMenuPage::scores) {return MenuDialogResult::closed;}
    if(page_==PlayableMenuPage::briefing) {
        if(selected==0||(selected==1&&briefingPage_+1==briefingPageCount)) {return MenuDialogResult::closed;}
        if(selected==1) {++briefingPage_;}
        else {viewer_->control(static_cast<BriefingViewerControl>(selected-2));}
        return MenuDialogResult::pending;
    }
    const auto control=static_cast<PreferenceControl>(selected);
    if(control==PreferenceControl::exit) {
        return savePreferences(configuration,scores,store);
    }
    const auto available=availablePreferences(pointerCapability_,detailCapability_,menuFeatures_,joystick_);notice_=available.at(selected);
    const auto request=preferenceRequest(configuration,control,PreferenceChange::increment,available);
    if(request&&request->control==PreferenceControl::mouse) {configuration.options[0]=request->value;}
    if(request&&request->control==PreferenceControl::joystick) {
        if(!joystick_||!joystickSample) {throw std::invalid_argument("joystick preference requires its retained controller and latest counter sample");}
        constexpr std::size_t joystickOption=1;
        if(request->value==0) {
            joystick_->disable();configuration.options[joystickOption]=0;
        } else {
            configuration.options[joystickOption]=joystick_->enable(*joystickSample) ? 1 : 0; //7D73 stores detection bit in12B2.
            if(configuration.options[joystickOption]==0) {notice_=fixedPlayablePreferences()[selected];}
        }
    }
    if(request&&request->control==PreferenceControl::sounds) {configuration.options[3]=request->value;}
    if(request&&request->control==PreferenceControl::music) {configuration.options[2]=request->value;}
    if(request&&request->control==PreferenceControl::difficulty) {configuration.options[4]=request->value;}
    if(request&&request->control==PreferenceControl::story) {configuration.options[5]=request->value;}
    return MenuDialogResult::pending;
}

std::optional<std::uint16_t> PlayableMenuDialog::menuCalibrationSaved(const CockpitSettings& configuration) {
    if(page_==PlayableMenuPage::preferences) {
        initialConfiguration_=configuration;
        if(joystick_) {initialJoystick_=*joystick_;}
        saveFailed_=false;notice_={};
    }
    constexpr std::uint16_t briefingRecognizedCountdown=15000; //1FC5 after callbackFF refreshes current page.
    return page_==PlayableMenuPage::briefing ? std::optional<std::uint16_t>{briefingRecognizedCountdown} : std::nullopt;
}

MenuDialogResult PlayableMenuDialog::savePreferences(CockpitSettings& configuration,HighscoreTable& scores,HighscoreStore* store) {
    auto candidate=scores;candidate.setConfiguration(configuration);
    if(store) {
        const auto saved=store->saveAndReload(candidate);
        if(saved.status!=HighscoreSaveStatus::saved||!saved.reloaded) {
            saveFailed_=true;return MenuDialogResult::pending;
        }
        candidate=*saved.reloaded;
    }
    configuration=decodeCockpitSettings(std::vector<std::uint8_t>(candidate.header().begin(),candidate.header().end()));
    scores=std::move(candidate);
    return MenuDialogResult::preferencesSaved;
}

void PlayableMenuDialog::paint(IndexedPages& pages,const GameplayAssets& assets,
    const CockpitSettings& configuration,const HighscoreTable& scores,std::uint16_t back,std::uint16_t front) {
    if(page_==PlayableMenuPage::scores) {
        paintPlayableScores(pages,assets.artGroups.at(0),assets.titleFont,scores,
            pressed_?MenuWidgetState::pressed:MenuWidgetState::released,back,front,pointerPosition_);return;
    }
    ProjectedPoint pointer{};
    if(page_==PlayableMenuPage::preferences) {
        pages.apply(preferencesPageEffects(assets.artGroups.at(0),assets.titleFont,assets.scoreFont,configuration,
            static_cast<PreferenceControl>(selection_),pressed_?std::optional{static_cast<PreferenceControl>(*pressed_)}:std::nullopt,
            saveFailed_?(layout_==PreferenceLayout::inGame?liveSaveFailureNotice:saveFailureNotice):notice_,back,layout_));
        pointer=preferencePointer(static_cast<PreferenceControl>(selection_),layout_);
    } else {
        pages.apply(briefingShellEffects(assets.artGroups.at(0),assets.titleFont,back));
        pages.apply(briefingTextEffects(text_,briefingPage_,assets.scoreFont,back));
        if(paintedBriefingPage_!=briefingPage_) {viewer_->select(briefingPage_,assets);paintedBriefingPage_=briefingPage_;}
        if(briefingPreview(briefingPage_)) {pages.apply(briefingStripEffects(viewer_->stripIndex(),assets,back));}
        pages.apply(viewer_->poll(assets,back));
        pages.apply(briefingDetailsEffects(briefingPage_,assets.titleFont,assets.scoreFont,back));
        for(std::size_t widget=0;widget<briefingPointers.size();++widget) {
            pages.apply(briefingWidgetEffects(assets.artGroups.at(0),assets.titleFont,widget,
                pressed_&&*pressed_==widget?MenuWidgetState::pressed:MenuWidgetState::released,back));
        }
        pointer=briefingPointers.at(selection_);
    }
    pages.apply(std::vector<IndexedPageEffect>{RadarPageCopy{back,front,{0,0},320,200}});
    paintPlayableMenuPointer(pages,assets.artGroups.at(0).at(0),pointerPosition_.value_or(pointer),back,front);
}

const std::vector<PointerHitBox>& PlayableMenuDialog::preferenceBoxes() const {
    if(layout_==PreferenceLayout::title) {return preferencePointerHitBoxes();}
    //Keep semantic indices aligned while omitting LEVEL and STORY.
    static const std::vector<PointerHitBox> boxes{{118,15,84,6},{109,48,17,14},
        {109,77,17,14},{109,106,17,14},{109,135,17,14},
        {-100,-100,0,0},{-100,-100,0,0},{123,173,81,11}};
    return boxes;
}

ProjectedPoint PlayableMenuDialog::pointerPosition() const {
    if(pointerPosition_) {return *pointerPosition_;}
    if(page_==PlayableMenuPage::scores) {return {164,173};}
    return page_==PlayableMenuPage::preferences ? preferencePointer(static_cast<PreferenceControl>(selection_),layout_)
        : briefingPointers.at(selection_);
}
}
