# Input Redirection Gyro Edition

Play Nintendo 3DS games with a compatible controller connected to your computer. Gyro Edition uses SDL2 to send button, stick, and motion input to your console over the local network.

## What’s new

- Gyroscope and accelerometer input from compatible SDL2 controllers.
- Circle Pad Pro emulation on Old 3DS and 2DS systems with the matching Rehid build. Games that support the accessory can use the controller’s right stick and ZL/ZR.
- Controller settings show extra buttons, paddles, and touchpad controls when the connected controller exposes them.
- Desktop builds for Windows, Linux, and macOS.

## Console setup

Gyro and Circle Pad Pro features require the matching [Luma3DS Gyro vCPP](https://github.com/UsujioNori/Luma3DS-Gyro-vCPP) and [Rehid Gyro vCPP](https://github.com/UsujioNori/rehid-Gyro-vCPP) builds. Start Input Redirection on the console, then enter the console’s IP address in the desktop app. Keep both devices on the same local network.

All four desktop packages built successfully. Compatibility can vary by controller and game. If you report an issue, please include your OS, controller model, console model, and whether buttons, sticks, or motion were affected.
