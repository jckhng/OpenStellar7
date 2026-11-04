#include "openxr_test_runtime.hpp"
namespace {
using namespace stellar7;
using namespace stellar7::test::xr;
void versionRequirements() {
    Runtime runtime;rt=&runtime;runtime.minimumGl=XR_MAKE_VERSION(4,3,0);
    auto binding=graphics();binding.contextVersion=XR_MAKE_VERSION(3,3,0);
    bool rejected=false;
    try {OpenXrSession xr(getProc,binding,{1000});}
    catch(const std::runtime_error& error) {
        rejected=true;const std::string message=error.what();
        require(message.find("3.3.0")!=std::string::npos&&message.find("4.3.0")!=std::string::npos
            &&message.find("4.6.0")!=std::string::npos,"GL version rejection omits actual/runtime versions");
    }
    require(rejected&&runtime.destroyedInstances==1&&runtime.destroyedSessions==0,"GL version rejection ownership");
}
void graphicsFactory() {
    for(const std::string fault:{"none","create","binding","profile","api","low","high","range"}) {
        Runtime runtime;rt=&runtime;runtime.minimumGl=XR_MAKE_VERSION(4,3,99);runtime.maximumGl=XR_MAKE_VERSION(4,6,99);
        if(fault=="range") {runtime.maximumGl=XR_MAKE_VERSION(4,2,0);}
        unsigned creations=0;
        const auto create=[&](XrVersion minimum,XrVersion maximum) {
            ++creations;
            require(minimum==XR_MAKE_VERSION(4,3,0)&&maximum==XR_MAKE_VERSION(4,6,0),"factory did not receive runtime major/minor requirements");
            require(!runtime.calls.empty()&&runtime.calls.back()=="requirements"&&runtime.createdChains==0,"graphics created before runtime requirements or after session");
            if(fault=="create") {throw std::runtime_error("injected graphics creation failure");}
            auto binding=graphics();binding.contextVersion=minimum;
            if(fault=="binding") {binding.binding=nullptr;}
            if(fault=="profile") {binding.profile=GlProfile::es30;}
            if(fault=="api") {binding.api=OpenXrBinding::egl;}
            if(fault=="low") {binding.contextVersion=XR_MAKE_VERSION(3,3,0);}
            if(fault=="high") {binding.contextVersion=XR_MAKE_VERSION(4,7,0);}
            return binding;
        };
        const auto construct=[&] {OpenXrSession xr(getProc,GlProfile::desktop33,OpenXrBinding::platform,{1000},create);};
        if(fault=="none") {construct();} else {rejects(construct);}
        require(creations==(fault=="range"?0:1),"graphics factory was skipped or retried");
        require(runtime.destroyedInstances==1&&runtime.destroyedSessions==(fault=="none"?1:0),"graphics factory failure leaked runtime owners");
    }
}
void swapchainFormats() {
    for(const auto profile:{GlProfile::desktop33,GlProfile::es30}) {
        for(const bool rgba:{false,true}) {
            Runtime runtime;rt=&runtime;runtime.profile=profile;runtime.rgba=rgba;runtime.srgb=true;
            const auto construct=[&] {OpenXrSession xr(getProc,graphics(profile),{1000});};
            if(profile==GlProfile::es30&&!rgba) {rejects(construct);}
            else {
                construct();
                require(runtime.chainFormat==(profile==GlProfile::desktop33?GL_SRGB8_ALPHA8:GL_RGBA8),"wrong swapchain colour encoding selected");
            }
            require(runtime.destroyedInstances==1,"swapchain format admission leaked instance");
        }
    }
}
void seatedStartup() {
    Runtime runtime;rt=&runtime;runtime.headPose.position={2,0.1f,3};
    OpenXrSession xr(getProc,graphics(),{1000});
    runtime.events={{XR_SESSION_STATE_READY},{XR_SESSION_STATE_VISIBLE}};
    SpatialMatrix preview{},seated{},leaned{},refocused{};
    const auto matrix=[&](SpatialMatrix& out) {return [&](const OpenXrEye& eye) {if(eye.eye==0) {out=eye.clipFromNative;}};};
    require(xr.frame(matrix(preview))==OpenXrFrame::submitted&&!xr.focused(),"unfocused startup preview failed");
    runtime.headPose.position.y=1.6f;runtime.events={{XR_SESSION_STATE_FOCUSED}};
    xr.frame(matrix(seated));
    for(unsigned i=0;i<16;++i) {
        require(std::abs(preview[i]-seated[i])<0.00001f,"seat anchored to unfocused headset height before wearing");
    }
    runtime.headPose.position.y+=0.2f;xr.frame(matrix(leaned));
    require(leaned!=seated,"established seat followed head movement");
    runtime.events={{XR_SESSION_STATE_VISIBLE},{XR_SESSION_STATE_FOCUSED}};xr.frame(matrix(refocused));
    require(refocused==leaned,"dashboard focus return silently recentered the seat");
    xr.recenter();xr.frame(matrix(refocused));
    for(unsigned i=0;i<16;++i) {require(std::abs(refocused[i]-seated[i])<0.00001f,"F8 did not restore seated eye height");}
}
void shutdown() {
    for(const bool running:{false,true}) {
        Runtime runtime;rt=&runtime;
        {
            OpenXrSession xr(getProc,graphics(),{1000});
            runtime.events={{XR_SESSION_STATE_READY},{XR_SESSION_STATE_FOCUSED}};
            if(running) {xr.frame([](const auto&) {},OpenXrLayer::none);}
            const auto frames=runtime.frames;
            require(xr.pollShutdown()&&xr.pollShutdown()&&!xr.focused(),"session shutdown did not finish idempotently");
            require(runtime.exitRequests==(running?1:0)&&runtime.ends==(running?1:0),"shutdown skipped exit request/STOPPING or started an idle session");
            require(runtime.frames==frames,"shutdown advanced another frame");
        }
        require(runtime.destroyedInstances==1&&runtime.destroyedSessions==1,"shutdown leaked runtime owners");
        if(running) {
            const auto exit=std::find(runtime.calls.begin(),runtime.calls.end(),"requestExit");
            const auto end=std::find(runtime.calls.begin(),runtime.calls.end(),"endSession");
            const auto destroy=std::find(runtime.calls.begin(),runtime.calls.end(),"destroySession");
            require(exit<end&&end<destroy,"session destroyed before graceful STOPPING/end sequence");
        }
    }
}
void normal(GlProfile profile) {
    Runtime runtime;rt=&runtime;runtime.profile=profile;
    {
        OpenXrSession xr(getProc,graphics(profile),{1000});unsigned draws=0;
        const auto draw=[&](const OpenXrEye& eye) {
            require(eye.eye==draws%2&&eye.texture==(eye.eye==0?102:201),"acquired runtime texture ignored");
            require(runtime.waited[eye.eye],"draw before wait");runtime.call("draw"+std::to_string(eye.eye));++draws;
        };
        require(xr.frame(draw)==OpenXrFrame::idle&&draws==0,"idle runtime started frames");
        runtime.events={{XR_SESSION_STATE_READY},{XR_SESSION_STATE_FOCUSED}};
        require(xr.frame(draw)==OpenXrFrame::submitted&&draws==2&&xr.focused(),"focused stereo frame");
        require(runtime.located==100&&runtime.ended==100&&runtime.layers==1,"predicted display time not retained");
        const auto begin=std::find(runtime.calls.begin(),runtime.calls.end(),"waitFrame");
        const std::vector<std::string> expected{"waitFrame","beginFrame","locateViews","locateSpace","acquire0","wait0","draw0","release0","acquire1","wait1","draw1","release1","endFrame"};
        require(std::vector<std::string>(begin,runtime.calls.end())==expected,"frame ownership order");
        runtime.events={{XR_SESSION_STATE_VISIBLE}};runtime.time=111;
        require(xr.frame(draw)==OpenXrFrame::submitted&&draws==4&&!xr.focused(),"focus loss stopped visible presentation");
        runtime.render=false;require(xr.frame(draw)==OpenXrFrame::empty&&draws==4&&runtime.layers==0,"shouldRender false acquired eyes");
        runtime.render=true;runtime.valid=false;require(xr.frame(draw)==OpenXrFrame::empty&&draws==4,"invalid tracking rendered stale poses");
        runtime.valid=true;runtime.headValid=false;require(xr.frame(draw)==OpenXrFrame::empty&&draws==4,"invalid head origin rendered");
        runtime.headValid=true;runtime.viewCount=1;require(xr.frame(draw)==OpenXrFrame::empty&&draws==4,"partial stereo views admitted");
        runtime.viewCount=2;xr.recenter();require(xr.frame(draw)==OpenXrFrame::submitted&&draws==6,"seated recenter broke frame");
        runtime.events={{XR_SESSION_STATE_STOPPING}};require(xr.frame(draw)==OpenXrFrame::idle&&runtime.ends==1,"STOPPING did not end session");
        runtime.events={{XR_SESSION_STATE_READY},{XR_SESSION_STATE_FOCUSED}};require(xr.frame(draw)==OpenXrFrame::submitted&&runtime.begins==2,"session restart");
        runtime.events={{XR_SESSION_STATE_LOSS_PENDING}};require(xr.frame(draw)==OpenXrFrame::exiting&&!xr.focused(),"loss pending not terminal");
        require(xr.frame(draw)==OpenXrFrame::exiting,"lost runtime restarted itself");
    }
    require(runtime.destroyedChains==2&&runtime.destroyedSpaces==2&&runtime.destroyedSessions==1&&runtime.destroyedInstances==1,"runtime cleanup ownership");
}
void faults() {
    for(const std::string fault:{"extension","version","format","blend","stereo","procedure","createSpace2","createChain2"}) {
        Runtime runtime;rt=&runtime;auto binding=graphics();
        if(fault=="extension") {runtime.extension=false;}
        else if(fault=="version") {binding.contextVersion=XR_MAKE_VERSION(2,0,0);}
        else if(fault=="format") {runtime.rgba=false;}
        else if(fault=="blend") {runtime.opaque=false;}
        else if(fault=="stereo") {runtime.configCount=1;}
        else if(fault=="procedure") {runtime.missingProc=true;}
        else {runtime.fail=fault;}
        rejects([&] {OpenXrSession xr(getProc,binding,{1000});});
        require(runtime.destroyedInstances==(fault=="extension"?0:1),"admission failure leaked instance");
        if(fault=="createChain2") {require(runtime.destroyedSpaces==2&&runtime.destroyedSessions==1&&runtime.destroyedChains==1,"partial startup leaked resources");}
        if(fault=="createSpace2") {require(runtime.destroyedSpaces==1&&runtime.destroyedSessions==1,"partial reference-space startup leaked resources");}
    }
    for(const std::string fault:{"draw0","draw1","acquire1","release0","endFrame","timeout","index"}) {
        Runtime runtime;rt=&runtime;OpenXrSession xr(getProc,graphics(),{1000});
        runtime.events={{XR_SESSION_STATE_READY},{XR_SESSION_STATE_FOCUSED}};
        runtime.fail=fault;runtime.timeout=fault=="timeout";runtime.badIndex=fault=="index";
        rejects([&] {xr.frame([&](const auto& eye) {if(fault=="draw"+std::to_string(eye.eye)) {throw std::runtime_error("injected draw failure");}});});
        require(runtime.frames==1&&runtime.layers==(fault=="endFrame"?1:0),"failed frame was retried or partially submitted");
        if(fault=="draw0"||fault=="draw1"||fault=="acquire1"||fault=="index") {
            require(!runtime.acquired[0]&&!runtime.acquired[1],"render failure leaked waited image");
        }
        if(fault=="timeout"||fault=="release0") {require(xr.frame([](const auto&) {})==OpenXrFrame::exiting,"unusable image ownership did not fail closed");}
    }
    Runtime runtime;rt=&runtime;OpenXrSession xr(getProc,graphics(),{1000});runtime.events={{XR_SESSION_STATE_READY},{XR_SESSION_STATE_FOCUSED}};
    SpatialMatrix original{},moved{},recentered{},changed{};
    const auto matrix=[&](SpatialMatrix& out) {return [&](const OpenXrEye& eye) {if(eye.eye==0) {out=eye.clipFromNative;}};};
    xr.frame(matrix(original));runtime.headPose.position.x+=0.3f;xr.frame(matrix(moved));
    require(original!=moved,"retained scene ignored late head motion");xr.recenter();xr.frame(matrix(recentered));
    for(unsigned i=0;i<16;++i) {require(std::abs(original[i]-recentered[i])<0.00001f,"recenter did not restore seat-relative view");}
    runtime.events={{XR_SESSION_STATE_UNKNOWN,200}};runtime.headPose.position.x+=0.2f;runtime.time=150;xr.frame(matrix(moved));
    require(moved!=recentered,"reference change applied before its effective time");runtime.time=200;xr.frame(matrix(changed));
    for(unsigned i=0;i<16;++i) {require(std::abs(original[i]-changed[i])<0.00001f,"reference change retained stale seat coordinates");}
    runtime.headPose.position.x+=0.1f;runtime.time=210;xr.frame(matrix(moved));
    runtime.events={{XR_SESSION_STATE_UNKNOWN,300,true,{{0,0,0,1},{0.5f,0,0}}}};
    runtime.headPose.position.x-=0.5f;runtime.time=300;xr.frame(matrix(changed));
    for(unsigned i=0;i<16;++i) {require(std::abs(moved[i]-changed[i])<0.00001f,"known reference-space change silently recentered the physical head");}
}
}
int main() {
    try {versionRequirements();graphicsFactory();swapchainFormats();seatedStartup();shutdown();normal(GlProfile::desktop33);normal(GlProfile::es30);faults();std::cout<<"OpenXR SDK-dispatch graphics negotiation/colour formats/seated startup/shutdown/admission/session/stereo/focus/recenter/loss/failure ownership controls pass\n";return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
