#pragma once

#include <vulkan/vulkan.hpp>
#include "Vec3.hpp"
#include "Vertex.hpp"
#include "Utility.hpp"

extern "C" {
    #include <game-activity/native_app_glue/android_native_app_glue.h>
}
	// struct {
	// 	matrial作ってSRP Batchっぽいことしたい

	// struct {
	// 	struct {
	// 		vks::Texture2D colorMap;
	// 		vks::Texture2D normalMap;
	// 	} model;
	// 	struct {
	// 		vks::Texture2D colorMap;
	// 		vks::Texture2D normalMap;
	// 	} floor;
	// } textures;

	// struct {
	// 	vkglTF::Model model;
	// 	vkglTF::Model floor;
	// } models;

	// struct UniformDataOffscreen  {
	// 	glm::mat4 projection;
	// 	glm::mat4 model;
	// 	glm::mat4 view;
	// 	glm::vec4 instancePos[3];
	// } uniformDataOffscreen;

	// struct Light {
	// 	glm::vec4 position;
	// 	glm::vec3 color;
	// 	float radius;
	// };

	// struct UniformDataComposition {
	// 	Light lights[6];
	// 	glm::vec4 viewPos;
	// 	int debugDisplayTarget = 0;
	// } uniformDataComposition;

	// struct {
	// 	vks::Buffer offscreen;
	// 	vks::Buffer composition;
	// } uniformBuffers;

	// struct {
	// 	VkPipeline offscreen{ VK_NULL_HANDLE };
	// 	VkPipeline composition{ VK_NULL_HANDLE };
	// } pipelines;
	// VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };

	// struct {
	// 	VkDescriptorSet model{ VK_NULL_HANDLE };
	// 	VkDescriptorSet floor{ VK_NULL_HANDLE };
	// 	VkDescriptorSet composition{ VK_NULL_HANDLE };
	// } descriptorSets;

	// VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };

	// // Framebuffers holding the deferred attachments
	// struct FrameBufferAttachment {
	// 	VkImage image;
	// 	VkDeviceMemory mem;
	// 	VkImageView view;
	// 	VkFormat format;
	// };
	// struct FrameBuffer {
	// 	int32_t width, height;
	// 	VkFramebuffer frameBuffer;
	// 	// One attachment for every component required for a deferred rendering setup
	// 	FrameBufferAttachment position, normal, albedo;
	// 	FrameBufferAttachment depth;
	// 	VkRenderPass renderPass;
	// } offScreenFrameBuf{};

	// // One sampler for the frame buffer color attachments
	// VkSampler colorSampler{ VK_NULL_HANDLE };

	// VkCommandBuffer offScreenCmdBuffer{ VK_NULL_HANDLE };

	// // Semaphore used to synchronize between offscreen and final scene rendering
	// VkSemaphore offscreenSemaphore{ VK_NULL_HANDLE };




// struct GpuBuffer
// {
//   VkBuffer buffer;
//   VkDeviceMemory memory;

//   void* mapped = nullptr;
// };

// struct GpuImage
// {
//   VkImage image;
//   VkDeviceMemory memory;
//   VkImageView view;
//   VkFormat format;
//   int mipmapCount;

//   VkAccessFlags2 accessFlags = VK_ACCESS_2_NONE;
//   VkImageLayout  layout = VK_IMAGE_LAYOUT_UNDEFINED;
// };
  struct FrameInfo
  {
    VkFence commandFence = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

    // 描画完了・Present完了待機のためのセマフォ.
    VkSemaphore renderCompleted = VK_NULL_HANDLE;
    VkSemaphore presentCompleted = VK_NULL_HANDLE;
  };
  
class GfxDevice
{
public:

  void Initialize(const DeviceInitParams& initParams);
  void Shutdown();

  VkInstance GetVkInstance() const { return m_vkInstance; }
  VkPhysicalDevice GetVkPhysicalDevice() const { return m_vkPhysicalDevice; }
  VkDevice GetVkDevice() const { return m_vkDevice; }

  uint32_t GetFrameIndex() const { return m_currentFrameIndex; }

  void NewFrame();
  VkCommandBuffer GetCurrentCommandBuffer();

  void Submit();
  void WaitForIdle();

  void GetSwapchainResolution(int& width, int& height) const;

  VkImage GetCurrentSwapchainImage();
  VkImageView GetCurrentSwapchainImageView();
  VkSurfaceFormatKHR GetSwapchainFormat() const;
  uint32_t GetSwapchainImageCount() const;
  VkImageView GetSwapchainImageView(int i) const;
  uint32_t GetSwapchainImageIndex() const { return m_swapchainImageIndex; }

  void TransitionLayoutSwapchainImage(VkCommandBuffer commandBuffer, VkImageLayout newLayout, VkAccessFlags2 newAccessFlag);
  void RecreateSwapchain(uint32_t width, uint32_t height);

  VkShaderModule CreateShaderModule(const void* code, size_t length);
  void DestroyShaderModule(VkShaderModule shaderModule);

  static const int InflightFrames = 2;

  // GPU上にバッファを確保する.
  //  srcData に元データのポインタが設定される場合その内容をバッファメモリに書き込む.
  //  DeviceLocal なメモリを要求する場合、ステージングバッファに書き込んだあと、転送も行う.
  GpuBuffer CreateBuffer(VkDeviceSize byteSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags flags, const void* srcData = nullptr);
  void DestroyBuffer(GpuBuffer& buffer);

  // GPU上にイメージ(テクスチャ)を確保する.
  GpuImage CreateImage2D(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags flags, uint32_t mipmapCount);
  void DestroyImage(GpuImage image);

  uint32_t GetGraphicsQueueFamily() const;
  VkQueue GetGraphicsQueue() const;
  VkDescriptorPool GetDescriptorPool() const;

  // コマンドバッファを新規に確保する.
  VkCommandBuffer AllocateCommandBuffer();

  // コマンドバッファを実行する. (ワンショット実行用)
  void SubmitOneShot(VkCommandBuffer commandBuffer);

  uint32_t GetMemoryTypeIndex(VkMemoryRequirements reqs, VkMemoryPropertyFlags memoryPropFlags);
  bool IsSupportVulkan13();
  
  void SetObjectName(uint64_t handle, const char* name, VkObjectType type);
private:
  void InitVkInstance();
  void InitPhysicalDevice();
  void InitVkDevice();
  void InitWindowSurface(const DeviceInitParams& initParams);
  void InitCommandPool();
  void InitSemaphores();
  void InitCommandBuffers();
  void InitDescriptorPool();

  void DestroyVkInstance();
  void DestroyVkDevice();
  void DestroyWindowSurface();
  void DestroySwapchain();
  void DestroyCommandPool();
  void DestroySemaphores();
  void DestroyCommandBuffers();
  void DestroyDescriptorPool();

  VkInstance m_vkInstance = VK_NULL_HANDLE;
  VkPhysicalDevice m_vkPhysicalDevice = VK_NULL_HANDLE;
  VkDevice m_vkDevice = VK_NULL_HANDLE;
  VkPhysicalDeviceMemoryProperties m_physDevMemoryProps;

  VkSurfaceKHR m_windowSurface = VK_NULL_HANDLE;
  VkSurfaceFormatKHR m_surfaceFormat{};

  int32_t m_width, m_height;
  struct SwapchainState
  {
    VkImage image = VK_NULL_HANDLE;
    VkImageView  view = VK_NULL_HANDLE;
    VkAccessFlags2 accessFlags = VK_ACCESS_2_NONE;
    VkImageLayout  layout = VK_IMAGE_LAYOUT_UNDEFINED;
  };
  VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
  std::vector<SwapchainState> m_swapchainState;

  // キューインデックス.
  uint32_t m_graphicsQueueIndex;
  VkQueue  m_graphicsQueue;

#if _DEBUG
  VkDebugUtilsMessengerEXT m_debugMessenger;
#endif

  VkCommandPool m_commandPool = VK_NULL_HANDLE;
  VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
  uint32_t m_currentFrameIndex = 0;
  uint32_t m_swapchainImageIndex = 0;

  struct FrameInfo
  {
    VkFence commandFence = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

    // 描画完了・Present完了待機のためのセマフォ.
    VkSemaphore renderCompleted = VK_NULL_HANDLE;
    VkSemaphore presentCompleted = VK_NULL_HANDLE;
  };
  FrameInfo  m_frameCommandInfos[InflightFrames];
};

class Renderer {

  struct PlatformParams
  {
#if defined(PLATFORM_WINDOWS) || defined(PLATFORM_LINUX)
    void* glfwWindow;
#elif defined(PLATFORM_ANDROID)
    void* window; // ANativeWindow*
#endif
  };

PUBLIC_GET_PRIVATE_SET(PlatformParams, _platformParams);

PUBLIC_GET_PRIVATE_SET(vk::ApplicationInfo, _applicationInfo);
PUBLIC_GET_PRIVATE_SET(std::vector<const char*>, _instanceRequiredExtensions);
PUBLIC_GET_PRIVATE_SET(vk::UniqueInstance, _instance);
PUBLIC_GET_PRIVATE_SET(vk::UniqueSurfaceKHR, _surface);
PUBLIC_GET_PRIVATE_SET(std::vector<vk::SurfaceFormatKHR>, _surfaceFormats);
PUBLIC_GET_PRIVATE_SET(vk::SurfaceCapabilitiesKHR, _surfaceCapabilities);
PUBLIC_GET_PRIVATE_SET(std::vector<vk::PresentModeKHR>, _surfacePresentModes);
PUBLIC_GET_PRIVATE_SET(std::vector<vk::PhysicalDevice>, _cachedPhysicalDevices);
PUBLIC_GET_PRIVATE_SET(vk::PhysicalDevice, _physicalDevice);
PUBLIC_GET_PRIVATE_SET(vk::PhysicalDeviceMemoryProperties, _cachedPhysicalDeviceMemoryProperties);
PUBLIC_GET_PRIVATE_SET(uint32_t, _queueFamilyIndex);
PUBLIC_GET_PRIVATE_SET(vk::UniqueDevice, _device);
PUBLIC_GET_PRIVATE_SET(vk::Queue, _graphicsQueue);
PUBLIC_GET_PRIVATE_SET(vk::UniqueSwapchainKHR, _swapchain);
PUBLIC_GET_PRIVATE_SET(std::vector<vk::Image>, _swapchainImages);
PUBLIC_GET_PRIVATE_SET(std::vector<vk::UniqueImageView>, _swapchainImageViews);
PUBLIC_GET_PRIVATE_SET(std::vector<vk::UniqueFramebuffer>, _framebuffer);
PUBLIC_GET_PRIVATE_SET(vk::UniqueFence, _swapchainImgFence);

PUBLIC_GET_PRIVATE_SET(std::vector<Vertex>, _vertices) = {
    Vertex{Vec3{1.0, 1.0, 1.0}, Vec3{0.0, 0.0, 1.0}},
    Vertex{Vec3{1.0, 1.0, 1.0}, Vec3{0.0, 1.0, 0.0}},
    Vertex{Vec3{1.0, 1.0, 1.0}, Vec3{1.0, 1.0, 1.0}},
};

PUBLIC_GET_PRIVATE_SET(vk::UniqueBuffer, _stagingVertexBuffer);
PUBLIC_GET_PRIVATE_SET(vk::UniqueDeviceMemory, _stagingVertexBufferMemory);
PUBLIC_GET_PRIVATE_SET(vk::UniqueBuffer, _vertexBuffer);
PUBLIC_GET_PRIVATE_SET(vk::UniqueDeviceMemory, _vertexBufferMemory);

PUBLIC_GET_PRIVATE_SET(std::vector<vk::UniqueDescriptorSetLayout>, _discriptorSetLayouts);


PUBLIC_GET_PRIVATE_SET(std::vector<vk::VertexInputBindingDescription>, _vertexInputBindingDescriptions);
PUBLIC_GET_PRIVATE_SET(std::vector<vk::VertexInputAttributeDescription>, _vertexInputAttributeDescriptions);

PUBLIC_GET_PRIVATE_SET(vk::UniquePipeline, _pipeline);
PUBLIC_GET_PRIVATE_SET(vk::UniquePipelineLayout, _pipelineLayout);
PUBLIC_GET_PRIVATE_SET(std::vector<vk::SubpassDescription>, _subpassDescriptions);

PUBLIC_GET_PRIVATE_SET(std::vector<vk::AttachmentReference>, _attachmentReference);
PUBLIC_GET_PRIVATE_SET(vk::UniqueRenderPass, _renderPass);

PUBLIC_GET_PRIVATE_SET(std::vector<vk::AttachmentDescription>, _attachmentDescriptions);

PUBLIC_GET_PRIVATE_SET(vk::UniqueCommandPool, _commandPool);
PUBLIC_GET_PRIVATE_SET(std::vector<vk::UniqueCommandBuffer>, _commandBuffers);


public:
    Renderer(PlatformParams, platformParams)
    {
        _platformParams = platformParams;

    }

    virtual ~Renderer()
    {
    }

    void handleInput();
    void render();

    void initialize();
    void finalize();

    void nextFrame();
    void submit();
    void wait();

    void submitOneShot();

    void createUI();

private:
    void createInstance();
    void createSurface();
    void selectPhysicalDeviceAndQueueFamilyIndex();
    void createGraphicsQueue();
    void cacheSurfaceData();
    static std::optional<uint32_t> getQueueFamilyIndex(vk::PhysicalDevice& physicalDevice, vk::UniqueSurfaceKHR& surface);
    void createDevice();
    void createSwapchain();
    void createFramebuffers();
    void createStagingVertexBuffer();
    void createVertexBuffer();
    void sendVertexBuffer();
    void createDiscriptorSetLayouts();
    void createVertexBindingDescription();
    void createRenderPass();
    void createSubpassDescriptions();
    void createPipeline();
    void createCommandBuffer();
};