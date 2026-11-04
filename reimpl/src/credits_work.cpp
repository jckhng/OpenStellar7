#include "stellar7/credits_work.hpp"
#include "stellar7/ui_fill_work.hpp"

#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr Viewport display{0,319,0,199};
constexpr int centerX=160;
constexpr int labelX=55;
constexpr int nameX=185;
constexpr int leaderLimit=180;
constexpr int rowPitch=8;
constexpr int footerPitch=7;
constexpr int titleY=16;
constexpr int titleBarY=9;
constexpr int titleBarStart=76;
constexpr int titleBarPitch=21;
constexpr int titleBarPieces=8;
constexpr int titleCapOffset=24;
constexpr int shadowShadeOffset=6;
constexpr std::array<std::pair<std::string_view, std::string_view>,14> rows{{
    {"Producer and Designer","DAMON SLYE"}, {"Lead Programmer","PIOTR LUKASZUK"},
    {"Art Director","MARK PEASLEY"}, {"3D Artist","CYRUS KANGA"},
    {"Shell Programmer","NELS BRUCKNER"}, {"Artist","ROBERT CARACOL"},
    {"Audio Director","ALAN McKEAN"}, {"Musical Composer","DON LATARSKI"},
    {"","KIM NAGEL"}, {"Panel Art Painter","D. BRENT BURKETT"},
    {"Image Dept. Director","RANDY DERSHAM"}, {"Sound Effects Creator","CHRIS STEVENS"},
    {"Q.A. Engineer","FORREST WALKER"}, {"Co-Producer","JEFF TUNNELL"}
}};
constexpr std::array<std::string_view,3> footer{{"Design Contributors and Playtesters",
    "DAREK LUKASZUK, PAUL BOWMAN", "DAVID SELLE, JERRY LUTTRELL"}};
}

CreditsDrawingWork creditsDrawingOperations(const std::vector<IndexedBitmap>& art,
    const BitmapFont& titleFont, const BitmapFont& scoreFont, const CreditsColors& colors,
    const std::uint16_t backPage) {
    if (art.size() <= 23) {
        throw std::invalid_argument("credits require the retained UI bitmap group");
    }
    CreditsDrawingWork result{{}, {}, {display,PolygonClipMode::Enabled,1,colors.panel.middle}, 0};
    const auto plain = [&](const std::uint64_t count) {
        result.operations.push_back({NormalWorkKind::instructions,count});
    };
    const auto append = [&](const auto& work) {
        result.operations.insert(result.operations.end(),work.operations.begin(),work.operations.end());
        result.effects.insert(result.effects.end(),work.effects.begin(),work.effects.end());
    };
    const auto width = [&](const BitmapFont& font, const UiFontLayout layout, const std::string_view text) {
        const auto work = uiTextWidthOperations(font,layout,text);
        result.operations.insert(result.operations.end(),work.operations.begin(),work.operations.end());
        return work.width;
    };
    const auto bitmap = [&](const std::size_t index,const int x,const int y) {
        append(uiBitmapOperations(art[index],{static_cast<std::int16_t>(x),static_cast<std::int16_t>(y)},display,backPage));
    };
    const auto text = [&](const BitmapFont& font,const UiFontLayout layout,const std::string_view value,
        const int x,const int y,const std::uint8_t color) {
        append(uiTextOperations(font,layout,value,{static_cast<std::int16_t>(x),static_cast<std::int16_t>(y)},color,backPage,display));
    };
    const auto shadow = [&](const std::string_view value,const int x,const int y) {
        plain(15); //B2CE..B2F3.
        text(scoreFont,UiFontLayout::proportional,value,x,y,static_cast<std::uint8_t>(colors.fill-shadowShadeOffset));
        plain(9); //B2F8..B30C.
        text(scoreFont,UiFontLayout::proportional,value,x+1,y+1,colors.panel.middle);
        plain(6); //B311..B319.
    };
    plain(14); //B12A..B151 zero19CA branch; native font0 selection.
    plain(2); //B156/CALL2CF4.
    plain(12); //2CF4..2D00 including complete5BFE.
    for (int y=0,x=0;y<200;y+=2,x+=3) {
        plain(2);
        if (y>174) {
            plain(8); bitmap(20,x-524,y); plain(1);
        }
        plain(8); bitmap(20,x-262,y); plain(1);
        plain(2);
        if (x<320) {
            plain(7); bitmap(20,x,y); plain(1);
        }
        plain(2);
        if (x<58) {
            plain(8); bitmap(20,x+262,y); plain(1);
        }
        plain(5);
    }
    plain(3); //2D7A..2D7C.
    plain(8); //B15C..B168.
    append(uiPanelOperations(art[12],{0,0,320,200},colors.panel,backPage));
    plain(8); //B16B..B17A.
    plain(23); bitmap(14,titleBarStart-titleCapOffset,titleBarY); plain(5); //55AC..55ED.
    for (int index=0;index<titleBarPieces;++index) {
        plain(7); bitmap(23,titleBarStart+titleBarPitch*index,titleBarY); plain(4);
    }
    plain(10); //560B..5623.
    append(uiMirroredBitmapOperations(art[14],{titleBarStart+titleBarPitch*titleBarPieces,titleBarY},display,backPage));
    plain(6); //5628..5635 CALL2FA6.
    plain(8);
    const auto titleWidth=width(titleFont,UiFontLayout::fixed,"C R E D I T S");
    const auto titleX=centerX-titleWidth/2;
    plain(11); text(titleFont,UiFontLayout::fixed,"C R E D I T S",titleX,titleY,colors.panel.dark);
    plain(10); text(titleFont,UiFontLayout::fixed,"C R E D I T S",titleX+1,titleY+1,colors.fill);
    plain(6); //2FED..2FF5.
    plain(6); //5638..5640.
    plain(3); //B17D..B184 native font1 selection.
    plain(13);
    append(uiFillOperations({21,33,276,156},colors.fill,backPage));
    plain(10);
    append(uiBevelOperations({21,33,276,156},colors.panel,backPage));
    plain(5); //B1C0..B1CF.
    int y=40;
    for (const auto& row:rows) {
        plain(5); shadow(row.first,labelX,y);
        plain(4);
        if (!row.first.empty()) {
            plain(3);
            if (!row.second.empty()) {
                plain(3);
                const auto labelWidth=width(scoreFont,UiFontLayout::proportional,row.first);
                plain(4);
                if (labelX+labelWidth<leaderLimit) {
                    plain(4);
                    for (int x=labelX+labelWidth;x<leaderLimit;x+=2) {
                        plain(7); shadow(".",x,y); plain(7);
                        ++result.leaderDots;
                    }
                    plain(2);
                }
            }
        }
        plain(5); shadow(row.second,nameX,y); plain(5);
        y+=rowPitch;
    }
    plain(4); //B24C..B256.
    y+=4;
    for (const auto value:footer) {
        plain(2);
        const auto valueWidth=width(scoreFont,UiFontLayout::proportional,value);
        plain(8); shadow(value,centerX-valueWidth/2,y); plain(6);
        y+=footerPitch;
    }
    return result;
}

} // namespace stellar7
