#pragma once

namespace stellar7 {
struct ChaseOptions {
    static constexpr int minimumDistance=2800,maximumDistance=7000,distanceStep=700;
    static constexpr int minimumHeight=1000,maximumHeight=3000,heightStep=200;
    int distance=4200,height=1800;
    bool operator==(const ChaseOptions& other) const {return distance==other.distance&&height==other.height;}
};
}
