#include "playable_joystick_calibration.hpp"
#include "stellar7/indexed_render.hpp"
#include "stellar7/ui_fill_work.hpp"
#include "stellar7/ui_text_work.hpp"

#include <array>
#include <stdexcept>
#include <string_view>

namespace stellar7 {
namespace {
constexpr std::uint16_t releaseDebounceRawIrqs=10; //88DB stores1DC1 before6638.
constexpr Viewport display{0,319,0,199};
constexpr CockpitCopyRegion calibrationPanel{64,68,193,35}; //8586:85-(35>>1).
constexpr CockpitCopyRegion innerPanel{78,78,165,16};
constexpr std::uint8_t panelFill=156,edgeLight=147,edgeDark=128,edgeInner=135;
constexpr std::uint8_t textColor=128; //8586:12F7 ->1FD0.
constexpr std::array<std::string_view,3> prompts{
    "MOVE STICK TO UPPER LEFT","MOVE STICK TO LOWER RIGHT","CENTER JOYSTICK"}; //DS14C0/14EA/1515.
constexpr std::string_view buttonPrompt="AND HIT A BUTTON"; //DS14AF/14D9/1504.

void append(std::vector<IndexedPageEffect>& to,const std::vector<IndexedPageEffect>& from) {
    to.insert(to.end(),from.begin(),from.end());
}
void text(std::vector<IndexedPageEffect>& effects,const BitmapFont& font,const UiFontLayout layout,
    const std::string_view value,const std::int16_t center,const std::int16_t y,const std::uint16_t page) {
    const auto width=uiTextWidthOperations(font,layout,value).width;
    append(effects,uiTextOperations(font,layout,value,{static_cast<std::int16_t>(center-width/2),y},textColor,page,display).effects);
}
std::vector<IndexedPageEffect> panel(const std::vector<IndexedBitmap>& art,const BitmapFont& primary,
    const std::string_view title,const std::uint16_t page) {
    if(art.size()<=12) {throw std::invalid_argument("joystick prompt requires source STELART panel12");}
    auto effects=dialogPanelPageEffects(art.at(12),calibrationPanel,page);
    const auto fill=[&](const CockpitCopyRegion region,const std::uint8_t color) {
        append(effects,uiFillOperations(region,color,page).effects);
    };
    fill(innerPanel,panelFill); //8586:8683 calls2E25.
    fill({78,78,165,1},edgeLight);fill({242,78,1,16},edgeLight); //5642 outer top/right.
    fill({78,78,1,16},edgeDark);fill({78,93,165,1},edgeDark);
    fill({79,79,163,1},edgeLight);fill({79,79,1,14},edgeLight);
    fill({241,79,1,14},edgeInner);fill({79,92,163,1},edgeInner);
    //4A5B..4A6E/4C06 returns0 without pixels when the layout glyph is absent.
    const auto discriminator=static_cast<unsigned char>('x');
    const bool absentDiscriminator=title=="x"&&(discriminator<primary.firstCharacter
        ||static_cast<std::size_t>(discriminator-primary.firstCharacter)>=primary.glyphs.size());
    if(absentDiscriminator) {uiTextWidthOperations(primary,UiFontLayout::fixed,{});}
    else {text(effects,primary,UiFontLayout::fixed,title,161,83,page);}
    return effects;
}
}

PlayableJoystickCalibration::PlayableJoystickCalibration(PlayableJoystick& joystick)
    :joystick_(&joystick),original_(joystick) {joystick_->beginCalibration();}

JoystickCalibrationPoint PlayableJoystickCalibration::point() const noexcept {return joystick_->calibrationPoint();}

JoystickCalibrationStep PlayableJoystickCalibration::advance(const JoystickCounterSample& sample,
    const JoystickCalibrationActivation activation,const JoystickCalibrationWait wait) {
    if(phase_==JoystickCalibrationPhase::complete||phase_==JoystickCalibrationPhase::unavailable||phase_==JoystickCalibrationPhase::rejected) {
        throw std::logic_error("joystick calibration already returned");
    }
    if(!sample.connected) {
        *joystick_=original_;phase_=JoystickCalibrationPhase::unavailable;
        return {phase_,point(),false,0};
    }
    if(wait==JoystickCalibrationWait::returned&&phase_!=JoystickCalibrationPhase::waitingDebounce) {
        throw std::invalid_argument("joystick calibration received an unrequested debounce return");
    }
    const bool held=activation==JoystickCalibrationActivation::held;
    if(phase_==JoystickCalibrationPhase::waitingPress) {
        if(held) {phase_=JoystickCalibrationPhase::waitingRelease;}
        return {phase_,point(),false,0};
    }
    if(phase_==JoystickCalibrationPhase::waitingRelease) {
        if(held) {return {phase_,point(),false,0};}
        phase_=JoystickCalibrationPhase::waitingDebounce;
        return {phase_,point(),false,releaseDebounceRawIrqs};
    }
    if(wait==JoystickCalibrationWait::pending) {return {phase_,point(),false,0};}
    if(held) {
        phase_=JoystickCalibrationPhase::waitingRelease;
        return {phase_,point(),false,0};
    }
    joystick_->captureCalibration(sample); //25A8 only after stable88C4 return.
    if(point()==JoystickCalibrationPoint::complete) {
        try {joystick_->analog({127,127});}
        catch(const std::invalid_argument&) {
            //Native recovery bound: never index beyond the recovered source table.
            *joystick_=original_;phase_=JoystickCalibrationPhase::rejected;
            return {phase_,point(),true,0};
        }
    }
    phase_=point()==JoystickCalibrationPoint::complete ? JoystickCalibrationPhase::complete : JoystickCalibrationPhase::waitingPress;
    return {phase_,point(),true,0};
}

std::vector<IndexedPageEffect> joystickCalibrationPageEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& primary,const BitmapFont& secondary,const JoystickCalibrationPoint point,const std::uint16_t page) {
    std::size_t prompt=0;
    switch(point) {
    case JoystickCalibrationPoint::upperLeft:break;
    case JoystickCalibrationPoint::lowerRight:prompt=1;break;
    case JoystickCalibrationPoint::center:prompt=2;break;
    default:throw std::invalid_argument("joystick prompt requires an active source calibration point");
    }
    auto effects=panel(art,primary,"x",page); //DS1532 is the source8586 layout discriminator/title.
    text(effects,secondary,UiFontLayout::proportional,prompts[prompt],160,80,page);
    text(effects,secondary,UiFontLayout::proportional,buttonPrompt,160,86,page);
    return effects;
}
std::vector<IndexedPageEffect> joystickDisabledPageEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& primary,const std::uint16_t page) {return panel(art,primary,"JOYSTICK OFF",page);}
std::vector<IndexedPageEffect> joystickCalibrationRejectedPageEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& primary,const BitmapFont& secondary,const std::uint16_t page) {
    auto effects=panel(art,primary,"x",page);
    text(effects,secondary,UiFontLayout::proportional,"CALIBRATION NOT SAVED",160,80,page);
    text(effects,secondary,UiFontLayout::proportional,"MOVE FULLY TO EACH CORNER",160,86,page);
    return effects;
}
std::vector<IndexedPageEffect> joystickCalibrationSaveFailedPageEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& primary,const BitmapFont& secondary,const std::uint16_t page) {
    auto effects=panel(art,primary,"x",page);
    text(effects,secondary,UiFontLayout::proportional,"SAVE FAILED",160,80,page);
    text(effects,secondary,UiFontLayout::proportional,"PRESS BUTTON TO RETRY",160,86,page);
    return effects;
}
} //namespace stellar7
