# User Manual

## Overview

Thor Vision is a GUI app designed for seamless control and video capture from USB cameras on the [XDAQ AIO](https://kontex.io/pages/xdaq). This user manual provides instructions on how to use the features of the application effectively.

```mermaid
graph LR;
    subgraph PC["PC (Windows)"]
        TV(Thor Vision GUI)
        subgraph DA[Data Acquisition Software]
            RHX(Intan RHX)
            OE(Open Ephys GUI)
        end
    end

    XDAQ(XDAQ AIO)

    subgraph CAMS[Cameras]
        CAM1(Camera 1)
        CAM2(Camera 2)
        CAM3(Camera 3)
        CAM4(Camera 4)
    end

    subgraph XH[X-Headstage]
        x6R1(x6R)
        x6R2(x6R)
        x6R3(x6R)
        x6R4(x6R)
    end

    XDAQ -->|Thunderbolt| PC;
    CAM1 -->|USB| XDAQ;
    CAM2 -->|USB| XDAQ;
    CAM3 -->|USB| XDAQ;
    CAM4 -->|USB| XDAQ;

    x6R1 -->|HDMI| XDAQ;
    x6R2 -->|HDMI| XDAQ;
    x6R3 -->|HDMI| XDAQ;
    x6R4 -->|HDMI| XDAQ;

    click XDAQ "https://kontex.io/pages/xdaq" "Go to XDAQ page" _blank
    click RHX "https://intantech.com/RHX_software.html" "Go to Intan RHX page" _blank
    click OE "https://open-ephys.org/gui" "Go to Open Ephys GUI page" _blank
    click TV "https://github.com/kontex-neuro/ThorVision" "Go to Thor Vision GUI page" _blank
```

---

## Hardware Requirements

* **PC**: Windows with a Thunderbolt 3.0 port or higher.
* [**XDAQ AIO**](https://kontex.io/pages/xdaq).
* **Thunderbolt 3.0 Cable** (or higher).
* [**USB Cameras**](usb-cameras.md): Compatible with XDAQ.

/// note | Note 
Support for macOS and Ubuntu is in development (coming soon)
///

---

## Installation

[:fontawesome-brands-windows:](https://github.com/kontex-neuro/ThorVision/releases/download/v0.0.1/XDAQ-VC-0.0.1-win64.exe){ .icon-large } 
[:fontawesome-brands-apple:](){ .icon-large } (coming soon) 
[:fontawesome-brands-ubuntu:](){ .icon-large } (coming soon)

---

## User Interface Overview

!["UI Overview"](ui-overview.png)

### 1. Record

Press the button to record videos with embedded [XDAQ metadata](xdaq-metadata.md).

### 2. Camera List

View and live stream cameras on the [XDAQ AIO](https://kontex.io/pages/xdaq).

### 3. Server status

Display current server status on the [XDAQ AIO](https://kontex.io/pages/xdaq).

### 4. Record Settings

Open the record settings window for advanced configuration options.

---

## Record Settings

!["Record Settings"](record-settings.png)

### 1. Cameras Recording Settings

Choose either `Continuous` or `Trigger on` to record camera.

* **Continuous**: Start recording by pressing the `REC` button.
* **Trigger on**: Start recording from hardware TTL or via the [**Brainwave simulator**](https://kontex.io/products/brain-signal-simulator). 

/// note | Note 
`Trigger on` option is in development (coming soon)
///

### 2. Record Path/Folder Name

* **Record Path**: Click `...` to select the folder for saving recordings, or manually enter the directory path.
* **Folder Name**: Choose between:
    * **Auto** Automatically generated folder name in `YYYY-MM-DD_HH-MM-SS` format.
    * **Custom**: Specify a custom folder name.

### 3. Record Mode

Choose either `Continuous` or `Split record into` to record cameras.

* **Continuous**: Record a single, uninterrupted video file for the entire recording session.
* **Split record into**: Record multiple video files, each split into predefined segments (e.g., 5 seconds, 10 seconds).

<!-- ### 4. Extract Metadata

Enable this option to store [XDAQ metadata](metadata.md) in a separate file for post-processing. -->

---

## Camera Control

!["Camera Control"](camera-control.png)

### 1. Stream Window

View live streams from cameras on the [XDAQ AIO](https://kontex.io/pages/xdaq).

### 2. [XDAQ Metadata](xdaq-metadata.md)

Display live metadata from cameras on the [XDAQ AIO](https://kontex.io/pages/xdaq).

### 3. Camera Control Options

!["Camera Control Options"](camera-control-options.png)

#### 1. Start/Stop Stream

Start or Stop the live stream from the camera.

#### 2. Resolution

Select a compatible resolution for the camera.

#### 3. FPS

Select a compatible frame rate (FPS) for the camera.

#### 4. Codec

Select a compatible codec for the camera.

/// tip | Tip
Each camera has its own unique capabilities. Dimmed options for resolution, FPS, and codec indicate that the currently selected camera does not support these settings. However, these options remain **clickable**, and selecting them will reset the camera's current settings.
///

#### 5. View

Toggle to show or hide the camera view.

#### 6. Audio

/// note | Note 
`Audio` option is in development (coming soon)
///

---