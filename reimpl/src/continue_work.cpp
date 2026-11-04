#include "stellar7/continue_work.hpp"
#include "stellar7/ui_fill_work.hpp"
#include "stellar7/ui_pointer_work.hpp"
#include "stellar7/ui_text_work.hpp"

#include <algorithm>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr Viewport display{0,319,0,199};
constexpr UiPanelColors colors{{147,141,135,128,135,141,147},147,135,128};
constexpr std::size_t recordBytes = 50;
constexpr std::array<std::array<std::uint16_t,25>,2> records{{
    {{21,6,81,11,0,0,116,30,20,5,22,0,60,7,14,14,99,14,1,0,0,0,0,0,0}},
    {{21,14,81,11,0,0,117,33,20,12,22,1,60,15,14,9,99,9,1,0,0,0,0,0,0}}
}};
constexpr std::array<std::uint8_t,8> directionScans{0x4f,0x50,0x51,0x47,0x48,0x49,0x4b,0x4d};
constexpr std::array<std::uint16_t,8> directionMasks{6,2,10,5,1,9,4,8};

std::array<std::uint16_t,25> sourceRecord(const std::size_t index) {
    auto words = records[index];
    const auto y = index == 0 ? 63 : 99;
    for (const auto offset : {0,4,8,12,14,16}) {
        words[offset] += 100;
        words[offset+1] += y;
    }
    words[19] = static_cast<std::uint16_t>(index == 0 ? 17 : 19);
    words[20] = static_cast<std::uint16_t>(index == 0 ? 0x16a5 : 0x16a2);
    return words;
}

void plain(ContinueWork& work, const std::uint64_t count) {
    if (work.steps.empty() || work.steps.back().palette) { work.steps.push_back({}); }
    auto& ops = work.steps.back().operations;
    if (!ops.empty() && ops.back().kind == NormalWorkKind::instructions) {
        ops.back().count += count;
    } else {
        ops.push_back({NormalWorkKind::instructions,count});
    }
}

template<class Work> void append(ContinueWork& work, const Work& child) {
    if (work.steps.empty() || work.steps.back().palette) { work.steps.push_back({}); }
    auto& step = work.steps.back();
    step.operations.insert(step.operations.end(),child.operations.begin(),child.operations.end());
    step.effects.insert(step.effects.end(),child.effects.begin(),child.effects.end());
    work.pages.apply(child.effects);
}

void provider(ContinueWork& work, const ContinueProviderKind kind, const std::uint16_t selector,
    const std::uint16_t value, const ProjectedPoint point = {}) {
    //Split before the provider boundary so the receipt identifies its exact position.
    work.steps.push_back({});
    work.providers.push_back({kind,selector,value,point,work.steps.size()-1,false});
}

void validate(const ContinueState& state) {
    const auto physical = [](const auto page) { return page == 0xa000 || page == 0xa400 || page == 0xa800; };
    const auto& old = state.oldRectangle;
    if (state.scratch.size() != 500 || state.transfer.size() < 16*21
        || !physical(state.frontPage) || !physical(state.backPage) || state.frontPage == state.backPage
        || old.x < 0 || old.y < 0 || old.width != 16 || old.height == 0 || old.height > 21
        || old.x+old.width > 320 || old.y+old.height > 200 || state.fadeTicks > 8 || state.ability != 0
        || state.joystickEnabled || state.overlayEnabled || state.widgetCallback != 0x5ac8
        || state.selected > 1 || state.centered > 1 || state.pressed > 2) {
        throw std::invalid_argument("Continue requires retained UI storage, physical pages and keyboard-only ordinary modal state");
    }
    if (state.stage != ContinueStage::inactive) {
        for (std::size_t index = 0; index < 2; ++index) {
            auto expected = sourceRecord(index);
            expected[21] = expected[22] = static_cast<std::uint16_t>(1-index);
            expected[23] = expected[24] = static_cast<std::uint16_t>(index);
            for (std::size_t n = 0; n < expected.size(); ++n) {
                const auto offset = index*recordBytes+n*2;
                const auto actual = state.scratch[offset] | (std::uint16_t{state.scratch[offset+1]} << 8);
                if (actual != expected[n]) { throw std::invalid_argument("Continue poll requires its retained source YES/NO records"); }
            }
        }
    }
}

std::uint16_t word(const ContinueState& state, const std::size_t index, const std::size_t offset) {
    const auto base = index*recordBytes+offset;
    return static_cast<std::uint16_t>(state.scratch[base] | (std::uint16_t{state.scratch[base+1]} << 8));
}

void store(ContinueState& state, const std::size_t index, const std::size_t offset, const std::uint16_t value) {
    const auto base = index*recordBytes+offset;
    state.scratch[base] = static_cast<std::uint8_t>(value);
    state.scratch[base+1] = static_cast<std::uint8_t>(value >> 8);
}

void record(ContinueWork& work, const std::size_t index) {
    const auto words = sourceRecord(index);
    for (std::size_t n = 0; n < words.size(); ++n) { store(work.state,index,n*2,words[n]); }
}

void copy(ContinueWork& work, const CockpitCopyRegion rectangle) {
    plain(work,1); //22A1 resident JMPF; CALLF belongs to the caller.
    const auto body = planarCopyOperations(rectangle.x,rectangle.width,rectangle.height);
    const auto width = (rectangle.width+2*(rectangle.x%4)+3)/4*4;
    UiBitmapWork child{body,{RadarPageCopy{work.state.backPage,work.state.frontPage,
        {static_cast<std::int16_t>(rectangle.x/4*4),rectangle.y},static_cast<std::uint16_t>(width),
        static_cast<std::uint16_t>(rectangle.height)}}};
    append(work,child);
}

void center(ContinueWork& work, const std::uint16_t selected) {
    plain(work, 6); /*5C14..5C1E.*/
    plain(work, 2); /*5C28..5C2B.*/
    plain(work, 2); /*5C32..5C36.*/
    plain(work, 2); /*5C40..5C42.*/
    plain(work, 15); /*5C46..5C68.*/
    work.state.centered = selected;
    work.state.pointer = {static_cast<std::int16_t>(word(work.state,selected,0)+word(work.state,selected,4)/2),
        static_cast<std::int16_t>(word(work.state,selected,2)+word(work.state,selected,6)/2)};
    plain(work, 8); /*5B9E..5BAF.*/
    plain(work, 2); /*5BBA..5BC0.*/
    plain(work, 2); /*5BC8..5BCD.*/
    plain(work, 2); /*5BD8..5BDE.*/
    plain(work, 2); /*5BE6..5BEB.*/
    if (work.state.mouseInstalled) {
        plain(work, 3); /*5BED..5BF5.*/
        provider(work,ContinueProviderKind::setMousePosition,0,0,work.state.pointer);
    }
    plain(work, 3); /*5BFA..5BFD.*/
    plain(work, 5); /*5C6B..5C72.*/
}

void navigate(ContinueWork& work, const std::uint16_t mask) {
    plain(work, 7); /*5230..523A.*/
    if (mask == 0) { plain(work, 1); /*523C..523C.*/ work.state.selected = 0; }
    plain(work, 6); /*5240..5250.*/
    if ((mask & 1) != 0) {
        plain(work, 2); /*5252..5255.*/
        work.state.selected = word(work.state,work.state.selected,0x2a);
    } else {
        plain(work, 2); /*5258..525B.*/
        if ((mask & 2) != 0) {
            plain(work, 2); /*525D..5260.*/
            work.state.selected = word(work.state,work.state.selected,0x2c);
        } else {
            plain(work, 2); /*5262..5265.*/
            if ((mask & 4) != 0) {
                plain(work, 2); /*5267..526A.*/
                work.state.selected = word(work.state,work.state.selected,0x2e);
            } else {
                plain(work, 2); /*526C..526F.*/
                if ((mask & 8) != 0) {
                    plain(work, 1); /*5271..5271.*/
                    work.state.selected = word(work.state,work.state.selected,0x30);
                }
            }
        }
    }
    if (mask != 0) { plain(work, 1); /*5274..5274.*/ }
    plain(work, 6); /*5277..5288.*/
    plain(work, 2); /*5294..5298.*/
    center(work,work.state.selected);
    plain(work, 7); /*529B..52A9.*/
    work.state.navigationArmed = false;
}

void pointer(ContinueWork& work, const std::vector<IndexedBitmap>& art) {
    const CockpitCopyRegion now{work.state.pointer.x,work.state.pointer.y,16,21};
    plain(work,20); //82EA..8324; current pointer is derived from a full-height widget.
    const auto saved = uiPointerSaveOperations(work.pages,now,work.state.backPage,work.state.transfer);
    if (work.steps.empty() || work.steps.back().palette) { work.steps.push_back({}); }
    auto& step = work.steps.back();
    step.operations.insert(step.operations.end(),saved.operations.begin(),saved.operations.end());
    work.state.transfer = saved.transfer;
    plain(work,7); //8329..833B.
    append(work,uiBitmapOperations(art.at(0),work.state.pointer,display,work.state.backPage));
    plain(work,8); //8340..8356; old copy is deliberate even after full entry copy.
    copy(work,work.state.oldRectangle);
    plain(work,6); //835B..8368; current rectangle is deliberately copied again.
    copy(work,now);
    plain(work,10); //836D..8388.
    append(work,uiPointerRestoreOperations(now,work.state.backPage,work.state.transfer));
    plain(work,12); //838D..83B6, excluding source overflow arm.
    const auto oldY = work.state.oldRectangle.y;
    work.state.oldRectangle = now;
    if (oldY+now.height > 199) {
        plain(work,3); //839E..83A5, based on previous oldY before storing currentY.
        work.state.oldRectangle.height = static_cast<std::uint16_t>(200-oldY);
    }
}

std::uint16_t hit(ContinueWork& work) {
    plain(work, 5); /*5A7C..5A85.*/
    for (std::size_t index = 0; index < 2; ++index) {
        plain(work, 4); /*5A89..5A90.*/
        const auto x = work.state.pointer.x;
        const auto y = work.state.pointer.y;
        const auto left = word(work.state,index,0);
        if (x >= left) {
            plain(work, 3); /*5A92..5A98.*/
            if (y >= word(work.state,index,2)) {
                plain(work, 4); /*5A9A..5AA4.*/
                if (y <= word(work.state,index,2)+word(work.state,index,6)) {
                    plain(work, 4); /*5AA6..5AAF.*/
                    if (x <= left+word(work.state,index,4)) {
                        plain(work, 4); /*5ABE..5AC2.*/
                        return static_cast<std::uint16_t>(index+1);
                    }
                }
            }
        }
        plain(work, 4); /*5AB1..5AB9.*/
        if (index == 1) { plain(work, 1); /*5ABB..5ABB.*/ }
    }
    plain(work, 3); /*5AC4..5AC7.*/
    return 0;
}

std::uint16_t callback(ContinueWork& work, const std::vector<IndexedBitmap>& art, const BitmapFont& font) {
    plain(work, 6); /*5AC8..5AD7.*/
    if (work.state.button != 0) {
        plain(work, 2); /*5AD9..5ADE.*/
        if (work.state.pressed == 0) {
            plain(work, 1); /*5AE0..5AE0.*/
            const auto active = hit(work);
            plain(work, 3); /*5AE3..5AE8.*/
            work.state.pressed = active;
            if (active != 0) {
                plain(work, 6); /*5AEA..5AF4.*/
                work.state.activeWidget = active;
                append(work,continueWidgetOperations(art,font,work.state.scratch,active-1,MenuWidgetState::pressed,work.state.backPage));
                plain(work, 5); /*5AF7..5AFF.*/
                append(work,menuWidgetCopyOperations(work.state.scratch,active-1,work.state.backPage,work.state.frontPage));
                plain(work, 2); /*5B02..5B05.*/
            }
            plain(work, 3); /*5B58..5B5B.*/
            return 0;
        }
    }
    plain(work, 2); /*5B08..5B0D.*/
    bool release = false;
    if (work.state.pressed != 0) {
        plain(work, 2); /*5B0F..5B14.*/
        release = work.state.button == 0;
    }
    if (!release) {
        plain(work, 2); /*5B16..5B1B.*/
        if (work.state.button == 0) { plain(work, 3); /*5B58..5B5B.*/ return 0; }
        plain(work, 1); /*5B1D..5B1D.*/
        const auto hovered = hit(work);
        plain(work, 2); /*5B20..5B24.*/
        if (hovered == work.state.pressed) { plain(work, 3); /*5B58..5B5B.*/ return 0; }
    }
    const auto active = work.state.pressed;
    plain(work, 7); /*5B26..5B31.*/
    work.state.activeWidget = 0;
    append(work,continueWidgetOperations(art,font,work.state.scratch,active-1,MenuWidgetState::released,work.state.backPage));
    plain(work, 5); /*5B34..5B3C.*/
    append(work,menuWidgetCopyOperations(work.state.scratch,active-1,work.state.backPage,work.state.frontPage));
    plain(work, 5); /*5B3F..5B51.*/
    work.state.pressed = 0;
    if (work.state.button == 0) { plain(work, 3); /*5B53..5B56.*/ return active; }
    plain(work, 3); /*5B58..5B5B.*/
    return 0;
}

std::uint16_t poll(ContinueWork& work, const std::vector<IndexedBitmap>& art, const BitmapFont& font,
    const ContinueKeyboardPacket& packet) {
    if (packet.peekScan == 0x0f) { throw std::invalid_argument("Continue Tab focus family is outside keyboard-only navigation"); }
    plain(work, 5); /*5936..593E.*/
    provider(work,ContinueProviderKind::keyboardPeek,0,packet.peekScan);
    plain(work, 7); /*5943..5951.*/
    provider(work,ContinueProviderKind::heldKey,0x1c,packet.enter);
    plain(work, 3); /*5956..595B.*/
    work.state.keyboardScan = packet.peekScan;
    work.state.button = packet.enter ? 1 : 0;
    if (!packet.enter) {
        plain(work, 2); /*595D..5961.*/
        if (work.state.spaceEnabled) {
            plain(work, 3); /*5963..5967.*/
            provider(work,ContinueProviderKind::heldKey,0x39,packet.space);
            plain(work, 3); /*596C..5971.*/
            work.state.button = packet.space ? 1 : 0;
        }
    }
    if (work.state.button != 0) { plain(work, 1); /*5973..5973.*/ }
    plain(work, 3); /*5979..597C.*/
    plain(work, 2); /*8BD0..8BD1.*/
    std::uint16_t direction = 0;
    for (std::size_t index = 0; index < directionScans.size(); ++index) {
        plain(work,3); //8BD0 family: MOV scan/PUSH/CALLF0437.
        provider(work,ContinueProviderKind::heldKey,directionScans[index],packet.directions[index]);
        plain(work,3); //cleanup/OR/JZ.
        if (packet.directions[index]) { plain(work,1); direction |= directionMasks[index]; }
    }
    plain(work,3); //8C70..8C73: MOV AX,SI/POP SI/RET.
    plain(work, 4); /*597F..5989.*/
    plain(work, 1); /*59C3..59C3.*/
    plain(work,6); //64A0 non-Tab through64ED; tabArmed becomes1.
    work.state.tabArmed = true;
    plain(work, 2); /*59C6..59C8.*/
    bool navigated = false;
    if (direction != 0) {
        plain(work, 2); /*59CA..59CE.*/
        if (work.state.navigationArmed) {
            plain(work, 2); /*59D0..59D1.*/
            navigate(work,direction);
            plain(work, 2); /*59D4..59D7.*/
            navigated = true;
        }
    }
    if (!navigated) {
        plain(work, 2); /*59DA..59DC.*/
        if (direction == 0) { plain(work, 1); /*59DE..59DE.*/ work.state.navigationArmed = true; }
    }
    plain(work, 2); /*59E4..59E9.*/
    if (work.state.mouseInstalled) {
        plain(work, 5); /*59EB..59F3.*/
        provider(work,ContinueProviderKind::mousePosition,0,0,work.state.pointer);
        plain(work, 3); /*59F8..59FF.*/
        if (work.state.button == 0) {
            plain(work, 3); /*5A01..5A04.*/
            provider(work,ContinueProviderKind::mouseButton,0,0);
            plain(work, 6); /*5A09..5A14.*/
            provider(work,ContinueProviderKind::mouseButton,1,0);
            plain(work, 5); /*5A19..5A23.*/
        }
    }
    plain(work, 3); /*5A2C..5A32.*/
    const auto selected = callback(work,art,font);
    plain(work, 2); /*5A36..5A38.*/
    pointer(work,art);
    plain(work, 4); /*5A3B..5A46.*/
    if (packet.peekScan != 0) {
        plain(work, 2); /*5A48..5A4A.*/
        if (selected == 0) {
            plain(work, 2); /*5A4C..5A4E.*/
            if (direction == 0) {
                plain(work, 2); /*5A50..5A52.*/
                if (work.state.button == 0) {
                    plain(work, 3); /*5A54..5A5C.*/
                    plain(work, 12); /*5E9C..5EAF.*/
                    plain(work,1); //5EB0 RET on modal guard.
                    plain(work, 3); /*5A5F..5A64.*/
                    plain(work, 1); /*5A69..5A69.*/
                }
            }
        }
    }
    plain(work, 2); /*5A6D..5A72.*/
    plain(work, 4); /*5A77..5A7B.*/
    return selected;
}

void finish(ContinueWork& work, const std::uint16_t selected) {
    plain(work, 6); /*87C2..87C9.*/
    plain(work, 6); /*8578..8585.*/
    plain(work, 3); /*A112..A116.*/
    if (selected == 1) { plain(work, 2); /*A118..A11B.*/ }
    else { plain(work, 2); /*A11C..A11E.*/ }
    plain(work, 2); /*0408..040A.*/
    work.state.stage = ContinueStage::finished;
    work.outcome = selected == 1 ? ContinueOutcome::yes : selected == 2 ? ContinueOutcome::no : ContinueOutcome::cancelled;
}
} // namespace

ContinueWork continueEntryOperations(const ContinueState& state, const IndexedPages& pages,
    const std::vector<IndexedBitmap>& art, const BitmapFont& font, const std::uint16_t remainingContinues) {
    if (state.stage != ContinueStage::inactive) { throw std::invalid_argument("Continue entry requires an inactive modal"); }
    validate(state);
    if (state.pressed != 0 || art.size() <= 22 || art[0].width != 16 || art[0].height != 20) {
        throw std::invalid_argument("Continue entry requires released prior widget and original UI resources");
    }
    ContinueWork work{state,pages,{},{}};
    plain(work, 2); /*0401..0403.*/
    if (remainingContinues == 0) {
        work.state.stage = ContinueStage::finished;
        work.outcome = ContinueOutcome::unavailable;
        return work;
    }
    plain(work, 1); /*0405..0405.*/
    plain(work, 4); /*A0EE..A0F7.*/
    plain(work,12); //2CF4..2D00; full backdrop includes5BFE.
    for (int y = 0, x = 0; y < 200; y += 2, x += 3) {
        const auto bitmap = [&](const int offset) {
            append(work,uiBitmapOperations(art[20],{static_cast<std::int16_t>(x+offset),static_cast<std::int16_t>(y)},display,state.backPage));
        };
        plain(work,2);
        if (y > 174) { plain(work,8); bitmap(-524); plain(work,1); }
        plain(work,8); bitmap(-262); plain(work,1);
        plain(work,2);
        if (x < 320) { plain(work,7); bitmap(0); plain(work,1); }
        plain(work,2);
        if (x < 58) { plain(work,8); bitmap(262); plain(work,1); }
        plain(work,5);
    }
    plain(work,3); //2D7A..2D7C.
    plain(work, 3); /*A0FA..A100.*/
    plain(work, 8); /*8484..8490.*/
    copy(work,{0,0,320,200});
    plain(work, 2); /*8495..8498.*/
    plain(work, 7); /*A103..A10F.*/
    plain(work, 6); /*853A..8543.*/
    plain(work, 6); /*8556..8568.*/
    plain(work, 9); /*8586..859C.*/
    plain(work, 2); /*85A4..85A7.*/
    plain(work, 6); /*85BB..85CB.*/
    plain(work, 2); /*85D5..85D9.*/
    plain(work, 13); /*85F4..861D.*/
    plain(work, 1); /*862E..862E.*/
    plain(work, 2); /*7D30..7D35.*/
    if (state.fadeTicks != 0) {
        plain(work, 2); /*7D37..7D3C.*/
        plain(work, 9); /*7D4C..7D5B.*/
        plain(work,11); //Resident1B38 prefix through CALLF077A; no extra alias stub.
        work.steps.push_back({{},{},PaletteRequest{0,256,238,63}});
        plain(work,3); //Resident1B38 epilogue.
        plain(work, 1); /*7D60..7D60.*/
    }
    plain(work, 1); /*7D63..7D63.*/
    plain(work, 14); /*8631..864D.*/
    append(work,uiPanelOperations(art[12],{64,30,193,110},colors,state.backPage));
    plain(work, 6); /*8650..8662.*/
    provider(work,ContinueProviderKind::fontSelection,0,0);
    plain(work, 13); /*8667..8683.*/
    append(work,uiFillOperations({78,40,165,16},156,state.backPage));
    plain(work, 10); /*8688..869B.*/
    append(work,uiBevelOperations({78,40,165,16},colors,state.backPage));
    plain(work, 7); /*869E..86AE.*/
    const auto width = uiTextWidthOperations(font,UiFontLayout::fixed,"CONTINUE?");
    if (work.steps.empty() || work.steps.back().palette) { work.steps.push_back({}); }
    work.steps.back().operations.insert(work.steps.back().operations.end(),width.operations.begin(),width.operations.end());
    plain(work, 7); /*86B3..86C1.*/
    append(work,uiTextOperations(font,UiFontLayout::fixed,"CONTINUE?",
        {static_cast<std::int16_t>(161-width.width/2),45},128,state.backPage,display));
    plain(work, 4); /*86C6..86D0.*/
    plain(work,2); //5718 reset/RET occurs at86C9, before86CC test; no intervening child.
    for (std::size_t index = 0; index < 2; ++index) {
        if (index == 0) { plain(work, 7); /*86D2..86E0.*/ }
        else { plain(work, 9); /*86F3..8707.*/ }
        plain(work,19); //5720..574A.
        work.steps.back().operations.push_back({NormalWorkKind::repeatString,25});
        record(work,index);
        plain(work,57); //574F..5800; both side origins present, including583A CALL.
        append(work,continueWidgetOperations(art,font,work.state.scratch,index,MenuWidgetState::released,state.backPage));
        plain(work,7); //5803..580F.
        if (index == 0) { plain(work, 8); /*86E3..86ED.*/ }
        else { plain(work, 14); /*870A..872A.*/ }
        plain(work,16); //5810..5838; exact caller links.
        store(work.state,index,0x2a,static_cast<std::uint16_t>(1-index));
        store(work.state,index,0x2c,static_cast<std::uint16_t>(1-index));
        store(work.state,index,0x2e,static_cast<std::uint16_t>(index));
        store(work.state,index,0x30,static_cast<std::uint16_t>(index));
        plain(work,1); //86F0/872D cleanup.
    }
    plain(work, 11); /*8730..8746.*/
    copy(work,{59,29,209,114});
    plain(work, 6); /*874B..8753.*/
    plain(work, 5); /*856B..8575.*/
    plain(work, 10); /*8754..8764.*/
    center(work,1);
    plain(work, 4); /*8767..876D.*/
    navigate(work,0);
    plain(work, 4); /*8770..8777.*/
    navigate(work,0xffff);
    plain(work, 3); /*877A..877F.*/
    work.state.stage = ContinueStage::polling;
    return work;
}

ContinueWork continuePollOperations(const ContinueState& state, const IndexedPages& pages,
    const std::vector<IndexedBitmap>& art, const BitmapFont& font, const ContinueKeyboardPacket& packet) {
    if (state.stage != ContinueStage::polling && state.stage != ContinueStage::cancelCleanup) {
        throw std::invalid_argument("Continue poll requires a live modal stage");
    }
    validate(state);
    ContinueWork work{state,pages,{},{}};
    plain(work,1); //8781 or87BF CALL5936; native provider supplies this packet.
    const auto selected = poll(work,art,font,packet);
    if (state.stage == ContinueStage::cancelCleanup) { finish(work,0); return work; }
    plain(work, 3); /*8784..8788.*/
    if (selected == 0) {
        plain(work, 2); /*878A..878F.*/
        if (packet.peekScan != 1) { plain(work, 1); /*8791..8791.*/ return work; }
    }
    plain(work, 2); /*87AC..87AE.*/
    if (selected != 0) { finish(work,selected); return work; }
    plain(work, 6); /*87B0..87B9.*/
    center(work,1);
    plain(work, 1); /*87BC..87BC.*/
    work.state.stage = ContinueStage::cancelCleanup;
    return work;
}

} // namespace stellar7
