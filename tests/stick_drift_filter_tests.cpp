#include <gtest/gtest.h>
#include "ship/controller/controldevice/controller/StickDriftFilter.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <vector>

using Ship::StickDriftFilter;
using Dir = StickDriftFilter::Direction;

namespace {

// Feeds one direction's raw magnitude per tick (others centered) and returns
// that direction's filtered values.
std::vector<float> Feed(StickDriftFilter& filter, Dir dir, const std::vector<float>& raw) {
    std::vector<float> out;
    for (float value : raw) {
        float values[StickDriftFilter::DirectionCount] = {};
        values[dir] = value;
        filter.Update(values);
        out.push_back(values[dir]);
    }
    return out;
}

// Deterministic thumb jitter in [-amplitude, amplitude].
float Jitter(float amplitude) {
    static unsigned state = 12345;
    state = state * 1103515245u + 12345u;
    return (((state >> 16) % 1000) / 1000.0f - 0.5f) * 2.0f * amplitude;
}

void Append(std::vector<float>& v, std::initializer_list<float> xs) {
    v.insert(v.end(), xs);
}

void Hold(std::vector<float>& v, float value, int ticks, float jitter = 0.0f) {
    for (int i = 0; i < ticks; i++) {
        v.push_back(value + Jitter(jitter));
    }
}

// The ghost measured on an Xbox Wireless Controller (Bluetooth, iOS): after
// letting go of a full flick, the value drops fast to `stuck`, then creeps
// toward center very slowly while the stick is physically centered.
void GhostCycle(std::vector<float>& v, float stuck, int ticks) {
    Append(v, { 85, 85, 85, 85, 70, stuck });
    for (int i = 0; i < ticks; i++) {
        v.push_back(stuck - 0.05f * i);
    }
}

float MaxError(const std::vector<float>& a, const std::vector<float>& b) {
    float err = 0.0f;
    for (size_t i = 0; i < a.size(); i++) {
        err = std::max(err, std::fabs(a[i] - b[i]));
    }
    return err;
}

constexpr float kNeutral = 4.0f;
constexpr float kTapJump = 0.62f * StickDriftFilter::kMaxValue; // SSB64 tap jump: 53 of ~85

} // namespace

TEST(StickDriftFilter, HealthyFlicksPassThrough) {
    StickDriftFilter filter;
    std::vector<float> raw;
    for (int i = 0; i < 10; i++) {
        Hold(raw, 0, 20);
        Hold(raw, 85, 6);
    }
    EXPECT_LT(MaxError(Feed(filter, Dir::Up, raw), raw), 0.01f);
    EXPECT_FALSE(filter.IsDrifty(Dir::Up));
}

TEST(StickDriftFilter, HealthySteeringPassesThrough) {
    // Ramps, jittery part-way holds, a snap to half held perfectly still (as a
    // quantized controller reports it), and clean releases.
    StickDriftFilter filter;
    std::vector<float> raw;
    for (int r = 0; r < 12; r++) {
        for (int i = 0; i <= 10; i++) {
            raw.push_back(8.5f * i);
        }
        Hold(raw, 50, 90, 0.6f);
        Append(raw, { 85, 85, 45 });
        Hold(raw, 45, 60);
        Append(raw, { 20, 0 });
        Hold(raw, 0, 30);
    }
    EXPECT_LT(MaxError(Feed(filter, Dir::Right, raw), raw), 0.01f);
    EXPECT_FALSE(filter.IsDrifty(Dir::Right));
}

TEST(StickDriftFilter, SlowDeliberateSteeringPassesThrough) {
    StickDriftFilter filter;
    std::vector<float> raw;
    for (int r = 0; r < 5; r++) {
        for (int i = 0; i < 150; i++) {
            raw.push_back(20 + 0.3f * i);
        }
        for (int i = 0; i < 150; i++) {
            raw.push_back(65 - 0.25f * i);
        }
        Append(raw, { 0 });
        Hold(raw, 0, 10);
    }
    EXPECT_LT(MaxError(Feed(filter, Dir::Right, raw), raw), 0.01f);
    EXPECT_FALSE(filter.IsDrifty(Dir::Right));
}

TEST(StickDriftFilter, LearnsStickyUpAndReadsReleaseAsNeutral) {
    StickDriftFilter filter;
    std::vector<float> raw;
    for (int c = 0; c < 6; c++) {
        GhostCycle(raw, 38, 150);
    }
    const auto out = Feed(filter, Dir::Up, raw);
    ASSERT_TRUE(filter.IsDrifty(Dir::Up));
    EXPECT_FALSE(filter.IsDrifty(Dir::Down));
    EXPECT_FALSE(filter.IsDrifty(Dir::Left));
    EXPECT_FALSE(filter.IsDrifty(Dir::Right));

    // Once learned, the stuck value reads neutral from the tick it lands.
    for (size_t i = raw.size() - 150; i < raw.size(); i++) {
        EXPECT_LT(out[i], kNeutral) << "tick " << i;
    }

    // A flick from the stuck value is a fresh full flick.
    const auto flick = Feed(filter, Dir::Up, { 33, 33, 85, 85 });
    EXPECT_LT(flick[1], kNeutral);
    EXPECT_GE(flick[2], kTapJump);
}

TEST(StickDriftFilter, LearnsPerDirection) {
    StickDriftFilter filter;
    std::vector<float> raw;
    for (int c = 0; c < 6; c++) {
        GhostCycle(raw, 38, 150);
    }
    Feed(filter, Dir::Left, raw);
    EXPECT_TRUE(filter.IsDrifty(Dir::Left));
    EXPECT_FALSE(filter.IsDrifty(Dir::Up));
}

TEST(StickDriftFilter, CancelsCreepAwayFromCenter) {
    StickDriftFilter filter;
    std::vector<float> raw;
    for (int i = 0; i < 400; i++) {
        raw.push_back(5 + 0.1f * i);
    }
    const auto out = Feed(filter, Dir::Up, raw);
    for (size_t i = 40; i < out.size(); i++) {
        EXPECT_LT(out[i], kNeutral) << "tick " << i;
    }
}

TEST(StickDriftFilter, CancelsFastGhostDecayBeforeLearning) {
    StickDriftFilter filter;
    std::vector<float> raw;
    Hold(raw, 0, 5);
    Hold(raw, 85, 5);
    for (int i = 0; i < 300; i++) {
        raw.push_back(84 - 0.14f * i);
    }
    const auto out = Feed(filter, Dir::Up, raw);
    for (size_t i = 60; i < out.size(); i++) {
        EXPECT_LT(out[i], kNeutral) << "tick " << i;
    }
}

TEST(StickDriftFilter, DriftyFullHoldStaysFull) {
    StickDriftFilter filter;
    filter.SetDriftyMask(1 << Dir::Up);
    std::vector<float> raw;
    Hold(raw, 0, 5);
    Hold(raw, 85, 200);
    const auto out = Feed(filter, Dir::Up, raw);
    for (size_t i = 6; i < out.size(); i++) {
        EXPECT_GT(out[i], 84.0f) << "tick " << i;
    }
}

TEST(StickDriftFilter, DriftyJitteryPartialHoldSurvives) {
    StickDriftFilter filter;
    filter.SetDriftyMask(1 << Dir::Up);
    std::vector<float> raw;
    Hold(raw, 0, 5);
    for (int i = 0; i <= 5; i++) {
        raw.push_back(10.0f * i);
    }
    Hold(raw, 50, 120, 0.6f);
    const auto out = Feed(filter, Dir::Up, raw);
    for (size_t i = 20; i < out.size(); i++) {
        EXPECT_GT(out[i], 35.0f) << "tick " << i;
    }
}

TEST(StickDriftFilter, DriftyFlickFromUncorrectedCreepGetsNeutralTick) {
    StickDriftFilter filter;
    filter.SetDriftyMask(1 << Dir::Up);
    std::vector<float> raw;
    Hold(raw, 0, 5);
    for (int i = 0; i < 25; i++) {
        raw.push_back(22 + 0.15f * i);
    }
    const size_t at = raw.size();
    Append(raw, { 85, 85, 85 });
    const auto out = Feed(filter, Dir::Up, raw);
    ASSERT_GE(out[at - 1], kNeutral); // the creep is not corrected yet
    EXPECT_LT(out[at], kNeutral);
    EXPECT_GE(out[at + 1], kTapJump);
}

TEST(StickDriftFilter, DriftyMaskRoundTrips) {
    StickDriftFilter filter;
    filter.SetDriftyMask(0b0110);
    EXPECT_EQ(filter.GetDriftyMask(), 0b0110);
    EXPECT_TRUE(filter.IsDrifty(Dir::Left));
    EXPECT_TRUE(filter.IsDrifty(Dir::Up));
    filter.Reset();
    EXPECT_EQ(filter.GetDriftyMask(), 0);
}
