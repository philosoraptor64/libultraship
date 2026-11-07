#pragma once

#include "ship/controller/controldevice/controller/mapping/ControllerAxisDirectionMapping.h"
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "ship/controller/controldevice/controller/mapping/keyboard/KeyboardScancodes.h"
#include "ship/controller/controldevice/controller/StickDriftFilter.h"
#include <string>

namespace Ship {

#define DEFAULT_STICK_SENSITIVITY_PERCENTAGE 100
#define DEFAULT_STICK_DEADZONE_PERCENTAGE 20
#define DEFAULT_NOTCH_SNAP_ANGLE 0

class ControllerStick {
  public:
    ControllerStick(uint8_t portIndex, StickIndex stickIndex);
    ~ControllerStick();

    void ReloadAllMappingsFromConfig();
    void AddDefaultMappings(PhysicalDeviceType physicalDeviceType);

    void ClearAllMappings();
    void ClearAllMappingsForDeviceType(PhysicalDeviceType physicalDeviceType);
    void UpdatePad(int8_t& x, int8_t& y);
    std::shared_ptr<ControllerAxisDirectionMapping> GetAxisDirectionMappingById(Direction direction, std::string id);
    std::unordered_map<Direction, std::unordered_map<std::string, std::shared_ptr<ControllerAxisDirectionMapping>>>
    GetAllAxisDirectionMappings();
    std::unordered_map<std::string, std::shared_ptr<ControllerAxisDirectionMapping>>
    GetAllAxisDirectionMappingByDirection(Direction direction);
    void AddAxisDirectionMapping(Direction direction, std::shared_ptr<ControllerAxisDirectionMapping> mapping);
    void ClearAxisDirectionMappingId(Direction direction, std::string id);
    void ClearAxisDirectionMapping(Direction direction, std::string id);
    void ClearAxisDirectionMapping(Direction direction, std::shared_ptr<ControllerAxisDirectionMapping> mapping);
    void SaveAxisDirectionMappingIdsToConfig();
    bool AddOrEditAxisDirectionMappingFromRawPress(Direction direction, std::string id);
    void Process(int8_t& x, int8_t& y);

    void ResetSensitivityToDefault();
    void SetSensitivity(uint8_t sensitivityPercentage);
    uint8_t GetSensitivityPercentage();
    bool SensitivityIsDefault();

    void ResetDeadzoneToDefault();
    void SetDeadzone(uint8_t deadzonePercentage);
    uint8_t GetDeadzonePercentage();
    bool DeadzoneIsDefault();

    void ResetNotchSnapAngleToDefault();
    void SetNotchSnapAngle(uint8_t notchSnapAngle);
    uint8_t GetNotchSnapAngle();
    bool NotchSnapAngleIsDefault();

    bool ProcessKeyboardEvent(KbEventType eventType, KbScancode scancode);
    bool ProcessMouseButtonEvent(bool isPressed, Ship::MouseBtn button);

    bool HasMappingsForPhysicalDeviceType(PhysicalDeviceType physicalDeviceType);
    StickIndex GetStickIndex();

    // Exposes the post-mapping, pre-Process() per-direction normalized magnitude
    // (0.0..MAX_AXIS_RANGE). Lets consumers implement custom stick-shaping
    // (e.g. per-axis remaps that need to bypass the circular deadzone /
    // octagonal gate / notch snap baked into Process()).
    // With drift correction on (the default), these are the drift-filtered
    // values from the last UpdatePad().
    float GetAxisDirectionValue(Direction direction);

    // Per-direction drift correction (see StickDriftFilter). The learned
    // drifty directions are saved per physical controller.
    void SetDriftCorrection(bool enabled);
    bool GetDriftCorrection();
    void ResetDriftProfile();
    uint8_t GetDriftyDirectionMask();

  private:
    double GetClosestNotch(double angle, double approximationThreshold);
    float GetRawAxisDirectionValue(Direction direction);
    void UpdateDriftFilter();
    std::string GetDriftProfileCVarName(const std::string& deviceKey);
    void LoadAxisDirectionMappingFromConfig(std::string id);

    uint8_t mPortIndex;
    StickIndex mStickIndex;

    uint8_t mSensitivityPercentage;
    float mSensitivity;

    // TODO: handle deadzones separately for X and Y?
    uint8_t mDeadzonePercentage;
    float mDeadzone;
    uint8_t mNotchSnapAngle;

    bool mDriftCorrection;
    StickDriftFilter mDriftFilter;
    float mFilteredAxisDirectionValues[4];
    std::string mDriftDeviceKey;
    uint8_t mDriftyMask;

    std::unordered_map<Direction, std::unordered_map<std::string, std::shared_ptr<ControllerAxisDirectionMapping>>>
        mAxisDirectionMappings;

    bool mUseEventInputToCreateNewMapping;
    KbScancode mKeyboardScancodeForNewMapping;
    MouseBtn mMouseButtonForNewMapping;
};
} // namespace Ship
