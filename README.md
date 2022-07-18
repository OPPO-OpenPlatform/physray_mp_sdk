# Overview

PhysRay SDK is a high performance modern graphics/compute SDK on OPLUS platform. It is mainly focusing on giving developers the ability to quickly utilize
the latest graphics features provided by modern Android platform, such as real time ray tracing and heterogenous computing.
Check out [MANUAL.md](doc/MANUAL.md) for detailed explanation for the SDK features and tutorials of how to use them.

# Build

Please follow this document for detailed build instructions: [BUILD.md](doc/BUILD.md)

# FAQ

## Q: Does the SDK support OpenGL/ES
A: No. This SDK is based on Vulkan 1.1+ API.

## Q: Does the SDK support ray tracing effect on mobile platform?
A: Yes. The SDK is tested on OPPO Find X5 Pro phone with with MTK D9000 chip. 

## Q: What phone model are currently supported?
A: OPPO Find X5 Pro with MTK D9000 chip is the only phone model we officially support so far.

## Q: Is the ray tracing effect accelerated by hardware? What about on mobile platform?
A: The rendering is fully hardware accelerated on ray tracing capable hardware, such as NVIDIA GeForce 20XX graphics card. On mobile platform though, the
   rendering is currently simulated via regular shader (FS, CS) pipeline.

# Credits

PhysRay SDK references the following 3rd party libraries:

| Name | License |
| ---- | ------- |
| [Eigen](https://eigen.tuxfamily.org/) | MPL2 |
| [Volk](https://github.com/zeux/volk) | MIT |
| [AMD Vulkan Memory Allocator](https://gpuopen.com/vulkan-memory-allocator/) | MIT |
| [MoodyCamel read write queue](https://github.com/cameron314/readerwriterqueue) | Simplified BSD |
| [stb image loader](https://github.com/nothings/stb/blob/master/stb_image.h) | Public Domain |
| [Stack Walker](https://github.com/JochenKalmbach/StackWalker) | BSD |
| [hash-library](https://github.com/stbrumme/hash-library)| zlib |
| [CLIII](https://github.com/CLIUtils/CLI11) | 3-Clause BSD |
| [JSON for Modern C++](https://github.com/nlohmann/json) | MIT |
| [tiny glTF](https://github.com/syoyo/tinygltf) | MIT |
| [Vulkan Minimal Compute](https://github.com/Erkaman/vulkan_minimal_compute) | MIT |
| [Assimp](https://www.assimp.org/) | 3-clause BSD |
| [Dear ImGui](https://github.com/ocornut/imgui) | MIT |
| [Qualcomm Hexagon SDK: v4.x (4.5.03)](https://developer.qualcomm.com/software/hexagon-dsp-sdk/tools) | Restricted Use Software |
| [KTX Software](https://github.com/KhronosGroup/KTX-Software) | Apache-2.0 |
| [Pierre-Antoine Lacaze's Sigslot Library](https://github.com/palacaze/sigslot) | MIT |
| [ARM ASTC Encoder](https://github.com/ARM-software/astc-encoder) | Apache 2.0 |

PhysRay SDK has the following third-party assets in the sample/dev assets directory:
| Name | License |
| ---- | ------- |
| [Dead Leaf](https://www.cgtrader.com/free-3d-models/plant/leaf/dead-leaf) | Royalty Free License |
| [Body Basemesh](https://www.cgtrader.com/3d-models/character/child/girl-basemesh-rigged) | Royalty Free License |
| [Dragonfly](https://www.cgtrader.com/3d-models/animals/insect/animated-dragonfly) | Royalty Free License |
| [Hairstyle 17](https://www.cgtrader.com/3d-models/character/woman/girl-hair-style-17) | Royalty Free License |
| [Lotus Collection](https://www.cgtrader.com/3d-models/plant/flower/3d-flower-collection-vol07-lotus) | Royalty Free License |
