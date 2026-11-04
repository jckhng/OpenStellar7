#include "stellar7/continue_work.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace {
template<class Function> void rejects(Function function) {
    bool rejected = false;
    try { function(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

std::uint64_t ordinary(const stellar7::UiBitmapWork& work) {
    std::uint64_t result = 0;
    for (const auto& op : work.operations) {
        if (op.kind == stellar7::NormalWorkKind::instructions) { result += op.count; }
    }
    return result;
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace stellar7;
    std::vector<IndexedBitmap> art(23);
    const auto bitmap = [&](const std::size_t index, const std::uint16_t width, const std::uint16_t height) {
        art[index] = {width,height,std::vector<std::uint8_t>(width*height)};
        for (std::size_t i = 0; i < art[index].pixels.size(); ++i) {
            art[index].pixels[i] = static_cast<std::uint8_t>((i*37+i/width*13)%256);
        }
    };
    bitmap(0,16,20); bitmap(11,8,7); bitmap(12,40,40); bitmap(17,120,33);
    bitmap(19,120,33); bitmap(20,264,2); bitmap(22,88,11);
    BitmapFont font{32,6,0,std::vector<IndexedBitmap>(64,{8,6,std::vector<std::uint8_t>(48)})};
    const auto normal = uiBitmapOperations(art[22],{120,111},{0,319,0,199},0xa800);
    const auto vertical = continueVerticalBitmapOperations(art[22],{120,111},{0,319,0,199},0xa800);
    assert(ordinary(vertical) == ordinary(normal)+5+4); //12D2..12DF and one extra source pitch instruction per plane.
    for (const auto& effect : vertical.effects) {
        const auto& pixel = std::get<RadarPixelWrite>(effect);
        assert(pixel.color == art[22].pixels[(10-(pixel.point.y-111))*88+pixel.point.x-120]);
    }
    rejects([&] { continueVerticalBitmapOperations(art[22],{-1,111},{0,319,0,199},0xa800); });

    IndexedPages pages;
    for (const auto page : {0xa000,0xa400,0xa800}) {
        for (int y = 0; y < 200; ++y) {
            for (int x = 0; x < 320; ++x) {
                pages.write(static_cast<std::uint16_t>(page),{static_cast<std::int16_t>(x),static_cast<std::int16_t>(y)},
                    static_cast<std::uint8_t>((x*29+y*17)^(x>>2)^(page>>4)));
            }
        }
    }
    ContinueState input{};
    input.scratch.assign(500,91);
    input.transfer.assign(1024,37);
    input.pointer = {163,69};
    input.oldRectangle = {163,69,16,21};
    input.frontPage = 0xa000;
    input.backPage = 0xa800;
    input.fadeTicks = 8;
    const auto entered = continueEntryOperations(input,pages,art,font,1);
    assert(entered.state.stage == ContinueStage::polling && entered.state.selected == 1 && entered.state.centered == 1);
    assert(entered.state.pointer.x == 161 && entered.state.pointer.y == 118 && !entered.state.navigationArmed);
    assert(entered.state.oldRectangle.x == 163 && entered.state.oldRectangle.y == 69);
    assert(entered.state.transfer == input.transfer);
    assert(std::equal(input.scratch.begin()+100,input.scratch.end(),entered.state.scratch.begin()+100));
    assert(std::count_if(entered.steps.begin(),entered.steps.end(),[](const auto& step) { return step.palette.has_value(); }) == 1);
    const auto entryNoFade = [&]() { auto state = input; state.fadeTicks = 0; return continueEntryOperations(state,pages,art,font,1); }();
    assert(std::none_of(entryNoFade.steps.begin(),entryNoFade.steps.end(),[](const auto& step) { return step.palette.has_value(); }));
    assert(continueEntryOperations(input,pages,art,font,0).outcome == ContinueOutcome::unavailable);

    const auto run = [&](const ContinueWork& before, const ContinueKeyboardPacket packet = {}) {
        return continuePollOperations(before.state,before.pages,art,font,packet);
    };
    ContinueKeyboardPacket up{};
    up.directions[4] = true;
    const auto unarmed = run(entered,up);
    assert(unarmed.state.selected == 1);
    const auto neutral = run(unarmed);
    assert(neutral.state.navigationArmed);
    const auto yes = run(neutral,up);
    assert(yes.state.selected == 0 && yes.state.pointer.y == 74);
    assert(run(yes,up).state.selected == 0);
    ContinueKeyboardPacket press{};
    press.enter = true;
    const auto noPressed = run(neutral,press);
    assert(noPressed.state.pressed == 2 && noPressed.state.activeWidget == 2);
    const auto noHeld = run(noPressed,press);
    assert(noHeld.state.pressed == 2 && noHeld.outcome == ContinueOutcome::pending);
    const auto no = run(noHeld);
    assert(no.outcome == ContinueOutcome::no && no.state.pressed == 0 && no.state.stage == ContinueStage::finished);
    const auto yesPressed = run(yes,press);
    assert(yesPressed.state.pressed == 1);
    assert(run(yesPressed).outcome == ContinueOutcome::yes);

    auto departPacket = press;
    departPacket.directions[4] = true;
    const auto departed = run(noPressed,departPacket);
    assert(departed.state.selected == 0 && departed.state.pressed == 0 && departed.outcome == ContinueOutcome::pending);
    ContinueKeyboardPacket escape{};
    escape.peekScan = 1;
    assert(run(yesPressed,escape).outcome == ContinueOutcome::yes);
    assert(run(noPressed,escape).outcome == ContinueOutcome::no);
    const auto cancelling = run(yes,escape);
    assert(cancelling.state.stage == ContinueStage::cancelCleanup && cancelling.outcome == ContinueOutcome::pending);
    assert(cancelling.state.centered == 1 && cancelling.state.selected == 0); //5C14 changes418C, not418A.
    const auto cancelled = run(cancelling);
    assert(cancelled.outcome == ContinueOutcome::cancelled && cancelled.state.stage == ContinueStage::finished);
    assert(std::equal(input.transfer.begin()+336,input.transfer.end(),cancelled.state.transfer.begin()+336));
    for (int y = 0; y < 200; ++y) {
        for (int x = 0; x < 320; ++x) {
            const ProjectedPoint point{static_cast<std::int16_t>(x),static_cast<std::int16_t>(y)};
            assert(cancelled.pages.read(0xa400,point) == pages.read(0xa400,point));
        }
    }
    rejects([&] { run(cancelled); });
    rejects([&] { continueEntryOperations(entered.state,entered.pages,art,font,1); });
    auto bad = input; bad.ability = 1;
    rejects([&] { continueEntryOperations(bad,pages,art,font,1); });
    bad = input; bad.joystickEnabled = true;
    rejects([&] { continueEntryOperations(bad,pages,art,font,1); });
    auto tab = ContinueKeyboardPacket{}; tab.peekScan = 0x0f;
    rejects([&] { run(entered,tab); });
}
