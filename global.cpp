#include "global.h"
#include <math.h>
#include "gpmanager.h"
#ifdef ENABLE_SDL_PRO3
#include "pro3motion.h"
#include <QDebug>
#endif

QSettings settings("TuxSH", "InputRedirectionClient-Qt");

Worker worker;
Settings btnSettings;
double tsRatio;

std::vector<ShortCut> listShortcuts;

QGamepadManager::GamepadButtons buttons = 0;
u32 interfaceButtons = 0;
int yAxisMultiplier = 1, yAxisMultiplierCpp = 1;
std::atomic<bool> rotateNew3dsCStick{settings.value("rotateNew3dsCStick", false).toBool()};
std::atomic<bool> invertGyroYaw{settings.value("invertGyroYaw", false).toBool()};
bool shouldSwapStick = false;
int CPAD_BOUND = (settings.contains("StickBound") ? settings.value("StickBound").toInt() : 1488);
int CPP_BOUND = (settings.contains("CppBound") ? settings.value("CppBound").toInt() : 127);

GamepadConfigurator *gpConfigurator;

QString ipAddress;

bool touchScreenPressed;
QSize touchScreenSize = QSize(TOUCH_SCREEN_WIDTH, TOUCH_SCREEN_HEIGHT);
QPoint touchScreenPosition;

QGamepadManager::GamepadButton homeButton = variantToButton(settings.value("ButtonHome", QGamepadManager::ButtonInvalid));
QGamepadManager::GamepadButton powerButton = variantToButton(settings.value("ButtonPower", QGamepadManager::ButtonInvalid));
QGamepadManager::GamepadButton powerLongButton = variantToButton(settings.value("ButtonPowerLong", QGamepadManager::ButtonInvalid));

QGamepadManager::GamepadButton touchButton1 = variantToButton(settings.value("ButtonT1", QGamepadManager::ButtonInvalid));
QGamepadManager::GamepadButton touchButton2 = variantToButton(settings.value("ButtonT2", QGamepadManager::ButtonInvalid));
QGamepadManager::GamepadButton touchButton3 = variantToButton(settings.value("ButtonT3", QGamepadManager::ButtonInvalid));
QGamepadManager::GamepadButton touchButton4 = variantToButton(settings.value("ButtonT4", QGamepadManager::ButtonInvalid));

TouchButton tbOne={.x=settings.value("touchButton1X").toInt(),
                   .y=settings.value("touchButton1Y").toInt()};

TouchButton tbTwo={.x=settings.value("touchButton2X").toInt(),
                   .y=settings.value("touchButton2Y").toInt()};

TouchButton tbThree={.x=settings.value("touchButton3X").toInt(),
                     .y=settings.value("touchButton3Y").toInt()};

TouchButton tbFour={.x=settings.value("touchButton4X").toInt(),
                    .y=settings.value("touchButton4Y").toInt()};

QGamepadManager::GamepadButton hidButtonsAB[2]={
variantToButton(settings.value("ButtonA", QGamepadManager::ButtonA)),
variantToButton(settings.value("ButtonB", QGamepadManager::ButtonB))};

QGamepadManager::GamepadButton hidButtonsMiddle[8] ={
variantToButton(settings.value("ButtonSelect", QGamepadManager::ButtonSelect)),
variantToButton(settings.value("ButtonStart", QGamepadManager::ButtonStart)),
variantToButton(settings.value("ButtonRight", QGamepadManager::ButtonRight)),
variantToButton(settings.value("ButtonLeft", QGamepadManager::ButtonLeft)),
variantToButton(settings.value("ButtonUp", QGamepadManager::ButtonUp)),
variantToButton(settings.value("ButtonDown", QGamepadManager::ButtonDown)),
variantToButton(settings.value("ButtonR", QGamepadManager::ButtonR1)),
variantToButton(settings.value("ButtonL", QGamepadManager::ButtonL1))};

QGamepadManager::GamepadButton hidButtonsXY[2] = {
    variantToButton(settings.value("ButtonX", QGamepadManager::ButtonX)),
    variantToButton(settings.value("ButtonY", QGamepadManager::ButtonY))};

QGamepadManager::GamepadButton irButtons[2] = {
    variantToButton(settings.value("ButtonZR", QGamepadManager::ButtonR2)),
    variantToButton(settings.value("ButtonZL", QGamepadManager::ButtonL2))};


void Worker::setLeftAxis(double x, double y)
{
    leftAxis.x = x;
    leftAxis.y = y;
}

void Worker::setRightAxis(double x, double y)
{
    rightAxis.x = x;
    rightAxis.y = y;
}

void Worker::setPreviousLAxis(double x, double y)
{
    previousLeftAxis.x = x;
    previousLeftAxis.y = y;
}

MyAxis Worker::getLeftAxis()
{
    return leftAxis;
}

MyAxis Worker::getRightAxis()
{
    return rightAxis;
}

MyAxis Worker::getPreviousLAxis()
{
    return previousLeftAxis;
}

void Worker::sendFrame(void)
{
    u32 hidPad = 0xfff;
    for(u32 i = 0; i < 2; i++)
    {
         if(buttons & (1 << hidButtonsAB[i]))
                hidPad &= ~(1 << i);
    }

    for(u32 i = 2; i < 10; i++)
    {
        if(buttons & (1 << hidButtonsMiddle[i-2]))
            hidPad &= ~(1 << i);
    }

    for(u32 i = 10; i < 12; i++)
   {
            if(buttons & (1 << hidButtonsXY[i-10]))
                hidPad &= ~(1 << i);
   }

    u32 irButtonsState = 0;
    for(u32 i = 0; i < 2; i++)
    {
            if(buttons & (1 << irButtons[i]))
                irButtonsState |= 1 << (i + 1);
    }

    u32 specialButtons = interfaceButtons;
    MyAxis cppAxis = rightAxis;
    MyAxis cpadAxis = leftAxis;
#ifdef ENABLE_SDL_PRO3
    Pro3MotionFrame pro3;
    auto &controller = selectedSdlController();
    if (controller.read(pro3)) {
        const bool mappedSdl = !controller.usesPro3Mapping();
        if (mappedSdl) {
            hidPad = 0xfff;
            specialButtons = 0;
        }
        cppAxis.x = pro3.rightX;
        cppAxis.y = -yAxisMultiplierCpp * pro3.rightY;
        if (btnSettings.isShouldSwapStick()) {
            cpadAxis.x = pro3.rightX;
            cpadAxis.y = -yAxisMultiplier * pro3.rightY;
            cppAxis.x = pro3.leftX;
            cppAxis.y = -yAxisMultiplierCpp * pro3.leftY;
        }
        if (btnSettings.isCStickDisabled())
            cppAxis.x = cppAxis.y = 0;
        irButtonsState = (pro3.zr ? 2u : 0u) | (pro3.zl ? 4u : 0u);
        if (mappedSdl) {
            cpadAxis.x = btnSettings.isShouldSwapStick() ? pro3.rightX : pro3.leftX;
            cpadAxis.y = -yAxisMultiplier *
                (btnSettings.isShouldSwapStick() ? pro3.rightY : pro3.leftY);
            irButtonsState |= (pro3.extraButtons >> 11) & 6u;
            specialButtons |= (pro3.extraButtons >> 14) & 1u;
        } else if (!btnSettings.isMonsterHunterCamera()) {
            hidPad |= 0xF0u;
        }
        hidPad &= ~((pro3.standardButtons | pro3.dpad | pro3.extraButtons) & 0xfffu);
    }
#endif

    u32 touchScreenState = 0x2000000;
    u32 circlePadState = 0x7ff7ff;
    u32 cppState = 0x80800081;

    if(touchScreenPressed)
    {

        u32 x = (u32)(0xfff * std::min(std::max(0, touchScreenPosition.x()),
                                       TOUCH_SCREEN_WIDTH*touchScreenPosition.x())) / touchScreenSize.width();
        u32 y = (u32)(0xfff * std::min(std::max(0, touchScreenPosition.y()),
                                       TOUCH_SCREEN_HEIGHT*touchScreenPosition.y())) / touchScreenSize.height();

        touchScreenState = (1 << 24) | (y << 12) | x;
    }

    if(cpadAxis.x != 0.0 || cpadAxis.y != 0.0)
      {
          u32 x = (u32)(cpadAxis.x * CPAD_BOUND + 0x800);
          u32 y = (u32)(cpadAxis.y * CPAD_BOUND + 0x800);
          x = x >= 0xfff ? (cpadAxis.x < 0.0 ? 0x000 : 0xfff) : x;
          y = y >= 0xfff ? (cpadAxis.y < 0.0 ? 0x000 : 0xfff) : y;

          circlePadState = (y << 12) | x;
      }

      if(cppAxis.x != 0.0 || cppAxis.y != 0.0 || irButtonsState != 0)
      {
          if (rotateNew3dsCStick.load(std::memory_order_relaxed))
          {
              constexpr double invSqrt2 = 0.7071067811865476;
              const double x = cppAxis.x;
              cppAxis.x = (x + cppAxis.y) * invSqrt2;
              cppAxis.y = (cppAxis.y - x) * invSqrt2;
          }

          const auto packAxis = [](double axis) -> u32 {
              return static_cast<u32>(std::max(0.0, std::min(255.0, axis * CPP_BOUND + 128.0)));
          };
          const u32 x = packAxis(cppAxis.x);
          const u32 y = packAxis(cppAxis.y);

          cppState = (y << 24) | (x << 16) | (irButtonsState << 8) | 0x81;
      }

      QByteArray ba(20, 0);
      qToLittleEndian(hidPad, (uchar *)ba.data());
      qToLittleEndian(touchScreenState, (uchar *)ba.data() + 4);
      qToLittleEndian(circlePadState, (uchar *)ba.data() + 8);
      qToLittleEndian(cppState, (uchar *)ba.data() + 12);
      qToLittleEndian(specialButtons, (uchar *)ba.data() + 16);
#ifdef ENABLE_SDL_PRO3
      static const auto accelMode = [] {
          const auto override = qgetenv("INPUTREDIRECTION_PRO3_ACCEL");
          return override.isEmpty() ? QByteArray("coherent") : override;
      }();
      static Pro3Gravity gravity;
      const auto motionTime = Pro3Gravity::Clock::now();
      if (accelMode == "coherent" && !pro3.hasGyro)
          gravity.update(pro3, motionTime);
      if (pro3.hasGyro) {
          if (accelMode == "off") {
              // Use the controller's front tilt as horizontal aim.
              static const double tiltGain = [] {
                  bool valid = false;
                  const double value = qgetenv("INPUTREDIRECTION_PRO3_TILT_GAIN").toDouble(&valid);
                  return valid && std::isfinite(value)
                      ? std::max(-2.0, std::min(2.0, value)) : 1.0;
              }();
              const double tilt = -pro3.gyroZ * tiltGain;
              pro3.gyroZ = std::lround(std::max(-32767.0, std::min(32767.0, tilt)));
              pro3.gyroY = 0;
          }
          static const double accelBlend = [] {
              bool valid = false;
              const double requested = qgetenv("INPUTREDIRECTION_PRO3_ACCEL").toDouble(&valid);
              return valid && std::isfinite(requested)
                  ? std::max(0.0, std::min(1.0, requested)) : 0.35;
          }();
          static Pro3MotionFrame neutral;
          if (accelMode == "hold" && pro3.hasAccel) {
              static Pro3MotionFrame heldTilt;
              static auto lastMovement = std::chrono::steady_clock::now();
              const auto now = std::chrono::steady_clock::now();
              const bool moving = pro3.gyroX || pro3.gyroY || pro3.gyroZ;
              if (moving)
                  lastMovement = now;
              if (!heldTilt.hasAccel || now - lastMovement <= std::chrono::milliseconds(50))
                  heldTilt = pro3;
              pro3.accelX = heldTilt.accelX;
              pro3.accelY = heldTilt.accelY;
              pro3.accelZ = heldTilt.accelZ;
          } else if (accelMode == "off" || (!pro3.hasAccel && accelMode != "coherent")) {
              // Keep remote acceleration active to avoid using console tilt.
              pro3.hasAccel = true;
              pro3.accelX = pro3.accelY = pro3.accelZ = 0;
          } else if (accelMode != "live" && accelMode != "coherent") {
              const int x = pro3.accelX, y = pro3.accelY, z = pro3.accelZ;
              const int lengthSquared = x * x + y * y + z * z;
              if (!neutral.hasAccel && lengthSquared >= 400 * 400 &&
                  lengthSquared <= 625 * 625) {
                  neutral = pro3;
                  qInfo("Pro 3 motion: neutral pose captured, accel blend %.2f", accelBlend);
              }
              if (neutral.hasAccel) {
                  pro3.accelX = std::lround(neutral.accelX + accelBlend * (x - neutral.accelX));
                  pro3.accelY = std::lround(neutral.accelY + accelBlend * (y - neutral.accelY));
                  pro3.accelZ = std::lround(neutral.accelZ + accelBlend * (z - neutral.accelZ));
              }
          }
          static const bool zeroTest = qgetenv("PRO3_ZERO_GYRO_TEST") == "1";
          static const auto zeroAt = std::chrono::steady_clock::now() + std::chrono::seconds(15);
          if (zeroTest && pro3.hasAccel && std::chrono::steady_clock::now() >= zeroAt) {
              static bool reported = false;
              if (!reported) {
                  qInfo("Pro 3 test: gyro now zero, accelerometer still live");
                  reported = true;
              }
              pro3.gyroX = pro3.gyroY = pro3.gyroZ = 0;
          }
          // SDL yaw is packed as the HID gyro Z axis.
          if (invertGyroYaw.load(std::memory_order_relaxed))
              pro3.gyroZ = -pro3.gyroZ;
          if (accelMode == "coherent")
              gravity.update(pro3, motionTime);
          ba.resize(pro3.hasAccel ? 40 : 32);
          ba.replace(20, 4, "GYR1", 4);
          qToLittleEndian<u16>(pro3.gyroX, reinterpret_cast<uchar *>(ba.data()) + 24);
          qToLittleEndian<u16>(pro3.gyroY, reinterpret_cast<uchar *>(ba.data()) + 26);
          qToLittleEndian<u16>(pro3.gyroZ, reinterpret_cast<uchar *>(ba.data()) + 28);
          qToLittleEndian<u16>(0, reinterpret_cast<uchar *>(ba.data()) + 30);
          if (pro3.hasAccel) {
              qToLittleEndian<u16>(pro3.accelX, reinterpret_cast<uchar *>(ba.data()) + 32);
              qToLittleEndian<u16>(pro3.accelY, reinterpret_cast<uchar *>(ba.data()) + 34);
              qToLittleEndian<u16>(pro3.accelZ, reinterpret_cast<uchar *>(ba.data()) + 36);
              qToLittleEndian<u16>(0, reinterpret_cast<uchar *>(ba.data()) + 38);
          }
      }
#endif
      const auto sent = QUdpSocket().writeDatagram(ba, QHostAddress(ipAddress), 4950);
#ifdef ENABLE_SDL_PRO3
      static bool reportedAccel = false;
      if (ba.size() == 40 && sent == 40 && !reportedAccel) {
          qInfo("Pro 3 motion: sending gyro and accelerometer packets");
          reportedAccel = true;
      }
      static bool reportedGyro = false;
      if (ba.size() == 32 && sent == 32 && !reportedGyro) {
          qInfo("Pro 3 gyro: sending 32-byte motion packets");
          reportedGyro = true;
      }
#else
      (void)sent;
#endif
}

QGamepadManager::GamepadButton variantToButton(QVariant variant)
{
    QGamepadManager::GamepadButton button;

    button = static_cast<QGamepadManager::GamepadButton>(variant.toInt());

    return button;
}

int appScreenTo3dsX(int posX)
{
    qDebug() << "PosX: " << posX;
    return TOUCH_SCREEN_WIDTH*((touchScreenSize.height()*posX)/TOUCH_SCREEN_HEIGHT)/touchScreenSize.width();
}

int appScreenTo3dsY(int posY)
{
    qDebug() << "PosX: " << posY;
    return TOUCH_SCREEN_HEIGHT*((touchScreenSize.width()*posY)/TOUCH_SCREEN_WIDTH)/touchScreenSize.height();
}
