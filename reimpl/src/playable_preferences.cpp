#include "playable_preferences.hpp"
#include "stellar7/ui_bitmap_work.hpp"
#include "stellar7/ui_text_work.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace stellar7 {
namespace {
constexpr Viewport display{0, 319, 0, 199};
constexpr int inGameDetailYOffset=-28;
constexpr ProjectedPoint playPointerOffset{61,19};
constexpr ProjectedPoint exitPointerOffset{61,11};
constexpr std::size_t controlCount = static_cast<std::size_t>(PreferenceControl::count);
using C = PreferenceControl;
//180C/5810 links in source up/down/left/right order.
constexpr std::array<PreferenceWidget, controlCount> widgets{{
    {{118,43},24,{C::exit,C::mouse,C::detail,C::detail}},
    {{42,72},25,{C::detail,C::music,C::mouse,C::joystick}},
    {{160,72},25,{C::detail,C::sounds,C::mouse,C::joystick}},
    {{42,101},25,{C::mouse,C::difficulty,C::music,C::sounds}},
    {{160,101},25,{C::joystick,C::story,C::music,C::sounds}},
    {{42,130},25,{C::music,C::exit,C::difficulty,C::story}},
    {{160,130},25,{C::sounds,C::exit,C::difficulty,C::story}},
    {{102,159},18,{C::story,C::detail,C::exit,C::exit}},
}};
//7D7E/5810: source six widgets, represented with the existing semantic IDs.
constexpr std::array<PreferenceWidget, controlCount> gameWidgets{{
    {{118,15},24,{C::exit,C::mouse,C::detail,C::detail}},
    {{99,43},25,{C::detail,C::joystick,C::mouse,C::mouse}},
    {{99,72},25,{C::mouse,C::music,C::joystick,C::joystick}},
    {{99,101},25,{C::joystick,C::sounds,C::music,C::music}},
    {{99,130},25,{C::music,C::exit,C::sounds,C::sounds}},
    {{0,0},0,{C::exit,C::exit,C::exit,C::exit}},
    {{0,0},0,{C::exit,C::exit,C::exit,C::exit}},
    {{102,159},19,{C::sounds,C::detail,C::exit,C::exit}},
}};
constexpr std::array<std::string_view, controlCount> labels{
    "GRAPHIC DETAIL", "MOUSE", "JOYSTICK", "MUSIC", "SOUNDS", "LEVEL", "STORY PANELS", "EXIT"};
constexpr std::array<std::string_view, 3> difficulties{"EASY", "STANDARD", "HARD"};
constexpr std::int16_t sliderLimit = 81; //8152 clamps interactive edits at51h; loaded82 is legal.
constexpr std::uint8_t twoLineFieldShadow = 135; //DS12F6.

std::size_t index(const C control) {
    const auto value = static_cast<std::size_t>(control);
    if (value >= controlCount) {
        throw std::invalid_argument("invalid preferences control");
    }
    return value;
}
} // namespace

const std::array<PreferenceWidget, controlCount>& preferenceWidgets(const PreferenceLayout layout) { return layout==PreferenceLayout::inGame?gameWidgets:widgets; }

C preferenceNeighbor(const C current, const PreferenceDirection direction, const PreferenceLayout layout) {
    return preferenceWidgets(layout).at(index(current)).neighbors.at(static_cast<std::size_t>(direction));
}

ProjectedPoint preferencePointer(const C current, const PreferenceLayout layout) {
    const auto origin = preferenceWidgets(layout).at(index(current)).origin;
    //5C14 centers the template hit rectangle, not the outer bitmap.
    const ProjectedPoint offset = current == C::detail ? ProjectedPoint{42,3}
        : current == C::exit ? (layout==PreferenceLayout::inGame?playPointerOffset:exitPointerOffset) : ProjectedPoint{18,12};
    return {static_cast<std::int16_t>(origin.x+offset.x),static_cast<std::int16_t>(origin.y+offset.y)};
}

std::string_view preferenceValue(const CockpitSettings& settings, const C control) {
    const auto item = index(control);
    if (control == C::detail || control == C::exit) { return {}; }
    if (control == C::difficulty) {
        const auto level = settings.options[4];
        return level >= 0 && level < static_cast<std::int16_t>(difficulties.size())
            ? difficulties[static_cast<std::size_t>(level)] : "INVALID";
    }
    if (control == C::story) { return settings.options[5] == 255 ? "ACTIVE" : "SKIPPED"; }
    return settings.options[item-1] != 0 ? "ON" : "OFF";
}

PreferenceAvailability fixedPlayablePreferences() {
    return {"GRAPHIC DETAIL FIXED BY RENDERER", "MOUSE INPUT UNAVAILABLE", "JOYSTICK INPUT UNAVAILABLE",
        "MUSIC CONTROL UNAVAILABLE", "SOUND CONTROL UNAVAILABLE", "EASY DIFFICULTY ONLY",
        "STORY PLAYBACK UNAVAILABLE", {}};
}

std::optional<PreferenceRequest> preferenceRequest(const CockpitSettings& current, const C control,
    const PreferenceChange change, const PreferenceAvailability& availability) {
    const auto item = index(control);
    if (control == C::exit || !availability[item].empty()) { return std::nullopt; }
    if (control == C::detail) {
        const int step = change == PreferenceChange::increment ? 1 : -1;
        return PreferenceRequest{control, static_cast<std::int16_t>(std::clamp<int>(current.viewSize+step,0,sliderLimit))};
    }
    if (control == C::difficulty) {
        const auto value = current.options[4];
        if (value < 0 || value >= static_cast<std::int16_t>(difficulties.size())) {
            throw std::invalid_argument("invalid current difficulty");
        }
        return PreferenceRequest{control, static_cast<std::int16_t>((value+1)%difficulties.size())};
    }
    if (control == C::story) {
        return PreferenceRequest{control, static_cast<std::int16_t>(current.options[5]^255)};
    }
    const auto value = current.options[item-1];
    //84CE first normalizes an invalid word to1, then flips its low bit.
    return PreferenceRequest{control, static_cast<std::int16_t>(value == 0 ? 1 : 0)};
}

std::vector<IndexedPageEffect> preferencesPageEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& titleFont, const BitmapFont& valueFont, const CockpitSettings& current,
    const C selected, const std::optional<C> pressed, const std::string_view notice, const std::uint16_t page, const PreferenceLayout layout) {
    index(selected);
    if (art.size() <= 26 || current.viewSize < 0 || current.viewSize > 82) {
        throw std::invalid_argument("preferences require source UI bitmaps and a legal detail setting");
    }
    std::vector<IndexedPageEffect> effects;
    const auto append = [&](const auto& work) { effects.insert(effects.end(),work.begin(),work.end()); };
    const auto bitmap = [&](const std::size_t frame, const int x, const int y) {
        append(uiBitmapOperations(art.at(frame),{static_cast<std::int16_t>(x),static_cast<std::int16_t>(y)},display,page).effects);
    };
    const auto fill = [&](const int x, const int y, const int width, const int height, const std::uint8_t color) {
        for (int row=y;row<y+height;++row) {
            for (int column=x;column<x+width;++column) {
                effects.emplace_back(RadarPixelWrite{{static_cast<std::int16_t>(column),static_cast<std::int16_t>(row)},color,page});
            }
        }
    };
    const auto text = [&](const BitmapFont& font, const UiFontLayout layout, const std::string_view value,
        const int center, const int y, const std::uint8_t color) {
        const auto width = uiTextWidthOperations(font,layout,value).width;
        append(uiTextOperations(font,layout,value,{static_cast<std::int16_t>(center-width/2),static_cast<std::int16_t>(y)},
            color,page,display).effects);
    };
    const auto title = [&](const std::string_view value, const int center, const int y) {
        text(titleFont,UiFontLayout::fixed,value,center,y,128);
        text(titleFont,UiFontLayout::fixed,value,center+1,y+1,156);
    };
    const auto& activeWidgets=preferenceWidgets(layout);
    if(layout==PreferenceLayout::title) {
    //2CF4 backdrop,180C panel and55AC's eight-tile title strip.
    for (int y=0,x=0;y<200;y+=2,x+=3) {
        if (y>174) { bitmap(20,x-524,y); }
        bitmap(20,x-262,y);
        if (x<320) { bitmap(20,x,y); }
        if (x<58) { bitmap(20,x+262,y); }
    }
    append(dialogPanelPageEffects(art.at(12),{35,5,250,186},page));
    bitmap(14,52,15);
    for (int tile=0;tile<8;++tile) { bitmap(23,76+21*tile,15); }
    append(uiMirroredBitmapOperations(art.at(14),{244,15},display,page).effects);
    title("P R E F E R E N C E S",160,22);
    } else {
        append(dialogPanelPageEffects(art.at(12),{91,1,138,198},page));
    }
    const int sliderY=layout==PreferenceLayout::inGame?inGameDetailYOffset:0;
    //8020 detail track. Slider widget24 itself has no base drawing.
    bitmap(17,100,38+sliderY);
    fill(118,43+sliderY,80,5,157); fill(123,49+sliderY,75,5,157); fill(118,43+sliderY,85,6,141);
    for (int x=2;x<current.viewSize;x+=2) { fill(118+x,44+sliderY,1,4,239); }
    for (int x=current.viewSize&~1;x<84;x+=2) { fill(118+x,44+sliderY,1,4,147); }
    append(uiTextOperations(valueFont,UiFontLayout::proportional,"GRAPHIC DETAIL",{126,static_cast<std::int16_t>(51+sliderY)},135,page,display).effects);
    bitmap(24,118+current.viewSize,42+sliderY);
    for (std::size_t item=1;item<controlCount;++item) {
        const auto control = static_cast<C>(item);
        if(layout==PreferenceLayout::inGame&&(control==C::difficulty||control==C::story)) {continue;}
        const auto origin = activeWidgets[item].origin;
        bitmap(activeWidgets[item].style,origin.x,origin.y);
        const bool livePlay=layout==PreferenceLayout::inGame&&control==C::exit;
        if (pressed && *pressed==control) {
            if(livePlay) {
                //19 template: flags1 reverses source rows for sides and overlay.
                const auto verticalBitmap=[&](const std::size_t frame,const int x,const int y) {
                    auto flipped=art.at(frame);
                    for(std::size_t row=0;row<flipped.height/2;++row) {
                        for(std::size_t column=0;column<flipped.width;++column) {
                            std::swap(flipped.pixels[row*flipped.width+column],
                                flipped.pixels[(flipped.height-1-row)*flipped.width+column]);
                        }
                    }
                    append(uiBitmapOperations(flipped,{static_cast<std::int16_t>(x),
                        static_cast<std::int16_t>(y)},display,page).effects);
                };
                verticalBitmap(11,origin.x+14,origin.y+9);
                verticalBitmap(11,origin.x+99,origin.y+9);
                verticalBitmap(22,origin.x+20,origin.y+12);
            } else {
                bitmap(11,origin.x+(control==C::exit?10:106),origin.y+(control==C::exit?7:8));
                if (control==C::exit) { bitmap(11,origin.x+104,origin.y+7); }
                bitmap(control==C::exit?21:26,origin.x+(control==C::exit?20:10),origin.y+5);
            }
        }
        if (control==C::exit) { title(livePlay?"PLAY":"EXIT",origin.x+60,origin.y+(livePlay?15:7)); continue; }
        fill(origin.x+31,origin.y+3,72,17,157);
        if (control==C::difficulty || control==C::story) {
            //1C8A..1CF9 expands the recessed field for both text baselines.
            fill(origin.x+31,origin.y+4,72,16,twoLineFieldShadow);
            fill(origin.x+31,origin.y+4,72,15,147);
            fill(origin.x+32,origin.y+5,70,14,0);
            text(valueFont,UiFontLayout::proportional,labels[item],origin.x+67,origin.y+6,239);
            text(valueFont,UiFontLayout::proportional,preferenceValue(current,control),origin.x+67,origin.y+13,239);
        } else {
            fill(origin.x+31,origin.y+7,72,9,141);
            fill(origin.x+31,origin.y+7,72,8,147);
            fill(origin.x+32,origin.y+8,70,7,0);
            const auto value=std::string(labels[item])+" "+std::string(preferenceValue(current,control));
            text(valueFont,UiFontLayout::proportional,value,origin.x+67,origin.y+9,239);
        }
    }
    //Explicit native capability disclosure; caller draws the source pointer last.
    if (!notice.empty()) { text(titleFont,UiFontLayout::fixed,notice,160,193,239); }
    return effects;
}
} // namespace stellar7
