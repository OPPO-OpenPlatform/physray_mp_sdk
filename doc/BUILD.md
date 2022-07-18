# Overview

PhysRay SDK contains pre-built binaries for the following platform:
  - Android : both 32-bit and 64-bit
  - Windows : 64-bit only.
  - Linux   : 64-bit only

Follow the instruction below to build SDK samples from source. 

## Android build

Once you have Android Studio and NDK installed, open [sample/android/hub](sample/android/hub) folder in Android Studio, then build as a regular Android app.

## Windows 10 Native Build

Windows build the following dependency libraries that you have to install manually:
- [Visualt Studio 2019+](https://visualstudio.microsoft.com/vs/)
  - Make sure you have "Desktop development with C++" and "Game development with C++" selected.
- [Git For Windows](https://gitforwindows.org/) (We also need the bash environment coming with it.)
- [Git LFS](https://git-lfs.github.com/)
- [CMake](https://cmake.org/download/): [B]Make sure you install the x64 version of the cmake.[/B]
- [Python 3.8+](https://www.python.org/downloads/windows/)
- [LunarG SDK 1.2.182.1](https://vulkan.lunarg.com/sdk/home)
- [Android Studio](https://developer.android.com/studio)

You can run the following script to install them via winget:
  ```
  winget install --id Microsoft.VisualStudio.2022.Professional --silent --accept-package-agreements --accept-source-agreements --override "-q --add Microsoft.VisualStudio.Workload.ManagedDesktop;includeRecommended --add Microsoft.VisualStudio.Workload.NativeDesktop;includeRecommended --add Microsoft.VisualStudio.Workload.NativeGame;includeRecommended"
  winget install --id Kitware.CMake --silent --accept-package-agreements --accept-source-agreements
  winget install --id Git.Git --silent --accept-package-agreements --accept-source-agreements
  winget install --id KhronosGroup.VulkanSDK --version 1.2.182.0 --silent --accept-package-agreements --accept-source-agreements
  winget install --id Python.Python.3 --silent --accept-package-agreements --accept-source-agreements
  winget install --id Google.AndroidStudio --silent --accept-package-agreements --accept-source-agreements
  ```

Once all dependencies are installed, run the [build-windows.cmd](build-windows.cmd) build all samples.

## Linux build on Ubuntu 20.04

For now, we only support Linux build on Ubuntu 20.04 LTS.

- Run this script install all dependencies:
  ```bash
    # add vulkan sdk source
    sudo apt-get update
    sudo apt-get install -y wget gnupg2

    echo "Downloading LunarG SDK and add to apt registry..."
    sudo wget -qO - https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo apt-key add -
    sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-1.2.182-focal.list https://packages.lunarg.com/vulkan/1.2.182/lunarg-vulkan-1.2.182-focal.list

    # install build dependencies 
    sudo apt-get update && sudo apt-get install -y \
        build-essential \
        cmake \
        git \
        git-lfs \
        ninja-build \
        vulkan-sdk \
        libglfw3-dev \
        libxinerama-dev \
        libxcursor-dev \
        libxi-dev
  ```
- Run [build-linux.sh](build-linux.sh) to build all samples.
