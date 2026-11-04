#pragma once

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>

namespace stellar7 {
//Transient index translation only. FF/fixed rows include retired physical owners;
//FD/FC identities retain their pool slots. Canonical storage remains in the model.
struct Level2CompactBridge {
    static constexpr std::size_t projectileBase = 32;
    static constexpr std::size_t effectBase = 40;
    static constexpr std::size_t keyEnd = 48;
    static constexpr std::size_t absent = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> semanticToCompact;
    std::vector<std::size_t> compactToSemantic;
    std::vector<std::size_t> physicalKeys;

    explicit Level2CompactBridge(const std::vector<std::size_t>& keys)
        : semanticToCompact(keys.size(),absent) {
        std::vector<bool> seen(keyEnd);
        for(std::size_t semantic=0;semantic<keys.size();++semantic) {
            const auto key=keys[semantic];
            if(key>=keyEnd || seen[key]) {throw std::invalid_argument("invalid Level2 physical bridge key");}
            seen[key]=true;
            if(key>=projectileBase) {continue;}
            semanticToCompact[semantic]=compactToSemantic.size();
            compactToSemantic.push_back(semantic);physicalKeys.push_back(key);
        }
    }

    std::size_t compact(const std::size_t semantic) const {
        if(semantic==absent) {return absent;}
        if(semantic>=semanticToCompact.size() || semanticToCompact[semantic]==absent) {
            throw std::invalid_argument("Level2 reference is not an FF/fixed owner");
        }
        return semanticToCompact[semantic];
    }

    template<class Actor> void project(Actor& actor) const {
        if(!actor.ownerIsPlayer) {actor.ownerAllocationIndex=compact(actor.ownerAllocationIndex);}
        if(!actor.collisionContactIsPlayer) {
            actor.collisionContactAllocationIndex=compact(actor.collisionContactAllocationIndex);
        }
    }
};
} // namespace stellar7
