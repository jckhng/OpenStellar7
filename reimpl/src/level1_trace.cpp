#include "stellar7/level1_trace.hpp"

#include <iomanip>
#include <sstream>

namespace stellar7 {
namespace {

void writeEscaped(std::ostream& output, const std::string& value) {
    output << '"';
    for (const unsigned char character : value) {
        switch (character) {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\b':
            output << "\\b";
            break;
        case '\f':
            output << "\\f";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (character < 0x20) {
                output << "\\u"
                       << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<unsigned>(character)
                       << std::dec << std::setfill(' ');
            } else {
                output << character;
            }
            break;
        }
    }
    output << '"';
}

void writePosition(std::ostream& output, const FixedPosition& position) {
    output << '['
           << position.x << ','
           << position.y << ','
           << position.z << ']';
}

void writeView(std::ostream& output, const ViewTransform& view) {
    output << "{\"origin\":";
    writePosition(output, view.origin);
    output << ",\"matrix_q14_column_major\":[";
    for (std::size_t index = 0; index < view.matrix.values.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << view.matrix.values[index];
    }
    output << "],\"matrix_mode\":" << view.matrix.mode
           << ",\"viewport\":["
           << view.viewport.left << ','
           << view.viewport.right << ','
           << view.viewport.top << ','
           << view.viewport.bottom << ']'
           << ",\"perspective_scale\":" << view.perspectiveScale
           << '}';
}

}  // namespace

std::string level1StateTraceJson(
    const Level1State& state,
    const std::string& boundary
) {
    std::ostringstream output;
    output << "{\"format_version\":1,\"boundary\":";
    writeEscaped(output, boundary);
    output << ",\"loop_tick\":" << state.loopTick()
           << ",\"level_index\":" << state.levelIndex()
           << ",\"presenter\":{\"type11_animation_variant\":"
           << static_cast<unsigned>(state.type11AnimationVariant())
           << '}'
           << ",\"player\":{\"type\":" << state.player().type
           << ",\"collision_radius\":" << state.player().collisionRadius
           << ",\"position\":";
    writePosition(output, state.player().position);
    output << ",\"angles\":["
           << state.player().pitch << ','
           << state.player().roll << ','
           << state.player().yaw
           << "],\"yaw_velocity\":" << state.player().yawVelocity
           << ",\"forward_velocity\":" << state.player().forwardVelocity
           << ",\"weapon_cooldown\":"
           << static_cast<unsigned>(state.playerWeaponCooldown())
           << ",\"projectile_slot_timers\":["
           << static_cast<unsigned>(
               state.playerProjectileTimers()[0]
           ) << ','
           << static_cast<unsigned>(
               state.playerProjectileTimers()[1]
           )
           << "],\"armor\":" << state.playerArmor()
           << "},\"mission_active\":"
           << (state.missionActive() ? "true" : "false")
           << ",\"failure_state\":"
           << (state.failureState() ? "true" : "false")
           << ",\"success_state\":"
           << (state.successState() ? "true" : "false")
           << ",\"exit_state\":"
           << (state.exitState() ? "true" : "false")
           << ",\"score\":" << state.score()
           << ",\"destroyed_type_streak\":{\"last_type\":"
           << state.lastDestroyedType()
           << ",\"count\":"
           << state.consecutiveDestroyedTypeCount()
           << "},\"systems\":[";
    for (std::size_t index = 0; index < Level1State::systemCount; ++index) {
        if (index != 0) {
            output << ',';
        }
        output << "{\"status\":"
               << static_cast<int>(state.systemStatusLevels()[index])
               << ",\"timer\":" << state.systemTimers()[index]
               << '}';
    }
    output
           << ']'
           << ",\"active_enemy_count\":" << state.activeEnemyCount()
           << ",\"spawned_enemy_count\":" << state.spawnedEnemyCount()
           << ",\"maximum_enemy_count\":" << state.maximumEnemyCount()
           << ",\"completion_object_allocation_index\":";
    const auto* completionObject = state.completionObject();
    if (completionObject == nullptr) {
        output << "null";
    } else {
        output << completionObject->allocationIndex;
    }
    output << ",\"post_wave_object_allocation_index\":";
    const auto postWaveObjectAllocationIndex =
        state.postWaveObjectAllocationIndex();
    if (postWaveObjectAllocationIndex >= state.entities().size()) {
        output << "null";
    } else {
        output << postWaveObjectAllocationIndex;
    }
    output
           << ",\"last_spawn_checkpoint\":";
    const auto* lastSpawn = state.lastSpawnCheckpoint();
    if (lastSpawn == nullptr) {
        output << "null";
    } else {
        output << "{\"loop_tick\":" << state.lastSpawnTick()
               << ",\"rng\":{\"index_a\":"
               << state.lastSpawnRng().indexA()
               << ",\"index_b\":" << state.lastSpawnRng().indexB()
               << ",\"last\":" << state.lastSpawnRng().last()
               << ",\"calls\":" << state.lastSpawnRng().calls()
               << "},\"entity\":{\"allocation_index\":"
               << lastSpawn->allocationIndex
               << ",\"flags\":" << static_cast<unsigned>(lastSpawn->flags)
               << ",\"type\":" << lastSpawn->type
               << ",\"collision_radius\":" << lastSpawn->collisionRadius
               << ",\"position\":";
        writePosition(output, lastSpawn->position);
        output << ",\"angles\":["
               << lastSpawn->pitch << ','
               << lastSpawn->roll << ','
               << lastSpawn->yaw
               << "],\"angular_velocity\":["
               << lastSpawn->pitchVelocity << ','
               << lastSpawn->rollVelocity << ','
               << lastSpawn->yawVelocity
               << "],\"linear_velocity\":["
               << lastSpawn->xVelocity << ','
               << lastSpawn->yVelocity << ','
               << lastSpawn->zVelocity
               << "],\"forward_velocity\":"
               << lastSpawn->forwardVelocity
               << ",\"state_bytes_4d_52\":[";
        for (
            std::size_t stateIndex = 0;
            stateIndex < lastSpawn->stateBytes4dTo52.size();
            ++stateIndex
        ) {
            if (stateIndex != 0) {
                output << ',';
            }
            output << static_cast<unsigned>(
                lastSpawn->stateBytes4dTo52[stateIndex]
            );
        }
        output << "],\"weapon_slot_count\":"
               << static_cast<unsigned>(lastSpawn->weaponSlotCount)
               << ",\"model_animation_variant\":"
               << static_cast<unsigned>(
                   lastSpawn->modelAnimationVariant
               )
               << ",\"weapon_cooldown\":"
               << static_cast<unsigned>(lastSpawn->weaponCooldown)
               << ",\"weapon_slot_timers\":[";
        for (
            std::size_t slot = 0;
            slot < lastSpawn->weaponSlotCount;
            ++slot
        ) {
            if (slot != 0) {
                output << ',';
            }
            output << static_cast<unsigned>(
                lastSpawn->weaponSlotTimers[slot]
            );
        }
        output << "]}}";
    }
    output << ",\"entities\":[";
    bool firstEntity = true;
    const auto writeEntity = [
        &output,
        &firstEntity
    ](const Level1Entity& entity) {
        if (!firstEntity) {
            output << ',';
        }
        firstEntity = false;
        output << "{\"allocation_index\":" << entity.allocationIndex
               << ",\"flags\":" << static_cast<unsigned>(entity.flags)
               << ",\"type\":" << entity.type
               << ",\"collision_radius\":" << entity.collisionRadius
               << ",\"position\":";
        writePosition(output, entity.position);
        output << ",\"angles\":["
               << entity.pitch << ',' << entity.roll << ',' << entity.yaw
               << "],\"angular_velocity\":["
               << entity.pitchVelocity << ','
               << entity.rollVelocity << ','
               << entity.yawVelocity
               << "],\"linear_velocity\":["
               << entity.xVelocity << ','
               << entity.yVelocity << ','
               << entity.zVelocity
               << "],\"forward_velocity\":" << entity.forwardVelocity
               << ",\"state_bytes_4d_52\":[";
        for (
            std::size_t stateIndex = 0;
            stateIndex < entity.stateBytes4dTo52.size();
            ++stateIndex
        ) {
            if (stateIndex != 0) {
                output << ',';
            }
            output << static_cast<unsigned>(
                entity.stateBytes4dTo52[stateIndex]
            );
        }
        output << "],\"behavior_timer\":"
               << static_cast<unsigned>(entity.behaviorTimer)
               << ",\"model_animation_variant\":"
               << static_cast<unsigned>(
                   entity.modelAnimationVariant
               )
               << ",\"presenter_distance\":" << entity.presenterDistance
               << ",\"target_heading\":" << entity.targetHeading
               << ",\"weapon_slot_count\":"
               << static_cast<unsigned>(entity.weaponSlotCount)
               << ",\"weapon_cooldown\":"
               << static_cast<unsigned>(entity.weaponCooldown)
               << ",\"weapon_slot_timers\":["
               << static_cast<unsigned>(entity.weaponSlotTimers[0]) << ','
               << static_cast<unsigned>(entity.weaponSlotTimers[1]) << ','
               << static_cast<unsigned>(entity.weaponSlotTimers[2]) << ']'
               << '}';
    };
    for (const auto& entity : state.entities()) {
        if (entity.active()) {
            writeEntity(entity);
        }
    }
    for (const auto& effect : state.deathEffects()) {
        if (effect.listed) {
            writeEntity(effect.entity);
        }
    }
    output << "],\"active_list_order\":[";
    for (
        std::size_t index = 0;
        index < state.semanticActiveListOrder().size();
        ++index
    ) {
        if (index != 0) {
            output << ',';
        }
        output << state.semanticActiveListOrder()[index];
    }
    output << "],\"projectile_order\":[";
    const auto projectileOrder = state.projectileOrder();
    for (std::size_t index = 0; index < projectileOrder.size(); ++index) {
        output << (index == 0 ? "" : ",") << projectileOrder[index];
    }
    output << "],\"projectile_pool\":[";
    for (std::size_t index = 0; index < state.projectiles().size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        const auto& projectile = state.projectiles()[index];
        output << "{\"pool_slot\":" << projectile.poolSlot
               << ",\"active\":"
               << (projectile.active() ? "true" : "false")
               << ",\"flags\":" << static_cast<unsigned>(projectile.flags)
               << ",\"type\":" << projectile.type
               << ",\"collision_radius\":" << projectile.collisionRadius
               << ",\"position\":";
        writePosition(output, projectile.position);
        output << ",\"angles\":["
               << projectile.pitch << ','
               << projectile.roll << ','
               << projectile.yaw
               << "],\"angular_velocity\":["
               << projectile.pitchVelocity << ','
               << projectile.rollVelocity << ','
               << projectile.yawVelocity
               << "],\"linear_velocity\":["
               << projectile.xVelocity << ','
               << projectile.yVelocity << ','
               << projectile.zVelocity
               << "],\"forward_velocity\":"
               << projectile.forwardVelocity
               << ",\"state_bytes_4d_54\":[";
        for (
            std::size_t stateIndex = 0;
            stateIndex < projectile.stateBytes4dTo54.size();
            ++stateIndex
        ) {
            if (stateIndex != 0) {
                output << ',';
            }
            output << static_cast<unsigned>(
                projectile.stateBytes4dTo54[stateIndex]
            );
        }
        output << "],\"owner_is_player\":"
               << (projectile.ownerIsPlayer ? "true" : "false")
               << ",\"owner_allocation_index\":";
        if (
            projectile.ownerAllocationIndex
            == static_cast<std::size_t>(-1)
        ) {
            output << "null";
        } else {
            output << projectile.ownerAllocationIndex;
        }
        output << '}';
    }
    output << "],\"audio_events\":[";
    for (std::size_t index = 0; index < state.audioEvents().size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        const auto& event = state.audioEvents()[index];
        output << "{\"loop_tick\":" << event.loopTick
               << ",\"event_id\":" << event.eventId
               << '}';
    }
    output << "],\"rng\":{\"index_a\":" << state.rng().indexA()
           << ",\"index_b\":" << state.rng().indexB()
           << ",\"last\":" << state.rng().last()
           << ",\"calls\":" << state.rng().calls()
           << "},\"views\":{\"main\":";
    writeView(output, initialLevel1MainView());
    output << ",\"radar\":";
    writeView(output, initialLevel1RadarView());
    output << "},\"ground_points\":[";
    const auto& ground = state.ground();
    for (std::size_t index = 0; index < ground.points().size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << '['
               << ground.points()[index].x << ','
               << ground.points()[index].y << ']';
    }
    output << "]}\n";
    return output.str();
}

}  // namespace stellar7
