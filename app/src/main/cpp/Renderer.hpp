#pragma once

#include <vulkan/vulkan.hpp>
#include "Vec3.hpp"
#include "Vertex.hpp"
#include "Utility.hpp"

extern "C" {
    #include <game-activity/native_app_glue/android_native_app_glue.h>
}

class Renderer {

PUBLIC_GET_PRIVATE_SET(android_app*, _pApp);
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
    Renderer(android_app* pApp)
    {
        _pApp = pApp;

        createInstance();
        createSurface();
        selectPhysicalDeviceAndQueueFamilyIndex();
        cacheSurfaceData();
        createDevice();
        createGraphicsQueue();
        createSwapchain();
        createFramebuffers();
        createStagingVertexBuffer();
        createVertexBuffer();
        sendVertexBuffer();
        createDiscriptorSetLayouts();
        createVertexBindingDescription();
        createRenderPass();
        createSubpassDescriptions();
        createPipeline();
        createCommandBuffer();
    }

    virtual ~Renderer()
    {
    }

    void handleInput();
    void render();

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