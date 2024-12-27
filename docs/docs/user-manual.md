# User Manual

## Overview

Thor Vision is a GUI application designed for seamless control and video capture of USB cameras connected to the [**XDAQ AIO**](https://kontex.io/pages/xdaq). This user manual provides instructions on how to use features provided by the GUI application to communicate with the [**XDAQ AIO**](https://kontex.io/pages/xdaq).

---

## Hardware Requirements

* **PC**: Windows with a Thunderbolt 3.0 port or higher.
* [**XDAQ AIO**](https://kontex.io/pages/xdaq).
* **Thunderbolt 3.0 Cable** (or higher).
* **USB Cameras**: Compatible with XDAQ.

/// note | Note 
Support for macOS and Ubuntu is coming soon...
///

---

## Installation

[:fontawesome-brands-windows:](https://github.com/kontex-neuro/XDAQ-VC/releases/download/v0.0.1/XDAQ-VC-0.0. 1-win64.exe){ .icon-large } 
[:fontawesome-brands-apple:](){ .icon-large } (coming soon...) 
[:fontawesome-brands-ubuntu:](){ .icon-large } (coming soon...)

---

## User Interface Overview

!["UI Overview"](ui-overview.png)

### 1. Record

Press the button to record videos with embedded XDAQ metadata.

### 2. Camera List

View and live stream USB cameras connected to the XDAQ.

### 3. Record Settings

Open the record settings window for advanced configuration options.

---

## Record Settings

!["Record Settings"](record-settings.png)

### 1. Record Settings of Camera List

Choose either `Continuous` or `Trigger on` to record USB camera.

* **Continuous**: Start recording by pressing the `REC` button.
* **Trigger on**: Start recording based on external triggers from the [**Brainwave simulator**](https://kontex.io/products/brain-signal-simulator).

### 2. Record Mode

Choose either `Continuous` or `Split camera record into` to record USB cameras.

* **Continuous**: Record a single, uninterrupted video file for the entire recording session.
* **Split**: Record multiple video files, each split into predefined segments (e.g., 5 seconds, 10 seconds).

### 3. Record Path/Folder Name

* **Record Path**: Click `...` to select the folder for saving recordings, or manually enter the directory path.
* **Folder Name**: Choose between:
    * **Auto** Automatically generated folder name in `YYYY-MM-DD_HH-MM-SS` format.
    * **Custom**: Specify a custom folder name.

### 4. Extract Metadata

Enable this option to store XDAQ metadata in a separate file for post-processing.

---

## Camera Control

!["Camera Control"](camera-control.png)

### 1. Stream Window

View live streams from USB cameras connected to the XDAQ AIO.

### 2. XDAQ Metadata

Display live metadata from USB cameras connected to the XDAQ AIO.

### 3. Camera Control Options

!["Camera Control Options"](camera-control-options.png)

#### 1. Start/Stop Stream

Start or Stop the live stream from a USB camera.

#### 2. Resolution

Select a compatible resolution for the USB camera.

#### 3. FPS

Select a compatible frame rate (FPS) for the USB camera.

#### 4. Codec

Select a compatible codec for the USB camera.

#### 5. View

Toggle to show or hide the USB camera view.

#### 6. Audio

/// note | Note 
`Audio` option is coming soon...
///

---