#pragma once

#include "briefing_preview.hpp"
#include "playable_preferences.hpp"
#include "playable_detail.hpp"
#include "playable_pointer.hpp"
#include "playable_joystick.hpp"
#include "stellar7/native_startup.hpp"

namespace stellar7 {
enum class MenuDialogResult { pending, closed, preferencesSaved };

class PlayableMenuDialog {
public:
    PlayableMenuDialog(PlayableMenuPage page,const GameplayAssets& assets,
        NativePointerCapability pointerCapability=NativePointerCapability::unavailable,
        NativeDetailCapability detailCapability=NativeDetailCapability::unavailable,
        NativeMenuFeatures menuFeatures=NativeMenuFeatures::legacy,
        PreferenceLayout layout=PreferenceLayout::title, PlayableModelDetail sharedDetail = {},
        PlayableJoystick* joystick=nullptr);
    ProjectedPoint pointerPosition() const;
    void cancelInteraction() noexcept;
    bool calibrationAvailable() const noexcept {return layout_==PreferenceLayout::title&&!pressed_&&!detailEditing_;}
    std::optional<std::uint16_t> menuCalibrationSaved(const CockpitSettings& configuration);
    bool backAvailable() const noexcept {return page_==PlayableMenuPage::scores||saveFailed_;}
    MenuDialogResult advance(NativeMenuInput input,CockpitSettings& configuration,
        HighscoreTable& scores,HighscoreStore* store,const JoystickCounterSample* joystickSample=nullptr);
    void paint(IndexedPages& pages,const GameplayAssets& assets,const CockpitSettings& configuration,
        const HighscoreTable& scores,std::uint16_t back,std::uint16_t front);
private:
    const std::vector<PointerHitBox>& preferenceBoxes() const;
    MenuDialogResult savePreferences(CockpitSettings& configuration,HighscoreTable& scores,HighscoreStore* store);
    bool advanceDetail(const NativeMenuInput& input,CockpitSettings& configuration);
    PlayableMenuPage page_;
    PreferenceLayout layout_;
    NativePointerCapability pointerCapability_;
    NativeDetailCapability detailCapability_;
    NativeMenuFeatures menuFeatures_;
    PlayableJoystick* joystick_=nullptr;
    std::optional<PlayableJoystick> initialJoystick_;
    bool detailEditing_=false;
    bool saveFailed_=false;
    std::optional<CockpitSettings> initialConfiguration_;
    std::optional<ProjectedPoint> pointerPosition_;
    std::size_t selection_=0;
    std::optional<std::size_t> pressed_;
    bool armed_=true;
    std::size_t briefingPage_=0;
    std::optional<std::size_t> paintedBriefingPage_;
    BriefingTextPages text_{};
    std::optional<BriefingViewer> viewer_;
    std::string_view notice_;
};
}
