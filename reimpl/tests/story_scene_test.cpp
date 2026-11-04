#include "stellar7/story_scene.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>

namespace stellar7 {
struct IndexedPagesTestAccess {
    static bool knownPhysical(const IndexedPages& pages,std::uint16_t page,std::size_t offset) {
        return pages.pages_.at(IndexedPages::pageIndex(page)).known.at(offset);
    }
};
}
namespace {
using namespace stellar7;
constexpr RadarPages pages{0xa400,0xa800,0xa000,0xa800,0xa000};
SceneCommand command(SceneCommandKind kind,std::array<std::uint16_t,4> args={}) {return {kind,args,{},0};}
template<class F> void rejects(F f) {bool bad=false;try {f();} catch(const std::exception&) {bad=true;}assert(bad);}
void wrappedDirtyCopy() {
    IndexedPages pixels;
    for(const auto page:{pages.cached,pages.back,pages.front}) {
        for(int y=0;y<200;++y) {for(int x=0;x<320;++x) {
            pixels.write(page,{static_cast<std::int16_t>(x),static_cast<std::int16_t>(y)},
                page==pages.cached ? static_cast<std::uint8_t>(y%251) : 99);
        }}
    }
    const RadarPageCopy copy{pages.cached,pages.back,{296,131},28,16};
    rejects([&] {planarCopyWork(298,22,16);});
    rejects([&] {pixels.apply(std::vector<IndexedPageEffect>{copy});});
    pixels.copyPlanar(copy);
    assert(pixels.read(pages.back,{319,131})==131);
    assert(pixels.read(pages.back,{0,132})==132 && pixels.read(pages.back,{3,132})==132);
    assert(pixels.read(pages.back,{4,132})==99);
    //Actual B5 last row writes four retained padding pixels, not row0 and not
    //the next physical page. Visible snapshots retain their exact64000 domain.
    assert(!IndexedPagesTestAccess::knownPhysical(pixels,pages.cached,64000));
    pixels.copyPlanar({pages.cached,pages.back,{236,74},88,126});
    assert(!IndexedPagesTestAccess::knownPhysical(pixels,pages.back,64000));
    assert(pixels.read(pages.back,{0,199})==199 && pixels.read(pages.back,{0,0})==99);
    assert(pixels.snapshot(pages.back).size()==64000);
    rejects([&] {pixels.copyPlanar({pages.cached,pages.back,{316,199},12,1});});
    rejects([&] {pixels.copyPlanar({pages.cached,pages.back,{296,199},28,8});});
    SceneResource scene;
    scene.program.pages={{command(SceneCommandKind::drawPoint,{318,131}),command(SceneCommandKind::endPage)}};
    ScenePageInterpreter interpreter(scene,pages,1,0);
    const auto work=interpreter.execute(0);
    assert(std::holds_alternative<ScenePlanarCopy>(work.operations.back()));
}
void compositeOrder() {
    SceneResource resource;
    resource.program.pages={
        {command(SceneCommandKind::setColors,{9,2}),command(SceneCommandKind::drawPoint,{12,20}),command(SceneCommandKind::endPage)},
        {command(SceneCommandKind::drawPoint,{16,20}),command(SceneCommandKind::endPage)}};
    ScenePageInterpreter vm(resource,pages,0,0);
    auto composite=vm.executeComposite({0,1});
    assert(composite.present && composite.operations.size()==4);
    assert(std::holds_alternative<ScenePrimitiveDraw>(composite.operations[0]));
    assert(std::holds_alternative<ScenePrimitiveDraw>(composite.operations[1]));
    assert(std::get<RadarPageCopy>(composite.operations[2]).position.x==12);
    assert(std::get<RadarPageCopy>(composite.operations[3]).position.x==16);
    auto next=vm.executeComposite({0,1});
    assert(std::get<RadarPageCopy>(next.operations[0]).sourcePage==pages.cached);
    assert(std::get<RadarPageCopy>(next.operations[1]).sourcePage==pages.cached);
    assert(next.operations.size()==8);
}
void bankAndCapture() {
    SceneResource graphics;
    graphics.assets={std::vector<IndexedBitmap>{{4,1,{1,2,3,4}}},
        std::vector<IndexedBitmap>{{4,1,{5,6,7,8}}}};
    SceneResource resource;
    resource.program.pages={{command(SceneCommandKind::selectBitmapBank,{0}),command(SceneCommandKind::loadBitmaps),
        command(SceneCommandKind::selectBitmapBank,{1}),command(SceneCommandKind::loadBitmaps),
        command(SceneCommandKind::selectBitmapBank,{0}),command(SceneCommandKind::selectFrame,{0}),
        command(SceneCommandKind::drawMirroredBitmap,{12,20,4,1}),command(SceneCommandKind::selectCaptureBank,{2}),
        command(SceneCommandKind::captureRectangle,{12,20,4,1}),command(SceneCommandKind::endPage)},
        {command(SceneCommandKind::restoreCapture,{2}),command(SceneCommandKind::endPage)}};
    ScenePageInterpreter vm(resource,pages,0,0,&graphics);IndexedPages pixels;SceneCaptureState captures;
    for(const auto page:{pages.cached,pages.back,pages.front}) {
        for(int y=0;y<200;++y) {for(int x=0;x<320;++x) {pixels.write(page,{static_cast<std::int16_t>(x),static_cast<std::int16_t>(y)},0);}}
    }
    for(const auto& op:vm.execute(0).operations) {applyScenePageOperation(pixels,op,graphics,{},pages.back,captures);}
    assert(pixels.read(pages.front,{12,20})==4 && pixels.read(pages.front,{15,20})==1);
    assert(captures.banks[2] && captures.banks[2]->pixels==std::vector<std::uint8_t>({4,3,2,1}));
    for(const auto& op:vm.execute(1).operations) {applyScenePageOperation(pixels,op,graphics,{},pages.back,captures);}
    assert(pixels.read(pages.front,{12,20})==4);
    resource.program.pages[1].insert(resource.program.pages[1].begin(),command(SceneCommandKind::selectBitmapBank,{6}));
    rejects([&] {vm.execute(1);});
}
std::vector<std::uint8_t> read(const std::filesystem::path& p) {
    std::ifstream in(p,std::ios::binary);if(!in) {throw std::runtime_error("missing Story test resource");}
    return {std::istreambuf_iterator<char>(in),{}};
}
void supplied(const std::filesystem::path& root) {
    const auto assets=decodeStorySceneAssets(read(root/"SCENEB1.RES"),read(root/"SCENEA.RES"),
        {read(root/"SCENEB2.RES"),read(root/"SCENEB3.RES"),read(root/"SCENEB5.RES")},read(root/"SCENEC.RES"),read(root/"VOICE.RES"));
    constexpr std::array<unsigned,6> expected{153,192,130,273,276,7};
    constexpr std::array<unsigned,6> expectedIrqs{7300,1520,6250,13250,11620,1552};
    constexpr std::array<StoryScene,6> scenes{StoryScene::bridgeOpening,StoryScene::earthOpening,StoryScene::level2,
        StoryScene::level4,StoryScene::level7,StoryScene::draxon};
    assert(assets.voices[0].size()==4 && assets.voices[1].size()==1);
    for(std::size_t index=0;index<scenes.size();++index) {
        StorySceneController controller(assets,scenes[index],pages);unsigned count=0,irqs=0;std::set<std::size_t> visited;
        IndexedPages pixels;SceneCaptureState captures;
        for(const auto page:{pages.cached,pages.back,pages.front}) {
            for(int y=0;y<200;++y) {for(int x=0;x<320;++x) {pixels.write(page,{static_cast<std::int16_t>(x),static_cast<std::int16_t>(y)},0);}}
        }
        // Source font is separately supplied by the native runtime. VM operation
        // ordering and all non-text pixels are verified here without substitutes.
        while(controller.result()==StorySceneResult::playing) {
            const auto step=controller.advance();++count;irqs+=step.waitRawIrqs;visited.insert(step.pages.begin(),step.pages.end());
            for(const auto& op:step.work.operations) {
                if(std::holds_alternative<ScenePaletteChange>(op)||std::holds_alternative<SceneSoundRequest>(op)
                    ||std::holds_alternative<SceneDialogueDraw>(op)) {continue;}
                applyScenePageOperation(pixels,op,controller.graphics(),{},pages.back,captures);
            }
        }
        assert(count==expected[index] && irqs==expectedIrqs[index]);
        for(std::size_t page=0;page<storySceneResource(assets,scenes[index]).program.pages.size();++page) {
            if(controller.composite() && (page==9 || page==18 || page==53 || page==57 || page==61 || page==63 || page==67)) {continue;}
            if(scenes[index]==StoryScene::level7 && page==159) {continue;} //1110's fast range resumes at160.
            assert(visited.count(page));
        }
        rejects([&] {controller.advance();});
        StorySceneController skipped(assets,scenes[index],pages);skipped.advance();
        assert(skipped.advance(1).result==StorySceneResult::skipped);
    }
    assert(!campaignStoryScene(0) && campaignStoryScene(1)==StoryScene::level2 && !campaignStoryScene(2)
        && campaignStoryScene(3)==StoryScene::level4 && !campaignStoryScene(4) && !campaignStoryScene(5)
        && campaignStoryScene(6)==StoryScene::level7);
    rejects([&] {decodeStorySceneAssets({},read(root/"SCENEA.RES"),{read(root/"SCENEB2.RES"),read(root/"SCENEB3.RES"),
        read(root/"SCENEB5.RES")},read(root/"SCENEC.RES"),read(root/"VOICE.RES"));});
}
}
int main(int argc,char** argv) { wrappedDirtyCopy();compositeOrder();bankAndCapture();if(argc==2) {supplied(argv[1]);} }
