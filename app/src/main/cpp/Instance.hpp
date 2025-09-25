//#pragma once
//
//#include <iostream>
//#include <memory>
//#include <vulkan/vulkan.hpp>
//#include "Utility.hpp"
//#include "Debug.hpp"
//
//using namespace Vulkan_Test;
//
//std::shared_ptr<vk::ApplicationInfo> getAppInfo()
//{
//    std::shared_ptr<vk::ApplicationInfo> result = std::make_shared<vk::ApplicationInfo>();
//    result->pApplicationName = "YourApp";
//    result->applicationVersion = VK_MAKE_VERSION(1, 0, 0);
//    result->pEngineName = "NoEngine";
//    result->engineVersion = VK_MAKE_VERSION(1, 0, 0);
//    result->apiVersion = vk::enumerateInstanceVersion();
//    return result;
//}
//
//std::shared_ptr<vk::InstanceCreateInfo> getInstanceCreateInfo(
//    vk::ApplicationInfo& appInfo,
//    std::vector<const char*>& instanceRequiredExtensions)
//{
//    std::shared_ptr<vk::InstanceCreateInfo> instanceCreateInfo = std::make_shared<vk::InstanceCreateInfo>();
//    instanceCreateInfo->pApplicationInfo = &appInfo;
//    instanceCreateInfo->enabledExtensionCount = instanceRequiredExtensions.size();
//    instanceCreateInfo->ppEnabledExtensionNames = instanceRequiredExtensions.data();
//#ifdef __APPLE__
//    instanceCreateInfo->flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
//#endif
//    return instanceCreateInfo;
//}
//
//#if !defined(__ANDROID__)
//std::shared_ptr<std::vector<const char*>> getGlfwRequiredInstanceExtensions()
//{
//    uint32_t glfwExtensionsCount;
//    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);
//
//    std::shared_ptr<std::vector<const char*>> result = std::make_shared<std::vector<const char*>>(glfwExtensionsCount);
//    std::memcpy(result->data(), glfwExtensions, sizeof(const char*) * glfwExtensionsCount);
//
//    return result;
//}
//#endif
//
//#ifdef __APPLE__
//std::shared_ptr<std::vector<const char*>> getAppleRequiredInstanceExtensions()
//{
//    std::shared_ptr<std::vector<const char*>> result = std::make_shared<std::vector<const char*>>();
//    result->push_back(vk::KHRPortabilityEnumerationExtensionName);
//    return result;
//}
//#endif
//
//// vk::Instance の主な役割は以下の通り
//// Vulkan API の初期化: Vulkan ライブラリをロードし、必要なグローバルレベルの機能 (レイヤーや拡張機能など) を初期化する
//// 利用可能な Vulkan 実装の列挙: システムにインストールされている Vulkan ドライバ (物理デバイス) を検出するために使用される
//// グローバルな操作の管理: Vulkan API 全体に関わる操作 (例えば、デバッグコールバックの設定など) を行う
//// 他の Vulkan オブジェクトの作成の基盤: vk::PhysicalDevice (物理デバイス)、vk::Device (論理デバイス)、vk::SurfaceKHR (サーフェス) などの他の主要な Vulkan オブジェクトは、vk::Instance を通して作成される
//std::shared_ptr<vk::UniqueInstance> getInstance(vk::ApplicationInfo& appInfo)
//{;
//    std::shared_ptr<vk::UniqueInstance> result = std::make_shared<vk::UniqueInstance>();
//
//#if !defined(__ANDROID__)
//    std::shared_ptr<std::vector<const char*>> instanceRequiredExtensions = getGlfwRequiredInstanceExtensions();
//#else
//
//    std::shared_ptr<std::vector<const char*>> instanceRequiredExtensions = std::make_shared<std::vector<const char*>>();
//    instanceRequiredExtensions->push_back(VK_KHR_SURFACE_EXTENSION_NAME);
//    instanceRequiredExtensions->push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
//    // デバッグレポート
//    //instanceRequiredExtensions->push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
//#endif
//
//#ifdef __APPLE__
//    std::shared_ptr<std::vector<const char*>> appleRequiredInstanceExtensions = getAppleRequiredInstanceExtensions();
//    std::copy(appleRequiredInstanceExtensions->begin(), appleRequiredInstanceExtensions->end(), std::back_inserter(*instanceRequiredExtensions));
//#endif
//
//    std::shared_ptr<vk::InstanceCreateInfo> instanceCreateInfo = getInstanceCreateInfo(appInfo, *instanceRequiredExtensions);
//    debugInstanceCreateInfo(*instanceCreateInfo);
//
//    *result = vk::createInstanceUnique(*instanceCreateInfo);
//    return result;
//}