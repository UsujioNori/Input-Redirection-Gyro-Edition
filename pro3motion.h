#pragma once

#include <SDL.h>
#include <QByteArray>
#include <QtGlobal>
#include <QSettings>

extern QSettings settings;
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>

#if !SDL_VERSION_ATLEAST(2, 0, 14)
#error SDL 2.0.14 or newer is required for controller sensors
#endif

struct Pro3MotionFrame {
    double leftX = 0, leftY = 0, rightX = 0, rightY = 0;
    bool zl = false, zr = false, hasGyro = false;
    std::uint32_t standardButtons = 0;
    std::uint32_t dpad = 0, extraButtons = 0;
    std::int16_t gyroX = 0, gyroY = 0, gyroZ = 0;
    bool hasAccel = false;
    std::int16_t accelX = 0, accelY = 0, accelZ = 0;
};

constexpr double Pro3GyroCountsPerRadian = 14.375 * 180.0 / 3.141592653589793;

class Pro3Gravity {
public:
    using Clock = std::chrono::steady_clock;

    void update(Pro3MotionFrame &frame, Clock::time_point now) {
        if (!frame.hasGyro) {
            ready = false;
            return;
        }

        const double seconds = std::chrono::duration<double>(now - previous).count();
        previous = now;
        const bool stationary = !(frame.gyroX || frame.gyroY || frame.gyroZ);
        const double x = frame.accelX, y = frame.accelY, z = frame.accelZ;
        const double length = std::sqrt(x*x + y*y + z*z);
        const bool validGravity = frame.hasAccel && length >= 400.0 && length <= 625.0;
        if (!ready && validGravity && stationary) {
            neutral = gravity = {512.0*x/length, 512.0*y/length, 512.0*z/length};
            ready = true;
            neutralSeconds = 0.0;
        } else if (ready && seconds > 0.0 && seconds <= 0.1 && !stationary) {
            const Vector rate = {double(frame.gyroX), double(frame.gyroY), double(frame.gyroZ)};
            const double speed = std::sqrt(rate.x*rate.x + rate.y*rate.y + rate.z*rate.z);
            if (speed > 0.0) {
                // Gravity rotates opposite to the controller in its local frame.
                const Vector axis = {rate.x/speed, rate.y/speed, rate.z/speed};
                const double angle = -speed * seconds / Pro3GyroCountsPerRadian;
                const double cosine = std::cos(angle), sine = std::sin(angle);
                const Vector cross = {
                    axis.y*gravity.z - axis.z*gravity.y,
                    axis.z*gravity.x - axis.x*gravity.z,
                    axis.x*gravity.y - axis.y*gravity.x
                };
                const double projection =
                    (axis.x*gravity.x + axis.y*gravity.y + axis.z*gravity.z) * (1.0 - cosine);
                gravity = {
                    gravity.x*cosine + cross.x*sine + axis.x*projection,
                    gravity.y*cosine + cross.y*sine + axis.y*projection,
                    gravity.z*cosine + cross.z*sine + axis.z*projection
                };
            }
        }
        // Re-anchor only in the original grip, never during a held tilt.
        constexpr double cosFiveDegrees = 0.996194698;
        if (ready && validGravity && stationary && seconds > 0.0 && seconds <= 0.1 &&
            x*neutral.x + y*neutral.y + z*neutral.z >= length * 512.0 * cosFiveDegrees) {
            neutralSeconds += seconds;
            if (neutralSeconds >= 0.35)
                gravity = neutral;
        } else {
            neutralSeconds = 0.0;
        }
        if (ready) {
            frame.hasAccel = true;
            frame.accelX = std::lround(gravity.x);
            frame.accelY = std::lround(gravity.y);
            frame.accelZ = std::lround(gravity.z);
        }
    }

private:
    struct Vector { double x, y, z; };
    Vector gravity {};
    Vector neutral {};
    double neutralSeconds = 0.0;
    Clock::time_point previous {};
    bool ready = false;
};

struct SdlExtraButton {
    SDL_GameControllerButton button;
    const char *key;
    const char *label;
};

constexpr SdlExtraButton sdlExtraButtons[] = {
    {SDL_CONTROLLER_BUTTON_GUIDE, "Guide", "Home / PS"},
    {SDL_CONTROLLER_BUTTON_LEFTSTICK, "L3", "L3"},
    {SDL_CONTROLLER_BUTTON_RIGHTSTICK, "R3", "R3"},
    {SDL_CONTROLLER_BUTTON_MISC1, "Misc1", "Capture / Mute"},
    {SDL_CONTROLLER_BUTTON_TOUCHPAD, "Touchpad", "Touchpad click"},
    {SDL_CONTROLLER_BUTTON_PADDLE1, "Paddle1", "Extra 1"},
    {SDL_CONTROLLER_BUTTON_PADDLE2, "Paddle2", "Extra 2"},
    {SDL_CONTROLLER_BUTTON_PADDLE3, "Paddle3", "Extra 3"},
    {SDL_CONTROLLER_BUTTON_PADDLE4, "Paddle4", "Extra 4"}
};

class Pro3Motion {
public:
    Pro3Motion(const QByteArray &selector, bool pro3Only)
        : selector(selector), pro3Only(pro3Only) {
        SDL_SetHint("SDL_GAMECONTROLLER_USE_BUTTON_LABELS", "0");
        initialized = SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0;
    }
    ~Pro3Motion() {
        if (controller)
            SDL_GameControllerClose(controller);
        if (initialized)
            SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    }

    bool read(Pro3MotionFrame &frame) {
        if (!updateConnection())
            return false;

        SDL_Joystick *joystick = SDL_GameControllerGetJoystick(controller);
        if (usesPro3Mapping()) {
            frame.leftX = axis(joystick, 0);
            frame.leftY = axis(joystick, 1);
            frame.rightX = axis(joystick, 2);
            frame.rightY = axis(joystick, 3);
            frame.zl = axis(joystick, 4) > 0.0;
            frame.zr = axis(joystick, 5) > 0.0;
            frame.dpad = readDpad(joystick);
            if (SDL_JoystickGetButton(joystick, 11))
                frame.extraButtons |= buttonMask("INPUTREDIRECTION_PRO3_L4", "Pro3/L4");
            if (SDL_JoystickGetButton(joystick, 12))
                frame.extraButtons |= buttonMask("INPUTREDIRECTION_PRO3_R4", "Pro3/R4");
            if (SDL_JoystickGetButton(joystick, 13))
                frame.extraButtons |= buttonMask("INPUTREDIRECTION_PRO3_PL", "Pro3/PL");
            if (SDL_JoystickGetButton(joystick, 14))
                frame.extraButtons |= buttonMask("INPUTREDIRECTION_PRO3_PR", "Pro3/PR");
        } else {
            readMappedControls(frame);
        }

        float motion[3];
        if (gyro && SDL_GameControllerGetSensorData(controller, SDL_SENSOR_GYRO, motion, 3) == 0 &&
            std::isfinite(motion[0]) && std::isfinite(motion[1]) && std::isfinite(motion[2])) {
            frame.hasGyro = true;
            frame.gyroX = gyroCounts(-motion[0]);
            frame.gyroY = gyroCounts(motion[2]);
            frame.gyroZ = gyroCounts(-motion[1]);
        }
        float acceleration[3];
        if (frame.hasGyro && accel &&
            SDL_GameControllerGetSensorData(controller, SDL_SENSOR_ACCEL, acceleration, 3) == 0 &&
            std::isfinite(acceleration[0]) && std::isfinite(acceleration[1]) &&
            std::isfinite(acceleration[2])) {
            frame.hasAccel = true;
            frame.accelX = accelCounts(-acceleration[0]);
            frame.accelY = accelCounts(acceleration[2]);
            frame.accelZ = accelCounts(acceleration[1]);
        }
        return true;
    }

    bool updateConnection() {
        if (!initialized)
            return false;

        SDL_PumpEvents();
        if (controller && !SDL_GameControllerGetAttached(controller)) {
            SDL_GameControllerClose(controller);
            controller = nullptr;
        }
        if (!controller)
            reconnect();
        return controller != nullptr;
    }

    bool usesPro3Mapping() const { return rawPro3; }

    bool hasMappedButton(SDL_GameControllerButton button) const {
        return controller && SDL_GameControllerHasButton(controller, button);
    }

private:
    void reconnect() {
        using Clock = std::chrono::steady_clock;
        const auto now = Clock::now();
        if (now < nextScan)
            return;
        nextScan = now + std::chrono::seconds(1);
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            const char *name = SDL_JoystickNameForIndex(i);
            if (!name || !SDL_IsGameController(i))
                continue;
            const bool isPro3 = std::strstr(name, "8BitDo Pro 3") != nullptr;
            if ((pro3Only && !isPro3) ||
                (!selector.isEmpty() && selector != "1" &&
                 !QByteArray(name).toLower().contains(selector.toLower())))
                continue;
            auto *candidate = SDL_GameControllerOpen(i);
            if (!candidate)
                continue;
            auto *joystick = SDL_GameControllerGetJoystick(candidate);
            rawPro3 = selector.isEmpty() && isPro3 && SDL_JoystickNumAxes(joystick) >= 6 &&
                SDL_JoystickNumButtons(joystick) >= 15;
            if (pro3Only && !rawPro3) {
                SDL_GameControllerClose(candidate);
                continue;
            }
            controller = candidate;
            gyro = SDL_GameControllerHasSensor(controller, SDL_SENSOR_GYRO) &&
                SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_GYRO, SDL_TRUE) == 0;
            accel = SDL_GameControllerHasSensor(controller, SDL_SENSOR_ACCEL) &&
                SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_ACCEL, SDL_TRUE) == 0;
            break;
        }
    }

    void readMappedControls(Pro3MotionFrame &frame) const {
        frame.leftX = normalizeAxis(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX));
        frame.leftY = normalizeAxis(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY));
        frame.rightX = normalizeAxis(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX));
        frame.rightY = normalizeAxis(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY));
        frame.zl = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 8192;
        frame.zr = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 8192;

        constexpr SDL_GameControllerButton layout[] = {
            SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_A,
            SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START,
            SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SDL_CONTROLLER_BUTTON_DPAD_LEFT,
            SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN,
            SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
            SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_X
        };
        for (unsigned bit = 0; bit < sizeof(layout) / sizeof(layout[0]); ++bit) {
            if (SDL_GameControllerGetButton(controller, layout[bit]))
                frame.standardButtons |= 1u << bit;
        }
        for (const auto &extra : sdlExtraButtons) {
            if (SDL_GameControllerGetButton(controller, extra.button))
                frame.extraButtons |= settings.value(QStringLiteral("SdlPad/") +
                    QString::fromLatin1(extra.key), 0u).toUInt() & 0x7fffu;
        }
    }

    std::uint32_t readDpad(SDL_Joystick *joystick) const {
        constexpr SDL_GameControllerButton directions[] = {
            SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SDL_CONTROLLER_BUTTON_DPAD_LEFT,
            SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN
        };
        constexpr std::uint8_t hatDirections[] = {
            SDL_HAT_RIGHT, SDL_HAT_LEFT, SDL_HAT_UP, SDL_HAT_DOWN
        };
        bool hasHat = SDL_JoystickNumHats(joystick) > 0;
        const auto hat = hasHat ? SDL_JoystickGetHat(joystick, 0) : 0;
        std::uint32_t mask = 0;
        for (unsigned i = 0; i < 4; ++i) {
            if (hasHat) {
                if (hat & hatDirections[i])
                    mask |= 0x10u << i;
                continue;
            }
            const auto binding = SDL_GameControllerGetBindForButton(controller, directions[i]);
            if (binding.bindType == SDL_CONTROLLER_BINDTYPE_BUTTON &&
                binding.value.button >= 11 && binding.value.button <= 14)
                continue;
            if (SDL_GameControllerGetButton(controller, directions[i]))
                mask |= 0x10u << i;
        }
        return mask;
    }

    static double axis(SDL_Joystick *joystick, int index) {
        return normalizeAxis(SDL_JoystickGetAxis(joystick, index));
    }

    static double normalizeAxis(int value) {
        return value / (value < 0 ? 32768.0 : 32767.0);
    }

    static std::int16_t gyroCounts(float radiansPerSecond) {
        constexpr double countsPerRadian = 14.375 * 180.0 / 3.141592653589793;
        // Suppress stationary sensor noise without lowering the response slope.
        constexpr double deadzone = 0.055;
        const double speed = std::abs(static_cast<double>(radiansPerSecond));
        const double adjusted = std::copysign(std::max(0.0, speed - deadzone), radiansPerSecond);
        const double counts = adjusted * countsPerRadian * 0.75;
        return std::lround(std::max(-32767.0, std::min(32767.0, counts)));
    }

    static std::uint32_t buttonMask(const char *environmentKey, const char *settingKey) {
        const auto override = qgetenv(environmentKey);
        return (override.isEmpty() ? settings.value(settingKey, 0).toUInt()
                                   : override.toUInt(nullptr, 0)) & 0xfff;
    }

    const QByteArray selector;
    const bool pro3Only;
    bool rawPro3 = false;
    bool initialized;
    static std::int16_t accelCounts(float metersPerSecondSquared) {
        constexpr double countsPerG = 512.0;
        const double counts = metersPerSecondSquared * (countsPerG / SDL_STANDARD_GRAVITY);
        return std::lround(std::max(-930.0, std::min(930.0, counts)));
    }

    bool gyro = false;
    bool accel = false;
    SDL_GameController *controller = nullptr;
    std::chrono::steady_clock::time_point nextScan {};
};


inline Pro3Motion &selectedSdlController() {
    static const auto selector = qgetenv("INPUTREDIRECTION_SDL_PAD");
    static Pro3Motion controller(selector,
        selector.isEmpty() && qgetenv("INPUTREDIRECTION_PRO3") == "1");
    return controller;
}
