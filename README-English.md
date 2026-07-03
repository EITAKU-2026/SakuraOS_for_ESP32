# SakuraOS (v6.1)

* SakuraOS is an original, lightweight CUI (Command-Line Interface) operating system designed to run on the ESP32 microcontroller board. It allows users to execute operations by entering text commands via the PC's serial monitor or terminals like Tera Term, utilizing an SD card for local storage.
* **Important Note : Please be aware that the system user interface (UI) and display text are only available in Japanese. We sincerely apologize for any inconvenience this may cause.**

---

## Hardware & Environment

### Hardware Component Requirements
* **Microcontroller**: FREENOVE ESP32 WROOM Development Board (with ESP32-WROOM-32E)
* **SD Card Module**: KKHMF SPI Interface Micro SD Card Shield Module (Model: 000443 / ASIN: B083DT3LQK)
* **RTC (Real-Time Clock)**: ELEGOO DS1307-Module-V (I2C communication / SQW pin is not used)
* **Audio Amplifier**: TVETE MAX98357A I2S Audio Amplifier Module (ASIN: BFF4QD7T4)
* **Speaker**: Any simple 2-pin speaker (connected directly to the MAX98357A module)
* **LED**: Standard 3mm/5mm white LED (connected in series with a 220Ω resistor)
* **Buzzer**: Piezoelectric buzzer (capable of 5V PWM frequency control)
* **Touch Sensor**: A simple bare wire connected to Pin 4 (triggered by direct skin contact)

### Libraries Used

| Header File in Code | Official Name | Provider / Author |
| :--- | :--- | :--- |
| `Arduino.h` | Arduino Core | Arduino / Espressif |
| `freertos/FreeRTOS.h` | FreeRTOS | Real Time Engineers Ltd. |
| `freertos/task.h` | FreeRTOS | Real Time Engineers Ltd. |
| `freertos/queue.h` | FreeRTOS | Real Time Engineers Ltd. |
| `AudioGeneratorWAV.h` | ESP8266Audio | earlephilhower |
| `AudioOutputI2S.h` | ESP8266Audio | earlephilhower |
| `AudioFileSourceSD.h` | ESP8266Audio | earlephilhower |
| `FS.h` | FS | Espressif |
| `SD.h` | SD | Arduino Official |
| `SPI.h` | SPI | Arduino Official |
| `Wire.h` | Wire | Arduino Official |
| `RTClib.h` | RTClib | Adafruit |

---

## Pin Connection Map

> **Important**: Ensure all connected components share a Common GND.

| Hardware Component | ESP32 Pin Number | Notes / Connection Instructions |
| :--- | :---: | :--- |
| **LED** | 13 | Connect a 220Ω resistor in series |
| **Piezo Buzzer** | 17 | |
| **RTC (SDA)** | 33 | Power Supply: 5V |
| **RTC (SCL)** | 32 | |
| **SD Card (MISO)** | 19 | Power Supply: 5V |
| **SD Card (MOSI)** | 23 | |
| **SD Card (SCK)** | 18 | |
| **SD Card (CS)** | 5 | |
| **Amplifier (BCLK)** | 26 | Power Supply: 3.3V |
| **Amplifier (LRC/LRCK)**| 25 | |
| **Amplifier (DOUT/DIN)** | 15 | |
| **Amplifier (GAIN)** | - | Connect to 3.3V via a 100kΩ resistor in series |
| **Amplifier (SD)** | - | Leave disconnected (floating) |
| **Touch Sensor** | 4 | Connect a single stripped wire only |

---

## Core Command List

After the OS boots up, you can control it by sending the following text commands through your serial terminal. Enter `help` to display the full interactive command menu on your screen.

### System & Information
* `help` : Displays the full command reference list.
* `sys.info` : Prints current operating system specifications.
* `sys.load` : Offers a quick, simplified layout of ESP32 memory consumption.
* `sys.taskinfo` : Displays active FreeRTOS task statistics, priority rankings, and stack margins.
* `sys.uptime` : Shows the elapsed operating time since the system booted up.
* `sys.checksetting` : Outputs existing system settings (volume levels, threshold parameters).
* `sys.lock` : Instantly locks SakuraOS (requires the active login password to restore access).

### Hardware Controls
* `led.on` / `led.off` : Directly turns the integrated LED on or off.
* `buzz.beep [Frequency]` : Generates a tone at the specified frequency for hardware testing.
* `sys.touchcheck` : Tracks real-time touch diagnostics to help calibrate sensory configurations.
* `sys.touchsetting [Value]` : Updates the dynamic baseline threshold for touch interactions.

### Audio & Sound Player
* `speaker.playwav [Filename]` : Initiates playback for a specific `.wav` file on the SD card.
* `speaker.stopwav` : Halts ongoing playback (can also be triggered via touch sensor contact).
* `sys.volume [0-100]` : Universally modifies master audio output volume scaling.

### File & Storage Management (SD Card)
* `dir.list` : Details all elements within the target file directory structure.
* `dir.now` / `dir.move [Path]` : Prints current path tracking / shifts system workspace directories.
* `file.open [Filename]` : Displays raw text data output from a chosen local document.
* `file.edit [Path/Filename]` : Launches the built-in "Cherry Blossom Text Editor" to draft or alter files.

---

## WAV Audio Export Configuration
To maintain seamless track playback with the I2S audio chip, render your sound files via **Audacity** or similar programs using these strict attributes:
* **File Type**: WAV (Microsoft)
* **Encoding**: Signed 16-bit PCM
* **Sampling Rate**: 22050 Hz
* **Channels**: Mono (recommended to reduce data overhead and boost streaming performance)
* **Metadata**: Strip out all embedded tags (e.g., Artist, Album Cover) as they generate digital noise.

---

## Generated System Files
SakuraOS deploys two unique file formats on the SD card (interchangeable as regular text documents on PCs):
1. **`.slf` (Sakura Log File)** : Automated rolling system logging that registers executed inputs with timestamps.
2. **`.ssf` (Sakura Setting File)** : Stores operational parameters including volume preferences, touch thresholds, and passwords (XOR-encrypted).

---

## Notice
This is a collaborative project where I came up with the original ideas and developed the system in partnership with AI.

## Default Factory Profile
* **Default Security Password**: `SakuraOS`
* **Default Audio Level**: `70`
* **Developer**: EITAKU
