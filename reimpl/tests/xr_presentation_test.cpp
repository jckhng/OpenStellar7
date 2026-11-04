#include "xr_presentation.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace stellar7;
namespace {
void require(bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
void worldScale() {
    SpatialScene source;source.camera=initialLevel1MainView();source.background.assign(64000,0);source.overlay.assign(64000,-1);
    SpatialWorld world;world.pilotCamera=source.camera;world.pilotCamera.origin.z=spatialPilotEyeHeight;source.world=world;
    source.surfaces.push_back({{{0,4000,0}},1});
    IndexedScreen indexed{std::vector<std::uint8_t>(64000,17),std::vector<Rgb>(256)};
    for(const float scale:{xrDefaultUnitsPerMetre,400.0f}) {
        const auto presentation=xrWorld(source,indexed,EnhancedView::cockpit,{},scale);
        require(presentation.scene.camera.origin.z==0,"XR seat retained the raised desktop camera");
        float cabinHalfWidth=0;
        for(const auto& surface:presentation.scene.surfaces) {
            if(!surface.tint||!surface.filled) {continue;}
            for(const auto p:surface.points) {cabinHalfWidth=std::max(cabinHalfWidth,std::abs(p.x)/scale);}
        }
        require(std::abs(cabinHalfWidth-1.7f)<0.0001f,"changing world scale changed physical cabin size");
        require(std::abs((*presentation.scene.billboards[0].corners)[0].y/scale-1.3f)<0.0001f,"instrument panel has world-dependent physical distance");
        const auto viewScale=xrWorldViewScale(scale);
        require(viewScale.farMetres*scale>=2000000,"physical world scale clips native panorama/world range");
        require(presentation.palette==indexed.palette&&presentation.scene.surfaces.front().points.front().y==4000,"XR scaling changed original geometry/palette");
    }
    require(source.world->pilotCamera.origin.z==spatialPilotEyeHeight&&source.billboards.empty(),"XR seat correction mutated its source view");
}
void check() {
    IndexedScreen indexed{std::vector<std::uint8_t>(64000,17),std::vector<Rgb>(256)};indexed.palette[17]={32,100,210};
    const auto panel=xrPanel(indexed,1000);
    require(panel.scene.surfaces.empty()&&panel.scene.billboards.size()==1,"menu panel contains gameplay geometry");
    const auto& image=panel.scene.billboards.front();
    require(image.corners&&image.image.pixels==indexed.pixels&&panel.palette==indexed.palette,"original menu/story artwork changed");
    require(image.alpha==BillboardAlpha::opaque&&image.depth==BillboardDepth::write,"panel transparency/depth ownership");
    const auto first=(*image.corners)[0],last=(*image.corners)[2];
    require(std::abs(first.y-1800)<0.01f&&std::abs(last.x-first.x-1400)<0.01f,"seated panel physical size/distance");
    require(std::abs((last.x-first.x)/(first.z-last.z)-4.0f/3)<0.0001f,"XR menu panel changed the original 4:3 display aspect");
    const auto larger=xrPanel(indexed,2000);require((*larger.scene.billboards[0].corners)[0].y==2*first.y,"panel scale was applied inconsistently");
    require(indexed.pixels[0]==17&&panel.scene.overlay[0]==-1,"panel changed original frame/added head-locked overlay");
    const auto options=xrOptionsScreen(std::vector<std::uint32_t>(64000,0xff4080c0));
    const auto c=options.palette[options.pixels[0]];
    require(std::abs(int(c.red)-64)<=18&&std::abs(int(c.green)-128)<=18&&std::abs(int(c.blue)-192)<=43,"native options panel colour conversion");
    bool rejected=false;try {xrPanel(indexed,0);} catch(const std::invalid_argument&) {rejected=true;}require(rejected,"invalid physical panel scale accepted");
    XrPresentationMailbox mailbox;require(!mailbox.latest(),"mailbox did not start empty");
    auto initial=std::make_shared<XrPresentation>(panel);mailbox.publish(initial);const auto held=mailbox.latest();
    std::atomic<bool> done{false};std::atomic<unsigned> observed{0};
    std::thread producer([&] {
        for(unsigned i=0;i<1000;++i) {auto next=std::make_shared<XrPresentation>();next->palette={{std::uint8_t(i),0,0}};mailbox.publish(next);}
        done=true;
    });
    while(!done.load()) {if(mailbox.latest()) {++observed;}}
    producer.join();require(held==initial&&held->scene.billboards[0].image.pixels==indexed.pixels,"publication replaced an eye's retained snapshot");
    require(mailbox.latest()->palette[0].red==std::uint8_t(999),"mailbox queued old frames rather than retaining latest");
    mailbox.publish({});require(!mailbox.latest(),"clearing the mailbox retained stale content");
    std::cout<<"XR seated original-art panel/options conversion/latest-wins concurrent publication pass\n";
}
}
int main() {try {worldScale();check();return 0;} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
