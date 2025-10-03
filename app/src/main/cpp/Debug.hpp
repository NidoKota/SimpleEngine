#pragma once

#include <iostream>
#include <memory>
#include <vulkan/vulkan.hpp>
#include "Utility.hpp"
#include "Renderer.hpp"

#if DEBUG
namespace Vulkan_Test
{
    void debug(Renderer* pRenderer)
    {
        // 全部1つの関数にして1度だけmainから呼ぶようにする
    }

    void debugApplicationInfo(Renderer* pRenderer)
    {
        uint32_t apiVersion = pRenderer->Get_applicationInfo().apiVersion;

        LOG("----------------------------------------");
        LOG("Debug ApplicationInfo");

        LOG("apiVersion: " << 
            VK_VERSION_MAJOR(apiVersion) << "." <<
            VK_VERSION_MINOR(apiVersion) << "." <<
            VK_VERSION_PATCH(apiVersion));
    }

    void debugInstanceCreateInfo(Renderer* pRenderer)
    {
        std::vector<const char*>& instanceRequiredExtensions = pRenderer->Get_instanceRequiredExtensions();

        LOG("----------------------------------------");
        LOG("Debug Instance Extensions");
        LOG("enabledExtensionCount: " << instanceRequiredExtensions.size());
        SET_LOG_INDEX(1);
        for (int i = 0; i < instanceRequiredExtensions.size(); i++)
        {
            LOG("----------------------------------------");
            LOG(instanceRequiredExtensions[i]);
        }
        SET_LOG_INDEX(0);
    }

    // UUIDを16進数で表示する関数
    std::string getUUID(const uint8_t* uuid, size_t size)
    {
        std::stringstream ss;
        for (size_t i = 0; i < size; ++i) 
        {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(uuid[i]);
            if (i < size - 1) 
            {
                ss << ":";
            }
        }
        return ss.str();
    }

    void debugPhysicalDevices(Renderer* pRenderer)
    {
        std::vector<vk::PhysicalDevice> &physicalDevices = pRenderer->Get_cachedPhysicalDevices();

        LOG("----------------------------------------");
        LOG("Debug Physical Devices");
        LOG("physicalDevicesCount: " << physicalDevices.size());
        SET_LOG_INDEX(1);
        for (vk::PhysicalDevice physicalDevice : physicalDevices)
        {
            LOG("----------------------------------------");
            vk::PhysicalDeviceProperties2 props;
            vk::PhysicalDeviceVulkan12Properties props12;
            vk::PhysicalDeviceVulkan11Properties props11;
            props.pNext = &props12;
            props12.pNext = &props11;
            physicalDevice.getProperties2(&props);

            LOG(props.properties.deviceName);
            LOG("deviceID: " << props.properties.deviceID);
            LOG("vendorID: " << props.properties.vendorID);
            LOG("deviceUUID: " << getUUID(props11.deviceUUID, VK_UUID_SIZE));
            LOG("driverName: " << props12.driverName);
            LOG("driverInfo: " << props12.driverInfo);
            LOG("maxMemoryAllocationSize: " << static_cast<unsigned long long>(props11.maxMemoryAllocationSize));
        }
        SET_LOG_INDEX(0);
    }

    void debugPhysicalDevice(Renderer* pRenderer)
    {
        vk::PhysicalDevice& physicalDevice = pRenderer->Get_physicalDevice();
        uint32_t queueFamilyIndex = pRenderer->Get_queueFamilyIndex();

        LOG("----------------------------------------");
        LOG("Debug Physical Devices");

        vk::PhysicalDeviceProperties2 props = physicalDevice.getProperties2();
        LOG("Found device and queue");
        LOG("physicalDevice: " << props.properties.deviceName);
        LOG("queueFamilyIndex: " << queueFamilyIndex);
    }

    void debugPhysicalMemory(Renderer* pRenderer)
    {
        vk::PhysicalDevice& physicalDevice = pRenderer->Get_physicalDevice();

        LOG("----------------------------------------");
        LOG("Debug Physical Memory");
        vk::PhysicalDeviceMemoryProperties memProps = physicalDevice.getMemoryProperties();
        LOG("memory type count: " << memProps.memoryTypeCount);
        LOG("memory heap count: " << memProps.memoryHeapCount);
        SET_LOG_INDEX(1);
        for (size_t i = 0; i < memProps.memoryTypeCount; i++)
        {
            LOG("----------------------------------------");
            LOG("memory index: " << i);
            LOG(to_string(memProps.memoryTypes[i].propertyFlags));
        }
        SET_LOG_INDEX(0);
    }

    void debugQueueFamilyProperties(Renderer* pRenderer)
    {
        vk::PhysicalDevice& physicalDevice = pRenderer->Get_physicalDevice();
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

        LOG("----------------------------------------");
        LOG("Debug Queue Family Properties");
        LOG("queue family count: " << queueFamilyProperties.size());
        SET_LOG_INDEX(1);
        for (size_t i = 0; i < queueFamilyProperties.size(); i++)
        {
            LOG("----------------------------------------");
            LOG("queue family index: " << i);
            LOG("queue count: " << queueFamilyProperties[i].queueCount);
            LOG(to_string(queueFamilyProperties[i].queueFlags));
        }
        SET_LOG_INDEX(0);
    }

    void debugSwapchainCreateInfo(Renderer* pRenderer)
    {
        std::vector<vk::SurfaceFormatKHR>& surfaceFormats = pRenderer->Get_surfaceFormats();
//        vk::SwapchainCreateInfoKHR& swapchainCreateInfo;

//        LOG("----------------------------------------");
//        LOG("Debug Swapchain Create Info");
//        LOG("swapchain minImageCount " << swapchainCreateInfo.minImageCount);
//        LOG("swapchainCreateInfo " << swapchainCreateInfo.imageExtent.width << ", " << swapchainCreateInfo.imageExtent.height);

        LOG("----------------------------------------");
        LOG("Debug Surface Formats");
        SET_LOG_INDEX(1);
        for (size_t i = 0; i < surfaceFormats.size(); i++)
        {
            LOG("----------------------------------------");
            LOG("surface formats index: " << i);
            LOG("format: " << to_string(surfaceFormats[i].format));
            LOG("colorSpace: " << to_string(surfaceFormats[i].colorSpace));
        }
        SET_LOG_INDEX(0);
    }
}
#endif