#pragma once

#include <cstdint>

namespace Ship {

// Detects and compensates analog-stick drift, separately for each of the
// four directions (right, left, up, down).
//
// Worn or misreporting sticks show two signatures a thumb never produces:
//  - creep: the value slides slowly and steadily for many ticks, either back
//    toward center after a release or away from it while untouched;
//  - sticky release: the value falls fast (the stick was let go) and then
//    freezes short of center.
// Creep is unambiguous, so it is corrected on any stick. Each sticky release
// or creep adds evidence for that direction; a clean release (fast fall to
// center) removes some. A direction with enough evidence is drifty, and only
// then do the more aggressive rules apply to it: a fast fall is assumed to
// reach neutral, a frozen part-way value becomes the zero, and a fast rise
// counts as a fresh flick measured from where it started. Healthy directions
// pass through unchanged, so steady part-way holds (e.g. steering) survive.
//
// Correcting means moving that direction's zero ("rest") to the drifted
// value and rescaling the remaining travel to full range. Input and output
// are per-direction magnitudes in [0, kMaxValue], as libultraship's
// ControllerStick::GetAxisDirectionValue() reports them.
//
// Game-neutral and deterministic for a given input sequence. Call Update()
// exactly once per input poll.
class StickDriftFilter {
  public:
    enum Direction : uint8_t { Right = 0, Left = 1, Up = 2, Down = 3, DirectionCount = 4 };

    static constexpr float kMaxValue = 85.0f;
    // Sticky-release or creep events before a direction counts as drifty.
    static constexpr uint8_t kDriftyEvidence = 3;

    // values: raw per-direction magnitudes, indexed by Direction. Overwritten
    // with the filtered values.
    void Update(float values[DirectionCount]);

    bool IsDrifty(Direction direction) const;
    // Bit i set = direction i is drifty. For saving a per-controller profile.
    uint8_t GetDriftyMask() const;
    void SetDriftyMask(uint8_t mask);
    // Forget runtime state and learned evidence (e.g. on controller change).
    void Reset();

  private:
    struct Half {
        float rest;
        float last;
        float out;
        int16_t falls;   // consecutive small falls
        int16_t rises;   // consecutive tiny rises
        int16_t creeps;  // consecutive creep-sized moves in one direction
        int8_t creepDir; // sign of the current creep streak
        int16_t still;   // consecutive frozen ticks
        bool flicking;
        bool released;   // saw a fast fall; waiting to see where it settles
    };

    float FilterHalf(int index, float value);
    void ClearRuntime(int index);
    void AddEvidence(int index);

    Half mHalves[DirectionCount] = {};
    uint8_t mEvidence[DirectionCount] = {};
};

} // namespace Ship
