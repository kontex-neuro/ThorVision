<div align="center">

<h1>
<img src="docs/docs/favicon.png" alt="XDAQ Logo" width="128">
<br>Thor Vision
</h1>

<p align="center">
    A GUI app for seamless control and video capture from USB cameras on the <a href="https://kontex.io/pages/xdaq">XDAQ AIO</a>.
    <br />
    <a href="https://github.com/kontex-neuro/ThorVision">Homepage</a>
    |
    <a href="https://developer.kontex.io/thorvision/">Documentation</a>
</p>

</div>

> [!WARNING]
>
> This software is currently in **beta**. While it offers exciting features and functionality, 
> please be aware that it might contain bugs, performance issues, or incomplete features. 
> We greatly appreciate your feedback to help us improve. 
> If you encounter any issues, please report them on our [GitHub Issues page](https://github.com/kontex-neuro/ThorVision/issues).
>
> Thank you for being an early adopter and helping us shape the future of **Thor Vision**!

## Features

* Works automatically with the [XDAQ AIO](https://kontex.io/pages/xdaq)
* Record videos with embedded [XDAQ metadata](docs/docs/xdaq-metadata.md)
* Record M-JPEG encoded videos

> [!NOTE]
> * Record H.265 encoded videos (coming soon)
> * Synchronized recording with [Open Ephys GUI](https://open-ephys.org/gui) and [Intan RHX](https://intantech.com/RHX_software.html) (coming soon)
> * Trigger recording from hardware TTL inputs or via [**Brainwave simulator**](https://kontex.io/products/brain-signal-simulator) (coming soon)

## Platforms

* Windows

> [!NOTE]
> Support for macOS and Ubuntu is in development (coming soon)

## Third-party

* CMake ([New BSD License](https://github.com/Kitware/CMake/blob/master/Copyright.txt))
* Conan ([MIT License](https://github.com/conan-io/conan/blob/develop2/LICENSE.md))
* Ninja ([Apache License 2.0](https://github.com/ninja-build/ninja/blob/master/COPYING))
* Qt 6 ([LGPL](http://doc.qt.io/qt-6/lgpl.html))