// This file is part of <ph/va.h>. Do NOT include it directly from your source code. Include <ph/va.h> instead.

namespace ph {
namespace va {

/// Compile GLSL to SPIR-V
/// \param name   Shader name. This is only for logging and can be safely set to any string or null.
///               When compiling failed, the function will print this name along with compile error.
/// \param stage  Shader stage to compile to.
/// \param source Shader source code
/// \param length Shader source code length (not including the zero terminator).
///               Set to 0, if the source string is zero terminated.
/// \param entry  Entry point function. Set to nullptr to use default entry point.
/// \return       Returns SPIR-V binary on success, or throws std::runtime_error() exception.
PH_API std::vector<uint32_t> glsl2spirv(const char * name, VkShaderStageFlagBits stage, const char * source, size_t length = 0, const char * entry = nullptr);

/// Create shader module from SPIR-V binary
PH_API AutoHandle<VkShaderModule> createSPIRVShader(const VulkanGlobalInfo & g, const ConstRange<uint32_t> & binary, const char * name = nullptr);

/// Create shader module from SPIR-V binary
inline AutoHandle<VkShaderModule> createSPIRVShader(const VulkanGlobalInfo & g, const ConstRange<uint8_t> & binary, const char * name = nullptr) {
    return createSPIRVShader(g, {(const uint32_t *) binary.data(), binary.size() / 4}, name);
}

/// Create shader module from GLSL source
PH_API AutoHandle<VkShaderModule> createGLSLShader(const VulkanGlobalInfo & g, const char * name, VkShaderStageFlagBits stage, const ConstRange<char> & source,
                                                   const char * entry = nullptr);

/// Load GLSL shader from file
PH_API AutoHandle<VkShaderModule> loadGLSLShaderFromFile(const VulkanGlobalInfo & g, const char * filePath, VkShaderStageFlagBits stage = VK_SHADER_STAGE_ALL);

/// Load GLSL shader from file
PH_API AutoHandle<VkShaderModule> loadGLSLShaderAsset(const VulkanGlobalInfo & g, AssetSystem & as, const char * assetPath,
                                                      VkShaderStageFlagBits stage = VK_SHADER_STAGE_ALL);

/// Load SPIR-V shader from file
PH_API AutoHandle<VkShaderModule> loadSPIRVShaderFromFile(const VulkanGlobalInfo & g, const char * filePath);

/// Load GLSL shader from asset
PH_API AutoHandle<VkShaderModule> loadSPIRVShaderAsset(const VulkanGlobalInfo & g, AssetSystem & as, const char * assetPath);

} // namespace va
} // namespace ph