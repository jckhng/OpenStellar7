#include "playable_joystick.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

namespace {
using namespace stellar7;
constexpr JoystickCounterSample centered{true,127,127,0};
void require(const bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
void axes(const JoystickAxes actual,const int x,const int y,const char* message) {
    require(actual.x==x&&actual.y==y,message);
}
template<class Function> void rejects(Function function) {
    bool rejected=false;
    try {function();}catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"unsupported joystick input was admitted");
}
JoystickCounterSample counter(const int x,const int y,const std::uint8_t buttons=0) {
    return {true,static_cast<std::uint8_t>(x),static_cast<std::uint8_t>(y),buttons};
}
void calibrate(PlayableJoystick& joystick,const JoystickCounterSample upperLeft,
    const JoystickCounterSample lowerRight,const JoystickCounterSample center=centered) {
    joystick.beginCalibration();
    require(joystick.captureCalibration(upperLeft)==JoystickCalibrationPoint::lowerRight,"upper-left capture order");
    require(joystick.captureCalibration(lowerRight)==JoystickCalibrationPoint::center,"lower-right capture order");
    require(joystick.captureCalibration(center)==JoystickCalibrationPoint::complete,"center capture order");
}

void providerAndDetection() {
    auto sample=joystickCounterSample({true,0,65535,3});
    require(sample.connected&&sample.x==0&&sample.y==254&&sample.buttons==3,"host/counter endpoint separation");
    sample=joystickCounterSample({true,32767,32768,0});
    require(sample.x==126&&sample.y==127,"portable provider rounding changed");
    sample=joystickCounterSample({false,32768,32768,3});
    require(!sample.connected&&sample.x==255&&sample.y==255&&sample.buttons==0,"disconnected provider timeout");
    rejects([] {joystickCounterSample({true,0,0,4});});
    require(joystickCounterDelay(1)==0x6fcc&&joystickCounterDelay(0x6fcc)==1
        &&joystickCounterDelay(0xffff)==1,"2494 quotient/minimum1 arithmetic");
    rejects([] {joystickCounterDelay(0);});

    PlayableJoystick joystick;
    require(!joystick.enabled()&&!joystick.poll(centered).connected,"cold driver is active");
    rejects([&] {joystick.beginCalibration();});
    rejects([&] {joystick.analog({});});
    require(!joystick.enable(counter(255,127))&&!joystick.enabled(),"X timeout must reject primary presence");
    require(joystick.enable(counter(127,255)),"2494 must not infer absence from Y timeout alone");
    axes(joystick.poll(counter(127,0)).normalized,0,-126,"Y timeout-center quotient differs");
    require(!joystick.enable({false,127,127,0}),"post-discharge absence must clear primary presence");
    require(joystick.enable(centered),"centered primary device not detected");
    joystick.disable();require(!joystick.poll(centered).connected,"7D78 failed to disable primary device");
}

void residentArithmetic() {
    PlayableJoystick joystick;joystick.enable(centered); //7F00/127=256.
    for(const auto row:std::array<std::array<int,3>,8>{{
        {{0,-127,-127}},{{119,-8,-8}},{{120,0,0}},{{127,0,0}},
        {{134,0,0}},{{135,8,8}},{{254,127,127}},{{255,127,127}}}}) {
        axes(joystick.poll(counter(row[0],row[0])).normalized,row[1],row[2],"245C signed scale/deadzone/clamp");
    }
    require(joystick.poll(counter(97,97)).directions==0,"-30 must remain inside digital neutral domain");
    require(joystick.poll(counter(96,157)).directions==6,"digital left/down asymmetric threshold");
    require(joystick.poll(counter(157,96)).directions==9,"digital right/up asymmetric threshold");
    require(joystick.poll(counter(157,157)).directions==10,"+30 must set positive digital directions");

    joystick.enable(counter(64,64)); //7F00/64=508; MUL low-word wrap is observable.
    axes(joystick.poll(counter(193,194)).normalized,127,0,"245C lost MUL low16 truncation");
    axes(joystick.poll(counter(254,0)).normalized,121,-127,"245C wrapped product boundary");
    //The incorrect32-bit multiplication would produce+127 for counter194.
    require(joystick.poll(counter(194,194)).normalized.x!=127,"negative control missed widened MUL");

    PlayableJoystick coldZero;coldZero.enable(counter(0,0));
    axes(coldZero.poll(counter(8,7)).normalized,8,0,"zero-center must retain initial zero gain");
    joystick.enable(counter(128,128));joystick.enable(counter(0,0));
    axes(joystick.poll(counter(8,9)).normalized,0,8,"zero-center must retain prior254 gain");
    rejects([&] {joystick.enable(counter(127,127,4));});
    axes(joystick.poll(counter(8,9)).normalized,0,8,"invalid enable mutated retained calibration");
    rejects([&] {joystick.poll(counter(127,127,4));});
}

void nonlinearAndCalibration() {
    PlayableJoystick joystick;joystick.enable(centered);
    for(const auto row:std::array<std::array<int,2>,10>{{
        {{0,0}},{{8,0}},{{14,0}},{{15,16}},{{29,16}},{{30,32}},
        {{45,48}},{{105,112}},{{119,112}},{{120,127}}}}) {
        axes(joystick.analog({static_cast<std::int16_t>(row[0]),static_cast<std::int16_t>(-row[0])}),
            row[1],-row[1],"8A48 original9-entry nonlinear response");
    }
    rejects([&] {joystick.analog({128,0});});
    rejects([&] {joystick.captureCalibration(centered);});
    calibrate(joystick,counter(0,0),counter(254,254));
    axes(joystick.calibration().upperLeft,-127,-127,"source upper-left capture");
    axes(joystick.calibration().lowerRight,127,127,"source lower-right capture");
    axes(joystick.calibration().center,0,0,"source center capture");
    rejects([&] {joystick.captureCalibration(centered);});

    calibrate(joystick,counter(0,0),counter(254,254),counter(167,167));
    axes(joystick.calibration().center,40,40,"8966 center must use previous driver normalization");
    axes(joystick.poll(counter(167,167)).normalized,0,0,"896E must replace driver center after capture");
    axes(joystick.analog({30,45}),-32,48,"8A48 center changes sign without subtracting before ABS");

    joystick.enable(centered);
    calibrate(joystick,counter(47,27),counter(227,207));
    axes(joystick.calibration().lowerRight,80,80,"89xx extent branch/order");
    axes(joystick.analog({80,-80}),127,-127,"calibrated extent table scaling");
    calibrate(joystick,counter(27,27),counter(48,48));
    axes(joystick.calibration().lowerRight,-79,-79,"89xx must retain signed smaller lower-right");
    axes(joystick.analog({40,-40}),112,-112,"8A48 negative extent requires SAR rounding down");

    calibrate(joystick,centered,centered);
    axes(joystick.calibration().lowerRight,128,128,"zero extents require source128 fallback");
    axes(joystick.analog({127,-127}),112,-112,"128 fallback must not become a fitted full-scale curve");
    calibrate(joystick,counter(118,118),counter(136,136));
    axes(joystick.calibration().lowerRight,9,9,"narrow calibration capture differs");
    axes(joystick.analog({8,8}),127,127,"last recovered table element narrow-boundary");
    rejects([&] {joystick.analog({9,9});}); //Source would read beyond the recovered table, not clamp to127.

    const auto previous=joystick;
    joystick.beginCalibration();joystick.captureCalibration(counter(0,0));
    const auto point=joystick.calibrationPoint();
    rejects([&] {joystick.captureCalibration({});});
    require(joystick.calibrationPoint()==point,"absent capture advanced calibration phase");
    joystick=previous;
    require(joystick.calibrationPoint()==JoystickCalibrationPoint::complete,"native rollback lost prior completed phase");
    axes(joystick.calibration().lowerRight,9,9,"native rollback lost prior completed calibration");
}

void buttons() {
    using A=JoystickButtonAction;
    PlayableJoystick joystick;
    require(joystick.advanceButtons(3)==A::none,"disabled driver consumed button action");
    joystick.enable(centered);
    require(joystick.advanceButtons(1)==A::fire&&joystick.advanceButtons(1)==A::fire
        &&joystick.advanceButtons(0)==A::none,"primary alone source firing/hold behavior");
    require(joystick.advanceButtons(2)==A::none&&joystick.advanceButtons(2)==A::none
        &&joystick.advanceButtons(0)==A::cycleSelected&&joystick.advanceButtons(0)==A::none,
        "secondary-alone release must cycle once");
    require(joystick.advanceButtons(3)==A::activateSelected&&joystick.advanceButtons(3)==A::none
        &&joystick.advanceButtons(1)==A::fire&&joystick.advanceButtons(2)==A::none
        &&joystick.advanceButtons(0)==A::none,"both-button activation latch/release ordering");
    require(joystick.advanceButtons(2)==A::none&&joystick.advanceButtons(1)==A::fire
        &&joystick.advanceButtons(0)==A::cycleSelected,"primary-alone must preserve prior secondary-arm history");
    joystick.advanceButtons(2);rejects([&] {joystick.advanceButtons(4);});
    require(joystick.advanceButtons(0)==A::cycleSelected,"invalid button mask mutated source history");
    joystick.advanceButtons(2);joystick.disable();
    require(joystick.advanceButtons(0)==A::none,"disabled driver ran release action");
    joystick.enable(centered);
    require(joystick.advanceButtons(0)==A::cycleSelected,"7D78 must not invent a41A2 reset");
    joystick.advanceButtons(2);const auto pending=joystick;
    require(!joystick.poll({}).connected,"disconnected host packet remained live");
    joystick.cancelButtons();require(joystick.advanceButtons(0)==A::none,"native cancellation emitted release action");
    joystick=pending;require(joystick.advanceButtons(0)==A::cycleSelected,"value rollback lost button history");
}
}

int main() {
    try {
        providerAndDetection();residentArithmetic();nonlinearAndCalibration();buttons();
        std::cout<<"Primary joystick counter/provider/detection/16-bit normalization/digital/analog/calibration/buttons PASS\n";
        return 0;
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
