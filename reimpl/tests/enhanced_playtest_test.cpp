#include "playable_startup_resources.hpp"
#include "world_presentation.hpp"
#include "failure_page.hpp"
#include "chase_view.hpp"
#include "stellar7/render_work.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <set>
#include <fstream>
#include <cmath>

namespace {
void require(bool condition,const char* message) {if(!condition) {throw std::runtime_error(message);}}
}
namespace stellar7 {
struct GameplayModelTestAccess {
    static std::unique_ptr<GameplayModel> scene(std::vector<Level1Entity> actors,std::uint8_t bank=0) {
        auto model=std::make_unique<GameplayModel>();model->phase_=GameplayPhase::playable;
        model->activeCampaignLevel_=bank;model->playableCampaignFrames_=true;
        model->level2Player_={};model->level2Player_.stateBytes4dTo52[1]=1;model->level2PlayerArmor_=680;
        model->level2PresentationEntities_=std::move(actors);return model;
    }
    static void sourceList(GameplayModel& model) {
        for(std::size_t index=0;index<model.level2PresentationEntities_.size();++index) {
            model.level2PresentationActiveListOrder_.push_back(index);
        }
    }
    static std::unique_ptr<GameplayModel> gate(const GameplayAssets& assets,std::uint8_t bank,std::int16_t type) {
        const auto shared=std::vector<Model3d>(assets.models.begin(),assets.models.begin()+44);
        StellarRng rng;const auto& local=assets.campaignLevels.at(bank).models;
        auto actors=constructCampaignLoaderActors(campaignLevelDefinition(bank,0).table,rng,shared,local);
        for(std::size_t index=0;index<actors.size();++index) {
            actors[index].position={100000+int(index)*10000,-100000,0};
            actors[index].forwardVelocity=actors[index].yawVelocity=0;
        }
        auto model=std::make_unique<GameplayModel>();model->playableCampaignFrames_=true;
        model->initializeRetainedCampaignLevel(bank,std::move(actors),rng,shared,local);
        if(bank==2) {model->completeLevel3Preloop();} else {model->completePlayableCampaignPreloop();}
        // Declared post-45C6 component boundary, with a genuine FE allocation.
        Level1Entity request{};request.type=type;request.collisionRadius=assets.models.at(type).collisionRadius;
        const auto index=model->allocateLevel2FixedObject(request);
        require(index<model->level2Entities_.size(),"gate entry could not allocate its physical FE record");
        auto& reward=model->level2Entities_[index];reward.position={0,8000,0};
        reward.pitch=reward.roll=reward.yaw=0;reward.forwardVelocity=reward.yawVelocity=0;
        reward.stateBytes4dTo52={0xfe,100,2,0,0,1};reward.renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;
        reward.animatedModelPresenterEnabled=false;model->level2PickupIndex_=index;
        model->level2ActiveListOrder_=model->level2AllocationListOrder_;
        model->syncPlayableCampaignPools();return model;
    }
};
}
namespace {
using namespace stellar7;
void backdrop(const GameplayAssets& assets) {
    Level1Entity rear{};rear.type=1;rear.position={0,-12000,0};rear.collisionRadius=assets.models.at(1).collisionRadius;
    rear.stateBytes4dTo52={0xff,1,0,0,1,0};rear.renderProfile=WorldActorRenderProfile::noHandles;
    auto empty=GameplayModelTestAccess::scene({}),culled=GameplayModelTestAccess::scene({rear});
    GameplayModelTestAccess::sourceList(*culled);
    WorldPresentationCapture capture;
    const auto a=renderPlayableLevel2ViewportIndexed(assets,*empty),b=renderPlayableLevel2ViewportIndexed(assets,*culled);
    require(a.pixels==b.pixels&&a.palette==b.palette,"culled actor changed the original source frame");
    require(renderWorldPresentation(a,4)==renderWorldPresentation(b,4),"culled actor discarded the enhanced background clear");
    std::cout<<"Source/enhanced backdrop agrees with and without a culled first actor\n";
}
void clipping(const GameplayAssets& assets) {
    for(const auto y:{-4300,-4100,-3900,-3000,-1000,0}) {
        for(const auto x:{0,1400}) {
            for(const std::uint16_t yaw:{std::uint16_t{0},std::uint16_t{0x4000}}) {
                Level1Entity boss{};boss.type=44;boss.position={x,y,0};boss.yaw=yaw;
                boss.collisionRadius=assets.campaignLevels[0].models[0].collisionRadius;
                boss.stateBytes4dTo52={0xff,15,2,0,5,0};
                boss.renderProfile=WorldActorRenderProfile::heapSelectorDeferredAnimation;boss.animatedModelPresenterEnabled=true;
                auto model=GameplayModelTestAccess::scene({boss});
                WorldPresentationCapture capture;
                try {
                    const auto frame=renderPlayableLevel2ViewportIndexed(assets,*model);
                    renderWorldPresentation(frame,4,nullptr,0,EnhancedView::chase);
                } catch(const std::exception& error) {
                    std::cerr<<"guardian pose x="<<x<<" y="<<y<<" yaw="<<yaw<<": ";throw;
                }
            }
        }
    }
    // Controlled large face at the chase near plane: exercise the same signed16
    // overflow class without requiring the player's lost combat state.
    auto wide=assets;Level1Entity boss{};boss.type=44;boss.collisionRadius=32700;
    boss.stateBytes4dTo52={0xff,15,0,0,5,0};boss.renderProfile=WorldActorRenderProfile::noHandles;
    Level1Entity player{};const auto view=chaseCamera(player,{});boss.position=view.origin;
    auto inverse=view;inverse.origin={};transpose(inverse.matrix);
    ModelGroup group{};group.animationOffset=-1;
    for(const FixedPosition point:std::array<FixedPosition,3>{{{-30000,150,-3000},{30000,150,3000},{0,150,5000}}}) {
        const auto world=transformToView(point,inverse);
        group.vertices.push_back({static_cast<std::int16_t>(world.x),static_cast<std::int16_t>(world.y),static_cast<std::int16_t>(world.z)});
    }
    ModelShape shape{};shape.flags=0x80;shape.colors={9,9,9,9};shape.normalVertex=255;shape.vertexIndices={0,1,2};
    ModelPrimitive primitive{};primitive.shapes={shape};group.primitives={primitive};
    ModelLod lod{};lod.threshold=256;lod.groups={group};wide.campaignLevels[0].models[0].lods={lod};
    auto model=GameplayModelTestAccess::scene({boss});
    WorldPresentationCapture capture;
    std::cout<<"Controlled wide guardian face at near plane\n";
    const auto frame=renderPlayableLevel2ViewportIndexed(wide,*model);
    renderWorldPresentation(frame,4,nullptr,0,EnhancedView::chase);
    require(std::any_of(frame.presentation->chase->draws.begin(),frame.presentation->chase->draws.end(),[](const auto& draw) {
        return draw.face&&draw.face->opacity==1;
    }),"wide guardian geometry was silently discarded");
    bool originalRejected=false;
    try {clipPolygon({{-32000,-3000},{32000,3000},{0,5000}},{0,319,0,199},PolygonClipMode::Enabled);}
    catch(const std::invalid_argument& error) {
        originalRejected=std::string(error.what())=="polygon clipping quotient exceeds original IDIV width";
    }
    require(originalRejected,"original signed16 IDIV qualification guard was relaxed");
    std::cout<<"24 close guardian poses cross the camera near plane without source-IDIV overflow\n";
}
void gates(const GameplayAssets& assets) {
    constexpr std::array<std::uint8_t,8> expected{3,2,1,0,1,2,3,2};
    for(const std::int16_t type:{25,26}) {
      for(std::uint8_t bank=0;bank<(type==25?6:7);++bank) {
        auto model=GameplayModelTestAccess::gate(assets,bank,type);
        std::set<std::vector<std::uint8_t>> pictures;
        for(const auto selector:expected) {
            model->advancePlayableCampaign({});
            const auto& actors=model->level2PresentationEntities();
            const auto gate=std::find_if(actors.begin(),actors.end(),[type](const auto& actor) {return actor.type==type;});
            require(gate!=actors.end(),"green gate disappeared before collection");
            std::cout<<"gate bank="<<unsigned(bank+1)<<" selector="<<unsigned(gate->stateBytes4dTo52[2])<<" expected="<<unsigned(selector)<<'\n';
            require(gate->stateBytes4dTo52[2]==selector,"green gate missed its shared source oscillator");
            const auto frame=renderPlayableLevel2ViewportIndexed(assets,*model);pictures.insert(frame.pixels);
            model->finishPlayableCampaignFrame();
        }
        require(pictures.size()>1,"green gate selectors did not reach visible geometry");
      }
    }
}
void preview(const std::filesystem::path& path,const IndexedScreen& frame,EnhancedView view) {
    std::ofstream file(path,std::ios::binary);file<<"P6\n1280 800\n255\n";
    for(const auto color:renderWorldPresentation(frame,4,nullptr,0,view)) {
        const char bytes[]{char(color>>16),char(color>>8),char(color)};file.write(bytes,3);
    }
    require(bool(file),"cannot write enhanced preview");
}
void shadowFloor(const GameplayAssets& assets) {
    int base=32767;
    for(const auto& group:assets.models.at(0).lods.front().groups) {
        for(const auto& primitive:group.primitives) {
            if(primitive.kind!=0) {continue;}
            for(const auto& shape:primitive.shapes) {for(const auto index:shape.vertexIndices) {
                base=std::min(base,int(group.vertices[index][2]));
            }}
        }
    }
    require(base==-300,"recovered Raven ground-contact baseline changed");
    Level1Entity player{};player.stateBytes4dTo52[1]=1;player.collisionRadius=assets.models[0].collisionRadius;
    const auto view=chaseCamera(player,{});const auto shadow=vehicleShadow(player,view,assets.cockpit.palette);
    require(bool(shadow),"Raven has no ground shadow");
    const auto radius=std::clamp(player.collisionRadius*0.35,160.0,1100.0);
    const auto contact=transformToView({static_cast<std::int32_t>(std::lround(radius)),0,base},view);
    const auto x=(view.viewport.left+view.viewport.right)/2+double(contact.x)*view.perspectiveScale/contact.y;
    const auto y=(view.viewport.top+view.viewport.bottom)/2-double(contact.z)*view.perspectiveScale/contact.y;
    require(std::abs(shadow->points.front().x-x)<1e-6&&std::abs(shadow->points.front().y-y)<1e-6,
        "shadow plane cuts through the recovered hull/scenery baseline");
}
void shadowCoverage(const GameplayAssets& assets) {
    // Observed Type11 pose at14s in the retained showcase (slot2, post-step350).
    Level1Entity player{},aircraft{};player.yaw=48544;
    aircraft.type=11;aircraft.position={13420,-2343,350};aircraft.collisionRadius=748;
    aircraft.stateBytes4dTo52={0xff,100,0,0,0,0};
    const auto shadow=vehicleShadow(aircraft,chaseCamera(player,{}),assets.cockpit.palette);
    require(bool(shadow),"visible showcase aircraft lost its shadow primitive");
    const auto vertical=std::minmax_element(shadow->points.begin(),shadow->points.end(),[](auto a,auto b) {return a.y<b.y;});
    const auto horizontal=std::minmax_element(shadow->points.begin(),shadow->points.end(),[](auto a,auto b) {return a.x<b.x;});
    require(vertical.second->y-vertical.first->y>=1.0-1e-9,"distant aircraft shadow collapsed below visible pixel coverage");
    require(horizontal.second->x-horizontal.first->x>=3.0-1e-9,"distant aircraft shadow lost its visible width");
    player.yaw=0;aircraft.position={100000,10000,1000};
    const auto offscreen=vehicleShadow(aircraft,chaseCamera(player,{}),assets.cockpit.palette);
    require(offscreen&&boundedPresentationPolygon(*offscreen).empty(),"coverage compensation pulled an off-screen shadow into view");
}
void cues(const GameplayAssets& assets,const std::filesystem::path& output) {
    Level1Entity player{},enemy{};player.stateBytes4dTo52[1]=1;
    enemy.type=1;enemy.position={2000,7000,1400};enemy.collisionRadius=1000;
    enemy.stateBytes4dTo52={0xff,1,0,0,1,0};enemy.renderProfile=WorldActorRenderProfile::noHandles;
    const auto view=chaseCamera(player,{});
    const auto low=vehicleShadow(enemy,view,assets.cockpit.palette);
    enemy.position.z=4200;const auto high=vehicleShadow(enemy,view,assets.cockpit.palette);
    require(low&&high&&low->points.size()==high->points.size(),"flying vehicle has no ground shadow");
    for(std::size_t i=0;i<low->points.size();++i) {
        require(low->points[i].x==high->points[i].x&&low->points[i].y==high->points[i].y,
            "shadow moved away from the vehicle's world XY when altitude changed");
    }
    require(high->opacity<low->opacity&&high->opacity>0,"altitude shadow opacity did not fade");
    enemy.flags=0x80;require(!vehicleShadow(enemy,view,assets.cockpit.palette),"shadow revealed a source-hidden enemy");
    enemy.flags=0;enemy.type=25;require(!vehicleShadow(enemy,view,assets.cockpit.palette),"emissive gate received a vehicle shadow");
    const std::vector<FixedPosition> face{{-10,100,-10},{10,100,-10},{0,100,10}};
    require(enhancedSurfaceLighting(face,{0,-100,0})>enhancedSurfaceLighting(face,{0,100,0}),
        "flat face did not respond to the light direction");
    enemy.type=1;enemy.position.z=1400;
    auto model=GameplayModelTestAccess::scene({enemy});GameplayModelTestAccess::sourceList(*model);
    const auto original=renderPlayableLevel2ViewportIndexed(assets,*model);
    IndexedScreen observed;
    {WorldPresentationCapture capture;observed=renderPlayableLevel2ViewportIndexed(assets,*model);}
    require(original.pixels==observed.pixels&&original.palette==observed.palette,"enhanced cues changed the source page");
    require(renderWorldPresentation(original,1)==renderWorldPresentation(observed,1),"enhanced lighting changed exact1x");
    std::size_t lit=0;
    for(const auto& draw:observed.presentation->draws) {if(draw.face&&draw.face->lighting<1) {++lit;}}
    require(lit>0,"flat enemy faces did not receive enhanced lighting");
    const auto& chase=*observed.presentation->chase;
    require(std::count_if(chase.draws.begin(),chase.draws.end(),[](const auto& draw) {
        return draw.face&&draw.face->opacity<1;
    })>=2,"chase view did not record both vehicle ground shadows");
    enemy.position.z=4200;auto raised=GameplayModelTestAccess::scene({enemy});
    IndexedScreen highFrame;
    {WorldPresentationCapture capture;highFrame=renderPlayableLevel2ViewportIndexed(assets,*raised);}
    if(!output.empty()) {
        std::filesystem::create_directories(output);
        preview(output/"cockpit.ppm",observed,EnhancedView::cockpit);
        preview(output/"low-shadow.ppm",observed,EnhancedView::chase);
        preview(output/"high-shadow.ppm",highFrame,EnhancedView::chase);
    }
    std::cout<<"Ground XY/height cues, source-hidden guard, flat face lighting and exact indexed source preserved\n";
}
}
int main(int argc,char** argv) {
    if(argc<2||argc>4) {return 2;}
    try {
        const auto assets=naturalRoute::assets(argv[1]);const std::string mode=argc>=3?argv[2]:"all";
        if(mode=="all"||mode=="--backdrop") {backdrop(assets);}
        if(mode=="all"||mode=="--clipping") {clipping(assets);}
        if(mode=="all"||mode=="--gate") {gates(assets);}
        if(mode=="all"||mode=="--cues") {cues(assets,argc==4?std::filesystem::u8path(argv[3]):std::filesystem::path{});}
        if(mode=="all"||mode=="--floor") {shadowFloor(assets);}
        if(mode=="all"||mode=="--shadow") {shadowCoverage(assets);}
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
