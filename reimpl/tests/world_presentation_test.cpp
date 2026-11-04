#include "world_presentation.hpp"
#include "enhanced_options.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>

namespace {
using namespace stellar7;
IndexedScreen triangle(double shift=0,unsigned tick=10) {
    IndexedScreen frame{std::vector<std::uint8_t>(320*200),std::vector<Rgb>(256)};
    frame.palette[1]={255,255,255};frame.palette[2]={255,0,0};
    WorldPresentationRecorder recorder(frame.pixels,0,1,tick);
    PresentationFace face{{{10.25+shift,10.25},{30.75+shift,10.25},{10.25+shift,30.75}},
        {0,319,0,199},1,true,PresentationIdentity{1,0,0,0,1,100,1}};
    recorder.beforeFace(frame.pixels,std::move(face));recorder.afterFace(frame.pixels);
    frame.presentation=recorder.finish(frame.pixels);return frame;
}
void images() {
    auto current=triangle(),next=triangle(8,11);
    const auto original=renderWorldPresentation(current,1);
    assert(original.size()==64000&&original[12*320+12]==0xff000000);
    const auto full=renderWorldPresentation(current,4);
    assert(full.size()==1280*800&&full[50*1280+50]==0xffffffff&&full[50*1280+140]==0xff000000);
    // A geometry edge crosses individual subpixels, unlike a resized source image.
    bool subpixel=false;
    for(int y=40;y<130;++y) {
        for(int x=40;x<130;++x) {
            if(full[y*1280+x]!=full[(y/4*4)*1280+(x/4*4)]) {subpixel=true;}
        }
    }
    assert(subpixel);
    assert(renderWorldPresentation(current,4,&next,0.5)==renderWorldPresentation(triangle(4),4));
    auto replacement=std::make_shared<WorldPresentation>(*next.presentation);
    replacement->draws.front().face->identity->incarnation=2;
    auto replaced=next;replaced.presentation=replacement;
    assert(renderWorldPresentation(current,4,&replaced,0.5)==full);
    replacement->draws.front().face->identity->incarnation.reset();
    assert(renderWorldPresentation(current,4,&replaced,0.5)==full);
    auto unqualified=std::make_shared<WorldPresentation>(*current.presentation);
    unqualified->draws.front().face->identity->incarnation.reset();
    auto unknown=current;unknown.presentation=unqualified;
    assert(renderWorldPresentation(unknown,4,&replaced,0.5)==full);
    auto cut=triangle(8,12);
    assert(renderWorldPresentation(current,4,&cut,0.5)==full);
    cut=triangle(8,11);cut.palette[1]={0,255,0};
    assert(renderWorldPresentation(current,4,&cut,0.5)==full);
    cut=triangle(80,11);
    assert(renderWorldPresentation(current,4,&cut,0.5)==full);
    auto changed=std::make_shared<WorldPresentation>(*next.presentation);changed->phase=2;cut=next;cut.presentation=changed;
    assert(renderWorldPresentation(current,4,&cut,0.5)==full);
    changed=std::make_shared<WorldPresentation>(*next.presentation);changed->camera.distance+=700;cut.presentation=changed;
    assert(renderWorldPresentation(current,4,&cut,0.5)==full);
    changed=std::make_shared<WorldPresentation>(*next.presentation);changed->draws.front().face->identity->allocation=2;cut.presentation=changed;
    assert(renderWorldPresentation(current,4,&cut,0.5)==full);
    current.pixels[20*320+20]=2;
    const auto hud=renderWorldPresentation(current,4);
    for(int y=80;y<84;++y) {for(int x=80;x<84;++x) {assert(hud[y*1280+x]==0xffff0000);}}
    bool rejected=false;try {renderWorldPresentation(current,3);} catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
    assert(!WorldPresentationCapture::enabled());
    const ChaseOptions custom{5600,2200};
    {WorldPresentationCapture outer(custom);assert(WorldPresentationCapture::camera()==custom);
        {WorldPresentationCapture inner;assert(WorldPresentationCapture::camera()==ChaseOptions{});}
        assert(WorldPresentationCapture::camera()==custom);}
    assert(WorldPresentationCapture::camera()==ChaseOptions{});
    {WorldPresentationCapture outer;assert(WorldPresentationCapture::enabled());
        {WorldPresentationCapture inner;assert(WorldPresentationCapture::enabled());}assert(WorldPresentationCapture::enabled());}
    assert(!WorldPresentationCapture::enabled());
}
void smallFaces() {
    IndexedScreen frame{std::vector<std::uint8_t>(320*200),std::vector<Rgb>(256)};
    frame.palette[1]={255,255,255};
    WorldPresentationRecorder recorder(frame.pixels,0,1,1);
    recorder.beforeFace(frame.pixels,{{{10,10},{10.05,10},{10,10.05}},{0,319,0,199},1,true,{}});
    recorder.afterFace(frame.pixels);frame.presentation=recorder.finish(frame.pixels);
    for(const auto scale:{2,4}) {
        const auto pixels=renderWorldPresentation(frame,scale);
        assert(std::count(pixels.begin(),pixels.end(),0xffffffffU)>0);
    }
    const auto classic=renderWorldPresentation(frame,1);
    assert(std::count(classic.begin(),classic.end(),0xffffffffU)==0);
}
void energyStrokes() {
    const auto capture=[](std::vector<PresentationPoint> points,bool filled,float width) {
        IndexedScreen frame{std::vector<std::uint8_t>(64000),std::vector<Rgb>(256)};
        frame.palette[1]={255,255,255};
        WorldPresentationRecorder recorder(frame.pixels,0,1,1);
        PresentationFace face{std::move(points),{0,319,0,199},1,filled,{}};
        face.rasterStrokeWidth=width;
        recorder.beforeFace(frame.pixels,std::move(face));recorder.afterFace(frame.pixels);
        frame.presentation=recorder.finish(frame.pixels);return frame;
    };
    for(const auto& shape:std::vector<std::vector<PresentationPoint>>{
        {{20,20},{60,20}},{{40,20}},{{20,20},{60,20},{40,20.01}}}) {
        const auto energy=capture(shape,shape.size()==3,1),ordinary=capture(shape,shape.size()==3,0);
        for(const unsigned scale:{2,4}) {
            const auto enhanced=renderWorldPresentation(energy,scale),plain=renderWorldPresentation(ordinary,scale);
            const auto width=320*scale,column=40*scale;
            unsigned rows=0;
            for(unsigned y=18*scale;y<23*scale;++y) {rows+=enhanced[y*width+column]==0xffffffff;}
            if(rows<scale) {throw std::runtime_error("energy line/point/edge remains one output pixel wide");}
            assert(std::count(enhanced.begin(),enhanced.end(),0xffffffff)>std::count(plain.begin(),plain.end(),0xffffffff));
        }
        assert(renderWorldPresentation(energy,1)==renderWorldPresentation(ordinary,1));
    }
    const auto clipped=renderWorldPresentation(capture({{-10,20},{10,20}},false,1),4);
    assert(clipped[80*1280]==0xffffffff&&clipped[84*1280]==0xff000000);
}
void views() {
    auto frame=triangle();const auto cockpit=renderWorldPresentation(frame,4);
    assert(renderWorldPresentation(frame,4,nullptr,0,EnhancedView::chase)==cockpit);
    auto outer=std::make_shared<WorldPresentation>(*frame.presentation);
    auto alternate=std::make_shared<WorldPresentation>(*triangle(40).presentation);
    alternate->source.assign(64000,2);outer->chase=alternate;frame.presentation=outer;
    const auto chase=renderWorldPresentation(frame,4,nullptr,0,EnhancedView::chase);
    assert(chase!=cockpit&&renderWorldPresentation(frame,4)==cockpit);
    const auto low=renderWorldPresentation(frame,1,nullptr,0,EnhancedView::chase);
    assert(low.front()==0xffff0000&&low.size()==64000);
    // A copied native menu overlay belongs to the cockpit page, not the chase HUD.
    frame.pixels[0]=1;
    assert(renderWorldPresentation(frame,4).front()==0xffffffff);
    assert(renderWorldPresentation(frame,4,nullptr,0,EnhancedView::chase).front()==chase.front());
}
void lighting() {
    auto frame=triangle();auto scene=std::make_shared<WorldPresentation>(*frame.presentation);
    auto& face=*scene->draws.front().face;face.lighting=0.5;face.opacity=0.5;frame.presentation=scene;
    assert(renderWorldPresentation(frame,4)[50*1280+50]==0xff404040);
    assert(renderWorldPresentation(frame,1)[12*320+12]==0xff000000);
    auto next=frame;auto future=std::make_shared<WorldPresentation>(*scene);future->tick++;
    future->draws.front().face->lighting=1;future->draws.front().face->opacity=1;next.presentation=future;
    auto middle=std::make_shared<WorldPresentation>(*scene);
    middle->draws.front().face->lighting=0.75;middle->draws.front().face->opacity=0.75;
    auto expected=frame;expected.presentation=middle;
    assert(renderWorldPresentation(frame,4,&next,0.5)==renderWorldPresentation(expected,4));
}
void options(const std::filesystem::path& root) {
    const auto path=root/("world-options-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    assert(std::filesystem::create_directory(path));
    const auto file=path/"enhanced.cfg";
    EnhancedOptions value;value.renderScale=2;value.presentationRate=120;value.gameRate=35;value.bindings[0].key="Q";
    value.camera={5600,2200};
    value.scene={CockpitStyle::original,false,false};
    EnhancedOptionsStore store(file);store.save(value);const auto loaded=store.load();
    assert(loaded.renderScale==2&&loaded.presentationRate==120&&loaded.gameRate==35&&loaded.bindings[0].key=="Q");
    assert(loaded.camera==value.camera);
    assert(loaded.scene==value.scene);
    for(const auto format:{1,2,3,4}) {
        {std::ofstream out(file);out<<"format="<<format<<"\nvolume=75\nkey.left=Q\n";}
        const auto old=store.load();assert(old.volume==75&&old.bindings[0].key=="Q"&&old.renderScale==4&&old.presentationRate==60);
        assert(old.camera==ChaseOptions{});
        assert(old.scene==SpatialOptions{});
    }
    {std::ofstream out(file);out<<"format=3\nrender_scale=3\n";}
    bool rejected=false;try {store.load();} catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
    for(const auto line:{"chase_distance=0","chase_distance=7001","chase_height=999","chase_height=3001","cockpit_style=2","ground_detail=2","local_lights=-1"}) {
        {std::ofstream out(file);out<<"format=4\n"<<line<<'\n';}
        rejected=false;try {store.load();} catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
    }
    std::filesystem::remove(file);std::filesystem::remove(path);
}
}
int main(int argc,char** argv) {
    assert(argc==2);images();smallFaces();energyStrokes();views();lighting();options(std::filesystem::u8path(argv[1]));
    std::cout<<"Geometry/subpixel, interpolation/cut guards, HUD, exact1x and options migration passed\n";
}
