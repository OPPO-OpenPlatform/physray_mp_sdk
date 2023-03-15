# Clone The Repository

1. Before cloning, make sure you have GLT LFS (https://git-lfs.github.com/) installed on your dev system.

2. Clone the repo with `git clone`. You might need to use `git lfs clone` if you are on a older version of GIT.

## Troubleshooting
- Permission denied while trying to connect to Docker daemon socket: Run the install-docker-engine.sh script. If that doesn't work, use the [fix described here](https://www.digitalocean.com/community/questions/how-to-fix-docker-got-permission-denied-while-trying-to-connect-to-the-docker-daemon-socket).
- Building from docker fails due to missing C compiler: The docker image only has gcc and g++, so if you've set CC and CXX to use a different compiler in your environment variables, build will fail to find them inside the docker image. Use gcc and g++ instead.

# Build Instruction

PhysRay SDK can be built on the following environment:
  - Windows : 64-bit only.

The build system is CMake based, with a few caveats:

- **Do NOT** install the Ubuntu's builtin Vulkan dev library `libvulkan-dev`.
  If you already have it installed, run `sudo apt remove libvulkan1` to remove it.
  Instead, you'll need to install the latest LunarG SDK from here: https://vulkan.lunarg.com/sdk/home

## Windows 10 Native Build

Windows build is currently done outside of Docker environment. You'll have to manually install the following dependency libraries:
- [Visualt Studio 2022](https://visualstudio.microsoft.com/vs/)
  - Make sure you have "Desktop development with C++" and "Game development with C++" selected.
- [Git For Windows](https://gitforwindows.org/) (We also need the bash environment coming with it.)
- [Git LFS](https://git-lfs.github.com/)
- [CMake](https://cmake.org/download/): [B]Make sure you install the x64 version of the cmake.[/B]
- [Python 3.8+](https://www.python.org/downloads/windows/)
- [LunarG SDK 1.2.182.1](https://vulkan.lunarg.com/sdk/home)

Other than visual studio, all of the dependencies can be installed via winget:

  ```
  winget install --id Kitware.CMake --silent --accept-package-agreements --accept-source-agreements
  winget install --id Git.Git --silent --accept-package-agreements --accept-source-agreements
  winget install --id KhronosGroup.VulkanSDK --version 1.2.182.0 --silent --accept-package-agreements --accept-source-agreements
  winget install python --silent --accept-package-agreements --accept-source-agreements
  ```

Once all dependencies are installed, run the [build-win64.cmd] to build.

