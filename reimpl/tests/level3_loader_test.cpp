#include "stellar7/campaign_loader.hpp"
#include "stellar7/level1.hpp"
#include "stellar7/resource_codec.hpp"
#include "stellar7/audio.hpp"

#include <array>
#include <cassert>
#include <stdexcept>
#include <string>

namespace {
using namespace stellar7;

std::vector<Model3d> models() {
    std::vector<Model3d> result(32);
    //Distinct synthetic metadata proves that radii are resource-owned.
    for (const auto type : {2, 10, 24, 31}) {
        result[type].collisionRadius = static_cast<std::uint16_t>(100 + type);
    }
    for (const auto type : {24, 31}) {
        result[type].flags = 1;
        result[type].animationLimits = {7};
    }
    return result;
}

void construction() {
    const auto shared = models();
    std::array<std::uint16_t, StellarRng::wordCount> table{};
    for (std::size_t index = 0; index < table.size(); ++index) {
        table[index] = static_cast<std::uint16_t>(index * 947 + 13);
    }
    StellarRng rng(table, 40, 102, 123, 71);
    auto expectedRng = rng;
    std::array<std::int16_t, 3> yawVelocities{};
    for (auto& velocity : yawVelocities) {
        expectedRng.next();
        const auto word = expectedRng.next();
        const auto speed = static_cast<std::int16_t>((7 + ((word >> 8U) & 3U)) * 256);
        velocity = (word & 1U) != 0 ? -speed : speed;
        expectedRng.next();
    }
    expectedRng.next();
    expectedRng.next();
    validateLevel3LoaderActors(shared, {});
    assert(rng.calls() == 71);
    const auto actors = constructLevel3LoaderActors(rng, shared, {});
    assert(actors.size() == 6 && rng.calls() == 82);
    const std::array<std::int16_t, 6> types{10, 10, 10, 2, 24, 31};
    const std::array<std::int32_t, 6> xs{-16384, -28672, -20480, 16384, -20480, -20480};
    const std::array<std::int32_t, 6> ys{-8192, -4096, -20480, 24576, -12288, -12288};
    const std::array<std::uint16_t, 6> yaws{0, 8192, 0, 16384, 0, 0};
    for (std::size_t index = 0; index < actors.size(); ++index) {
        const auto& actor = actors[index];
        assert(actor.allocationIndex == index && actor.type == types[index]);
        assert(actor.position.x == xs[index] && actor.position.y == ys[index] && actor.position.z == 0);
        assert(actor.yaw == yaws[index] && actor.pitch == 0 && actor.roll == 0);
        assert(actor.collisionRadius == shared[types[index]].collisionRadius);
        assert(actor.flags == 0 && actor.modelAnimationVariant == 0 && actor.presentationModelAnimationVariant == 0);
        assert(actor.stateBytes4dTo52[3] == 0 && actor.weaponCooldown == 0);
        assert((actor.weaponSlotTimers == std::array<std::uint8_t, 3>{0, 0, 0}));
        assert(actor.xVelocity == 0 && actor.yVelocity == 0 && actor.zVelocity == 0);
        if (index < 4) {
            assert(actor.renderProfile == WorldActorRenderProfile::noHandles);
            assert(!actor.animatedModelPresenterEnabled);
            assert(actor.stateBytes4dTo52[0] == 0xff);
            assert(actor.stateBytes4dTo52[1] == (index == 3 ? 2 : 1));
            assert(actor.stateBytes4dTo52[4] == (index == 3 ? 1 : 2));
            assert(actor.weaponSlotCount == (index == 3 ? 1 : 3));
            assert(actor.forwardVelocity == (index == 3 ? 144 : 0));
            assert(actor.yawVelocity == (index == 3 ? 0 : yawVelocities[index]));
        } else {
            assert(actor.forwardVelocity == 0 && actor.yawVelocity == 0 && actor.weaponSlotCount == 0);
            assert(actor.stateBytes4dTo52[1] == 100);
        }
    }
    assert((actors[4].stateBytes4dTo52 == std::array<std::uint8_t, 6>{0xfc, 100, 2, 0, 0, 0}));
    assert(actors[4].renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation);
    assert(actors[4].animatedModelPresenterEnabled);
    assert((actors[5].stateBytes4dTo52 == std::array<std::uint8_t, 6>{0xfe, 100, 0, 0, 0, 0}));
    assert(actors[5].renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback);
    assert(!actors[5].animatedModelPresenterEnabled);
    for (std::size_t index = 0; index < StellarRng::wordCount; ++index) {
        assert(rng.next() == expectedRng.next());
    }
}

void invalidMetadata() {
    for (unsigned control = 0; control < 9; ++control) {
        auto shared = models();
        switch (control) {
        case 0: shared.resize(31); break;
        case 1: shared[10].flags = 1; break;
        case 2: shared[2].collisionRadius = 0; break;
        case 3: shared[24].flags = 0; break;
        case 4: shared[24].animationLimits = {}; break;
        case 5: shared[24].animationLimits = {7, 8}; break;
        case 6: shared[31].animationLimits = {0}; break;
        case 7: shared[31].flags = 2; break;
        case 8: shared[24].collisionRadius = 0; break;
        }
        StellarRng rng;
        auto expected = rng;
        bool rejected = false;
        try {
            constructLevel3LoaderActors(rng, shared, {});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected && rng.calls() == expected.calls());
        for (std::size_t index = 0; index < StellarRng::wordCount; ++index) {
            assert(rng.next() == expected.next());
        }
    }
}

class Order final : public CampaignActorLoader {
public:
    std::vector<std::string> events;
    Actor allocateOrdinary(std::uint8_t type) override { return allocate("ordinary", type); }
    Actor allocateFixed(std::uint8_t type) override { return allocate("fixed", type); }
    void write(Actor, CampaignActorField, std::int16_t) override {}
    void setPosition(Actor actor, std::int16_t, std::int16_t) override {
        events.push_back("xy" + std::to_string(actor));
    }
    void setYaw(Actor actor, std::uint16_t) override { events.push_back("yaw" + std::to_string(actor)); }
    void presentFixed(Actor actor) override { events.push_back("present" + std::to_string(actor)); }
    void aliasAnimation(Actor actor) override { events.push_back("alias" + std::to_string(actor)); }
private:
    std::size_t count_ = 0;
    Actor allocate(const std::string& kind, std::uint8_t type) {
        events.push_back(kind + std::to_string(type));
        return count_++;
    }
};

void musicPreflight() {
    SoundBlasterSynth synth;
    const auto before = synth.driverState();
    const auto writes = synth.registerWrites().size();
    for (unsigned fixture = 0; fixture < 3; ++fixture) {
        std::vector<std::uint8_t> sequence(fixture == 0 ? 0 : 36, 0);
        if (fixture == 1) {
            sequence[0] = 2;
        } else if (fixture == 2) {
            sequence[0] = 0x84;
            sequence[2] = 2;
        }
        bool rejected = false;
        try { synth.validateCampaignMusicLoad(sequence); } catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected && synth.registerWrites().size() == writes);
        const auto after = synth.driverState();
        assert(after.clockPhase == before.clockPhase && after.sequencePosition == before.sequencePosition);
        assert(after.notes == before.notes && after.velocities == before.velocities);
    }
    synth.validateCampaignMusicLoad(std::vector<std::uint8_t>(36, 0));
    assert(synth.registerWrites().size() == writes);
}

void operationOrder() {
    Order order;
    initializeCampaignActors(campaignLevelDefinition(2, 0).table, order);
    assert((order.events == std::vector<std::string>{
        "ordinary10", "yaw0", "xy0", "ordinary10", "yaw1", "xy1",
        "ordinary10", "yaw2", "xy2", "ordinary2", "yaw3", "xy3",
        "fixed24", "xy4", "present4", "fixed31", "alias5", "xy5"}));
}
}

int main() {
    operationOrder();
    construction();
    invalidMetadata();
    musicPreflight();
}
