#include "ship/controller/controldevice/controller/StickDriftFilter.h"

#include <algorithm>
#include <cmath>

namespace Ship {

namespace {

// All thresholds are per-direction magnitudes (of kMaxValue = 85) per tick.

// Below this a direction is centered: runtime state clears.
constexpr float kClear = 4.0f;

// Creep: the value keeps changing the same way at a slow average rate. Many
// devices report only on change, so it is judged per report, not per tick.
// A ghost measured on an Xbox Wireless Controller (Bluetooth, iOS) stepped
// 0.6 every ~25-30 ticks, or ~0.1 per tick right after a release; a
// deliberate slow steering ease is ~0.25 per tick or more. A creep is at
// least kCreepReports same-way reports, each under kCreepStep and slower than
// kCreepRate, spanning kCreepTics or more.
constexpr float kCreepStep = 1.0f;
constexpr float kCreepRate = 0.2f;
constexpr int16_t kCreepReports = 3;
constexpr int16_t kCreepTics = 30;

// Frozen: kStillTics consecutive moves smaller than kStill. A thumb holding
// part-way jitters more than this.
constexpr float kStill = 0.15f;
constexpr int16_t kStillTics = 8;

// Sticky release: a fall of at least kFastFall in one tick, after which the
// value is still at kStickyMin or more kStickyTics later without having risen
// (a re-push). A healthy stick reaches center within a few ticks.
constexpr float kFastFall = 3.0f;
constexpr float kStickyMin = 10.0f;
constexpr int16_t kStickyTics = 12;

// Drifty-direction rules.
constexpr int16_t kSlideReports = 2;  // slow same-way reports toward center
constexpr int16_t kSlideTics = 6;     // ...spanning this long = ghost decay
constexpr float kFlickRise = 8.0f;    // a rise this fast is a fresh flick
constexpr float kFullHold = 80.0f;    // frozen at/above this = a real full hold
constexpr float kMinSpan = 3.0f;      // travel kept above a zero near full tilt

} // namespace

void StickDriftFilter::ClearRuntime(int index) {
    mHalves[index] = {};
}

void StickDriftFilter::AddEvidence(int index, uint8_t weight) {
    mEvidence[index] = static_cast<uint8_t>(std::min<int>(mEvidence[index] + weight, UINT8_MAX));
}

bool StickDriftFilter::IsDrifty(Direction direction) const {
    return mEvidence[direction] >= kDriftyEvidence;
}

uint8_t StickDriftFilter::GetDriftyMask() const {
    uint8_t mask = 0;
    for (int i = 0; i < DirectionCount; i++) {
        if (mEvidence[i] >= kDriftyEvidence) {
            mask |= 1 << i;
        }
    }
    return mask;
}

void StickDriftFilter::SetDriftyMask(uint8_t mask) {
    for (int i = 0; i < DirectionCount; i++) {
        mEvidence[i] = (mask & (1 << i)) ? kDriftyEvidence : 0;
    }
}

void StickDriftFilter::Reset() {
    for (int i = 0; i < DirectionCount; i++) {
        ClearRuntime(i);
        mEvidence[i] = 0;
    }
}

float StickDriftFilter::FilterHalf(int index, float value) {
    Half& h = mHalves[index];
    const bool drifty = mEvidence[index] >= kDriftyEvidence;

    if (value < kClear) {
        // A fast fall straight to center is a clean release: evidence against
        // drift (e.g. a steady part-way steering hold that looked sticky).
        if (!drifty && h.last - value >= kFastFall && mEvidence[index] > 0) {
            mEvidence[index]--;
        }
        ClearRuntime(index);
        return value;
    }

    const float delta = value - h.last; // > 0 rising (away from center)

    // Fresh flick on a drifty direction: zero moves to where the rise began.
    if (drifty && delta >= kFlickRise) {
        if (!h.flicking) {
            const bool wasActive = h.out >= kClear;
            h.rest = h.last;
            h.flicking = true;
            h.released = false;
            h.creeps = h.creepTics = h.still = 0;
            h.last = value;
            if (wasActive) {
                // One neutral tick, so games that require leaving neutral
                // (e.g. tap jumps) see the flick start.
                h.out = 0.0f;
                return h.out;
            }
        }
        h.last = value;
        h.out = std::max(0.0f, (value - h.rest) * kMaxValue / std::max(kMaxValue - h.rest, kMinSpan));
        return h.out;
    }
    h.flicking = false;

    // Streaks. Unchanged ticks (no new report from the device) keep them.
    const float step = std::fabs(delta);
    h.sinceReport++;
    if (delta != 0.0f) {
        const int8_t dir = (delta > 0.0f) ? 1 : -1;
        const bool slow = step < kCreepStep && step / h.sinceReport < kCreepRate;
        if (slow && dir == h.creepDir) {
            h.creeps++;
            h.creepTics += h.sinceReport;
        } else if (slow) {
            h.creeps = 1;
            h.creepTics = h.sinceReport;
        } else {
            h.creeps = 0;
            h.creepTics = 0;
        }
        h.creepDir = dir;
        h.sinceReport = 0;
    }
    h.still = (step < kStill) ? h.still + 1 : 0;

    bool rezero = false;

    // Creep is drift on any stick, and no thumb produces it: one creep marks
    // the direction drifty at once.
    const bool creeping = h.creeps >= kCreepReports && h.creepTics >= kCreepTics;
    if (creeping && !h.creepCounted) {
        AddEvidence(index, kDriftyEvidence);
        h.creepCounted = true;
    }
    if (creeping) {
        rezero = true;
    }

    // Sticky-release evidence (any stick): a fast fall that stays short of
    // center.
    if (-delta >= kFastFall) {
        h.released = true;
        h.sinceRelease = 0;
    } else if (h.released && delta > 0.0f) {
        h.released = false; // pushed again: not a release
    }
    if (h.released && ++h.sinceRelease == kStickyTics) {
        if (value >= kStickyMin && value < kFullHold) {
            AddEvidence(index, 1);
        }
        h.released = false;
    }

    if (drifty) {
        const bool frozen = h.still >= kStillTics && value < kFullHold;
        const bool sliding = h.creepDir < 0 && h.creeps >= kSlideReports && h.creepTics >= kSlideTics;
        if (-delta >= kFastFall || sliding || frozen) {
            rezero = true;
        }
    }

    h.last = value;
    if (rezero || value < h.rest) {
        h.rest = value;
    }
    h.out = std::max(0.0f, (value - h.rest) * kMaxValue / std::max(kMaxValue - h.rest, kMinSpan));
    return h.out;
}

void StickDriftFilter::Update(float values[DirectionCount]) {
    for (int axis = 0; axis < 2; axis++) {
        const int pos = axis * 2;     // Right or Up
        const int neg = axis * 2 + 1; // Left or Down
        // Only one side of an axis is live at a time; the other is centered.
        if (values[pos] >= values[neg]) {
            values[pos] = FilterHalf(pos, values[pos]);
            values[neg] = FilterHalf(neg, 0.0f);
        } else {
            values[neg] = FilterHalf(neg, values[neg]);
            values[pos] = FilterHalf(pos, 0.0f);
        }
    }
}

} // namespace Ship
