# TFT_clock

![quadclock-pcb-wide2](https://github.com/user-attachments/assets/aef64c81-fa44-42b3-9fca-4c5e3404ec67)

TFT_clock is a complete software solution designed to drive four circular TFT displays as an advanced alarm clock running an ESP32-S3 MCU. It features a customizable interface with a selection of TrueType fonts, adjustable brightness control, and auto-rotation using an accelerometer. Additional options include 7-day alarms, optional minute/hour chimes, and a night mode that dims the display and shifts colors to red.

This software powers the [QuadClock](https://www.quadblock.com/) product line, offering a refined and stylish way to keep time. A fully compatible PCB with all necessary hardware is also available: [QuadClock PCB](https://quadclock.com/pcb/). While it was developed for the QuadClock project, it can also be adapted for other uses.

![menu](https://github.com/user-attachments/assets/ba737011-526d-4891-a56f-4068fa5ed6fb)

## Features
- **Multi-display support** – Designed to run on four TFT displays. You can use any display type that's compatible with the TFT_eSPI library.
- **Custom fonts** – Includes a selection of TrueType fonts for a unique and personalized look.
- **Adaptive brightness** – Automatic brightness adjustment based on ambient light conditions.
- **Auto-rotation** – Uses an accelerometer to detect and adjust the display orientation.
- **7-day alarms** – Set individual alarms for each day of the week.
- **Minute & hourly sounds** – Optional chimes and flip sounds to enhance the experience.
- **Night mode** – Dims the display and shifts to red at night for comfortable viewing.
- **Physical controls** – Navigate menus and adjust settings with 4 physical buttons or a rotary encoder.

To build and upload the firmware, clone this repository and open it in VSCode with PlatformIO.
Don't forget to update the littleFS partition. In `/data` you will find multiple file system partitions. In `platformio.ini` you can select which file system partition you want to use.
The software is open source, so feel free to modify it or add new features. If you have improvements, you can submit a pull request on GitHub, and I’ll review them for inclusion in the main branch.
