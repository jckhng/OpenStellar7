#include "native_level2_frame_fixture.hpp"
#include "playable_campaign_comparison.hpp"
#include "playable_startup_resources.hpp"
#include "world_presentation.hpp"
#include "failure_page.hpp"
#include "chase_view.hpp"
#include "stellar7/completion_beam.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>

namespace stellar7 {
struct GameplayModelTestAccess {
    static void beam(GameplayModel& model) {
        auto& craft=model.level2PresentationEntities_.front();
        auto view=initialLevel1MainView();
        model.level2BeamPresentations_={presentCompletionBeam(craft,view,model.level2Rng_,CompletionBeamView::main)};
    }
    static void airborne(GameplayModel& model) {model.level2Player_.position.z=1000;}
    static std::unique_ptr<GameplayModel> scene(std::vector<Level1Entity> actors) {
        auto model=std::make_unique<GameplayModel>();model->phase_=GameplayPhase::playable;
        model->activeCampaignLevel_=0;model->playableCampaignFrames_=true;
        model->level2Player_={};model->level2Player_.stateBytes4dTo52[1]=1;
        model->level2PlayerArmor_=680;
        model->level2PresentationEntities_=std::move(actors);
        // Declared source list empty: a chase camera needs its own visual culling.
        model->level2PresentationActiveListOrder_={};return model;
    }
};
}
namespace {
using namespace stellar7;
using stellar7::test::requireCampaignComparison;
void ppm(const std::filesystem::path& path,const std::vector<std::uint32_t>& pixels,unsigned scale) {
    std::ofstream file(path,std::ios::binary);file<<"P6\n"<<320*scale<<' '<<200*scale<<"\n255\n";
    for(auto value:pixels) {const char bytes[]{char(value>>16),char(value>>8),char(value)};file.write(bytes,3);}
    requireCampaignComparison(bool(file),"cannot write chase preview");
}
void camera() {
    Level1Entity player{};player.position={100,200,700};
    const auto source=player.position;
    for(const std::uint16_t yaw:{std::uint16_t{0},std::uint16_t{0x4000},std::uint16_t{0x8000},std::uint16_t{0xc000}}) {
        player.yaw=yaw;const auto view=chaseCamera(player,{});const auto center=transformToView(player.position,view);
        requireCampaignComparison(std::abs(center.x)<=2&&center.y>0&&view.origin.z>player.position.z,
            "chase camera did not follow the hull at a cardinal heading");
        const auto point=projectToViewport(center,view);
        requireCampaignComparison(point.x>120&&point.x<200&&point.y>80&&point.y<169,"hull is outside the usable chase viewport");
    }
    player.yaw=0;const auto original=chaseCamera(player,{});
    const auto adjusted=chaseCamera(player,{},ChaseOptions{5600,2200});
    requireCampaignComparison(adjusted.origin.y<original.origin.y&&adjusted.origin.z>original.origin.z
        &&adjusted.viewport.bottom==199,"chase adjustment changed viewport or failed to move camera");
    Level1Entity wall{};wall.type=30;wall.stateBytes4dTo52={0xfe,100,0,0,0,0};
    wall.position={100,-2300,700};wall.collisionRadius=2000;
    const auto blocked=chaseCamera(player,{wall});
    requireCampaignComparison(blocked.origin.y>original.origin.y&&player.position==source,"camera obstruction moved gameplay or failed to shorten boom");
}
void closeRangeAim(const GameplayAssets& assets) {
    Level1Entity target{};target.type=1;target.position={0,6000,0};
    target.collisionRadius=1000;target.stateBytes4dTo52={0xff,10,0,0,1,0};
    auto model=GameplayModelTestAccess::scene({target});
    const auto view=chaseCamera(model->level2Player(),{});
    Level1Entity shot{};shot.position=model->level2Player().position;shot.forwardVelocity=500;
    const auto threshold=(target.collisionRadius+assets.models[23].collisionRadius)/2;
    do {advanceEntityKinematics(shot);} while(target.position.y-shot.position.y>threshold);
    const auto point=projectToViewport(transformToView(shot.position,view),view);
    std::vector<std::uint8_t> pixels(64000);
    const auto palette=renderPlayableLevel2ViewportIndexed(assets,*model).palette;
    drawChaseHud(pixels,assets,*model,view,palette);
    unsigned arms=0;
    for(int offset=3;offset<=6;++offset) {
        arms+=pixels[point.y*320+point.x-offset]!=0;
        arms+=pixels[point.y*320+point.x+offset]!=0;
    }
    requireCampaignComparison(arms==8,"chase reticle does not meet the close target on the primary shot path");
    IndexedScreen held;
    {WorldPresentationCapture capture;held=renderPlayableLevel2ViewportIndexed(assets,*model);}
    const auto oldPresentation=held.presentation;
    GameplayModelTestAccess::airborne(*model);
    refreshChaseHud(held,assets,*model);
    requireCampaignComparison(std::equal(held.presentation->chase->source.begin(),
        held.presentation->chase->source.begin()+169*320,oldPresentation->chase->source.begin()),
        "late status publication redrew an aiming marker over its held world image");
    auto airborne=pixels;
    drawChaseHud(airborne,assets,*model,chaseCamera(model->level2Player(),{}),palette);
    bool changed=false;
    for(int y=6;y<18;++y) {
        for(int x=220;x<319;++x) {changed=changed||airborne[y*320+x]!=pixels[y*320+x];}
    }
    requireCampaignComparison(changed,"airborne player lacks a jump/height cue");
}
void visibility(const GameplayAssets& assets,const std::filesystem::path& output) {
    auto data=assets;data.cockpit.pixels.assign(64000,0);
    Level1Entity rear{};rear.type=1;rear.position={1800,-1000,0};rear.collisionRadius=data.models[1].collisionRadius;
    rear.stateBytes4dTo52={0xff,1,0,0,1,0};rear.renderProfile=WorldActorRenderProfile::noHandles;
    auto empty=GameplayModelTestAccess::scene({}),occupied=GameplayModelTestAccess::scene({rear});
    WorldPresentationCapture capture({},PresentationGeometry::spatial);
    const auto a=renderPlayableLevel2ViewportIndexed(data,*empty),b=renderPlayableLevel2ViewportIndexed(data,*occupied);
    requireCampaignComparison(a.pixels==b.pixels&&a.palette==b.palette,"chase culling changed original viewport");
    requireCampaignComparison(a.presentation&&a.presentation->chase&&b.presentation&&b.presentation->chase,"ordinary presentation lacks alternate camera");
    const auto cockpit=renderWorldPresentation(b,4),chase=renderWorldPresentation(b,4,nullptr,0,EnhancedView::chase);
    requireCampaignComparison(chase!=renderWorldPresentation(a,4,nullptr,0,EnhancedView::chase),"chase failed to draw a rearward source-culled object");
    rear.flags=0x80;auto hidden=GameplayModelTestAccess::scene({rear});
    const auto masked=renderPlayableLevel2ViewportIndexed(data,*hidden);
    requireCampaignComparison(renderWorldPresentation(masked,4,nullptr,0,EnhancedView::chase)
        ==renderWorldPresentation(a,4,nullptr,0,EnhancedView::chase),"chase revealed a hidden mesh");
    for(const unsigned scale:{1,2,4}) {
        requireCampaignComparison(renderWorldPresentation(b,scale,nullptr,0,EnhancedView::chase).size()==64000*scale*scale,
            "chase raster scale is invalid");
    }
    ppm(output/"chase-rear-object.ppm",chase,4);ppm(output/"chase-empty.ppm",renderWorldPresentation(a,4,nullptr,0,EnhancedView::chase),4);
    ppm(output/"chase-cockpit-control.ppm",cockpit,4);
    auto behind=rear;behind.flags=0;behind.position={0,-12000,0};
    const auto other=GameplayModelTestAccess::scene({behind});
    const auto behindFrame=renderPlayableLevel2ViewportIndexed(data,*other);
    requireCampaignComparison(behindFrame.presentation->chase->spatial!=nullptr,"offscreen mesh lacks spatial snapshot");
    const auto& surfaces=behindFrame.presentation->chase->spatial->surfaces;
    requireCampaignComparison(std::any_of(surfaces.begin(),surfaces.end(),[](const auto& surface) {
        return surface.type==1&&std::any_of(surface.points.begin(),surface.points.end(),[](auto point) {return point.y<0;});
    }),"spatial snapshot inherited the base-camera frustum cull");
    Level1Entity craft{};craft.type=7;craft.position={0,9000,1600};craft.collisionRadius=data.models[7].collisionRadius;
    craft.flags=12;craft.behaviorTimer=40;craft.stateBytes4dTo52={0xff,100,2,0,0,3};
    craft.renderProfile=WorldActorRenderProfile::inlineSelectorBeamCallback;
    auto delivery=GameplayModelTestAccess::scene({craft});GameplayModelTestAccess::beam(*delivery);
    const auto rng=delivery->level2Rng();const auto* beam=delivery->level2CompletionBeam(0);
    requireCampaignComparison(beam&&beam->projected,"controlled source beam did not project");
    const auto projected=renderPlayableLevel2ViewportIndexed(data,*delivery);
    test::compareCampaignRng(delivery->level2Rng(),rng,test::CampaignComparisonStage::boundary);
    unsigned columns=0;
    for(const auto& draw:projected.presentation->chase->draws) {
        if(draw.face&&draw.face->identity&&draw.face->identity->group==std::numeric_limits<std::size_t>::max()) {
            requireCampaignComparison(columns<beam->colors.size()&&draw.face->color==beam->colors[columns],
                "chase beam changed the source color sequence");++columns;
        }
    }
    requireCampaignComparison(columns==beam->colors.size(),"chase omitted the retained delivery beam");
}
void retained(const std::filesystem::path& root,const std::filesystem::path& output) {
    double totalRender=0;unsigned renders=0;
    for(std::uint8_t bank=0;bank<7;++bank) {
        auto source=test::makePlayableGuardianSession(root,bank,test::PlayableGuardianEntry::clearedWave);
        auto enhanced=test::makePlayableGuardianSession(root,bank,test::PlayableGuardianEntry::clearedWave);
        const auto* synth=&enhanced->synth();test::CampaignBudgetComparison comparison;
        unsigned captures=0;IndexedScreen previous;
        for(unsigned tick=0;tick<6;++tick) {
            GameplayInput input;input.left=tick==2;input.forward=tick==3;input.fire=tick==4;
            const auto a=source->advancePlayableCampaign(input,10000);
            NativeSessionDelivery b;
            const ChaseOptions cameraSettings{5600,2200};
            {WorldPresentationCapture capture(cameraSettings,PresentationGeometry::spatial);b=enhanced->advancePlayableCampaign(input,257);}
            requireCampaignComparison(&enhanced->synth()==synth,"camera replaced the retained synth");
            const auto& frame=enhanced->frame();
            if(!frame.presentation||!frame.presentation->chase) {
                comparison.compare(*source,*enhanced,a,b,test::CampaignComparisonStage::boundary);continue;
            }
            ++captures;const auto original=renderWorldPresentation(frame,4);
            requireCampaignComparison(frame.presentation->chase->spatial!=nullptr
                &&!spatialGeometry(*frame.presentation->chase->spatial,frame.palette).triangles.empty(),"level bank lacks valid spatial mesh");
            requireCampaignComparison(frame.presentation->chase->camera==cameraSettings,"retained chase lost its camera settings");
            const auto begin=std::chrono::steady_clock::now();
            const auto chase=renderWorldPresentation(frame,4,nullptr,0,EnhancedView::chase);
            totalRender+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();++renders;
            requireCampaignComparison(chase!=original&&renderWorldPresentation(frame,4)==original,"view toggle altered or failed to restore cockpit");
            if(previous.presentation) {renderWorldPresentation(previous,4,&frame,0.5,EnhancedView::chase);}
            comparison.compare(*source,*enhanced,a,b,test::CampaignComparisonStage::boundary);
            const auto& draws=frame.presentation->chase->draws;
            requireCampaignComparison(std::any_of(draws.begin(),draws.end(),[](const auto& draw) {
                return draw.face&&draw.face->identity&&draw.face->identity->type==0;
            }),"chase player mesh is absent");
            if(tick==5) {
                ppm(output/("chase-level"+std::to_string(bank+1)+".ppm"),chase,4);
                ppm(output/("cockpit-level"+std::to_string(bank+1)+".ppm"),original,4);
            }
            previous=frame;
        }
        requireCampaignComparison(captures>=3,"chase lacked retained adjacent gameplay frames");
        std::cout<<"Chase Level"<<unsigned(bank+1)<<" retained frames="<<captures<<" source state/pages/audio/clock unchanged\n";
    }
    std::cout<<"chase4x_render_ms="<<totalRender/renders<<" observations="<<renders<<'\n';
}
std::unique_ptr<RecoveredFrontend> begin(const GameplayAssets& assets,const NativeStartupAssets& startup,
    FrameCopyProfile copy,std::size_t budget,std::vector<std::int16_t>& pcm) {
    auto frontend=std::make_unique<RecoveredFrontend>(assets,startup,copy,budget,
        std::shared_ptr<HighscoreStore>{},CampaignExecution::playable);
    unsigned menuPoll=0;
    for(unsigned call=0;!frontend->session()&&call<10000;++call) {
        NativeMenuInput input;
        if(frontend->startup().phase()==NativeStartupPhase::awaitingMenuDecision) {input.confirm=menuPoll++==1;}
        const auto delivery=frontend->advanceStartup(input);
        pcm.insert(pcm.end(),delivery.samples.begin(),delivery.samples.end());
    }
    requireCampaignComparison(frontend->session()&&menuPoll==3,"chase public test did not execute actual Begin");
    return frontend;
}
void publicEntry(const std::filesystem::path& root,const GameplayAssets& assets,const std::filesystem::path& output) {
    const auto startup=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto drivers=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    const auto copy=decodeFrameCopyProfile(drivers.at(0));
    std::vector<std::int16_t> aPcm,bPcm;
    auto source=begin(assets,startup,copy,10000,aPcm),enhanced=begin(assets,startup,copy,257,bPcm);
    requireCampaignComparison(aPcm==bPcm,"chase startup changed PCM");
    const auto* owner=enhanced->session();const auto* synth=&owner->synth();
    test::CampaignBudgetComparison comparison;comparison.leftPcm=aPcm;comparison.rightPcm=bPcm;
    unsigned captured=0;
    for(unsigned tick=0;tick<40;++tick) {
        GameplayInput input;
        if(tick>0) {input.forward=tick<10;input.left=tick>=10&&tick<20;input.fire=tick>=20;}
        if(tick==15) {input.systemAction=GameplaySystemAction::shield;}
        const auto a=source->advanceGameplay(input);
        NativeSessionDelivery b;
        {WorldPresentationCapture capture({},PresentationGeometry::spatial);b=enhanced->advanceGameplay(input);}
        comparison.compare(*source->session(),*owner,a,b,test::CampaignComparisonStage::boundary);
        requireCampaignComparison(enhanced->session()==owner&&&owner->synth()==synth,"public view lost native owners");
        const auto& frame=owner->frame();
        if(!frame.presentation||!frame.presentation->chase) {continue;}
        requireCampaignComparison(frame.presentation->chase->spatial!=nullptr,"retained chase lacks unprojected scene");
        ++captured;
        const auto cockpit=renderWorldPresentation(frame,4);
        const auto chase=renderWorldPresentation(frame,4,nullptr,0,EnhancedView::chase);
        requireCampaignComparison(cockpit!=chase&&renderWorldPresentation(frame,4)==cockpit,"public view toggle failed");
        if(tick==15) {
            auto expected=frame.presentation->chase->source;
            drawChaseHud(expected,assets,owner->model(),chaseCamera(owner->model().level2Player(),owner->model().level2PresentationEntities()),frame.palette);
            requireCampaignComparison(std::equal(expected.begin()+188*320,expected.end(),
                frame.presentation->chase->source.begin()+188*320),"chase HUD missed the post-world ability publication");
            requireCampaignComparison(b.presentations.size()>=2,"shield activation did not publish its own source HUD boundary");
            const auto& first=b.presentations.front().indexed.presentation;
            const auto& last=b.presentations.back().indexed.presentation;
            requireCampaignComparison(first&&last&&first->chase&&last->chase&&first->chase!=last->chase
                &&first->chase->source!=last->chase->source,"late HUD update rewrote an already published camera frame");
            requireCampaignComparison(first->chase->spatial&&last->chase->spatial
                &&first->chase->spatial!=last->chase->spatial,"late HUD update rewrote the shared spatial scene");
        }
        if(tick==39) {ppm(output/"public-cockpit.ppm",cockpit,4);ppm(output/"public-chase.ppm",chase,4);}
    }
    requireCampaignComparison(captured>=35,"actual Begin lacks continuous chase frames");
    constexpr unsigned repeats=20;
    const auto before=std::chrono::steady_clock::now();
    for(unsigned repeat=0;repeat<repeats;++repeat) {
        WorldPresentationCapture capture;renderPlayableLevel2ViewportIndexed(assets,owner->model());
    }
    std::cout<<"both_views_capture_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count()/repeats
        <<" repeats="<<repeats<<'\n';
    std::cout<<"Actual startup/Begin +40 own frames: views preserve source state/pages/PCM/events/clock; captures="<<captured<<'\n';
}
}
int main(int argc,char** argv) {
    if(argc!=3) {return 2;}
    try {
        camera();const auto assets=naturalRoute::assets(argv[1]);closeRangeAim(assets);visibility(assets,argv[2]);retained(argv[1],argv[2]);
        publicEntry(argv[1],assets,argv[2]);
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
