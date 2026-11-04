#include "stellar7/level2_loader_pages_work.hpp"
#include "stellar7/indexed_render.hpp"
#include "stellar7/ui_text_work.hpp"
#include "stellar7/ui_fill_work.hpp"
#include <stdexcept>
namespace stellar7 {
Level1LoaderPages planLevel2LoaderPages(const GameplayAssets& assets,const Level2LoaderPageInput& input,
    const CockpitPageState& incomingPages,const IndexedPages& incomingPixels,const RadarState& incomingRadar) {
    constexpr std::size_t panelGroup = 3;
    constexpr std::size_t overlayGroup = 4;
    constexpr Viewport display{0,319,0,199};
    constexpr std::uint8_t titleColor = 239;
    const auto& record=input.cockpit;
    const bool displayRoles=(incomingPages.frontPage()==0xa000&&incomingPages.backPage()==0xa800)
        ||(incomingPages.frontPage()==0xa800&&incomingPages.backPage()==0xa000);
    const bool sourceCut=input.player.type==0 && input.player.stateBytes4dTo52[1]==1
        && input.player.position==FixedPosition{} && input.player.pitch==0 && input.player.roll==0 && input.player.yaw==0
        && input.player.xVelocity==0 && input.player.yVelocity==0 && input.player.zVelocity==0
        && input.player.forwardVelocity==0 && input.player.pitchVelocity==0 && input.player.rollVelocity==0
        && input.player.yawVelocity==0 && input.armor>=80 && input.armor<=680;
    if (!assets.cockpitSettings || assets.artGroups.size() <= overlayGroup
        || assets.artGroups[panelGroup].size() < 3 || assets.artGroups[overlayGroup].size() < 7
        || !sourceCut || record.levelIndex != 1 || record.displaySteps != 2
        || incomingPages.cachePage() != 0xa400 || !displayRoles) {
        throw std::invalid_argument("Level2 loader pages require the retained destination resource/profile cut");
    }
    Level1LoaderPages plan{incomingPages,incomingPixels,incomingRadar,{},{},{}};
    plan.radar.resetArmorStrip(); //50C4 initializes the empty-list strip on the retained radar owner.
    auto& bitmaps = plan.bitmaps;
    bitmaps.strip = assets.artGroups[panelGroup][1];
    bitmaps.matchingPageBackground = assets.artGroups[panelGroup][2];
    bitmaps.indicator = assets.artGroups[overlayGroup][6];
    for (std::size_t index = 0; index < bitmaps.overlays.size(); ++index) {
        bitmaps.overlays[index] = assets.artGroups[overlayGroup][index];
    }
    for (std::size_t index = 0; index < plan.cockpit.size(); ++index) {
        const auto state = record.states[index];
        plan.cockpit[index] = cockpitOperations({state.x,state.velocity,assets.cockpitSettings->viewSize,
            index == 0 ? CockpitPageRelation::matching : CockpitPageRelation::differing},bitmaps);
        if (plan.cockpit[index].state.indicatorX != record.states[index+1].x
            || plan.cockpit[index].state.indicatorVelocity != record.states[index+1].velocity) {
            throw std::logic_error("Level1 loader cockpit differs from its canonical provider record");
        }
    }
    const auto append = [&](const auto& operations) {
        plan.operations.insert(plan.operations.end(),operations.begin(),operations.end());
    };
    const auto plain = [&](const std::uint64_t count) { plan.operations.push_back({NormalWorkKind::instructions,count}); };
    const auto fullCopy = [&](const std::uint16_t source, const std::uint16_t destination) {
        plain(5 + 8 + 1); //50D3/5104 caller;8484..8490;22A1 JMPF.
        append(planarCopyOperations(0,320,200));
        plain(2); //8495 ADD SP,8 and8498 RET follow the resident RETF.
        plan.pixels.apply(std::vector<IndexedPageEffect>{RadarPageCopy{source,destination,{0,0},320,200}});
    };
    plan.pixels.apply(cockpitResourcePageEffects(assets.cockpit,plan.pages.cachePage(),CockpitPageLayout::recoveredStartup));
    plain(4); //50C4..50CA.
    plan.pages.selectCockpitDraw();
    plan.pages.selectCockpitCopy();
    append(plan.cockpit[0].operations);
    plan.pixels.apply(cockpitPageEffects(plan.cockpit[0],bitmaps,plan.pages.cachePage(),plan.pages.drawPage()));
    plain(1 + 11); //50D0/7936 through width CALLF.
    const auto width = uiTextWidthOperations(assets.scoreFont,UiFontLayout::proportional,"ANTARES");
    append(width.operations);
    plain(7);
    const auto title = uiTextOperations(assets.scoreFont,UiFontLayout::proportional,"ANTARES",
        {static_cast<std::int16_t>(160-width.width/2),30},titleColor,plan.pages.cachePage(),display);
    append(title.operations);
    plan.pixels.apply(title.effects);
    plain(3);
    plan.pages.beginFrame();
    fullCopy(plan.pages.cachePage(),plan.pages.backPage());
    plain(11); //50E2..50F7.
    const auto clear = uiFillOperations({24,24,272,122},0,plan.pages.backPage());
    append(clear.operations);
    plan.pixels.apply(clear.effects);
    plain(1); //50FC.
    append(plan.cockpit[1].operations);
    plan.pixels.apply(cockpitPageEffects(plan.cockpit[1],bitmaps,plan.pages.cachePage(),plan.pages.backPage()));
    const auto& player=input.player;
    const auto radar = plan.radar.initializeLevel1(input.armor,player.position,player.yaw,
        {plan.pages.cachePage(),plan.pages.backPage(),plan.pages.frontPage(),plan.pages.drawPage(),plan.pages.copyPage()});
    append(radar.operations);
    plan.pixels.apply(radar);
    plan.pages.selectArmorCopy();
    fullCopy(plan.pages.backPage(),plan.pages.frontPage());
    plain(5); //5113..512D skips optional upload and returns.
    return plan;
}


} //namespace stellar7
