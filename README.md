# TFT_clock

![quadclock-pcb-wide2](https://github.com/user-attachments/assets/aef64c81-fa44-42b3-9fca-4c5e3404ec67)

TFT_clock is a complete software solution designed to drive four TFT displays as an advanced alarm clock running an ESP32-S3 MCU. It features a customizable interface with a selection of TrueType fonts, adjustable brightness control, and auto-rotation using an accelerometer. Additional options include 7-day alarms, optional minute/hour chimes, and a night mode that dims the display and shifts colors to red.

This software powers the [QuadClock](https://www.quadblock.com/) product line, offering a series of stylish clocks. A PCB to create your own clock design is also available: [QuadClock PCB](https://quadclock.com/pcb/). The devices are [available for sale on Tindie](https://www.tindie.com/stores/electronics-by-nic/).

![menu](https://github.com/user-attachments/assets/ba737011-526d-4891-a56f-4068fa5ed6fb)

## Features
- **Multi-display support** – Designed to run on four TFT displays. You can use any display type that's compatible with the TFT_eSPI library.
- **Custom fonts** – Able to render TrueType fonts, or to display digits from jpeg files.
- **Adaptive brightness** – Automatic brightness adjustment based on ambient light conditions.
- **Auto-rotation** – Uses an accelerometer to detect and adjust the display orientation.
- **7-day alarms** – Set individual alarms for each day of the week, and one daily alarm.
- **Minute & hourly sounds** – Optional chimes and flip sounds to enhance the experience.
- **Night mode** – Dims the display and shifts to red at night for comfortable viewing.
- **Physical controls** – Navigate menus and adjust settings with 4 physical buttons or a rotary encoder.
- **Internet Radio** – By connecting to a WiFi access point, you get a radio alarm clock. You can set your preferred radio station using the web interface.

To build and upload the firmware, clone this repository and open it in VSCode with PlatformIO. To build and upload the firmware, clone this repository and open it in VSCode with PlatformIO. The project relies on the I2S library, which needs to run on Arduino 3.x. Therefore, it uses [pioarduino](https://github.com/pioarduino/platform-espressif32), a community-driven platform compatible with PlatformIO, as PlatformIO is still using Arduino 2.x .

Don't forget to upload the LittleFS partition. In `/data,` you'll find multiple filesystem partitions, one for each build environment. The correct partition is selected automatically.

The software is open source, so feel free to modify it or add new features. If you have improvements, you can submit a pull request on GitHub, and I'll review it for inclusion in the main branch.
