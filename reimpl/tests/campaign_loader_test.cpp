#include "stellar7/campaign_loader.hpp"
#include "stellar7/resource_codec.hpp"
#include "stellar7/level1.hpp"

#include <cassert>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace {
class Recorder final : public stellar7::CampaignActorLoader {
public:
    std::vector<std::string> events;
    std::size_t allocations = 0;
    Actor allocateOrdinary(std::uint8_t type) override {
        events.push_back("ordinary:" + std::to_string(type));
        return allocations++;
    }
    Actor allocateFixed(std::uint8_t type) override {
        events.push_back("fixed:" + std::to_string(type));
        return allocations++;
    }
    void write(Actor actor, stellar7::CampaignActorField field, std::int16_t value) override {
        events.push_back("write:" + std::to_string(actor) + ":"
            + std::to_string(static_cast<int>(field)) + ":" + std::to_string(value));
    }
    void setPosition(Actor actor, std::int16_t x, std::int16_t y) override {
        events.push_back("xy:" + std::to_string(actor) + ":" + std::to_string(x) + ":" + std::to_string(y));
    }
    void setYaw(Actor actor, std::uint16_t yaw) override {
        events.push_back("yaw:" + std::to_string(actor) + ":" + std::to_string(yaw));
    }
    void presentFixed(Actor actor) override {
        events.push_back("present:" + std::to_string(actor));
    }
    void aliasAnimation(Actor actor) override {
        events.push_back("alias:" + std::to_string(actor));
    }
};

void putWord(std::vector<std::uint8_t>& ds, std::size_t offset, std::uint16_t value) {
    ds.at(offset) = static_cast<std::uint8_t>(value);
    ds.at(offset + 1) = static_cast<std::uint8_t>(value >> 8U);
}

std::uint16_t getWord(const std::vector<std::uint8_t>& ds, std::size_t offset) {
    return static_cast<std::uint16_t>(ds.at(offset) | (ds.at(offset + 1) << 8U));
}

class Services final : public stellar7::CampaignAllocationServices {
public:
    std::vector<std::uint16_t> random;
    std::size_t cursor = 0;
    std::vector<std::uint16_t> released;
    std::vector<std::uint16_t> presented;
    std::vector<std::uint8_t> radiusTypes;
    std::uint16_t nextRandom() override {
        return random.at(cursor++);
    }
    std::uint32_t modelRadius(std::uint8_t type) override {
        radiusTypes.push_back(type);
        return 0x12345678;
    }
    void releaseAnimation(std::uint16_t pointer) override {
        released.push_back(pointer);
    }
    void presentFixed(std::uint16_t actor) override {
        presented.push_back(actor);
    }
};

void transitionTests() {
    using stellar7::CampaignExit;
    for (std::uint8_t level = 0; level < 7; ++level) {
        assert(stellar7::campaignExitDestination(level, CampaignExit::greenReward) == level + 1);
        assert(stellar7::campaignExitDestination(level, CampaignExit::blueWarp) == level + 3);
    }
    assert(stellar7::campaignExitDestination(1, CampaignExit::greenReward) == 2);
    assert(stellar7::campaignExitDestination(1, CampaignExit::blueWarp) == 4);
    assert(stellar7::campaignExitDestination(4, CampaignExit::greenReward) == 5);
    assert(stellar7::campaignExitDestination(4, CampaignExit::blueWarp) == 7);
    for (const auto level : {7, 8, 9, 255}) {
        bool rejected = false;
        try {
            stellar7::campaignExitDestination(static_cast<std::uint8_t>(level), CampaignExit::greenReward);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }
    for (const auto type : {0, 24, 27, 255}) {
        bool rejected = false;
        try {
            stellar7::campaignExitDestination(1, static_cast<CampaignExit>(type));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }
}

void definitionTests() {
    constexpr std::array<std::size_t, 7> counts{{6, 7, 5, 8, 6, 7, 7}};
    constexpr std::array<std::uint8_t, 7> skies{{5, 9, 11, 13, 15, 17, 21}};
    for (std::uint8_t level = 0; level < counts.size(); ++level) {
        const auto easy = stellar7::campaignLevelDefinition(level, 0);
        const auto normal = stellar7::campaignLevelDefinition(level, 1);
        const auto hard = stellar7::campaignLevelDefinition(level, 2);
        assert(easy.table.level == level && easy.table.actors.size() == counts[level]);
        assert(easy.table.sky == skies[level]);
        assert(easy.table.enemyLimit < normal.table.enemyLimit);
        assert(normal.table.enemyLimit < hard.table.enemyLimit);
        assert(easy.table.schedulerPointer == 0 && easy.table.weightsPointer == 0);
        assert(easy.scheduler == normal.scheduler && normal.scheduler == hard.scheduler);
        assert(easy.weights == normal.weights && normal.weights == hard.weights);
        const auto companion = level == 2 || level == 5 ? 1U : 0U;
        for (const auto* definition : {&easy, &normal, &hard}) {
            Recorder recorder;
            stellar7::initializeCampaignActors(definition->table, recorder);
            assert(recorder.allocations == counts[level] + companion);
        }
    }
    Recorder firstRecorder;
    stellar7::initializeCampaignActors(stellar7::campaignLevelDefinition(0, 0).table, firstRecorder);
    assert(std::find(firstRecorder.events.begin(), firstRecorder.events.end(), "write:5:4:-13")
        != firstRecorder.events.end());
    const auto fourth = stellar7::campaignLevelDefinition(3, 0);
    assert(fourth.table.actors[0].type == 9 && fourth.table.actors[1].type == 5);
    assert(fourth.table.actors[6].type == 29);
    Recorder fourthRecorder;
    stellar7::initializeCampaignActors(fourth.table, fourthRecorder);
    assert(std::find(fourthRecorder.events.begin(), fourthRecorder.events.end(), "fixed:32")
        != fourthRecorder.events.end());
    assert(std::find(fourthRecorder.events.begin(), fourthRecorder.events.end(), "write:6:4:-13")
        != fourthRecorder.events.end());
    const auto second = stellar7::campaignLevelDefinition(1, 0);
    assert(stellar7::campaignSpawnThreshold(1, 10) == 150);
    assert(stellar7::campaignSpawnThreshold(6, 10) == 100);
    assert(stellar7::campaignSpawnThreshold(6, 12) == 170);
    assert(stellar7::campaignSpawnType(second, 0, 6, 0x4000) == 1);
    assert(stellar7::campaignSpawnType(second, 0, 6, 0x4100) == 4);
    assert(stellar7::campaignSpawnType(second, 0, 6, 0xa500) == 8);
    assert(stellar7::campaignSpawnType(second, 0, 6, 0xcd00) == 13);
    assert(!stellar7::campaignSpawnType(second, 0, 6, 0xff00));
    assert(!stellar7::campaignSpawnType(second, 0, 6, 0x408c));
    assert(!stellar7::campaignSpawnType(second, 0, 6, 0x0000));
    assert(!stellar7::campaignSpawnType(second, 0, 12, 0x4000));
    assert(stellar7::campaignSpawnType(second, 10, 6, 0x4095) == 1);
    assert(stellar7::campaignSpawnType(second, 0, 7, 0x4000) == 4);
    assert(stellar7::campaignSpawnType(second, 0, 9, 0x4000) == 4);
    assert(stellar7::campaignSpawnType(second, 0, 10, 0x4000) == 1);
    assert(stellar7::campaignSpawnType(fourth, 0, 8, 0x9600) == 5);
    assert(stellar7::campaignSpawnType(fourth, 0, 9, 0x9600) == 4);
    assert(stellar7::campaignSpawnType(fourth, 0, 11, 0x9600) == 4);
    assert(stellar7::campaignSpawnType(fourth, 0, 12, 0x9600) == 5);
    auto editedCopy = second;
    editedCopy.table.actors[0].x = 99;
    assert(stellar7::campaignLevelDefinition(1, 0).table.actors[0].x == 6);
    assert(second.table.actors[6].yaw == 0xab00); // Preserve original yaw-byte stride.
    assert(second.table.actors[3].x == -12 && second.table.actors[3].yaw == 0x0300);
    assert(stellar7::campaignLevelDefinition(6, 2).table.enemyLimit == 30);
    assert(stellar7::campaignLevelDefinition(6, 0).scheduler[4] == 30);
    assert(stellar7::campaignLevelDefinition(6, 0).weights[12] == 255);
    assert(stellar7::campaignOrdinaryDefaults(12).speed == 360);
    assert(!stellar7::campaignOrdinaryDefaults(13).speed.has_value());
    assert(stellar7::campaignOrdinaryDefaults(14).health == 1);
    assert(stellar7::campaignOrdinaryDefaults(15).health == 0);
    for (const auto invalid : {std::pair<std::uint8_t, std::uint8_t>{7, 0}, {0, 3}}) {
        bool rejected = false;
        try { stellar7::campaignLevelDefinition(invalid.first, invalid.second); }
        catch (const std::runtime_error&) { rejected = true; }
        assert(rejected);
    }
    bool rejected = false;
    try { stellar7::campaignOrdinaryDefaults(17); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);
    rejected = false;
    try { stellar7::campaignSpawnThreshold(0, 16); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);
    rejected = false;
    try { stellar7::campaignSpawnType(second, 16, 0, 0x4000); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);
}

void allocatorTests() {
    constexpr std::uint16_t actor = 0x6000;
    constexpr std::uint16_t occupied = 0x6100;
    std::vector<std::uint8_t> ds(0x10000);
    putWord(ds, 0x4c16, occupied);
    putWord(ds, 0x4c18, actor);
    ds[occupied + 0x4e] = 1;
    std::fill_n(ds.begin() + actor, 100, 0xa5);
    ds[actor + 0x4e] = 0;
    putWord(ds, actor + 0x1c, 0x7000);
    ds[0x9ee + 10] = 42;
    ds[0xabe + 10] = 2;
    ds[0xa56 + 10] = 3;
    putWord(ds, 0xa00 + 20, 0xfffe);
    Services services;
    services.random = {0x0307, 0x0201, 0x0010};
    stellar7::CampaignMemoryLoader loader(ds, services);
    assert(loader.allocateOrdinary(10) == actor);
    assert(services.cursor == 3);
    assert(services.released == std::vector<std::uint16_t>{0x7000});
    assert(getWord(ds, actor + 8) == 32767);
    assert(getWord(ds, actor + 10) == 0xffff);
    assert(getWord(ds, actor + 0xc) == static_cast<std::uint16_t>(-6145));
    assert(getWord(ds, actor + 0x34) == static_cast<std::uint16_t>(-2304));
    assert(getWord(ds, actor + 0x18) == 0x8000);
    assert(getWord(ds, actor + 4) == 0x5678);
    assert(getWord(ds, actor + 6) == 0x1234);
    assert(ds[actor + 0x4e] == 42);
    assert(ds[actor + 0x62] == 0);
    assert(ds[actor + 0x63] == 0xa5);
    assert(getWord(ds, 0x49f8) == actor);
    assert(getWord(ds, 0x12aa) == 1);
    assert(getWord(ds, 0x12ac) == 1);

    ds[actor + 0x4e] = 0;
    putWord(ds, actor + 0x1c, actor + 0x4f);
    services.random = {0, 0};
    services.cursor = 0;
    putWord(ds, 0x51f4, 3);
    putWord(ds, 0x12ac, 9);
    loader.allocateOrdinary(13);
    assert(services.cursor == 2);
    assert(getWord(ds, actor + 2) == 4);
    assert(services.released.size() == 1);

    ds[actor + 0x4e] = 0;
    putWord(ds, 0x51f4, 5);
    putWord(ds, 0x12ac, 0);
    services.cursor = 0;
    loader.allocateOrdinary(14);
    assert(getWord(ds, actor + 2) == 15);
    assert(services.radiusTypes.back() == 14);

    ds[actor + 0x4e] = 0;
    services.cursor = 0;
    loader.allocateOrdinary(11);
    assert(services.presented.back() == actor);
    assert(getWord(ds, actor + 0x10) == 1000);

    putWord(ds, 0x45b0, occupied);
    putWord(ds, 0x45b2, actor);
    ds[actor + 0x4e] = 0;
    ds[actor + 0x5f] = 0x77;
    const auto cursor = services.cursor;
    assert(loader.allocateFixed(32) == actor);
    assert(services.cursor == cursor);
    assert(ds[actor + 0x5f] == 0x77);
    assert(ds[actor + 0x4e] == 100);
    loader.aliasAnimation(actor);
    assert(getWord(ds, actor + 0x1c) == actor + 0x4f);
    loader.setPosition(actor, 16, -1);
    assert(getWord(ds, actor + 8) == 0x8000);
    assert(getWord(ds, actor + 10) == 0xffff);
    assert(getWord(ds, actor + 0xc) == 0xf800);
    assert(stellar7::campaignCoordinate(32) == 0);
    const auto exported = stellar7::decodeCampaignActor(ds, actor, 7);
    assert(exported.type == 32);
    assert(exported.allocationIndex == 7);
    assert(exported.position.x == -32768);
    assert(exported.position.y == -2048);
    assert(exported.collisionRadius == 0x12345678);
    assert(exported.stateBytes4dTo52[1] == 100);
}

void typedOrdinaryEntryTests() {
    using namespace stellar7;
    std::vector<Model3d> models(44);
    for (auto& model : models) {
        model.collisionRadius = 1;
    }
    models[0].collisionRadius = 723;
    models[11].collisionRadius = 748;
    models[11].flags = 1;
    models[11].animationLimits = {8};
    models[13].collisionRadius = 559;
    StellarRng rng;
    auto reference = rng;
    std::vector<std::uint16_t> words;
    for (unsigned index = 0; index < 15; ++index) {
        words.push_back(reference.next());
    }
    const auto definition = campaignLevelDefinition(4, 0);
    const auto actors = constructCampaignOrdinaryActors(definition.table, rng, models, {});
    assert(definition.table.enemyLimit == 16 && definition.table.sky == 15);
    assert(actors.size() == 6 && rng.calls() == reference.calls());
    assert(rng.next() == reference.next());
    for (std::size_t index = 0; index < actors.size(); ++index) {
        const auto& actor = actors[index];
        const auto& position = definition.table.actors[index];
        assert(actor.allocationIndex == index && actor.type == (index < 3 ? 11 : 13));
        assert(actor.position.x == campaignCoordinate(position.x) && actor.position.y == campaignCoordinate(position.y));
        assert(actor.yaw == position.yaw && actor.stateBytes4dTo52[0] == 0xff && actor.stateBytes4dTo52[1] == 1);
        if (index < 3) {
            assert(actor.collisionRadius == 748 && actor.position.z == 1000 && actor.forwardVelocity == 270);
            assert(actor.renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation);
            assert(actor.animatedModelPresenterEnabled && actor.modelAnimationVariant == 0
                && actor.presentationModelAnimationVariant == 0);
            assert((actor.stateBytes4dTo52 == std::array<std::uint8_t, 6>{0xff, 1, 5, 0, 3, 0}));
        } else {
            const auto random = words[7 + (index - 3) * 3];
            const auto speed = static_cast<std::int16_t>((((random >> 8) & 3) + 7) << 8);
            assert(actor.yawVelocity == ((random & 1) ? -speed : speed));
            assert(actor.collisionRadius == 559 && actor.position.z == 0 && actor.forwardVelocity == 0);
            assert(actor.renderProfile == WorldActorRenderProfile::noHandles);
            assert(!actor.animatedModelPresenterEnabled);
        }
    }
    const auto before = rng.calls();
    auto invalid = definition.table;
    invalid.actors.back().type = 24;
    bool rejected = false;
    try { constructCampaignOrdinaryActors(invalid, rng, models, {}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && rng.calls() == before);
    models[11].flags = 2;
    rejected = false;
    try { constructCampaignOrdinaryActors(definition.table, rng, models, {}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && rng.calls() == before);
}

void ordinaryConstructorTests() {
    using namespace stellar7;
    std::vector<Model3d> models(14);
    models[5].collisionRadius = 123;
    models[8].collisionRadius = 860;
    models[5].flags = models[8].flags = 1;
    models[11].collisionRadius = 748;
    models[11].flags = 1;
    models[11].animationLimits = {8};
    models[13].collisionRadius = 559;
    const CampaignOrdinaryConstructor constructor(4, {5, 8, 11, 13}, models, {});
    // Constructor owns immutable metadata, not the caller's resource storage.
    models[11].collisionRadius = 0;
    models[13].collisionRadius = 0;
    StellarRng rng;
    auto reference = rng;
    for (const auto type : {5, 8, 11, 13}) {
        const auto placement = reference.next();
        const auto turn = type == 13 ? reference.next() : 0;
        const auto heading = reference.next();
        auto x = static_cast<std::int16_t>((placement >> 8U) & 15U);
        std::int16_t y = 16;
        if ((placement & 1U) != 0) { std::swap(x, y); }
        if ((placement & 2U) != 0) { x = static_cast<std::int16_t>(-x); }
        if ((placement & 4U) != 0) { y = static_cast<std::int16_t>(-y); }
        const auto actor = constructor.construct(static_cast<std::uint8_t>(type), rng);
        assert(rng.calls() == reference.calls());
        assert(actor.type == type && actor.flags == 0);
        assert(actor.position.x == campaignCoordinate(x) - 1 && actor.position.y == campaignCoordinate(y) - 1);
        assert(actor.yaw == static_cast<std::uint16_t>(campaignCoordinate(heading & 0xff0U)));
        assert(actor.modelAnimationVariant == 0 && actor.presentationModelAnimationVariant == 0);
        assert(actor.stateBytes4dTo52[3] == 0 && actor.behaviorTimer == 0);
        if (type == 11) {
            assert(actor.collisionRadius == 748 && actor.position.z == 1000 && actor.forwardVelocity == 270);
            assert(actor.renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation);
            assert(actor.animatedModelPresenterEnabled && actor.weaponSlotCount == 1);
            assert((actor.stateBytes4dTo52 == std::array<std::uint8_t, 6>{0xff, 1, 5, 0, 3, 0}));
        } else if (type == 13) {
            const auto speed = static_cast<std::int16_t>((((turn >> 8U) & 3U) + 7U) << 8U);
            assert(actor.yawVelocity == ((turn & 1U) != 0 ? -speed : speed));
            assert(actor.collisionRadius == 559 && actor.position.z == 0 && actor.forwardVelocity == 0);
            assert(actor.renderProfile == WorldActorRenderProfile::noHandles);
            assert(!actor.animatedModelPresenterEnabled && actor.weaponSlotCount == 0);
            assert((actor.stateBytes4dTo52 == std::array<std::uint8_t, 6>{0xff, 1, 0, 0, 4, 0}));
        } else {
            assert(actor.position.z == 0 && actor.yawVelocity == 0);
            assert(actor.renderProfile == WorldActorRenderProfile::noHandles && !actor.animatedModelPresenterEnabled);
            assert(actor.collisionRadius == (type == 5 ? 123 : 860));
            assert(actor.forwardVelocity == (type == 5 ? 135 : 0) && actor.weaponSlotCount == (type == 5 ? 3 : 2));
            assert(actor.stateBytes4dTo52[1] == (type == 5 ? 3 : 2));
            assert(actor.stateBytes4dTo52[4] == (type == 5 ? 1 : 2));
        }
    }
    bool rejected = false;
    try { constructor.construct(12, rng); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && rng.calls() == reference.calls());
    for (unsigned index = 0; index < 56; ++index) {
        assert(rng.next() == reference.next());
    }
    for (unsigned invalidCase = 0; invalidCase < 3; ++invalidCase) {
        models[11].collisionRadius = 748;
        models[13].collisionRadius = 559;
        models[11].flags = invalidCase == 0 ? 2 : 1;
        models[11].animationLimits = invalidCase == 1 ? std::vector<std::uint8_t>{0} : std::vector<std::uint8_t>{8};
        if (invalidCase == 2) { models[13].collisionRadius = 0; }
        rejected = false;
        const auto before = rng.calls();
        try { CampaignOrdinaryConstructor invalid(4, {11, 13}, models, {}); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected && rng.calls() == before);
        assert(rng.next() == reference.next());
    }
}

void resetTests() {
    std::vector<std::uint8_t> ds(0x10000);
    for (const auto pool : {0x4c16, 0x45b0, 0x4972, 0x47d8}) {
        const auto slots = pool == 0x4c16 || pool == 0x45b0 ? 16 : 8;
        const auto start = pool == 0x4c16 ? 0x6000 : pool == 0x45b0 ? 0x6800 : pool == 0x4972 ? 0x7000 : 0x7400;
        for (int slot = 0; slot < slots; ++slot) {
            const auto actor = static_cast<std::uint16_t>(start + slot * 100);
            putWord(ds, pool + slot * 2, actor);
            std::fill_n(ds.begin() + actor, 100, 0xa5);
            putWord(ds, actor + 0x1c, actor + 0x4f);
        }
    }
    putWord(ds, 0x522e, 0x7800);
    putWord(ds, 0x51f4, 3);
    putWord(ds, 0x47f8, 670);
    putWord(ds, 0x12d0, 77);
    Services services;
    stellar7::CampaignMemoryLoader loader(ds, services);
    loader.resetLevel();
    assert(services.cursor == 0);
    assert(services.released.empty());
    assert(ds[0x6000 + 0x4d] == 0xff);
    assert(ds[0x6800 + 0x4d] == 0xfe);
    assert(ds[0x6b20 + 0x4d] == 0xa5);
    assert(ds[0x7000 + 0x4d] == 0xfd);
    assert(getWord(ds, 0x7000 + 0x1c) == 0x704f);
    assert(ds[0x7400] == 0x88);
    assert(ds[0x7400 + 0x4d] == 0xfc);
    assert(ds[0x7800 + 0x4e] == 1);
    assert(getWord(ds, 0x47f8) == 680);
    assert(getWord(ds, 0x12d0) == 0);
}

void type6ConstructorTests() {
    using namespace stellar7;
    std::vector<Model3d> models(17);
    models[6].collisionRadius=884;models[6].flags=1;models[6].animationLimits={2};
    auto table=campaignLevelDefinition(5,0).table;table.actors={{6,2,-4,0x2400}};
    validateCampaignOrdinaryActors(table,models,{});
    CampaignOrdinaryConstructor constructor(5,{6},models,{});
    StellarRng rng;auto reference=rng;reference.next();reference.next();
    const auto actor=constructor.construct(6,rng);
    assert(rng.calls()==2 && rng.indexA()==reference.indexA() && rng.indexB()==reference.indexB());
    assert(actor.type==6 && actor.collisionRadius==884 && actor.forwardVelocity==180 && actor.weaponSlotCount==1);
    assert((actor.stateBytes4dTo52==std::array<std::uint8_t,6>{255,2,0,0,1,0}));
    assert(actor.behaviorTimer==0 && actor.type12MissGate==2 && actor.modelAnimationVariant==0);
    assert(actor.renderProfile==WorldActorRenderProfile::inlineSelectorNoCallback && !actor.animatedModelPresenterEnabled);
    assert(actor.type6CloakTimer==0);
    for(const auto invalid:{0,1}) {
        auto bad=models;
        if(invalid==0) {bad[6].flags=0;} else {bad[6].animationLimits={1};}
        bool rejected=false;
        try {CampaignOrdinaryConstructor invalidConstructor(5,{6},bad,{});}
        catch(const std::invalid_argument&) {rejected=true;}
        assert(rejected);
    }

    constexpr std::uint16_t physicalActor=0x6000;
    std::vector<std::uint8_t> ds(0x10000);
    putWord(ds,0x4c16,physicalActor);putWord(ds,physicalActor+0x55,0xabcd);
    ds[0x9ee + 6]=2;ds[0xabe + 6]=1;ds[0xa56+6]=1;putWord(ds,0xa00+12,180);
    Services services;services.random={0x0307,0x0010};
    CampaignMemoryLoader memory(ds,services);
    assert(memory.allocateOrdinary(6)==physicalActor && services.cursor==2 && services.presented.empty());
    assert(getWord(ds,physicalActor+0x55)==0 && getWord(ds,physicalActor+0x1c)==0);
    const auto decoded=decodeCampaignActor(ds,physicalActor,0);
    assert(decoded.type==6 && decoded.type12MissGate==2 && decoded.behaviorTimer==0 && decoded.type6CloakTimer==0);
    putWord(ds,physicalActor+0x55,0xabcd);
    assert(decodeCampaignActor(ds,physicalActor,0).type6CloakTimer==0xabcd);
    putWord(ds,physicalActor+2,12);
    const auto type12=decodeCampaignActor(ds,physicalActor,0);
    assert(type12.type6CloakTimer==0 && type12.type12MissGate==2);
}

void mixedLoaderBankTests(const std::vector<stellar7::Model3d>& shared,
    const std::vector<stellar7::Model3d>& local, const std::uint8_t level) {
    using namespace stellar7;
    const auto table=campaignLevelDefinition(level,0).table;
    validateCampaignLoaderActors(table,shared,local);
    StellarRng rng;auto reference=rng;
    const auto actors=constructCampaignLoaderActors(table,rng,shared,local);
    std::size_t allocation=0;
    for(std::size_t index=0;index<table.actors.size();++index) {
        const auto& entry=table.actors[index];
        const auto& actor=actors.at(allocation);
        assert(actor.allocationIndex==allocation);
        assert(actor.position.x==campaignCoordinate(entry.x) && actor.position.y==campaignCoordinate(entry.y));
        if(entry.type==29) {
            assert(actor.type==29+level && actor.collisionRadius==campaignModelRadius(shared,local,29+level));
            assert(actor.stateBytes4dTo52[0]==0xfe && actor.yaw==0);
            const bool special=(level==0 && index==5)||(level==3 && index==6);
            assert(actor.stateBytes4dTo52[1]==(special?16:100));
            assert(actor.stateBytes4dTo52[4]==(special?0xf3:0));
            assert(actor.renderProfile==WorldActorRenderProfile::noHandles && !actor.animatedModelPresenterEnabled);
        } else if(entry.type==24) {
            assert(actor.type==24 && actor.stateBytes4dTo52[0]==0xfc && actor.stateBytes4dTo52[1]==100);
            assert(actor.stateBytes4dTo52[2]==2 && actor.position.z==0 && actor.yaw==0);
            assert(actor.renderProfile==WorldActorRenderProfile::heapSelectorDeferredAnimation && actor.animatedModelPresenterEnabled);
            const auto& companion=actors.at(++allocation);
            assert(companion.allocationIndex==allocation && companion.type==31);
            assert(companion.collisionRadius==campaignModelRadius(shared,local,31));
            assert(companion.stateBytes4dTo52[0]==0xfe && companion.stateBytes4dTo52[1]==100 && companion.stateBytes4dTo52[2]==0);
            assert(companion.position.x==actor.position.x && companion.position.y==actor.position.y);
            assert(companion.renderProfile==WorldActorRenderProfile::inlineSelectorNoCallback && !companion.animatedModelPresenterEnabled);
        } else {
            reference.next();
            if(entry.type==10 || entry.type==13) {reference.next();}
            reference.next();
            const auto expectedType=entry.type==14 && level>4?level+10:entry.type;
            assert(actor.type==expectedType && actor.collisionRadius==campaignModelRadius(shared,local,entry.type));
            assert(actor.yaw==entry.yaw && actor.stateBytes4dTo52[0]==0xff);
            const auto defaults=campaignOrdinaryDefaults(entry.type);
            assert(actor.stateBytes4dTo52[1]==defaults.health && actor.stateBytes4dTo52[4]==defaults.behavior);
            assert(actor.weaponSlotCount==defaults.weaponSlots && actor.type6CloakTimer==0);
            if(entry.type==6) {assert(actor.renderProfile==WorldActorRenderProfile::inlineSelectorNoCallback);}
        }
        ++allocation;
    }
    assert(actors.size()==allocation && rng.calls()==reference.calls());
    assert(rng.next()==reference.next());
    if(level==2) {
        StellarRng compatibilityRng;
        const auto compatibility=constructLevel3LoaderActors(compatibilityRng,shared,local);
        assert(compatibility.size()==actors.size() && compatibilityRng.calls()+1==rng.calls());
        for(std::size_t index=0;index<actors.size();++index) {
            assert(compatibility[index].type==actors[index].type);
            assert(compatibility[index].stateBytes4dTo52==actors[index].stateBytes4dTo52);
            assert(compatibility[index].renderProfile==actors[index].renderProfile);
            assert(compatibility[index].yawVelocity==actors[index].yawVelocity);
        }
    }
}

void mixedLoaderTests() {
    using namespace stellar7;
    std::vector<Model3d> shared(44);
    for(auto& model:shared) {model.collisionRadius=100;}
    for(const auto type:{1,3,5,7}) {shared[type].flags=1;shared[type].animationLimits={4};}
    for(const auto type:{6,8,12,31}) {shared[type].flags=1;shared[type].animationLimits={2};}
    shared[11].flags=1;shared[11].animationLimits={8};
    shared[24].flags=1;shared[24].animationLimits={5};
    for(std::uint8_t level=0;level<7;++level) {mixedLoaderBankTests(shared,{},level);}
    auto interleaved=campaignLevelDefinition(1,0).table;
    interleaved.actors={{29,0,0,0}};
    interleaved.actors.insert(interleaved.actors.end(),10,{1,1,-1,0x1000});
    StellarRng substitutedRng;
    const auto substituted=constructCampaignLoaderActors(interleaved,substitutedRng,shared,{});
    assert(substituted.size()==11 && substitutedRng.calls()==20);
    for(std::size_t spawned=0;spawned<10;++spawned) {
        assert(substituted[spawned+1].type==(spawned>6 && spawned<10?4:1));
    }
    const auto table=campaignLevelDefinition(5,0).table;
    for(const auto invalid:{0,1,2,3,4}) {
        auto bad=shared;
        auto badTable=table;
        if(invalid==0) {bad[31].animationLimits={2,2};}
        if(invalid==1) {bad[24].animationLimits={0};}
        if(invalid==2) {badTable.actors.back().type=29;bad[34].flags=2;}
        if(invalid==3) {bad[6].animationLimits={2,2};}
        if(invalid==4) {badTable.actors.front().type=44;}
        StellarRng rng;
        bool rejected=false;
        try {constructCampaignLoaderActors(badTable,rng,bad,{});}
        catch(const std::invalid_argument&) {rejected=true;}
        assert(rejected && rng.calls()==0);
    }
    auto fixedOnly=campaignLevelDefinition(0,0).table;
    fixedOnly.actors={{29,1,2,0x3000}};
    StellarRng rng;
    assert(constructCampaignLoaderActors(fixedOnly,rng,shared,{}).size()==1 && rng.calls()==0);
    fixedOnly.actors.assign(17,{29,0,0,0});
    bool rejected=false;
    try {constructCampaignLoaderActors(fixedOnly,rng,shared,{});}
    catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected && rng.calls()==0);
}

void actualMixedLoaderTests(const std::filesystem::path& root) {
    const auto models=[&root](const std::string& name) {
        std::ifstream file(root/name,std::ios::binary);
        if(!file) {throw std::runtime_error("missing campaign model resource: "+name);}
        return stellar7::decodeFirstModelTableResource({std::istreambuf_iterator<char>(file),{}});
    };
    const auto shared=models("COCKPIT.RES");
    assert(shared.size()==44);
    for(std::uint8_t level=0;level<7;++level) {
        const auto local=models("LEVEL"+std::to_string(level+1)+".RES");
        mixedLoaderBankTests(shared,local,level);
        const auto definition=stellar7::campaignLevelDefinition(level,0);
        std::vector<std::uint8_t> types;
        for(std::size_t index=0;index<definition.weights.size();++index) {
            if(definition.weights[index]!=0) {types.push_back(static_cast<std::uint8_t>(index+1));}
        }
        const stellar7::CampaignOrdinaryConstructor constructor(level,types,shared,local);
        stellar7::StellarRng rng;
        for(const auto type:types) {
            const auto before=rng.calls();
            const auto actor=constructor.construct(type,rng);
            assert(rng.calls()-before==(type==10 || type==13?3:2));
            const auto profile=type==6?stellar7::WorldActorRenderProfile::inlineSelectorNoCallback
                : type==11 || type==12?stellar7::WorldActorRenderProfile::heapSelectorDeferredAnimation
                : stellar7::WorldActorRenderProfile::noHandles;
            assert(actor.renderProfile==profile && actor.animatedModelPresenterEnabled==(type==11 || type==12));
        }
    }
}

void constructionTests() {
    std::vector<std::uint8_t> ds(0x10000, 0xa5);
    Services services;
    stellar7::CampaignMemoryLoader loader(ds, services);
    std::uint16_t next = 0x6000;
    loader.constructPools([&next](std::size_t bytes) {
        assert(bytes == 99);
        const auto result = next;
        next = static_cast<std::uint16_t>(next + 100);
        return result;
    });
    assert(services.cursor == 0);
    assert(services.radiusTypes.size() == 48);
    assert(getWord(ds, 0x4c16) == 0x6000);
    assert(getWord(ds, 0x45b0) == 0x6640);
    const auto projectile = getWord(ds, 0x4972);
    assert(getWord(ds, projectile + 2) == 23);
    assert(ds[projectile + 98] == 0);
    assert(ds[projectile + 99] == 0xa5);
}
}

int main(int argc, char** argv) {
    transitionTests();
    definitionTests();
    allocatorTests();
    typedOrdinaryEntryTests();
    ordinaryConstructorTests();
    resetTests();
    constructionTests();
    type6ConstructorTests();
    mixedLoaderTests();
    if(argc>1) {actualMixedLoaderTests(argv[1]);}
    stellar7::Model3d sharedModel{};
    sharedModel.collisionRadius = 0xffff;
    sharedModel.scaleShift = 1;
    stellar7::Model3d localModel{};
    localModel.collisionRadius = 3;
    localModel.scaleShift = 4;
    assert(stellar7::campaignModelRadius({sharedModel}, {localModel}, 0) == 0xfffffffeU);
    assert(stellar7::campaignModelRadius({sharedModel}, {localModel}, 1) == 48);
    assert(stellar7::campaignModelRadius({sharedModel}, {localModel}, 2) == 0);
    std::vector<std::uint8_t> ds(0x1000);
    for (std::uint8_t level = 0; level < 7; ++level) {
        ds[0x962 + level] = 2;
        ds[0x994 + 14 + level] = static_cast<std::uint8_t>(20 + level);
        ds[0x9aa + level] = static_cast<std::uint8_t>(30 + level);
        for (const auto base : {0x96a, 0x9b2, 0x9c0, 0x9ce}) {
            putWord(ds, base + level * 2, 0xb00 + (base - 0x96a) * 4);
        }
    }
    ds[0xb00] = 9;
    ds[0xb01] = 29;
    putWord(ds, 0xc20, 0xffff);
    putWord(ds, 0xc22, 0x8000);
    putWord(ds, 0xc58, 7);
    putWord(ds, 0xc5a, 8);
    ds[0xc90] = 160;
    ds[0xc91] = 77;
    ds[0xc92] = 112;
    for (std::uint8_t level = 0; level < 7; ++level) {
        const auto table = stellar7::decodeCampaignLevel(ds, level, 2);
        assert(table.enemyLimit == 20 + level);
        assert(table.sky == 30 + level);
        assert(table.actors[0].x == -1);
        assert(table.actors[1].x == -32768);
        assert(table.actors[1].yaw == 112U << 8U);
        Recorder recorder;
        stellar7::initializeCampaignActors(table, recorder);
        assert(recorder.events[0] == "ordinary:9");
        assert(recorder.events[1] == "yaw:0:40960");
        assert(recorder.events[3] == "fixed:" + std::to_string(29 + level));
    }
    stellar7::CampaignLevelTable animated;
    animated.level = 5;
    animated.actors = {{24, -10, -6, 0}};
    Recorder recorder;
    stellar7::initializeCampaignActors(animated, recorder);
    assert(recorder.allocations == 2);
    assert(recorder.events[5] == "xy:0:-10:-6");
    assert(recorder.events[6] == "present:0");
    assert(recorder.events[7] == "fixed:31");
    assert(recorder.events[10] == "alias:1");
    assert(recorder.events[11] == "xy:1:-10:-6");
    stellar7::CampaignLevelTable overrides;
    overrides.level = 3;
    overrides.actors.resize(7, {29, 0, 0, 0});
    Recorder special;
    stellar7::initializeCampaignActors(overrides, special);
    assert(special.events[special.events.size() - 3] == "write:6:4:-13");
    assert(special.events[special.events.size() - 2] == "write:6:2:16");
    bool rejected = false;
    try {
        stellar7::decodeCampaignLevel(ds, 7, 0);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}

