#include "Renderer copy.hpp"

////////////////// public //////////////////

void Renderer::initialize()
{
   createInstance();
   createSurface();
   selectPhysicalDeviceAndQueueFamilyIndex();
   cacheSurfaceData();
   createDevice();
   createSwapchain();
   createDiscriptorSetLayouts();
   createCommandBuffer();
}

void Renderer::finalize()
{
    wait();
}

void Renderer::nextFrame()
{
    auto& frameInfo = m_frameCommandInfos[m_currentFrameIndex];
    auto fence = frameInfo.commandFence;
    vkWaitForFences(m_vkDevice, 1, &fence, VK_TRUE, UINT64_MAX);
    auto res = vkAcquireNextImageKHR(m_vkDevice, m_swapchain, UINT64_MAX, frameInfo.presentCompleted, VK_NULL_HANDLE, &m_swapchainImageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR)
    {
      return;
    }
    vkResetFences(m_vkDevice, 1, &fence);

    // コマンドバッファを開始.
    vkResetCommandBuffer(frameInfo.commandBuffer, 0);
    VkCommandBufferBeginInfo commandBeginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    vkBeginCommandBuffer(frameInfo.commandBuffer, &commandBeginInfo);
}

void Renderer::submit()
{
    auto& frameInfo = m_frameCommandInfos[m_currentFrameIndex];
    vkEndCommandBuffer(frameInfo.commandBuffer);

    // コマンドを発行する.
    VkPipelineStageFlags waitStage{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSubmitInfo submitInfo{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &frameInfo.presentCompleted,
      .pWaitDstStageMask = &waitStage,
      .commandBufferCount = 1,
      .pCommandBuffers = &frameInfo.commandBuffer,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &frameInfo.renderCompleted,
    };
    vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, frameInfo.commandFence);

    m_currentFrameIndex = (++m_currentFrameIndex) % InflightFrames;

    // プレゼンテーションの実行.
    VkPresentInfoKHR presentInfo{
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &frameInfo.renderCompleted,
      .swapchainCount = 1,
      .pSwapchains = &m_swapchain,
      .pImageIndices = &m_swapchainImageIndex
    };
    vkQueuePresentKHR(m_graphicsQueue, &presentInfo);

}

void Renderer::wait()
{
    if (m_vkDevice != VK_NULL_HANDLE)
    {
      vkDeviceWaitIdle(m_vkDevice);
    }
}

void Renderer::submitOneShot()
{
    vkEndCommandBuffer(commandBuffer);

    VkFenceCreateInfo fenceCI{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    VkFence waitFence;
    vkCreateFence(m_vkDevice, &fenceCI, nullptr, &waitFence);

    VkSubmitInfo submitInfo{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &commandBuffer,
    };
    vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, waitFence);

    // 実行完了を待機して、廃棄処理.
    vkWaitForFences(m_vkDevice, 1, &waitFence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(m_vkDevice, waitFence, nullptr);
    vkFreeCommandBuffers(m_vkDevice, m_commandPool, 1, &commandBuffer);
}

void Renderer::createUI()
{
    ImGui::Begin("Information");
    ImGui::Text("FPS: %.2f", ImGui::GetIO().Framerate);
    ImGui::Text(useDynamicRendering ? "USE Dynamic Rendering" : "USE RenderPass");
    {
      float* v = reinterpret_cast<float*>(&m_lightDir);
      ImGui::InputFloat3("LightDir", v);
    }
    ImGui::End();

    // ImGui の描画処理.
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

////////////////// private //////////////////

void Renderer::createInstance()
{
    _applicationInfo.pApplicationName = "DrawModel";
    _applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    _applicationInfo.pEngineName = "SimpleEngine";
    _applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    _applicationInfo.apiVersion = vk::enumerateInstanceVersion();

    _instanceRequiredExtensions = std::vector<const char*>();
    _instanceRequiredExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    _instanceRequiredExtensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
//    _instanceRequiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    vk::InstanceCreateInfo instanceCreateInfo = vk::InstanceCreateInfo();
    instanceCreateInfo.pApplicationInfo = &_applicationInfo;
    instanceCreateInfo.enabledExtensionCount = _instanceRequiredExtensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = _instanceRequiredExtensions.data();

    _instance = vk::createInstanceUnique(instanceCreateInfo);
}

void Renderer::createSurface()
{
    vk::AndroidSurfaceCreateInfoKHR surfaceCreateInfo = vk::AndroidSurfaceCreateInfoKHR(vk::AndroidSurfaceCreateFlagsKHR(), _pApp->window);
    _surface = _instance->createAndroidSurfaceKHRUnique(surfaceCreateInfo);
}

std::optional<uint32_t> Renderer::getQueueFamilyIndex(vk::PhysicalDevice& physicalDevice, vk::UniqueSurfaceKHR& surface)
{
    // Vulkanにおけるキューとは、GPUの実行するコマンドを保持する待ち行列 = GPUのやることリスト
    // GPUにコマンドを送るときは、このキューにコマンドを詰め込むことになる
    //
    // 1つのGPUが持っているキューは1つだけとは限らない
    // キューによってサポートしている機能とサポートしていない機能がある
    // キューにコマンドを送るときは、そのキューが何の機能をサポートしているかを事前に把握しておく必要がある
    //
    // ある物理デバイスの持っているキューの情報は getQueueFamilyProperties メソッドで取得できる
    // メソッド名にある「キューファミリ」というのは、同じ能力を持っているキューをひとまとめにしたもの
    // 1つの物理デバイスには1個以上のキューファミリがあり、1つのキューファミリには1個以上の同等の機能を持ったキューが所属している
    // 1つのキューファミリの情報は vk::QueueFamilyProperties 構造体で表される
    std::vector<vk::QueueFamilyProperties> queueProps = physicalDevice.getQueueFamilyProperties();
    std::vector<vk::ExtensionProperties> extProps = physicalDevice.enumerateDeviceExtensionProperties();
    for (size_t i = 0; i < queueProps.size(); i++)
    {
        // グラフィックス機能に加えてサーフェスへのプレゼンテーションもサポートしているキューを厳選
        if (!(queueProps[i].queueFlags & vk::QueueFlagBits::eGraphics) ||
            !physicalDevice.getSurfaceSupportKHR(i, *surface))
        {
            continue;
        }

        for (size_t j = 0; j < extProps.size(); j++)
        {
            // enumerateDeviceExtensionPropertiesメソッドでその物理デバイスがサポートしている拡張機能の一覧を取得
            // その中にスワップチェーンの拡張機能の名前が含まれていなければそのデバイスは使わない
            if (std::string_view(extProps[j].extensionName.data()) != VK_KHR_SWAPCHAIN_EXTENSION_NAME)
            {
                continue;
            }

            return i;
        }
    }

    return std::nullopt;
}

void Renderer::selectPhysicalDeviceAndQueueFamilyIndex()
{
    // vk::Instanceにはそれに対応するvk::UniqueInstanceが存在したが、
    // vk::PhysicalDeviceに対応するvk::UniquePhysicalDevice は存在しない
    // destroyなどを呼ぶ必要もない
    // vk::PhysicalDeviceは単に物理的なデバイスの情報を表しているに過ぎないので、構築したり破棄したりする必要がある類のオブジェクトではない
    _cachedPhysicalDevices = _instance.get().enumeratePhysicalDevices();

    // GPUが複数あるなら頼む相手をまず選ぶ必要がある
    // GPUの型式などによってサポートしている機能とサポートしていない機能があったりするため、
    // だいたい「インスタンスを介して物理デバイスを列挙する」→「それぞれの物理デバイスの情報を取得する」→「一番いいのを頼む」という流れになる

    // デバイスとキューの検索
    bool success = false;
    for (size_t i = 0; i < _cachedPhysicalDevices.size(); i++)
    {
        _physicalDevice = _cachedPhysicalDevices[i];

        // デバイスがサーフェスを間違いなくサポートしていることを確かめる
        if (_physicalDevice.getSurfaceFormatsKHR(*_surface).empty() &&
            _physicalDevice.getSurfacePresentModesKHR(*_surface).empty())
        {
            continue;
        }

        std::optional<uint32_t> queueFamilyIndex = getQueueFamilyIndex(_physicalDevice, _surface);
        if (!queueFamilyIndex)
        {
            continue;
        }
        _queueFamilyIndex = queueFamilyIndex.value();

        success = true;
        break;
    }

    if (!success)
    {
        LOGERR("No physical devices are available");
        exit(EXIT_FAILURE);
    }

    _cachedPhysicalDeviceMemoryProperties = _physicalDevice.getMemoryProperties();
}

void Renderer::cacheSurfaceData()
{
    // 「物理デバイスが対象のサーフェスを扱う能力」の情報を取得する
    _surfaceFormats = _physicalDevice.getSurfaceFormatsKHR(_surface.get());

    // 「サーフェスの情報」の情報を取得する
    _surfaceCapabilities = _physicalDevice.getSurfaceCapabilitiesKHR(_surface.get());

    // 「物理デバイスが対象のサーフェスを扱う能力」の情報を取得する
    _surfacePresentModes = _physicalDevice.getSurfacePresentModesKHR(_surface.get());
}

void Renderer::createGraphicsQueue() 
{
    _graphicsQueue = _device.get().getQueue(_queueFamilyIndex, 0);
}

void Renderer::createDevice()
{
    // インスタンスを作成するときにvk::InstanceCreateInfo構造体を使ったのと同じように、
    // 論理デバイス作成時にもvk::DeviceCreateInfo構造体の中に色々な情報を含めることができる
    std::vector<const char*> deviceRequiredLayers = std::vector<const char*>();
    deviceRequiredLayers.push_back("VK_LAYER_KHRONOS_validation");

    // スワップチェーンは拡張機能なので、機能を有効化する必要がある
    // スワップチェーンは「デバイスレベル」の拡張機能
    std::vector<const char*> deviceRequiredExtensions = std::vector<const char*>();
    deviceRequiredExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    // 今のところ欲しいキューは1つだけなので要素数1の配列にする
    std::vector<float> queuePriorities = std::vector<float>();
    queuePriorities.push_back(1.0f);

    // queueFamilyIndex はキューの欲しいキューファミリのインデックスを表し、
    // queueCount はそのキューファミリからいくつのキューが欲しいかを表す
    // 今欲しいのはグラフィック機能をサポートするキューを1つだけ
    // pQueuePriorities はキューのタスク実行の優先度を表すfloat値配列を指定
    // 優先度の値はキューごとに決められるため、この配列はそのキューファミリから欲しいキューの数だけの要素数を持ちます
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfo = std::vector<vk::DeviceQueueCreateInfo>();
    queueCreateInfo.emplace_back();
    queueCreateInfo[0].queueFamilyIndex = _queueFamilyIndex;
    queueCreateInfo[0].queueCount = queuePriorities.size();
    queueCreateInfo[0].pQueuePriorities = queuePriorities.data();

    vk::DeviceCreateInfo deviceCreateInfo = vk::DeviceCreateInfo();
    deviceCreateInfo.enabledLayerCount = deviceRequiredLayers.size();
    deviceCreateInfo.ppEnabledLayerNames = deviceRequiredLayers.data();
    deviceCreateInfo.enabledExtensionCount = deviceRequiredExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceRequiredExtensions.data();
    deviceCreateInfo.queueCreateInfoCount = queueCreateInfo.size();
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfo.data();

    // GPUが複数あるなら頼む相手をまず選ぶ
    // ここで言う「選ぶ」とは、特定のGPUを完全に占有してしまうとかそういう話ではない
    //
    // 物理デバイスを選んだら次は論理デバイスを作成する
    // VulkanにおいてGPUの能力を使うような機能は、全てこの論理デバイスを通して利用する
    // GPUの能力を使う際、物理デバイスを直接いじることは出来ない
    //
    // コンピュータの仕組みについて多少知識のある人であれば、
    // この「物理」「論理」の区別はメモリアドレスの「物理アドレス」「論理アドレス」と同じ話であることが想像できる
    // マルチタスクOS上で、1つのプロセスが特定のGPU(=物理デバイス)を独占して管理するような状態はよろしくない
    //
    // 仮想化されたデバイスが論理デバイス
    // これならあるプロセスが他のプロセスの存在を意識することなくGPUの能力を使うことができる
    _device = _physicalDevice.createDeviceUnique(deviceCreateInfo);
}

void Renderer::createSwapchain() {
    vk::SwapchainCreateInfoKHR swapchainCreateInfo;

    swapchainCreateInfo.surface = _surface.get();
    // minImageCountはスワップチェーンが扱うイメージの数
    // getSurfaceCapabilitiesKHRで得られたminImageCount(最小値)とmaxImageCount(最大値)の間の数なら何枚でも問題ない
    // https://vulkan-tutorial.com/
    // の記述に従って最小値+1を指定する
    swapchainCreateInfo.minImageCount = _surfaceCapabilities.minImageCount + 1;
    // imageFormatやimageColorSpaceなどには、スワップチェーンが取り扱う画像の形式などを指定する
    // しかしこれらに指定できる値はサーフェスとデバイスの事情によって制限されるものであり、自由に決めることができるものではない
    // ここに指定する値は、必ずgetSurfaceFormatsKHRが返した配列に含まれる組み合わせでなければならない
    swapchainCreateInfo.imageFormat = _surfaceFormats[0].format;
    swapchainCreateInfo.imageColorSpace = _surfaceFormats[0].colorSpace;
    // imageExtentはスワップチェーンの画像サイズを表す
    // getSurfaceCapabilitiesKHRで得られたminImageExtent(最小値)とmaxImageExtent(最大値)の間でなければならない
    // currentExtentで現在のサイズが得られるため、それを指定
    swapchainCreateInfo.imageExtent = _surfaceCapabilities.currentExtent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    swapchainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
    // preTransformは表示時の画面反転・画面回転などのオプションを指定する
    // これもgetSurfaceCapabilitiesKHRの戻り値に依存する
    // supportedTransformsでサポートされている中に含まれるものでなければならないので、無難なcurrentTransform(現在の画面設定)を指定
    swapchainCreateInfo.preTransform = _surfaceCapabilities.currentTransform;
    // presentModeは表示処理のモードを示すもの
    // getSurfacePresentModesKHRの戻り値の配列に含まれる値である必要がある
    swapchainCreateInfo.presentMode = _surfacePresentModes[0];
    swapchainCreateInfo.clipped = VK_TRUE;

    // 一般的なコンピュータは、アニメーションを描画・表示する際に「描いている途中」が見えないようにするため、
    // 2枚以上のキャンバスを用意して、1枚を使って現在のフレームを表示させている裏で別の1枚に次のフレームを描画する、という仕組みを採用している
    // スワップチェーンは、一言で言えば「画面に表示されようとしている画像の連なり」
    //
    // スワップチェーンは自分でイメージを保持している
    // 我々はイメージを作成する必要はなく、スワップチェーンが元から保持しているイメージを取得してそこに描画することになる
    //
    // スワップチェーンからイメージを取得した後は基本的に同じ
    // イメージからイメージビューを作成、
    // レンダーパス・パイプラインなどの作成、
    // フレームバッファを作成してイメージビューと紐づけ、
    // コマンドバッファにコマンドを積んでキューに送信
    _swapchain = _device->createSwapchainKHRUnique(swapchainCreateInfo);

    _swapchainImages = _device.get().getSwapchainImagesKHR(_swapchain.get());

    _swapchainImageViews.resize(_swapchainImages.size());
    for (size_t i = 0; i < _swapchainImageViews.size(); i++)
    {
        vk::ImageViewCreateInfo imgViewCreateInfo;
        imgViewCreateInfo.image = _swapchainImages[i];
        imgViewCreateInfo.viewType = vk::ImageViewType::e2D;
        // ここに指定するフォーマットは元となるイメージに合わせる必要がある
        // スワップチェーンのイメージの場合、イメージのフォーマットはスワップチェーンを作成する時に指定したフォーマットになるため、その値を指定
        imgViewCreateInfo.format = _surfaceFormats[0].format;
        imgViewCreateInfo.components.r = vk::ComponentSwizzle::eIdentity;
        imgViewCreateInfo.components.g = vk::ComponentSwizzle::eIdentity;
        imgViewCreateInfo.components.b = vk::ComponentSwizzle::eIdentity;
        imgViewCreateInfo.components.a = vk::ComponentSwizzle::eIdentity;
        imgViewCreateInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        imgViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imgViewCreateInfo.subresourceRange.levelCount = 1;
        imgViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imgViewCreateInfo.subresourceRange.layerCount = 1;

        _swapchainImageViews[i] = _device.get().createImageViewUnique(imgViewCreateInfo);
    }

    vk::FenceCreateInfo fenceCreateInfo;
    _swapchainImgFence = _device->createFenceUnique(fenceCreateInfo);
}

void Renderer::createDiscriptorSetLayouts()
{
    // vk::DescriptorSetLayoutBindingがデスクリプタ1つの情報を表す
    // これの配列からデスクリプタセットレイアウトを作成
    //
    // binding はシェーダと結びつけるための番号を表す = バインディング番号
    //    頂点シェーダにlayout(set = 0, binding = 0)などと指定したが、このときのbindingの数字と揃えてあることに注意
    // descriptorType はデスクリプタの種別を示す
    //    今回シェーダに渡すものはバッファなのでvk::DescriptorType::eUniformBufferを指定
    // descriptorCount はデスクリプタの個数を表す
    //    デスクリプタは配列として複数のデータを持てるが、ここにその要素数を指定する 今回は1個だけなので1を指定
    // stageFlags はデータを渡す対象となるシェーダを示す
    //    今回は頂点シェーダだけに渡すのでvk::ShaderStageFlagBits::eVertexを指定 フラグメントシェーダに渡したい場合はvk::ShaderStageFlagBits::eFragmentを指定します。ビットマスクなので、ORで重ねれば両方に渡すことも可能です。

    vk::DescriptorSetLayoutBinding descSetLayoutBinding[1];
    descSetLayoutBinding[0].binding = 0;
    descSetLayoutBinding[0].descriptorType = vk::DescriptorType::eUniformBuffer;
    descSetLayoutBinding[0].descriptorCount = 1;
    descSetLayoutBinding[0].stageFlags = vk::ShaderStageFlagBits::eVertex;

    vk::DescriptorSetLayoutCreateInfo descSetLayoutCreateInfo{};
    descSetLayoutCreateInfo.bindingCount = std::size(descSetLayoutBinding);
    descSetLayoutCreateInfo.pBindings = descSetLayoutBinding;

    // デスクリプタセットレイアウトを作成したあとはそれをパイプラインレイアウトに設定する必要がある
    // パイプラインは描画の手順を表すオブジェクト
    // 頂点入力デスクリプションなどと同様、シェーダへのデータの読み込ませ方はここで設定する
    _discriptorSetLayouts.push_back(_device->createDescriptorSetLayoutUnique(descSetLayoutCreateInfo));
}








void Renderer::createFramebuffers() {
    // レンダーパスは処理(サブパス)とデータ(アタッチメント)のつながりと関係性を記述するが、具体的な処理内容やどのデータを扱うかについては関与しない
    // 具体的な処理内容はコマンドバッファに積むコマンドやパイプラインによって決まるが、具体的なデータの方を決めるためのものがフレームバッファである

    // フレームバッファはレンダーパスのアタッチメントとイメージビューを対応付けるもの
    // 今回は画面へ表示するにあたり、スワップチェーンの各イメージビューに描画していく
    // ということは、フレームバッファは1つだけではなくイメージビューごとに作成する必要がある
    _framebuffer = std::vector<vk::UniqueFramebuffer>(_swapchainImages.size());

    for (size_t i = 0; i < _swapchainImages.size(); i++)
    {
        // イメージビューの情報を初期化用構造体に入れている
        // これで0番のアタッチメントがどのイメージビューに対応しているのかを示すことができる
        vk::ImageView frameBufferAttachments[1];
        frameBufferAttachments[0] = _swapchainImageViews[i].get();

        // フレームバッファを介して「0番のアタッチメントはこのイメージビュー、1番のアタッチメントは…」という結び付けを行うことで初めてレンダーパスが使える
        vk::FramebufferCreateInfo framebufferCreateInfo;
        framebufferCreateInfo.width = _surfaceCapabilities.currentExtent.width;
        framebufferCreateInfo.height = _surfaceCapabilities.currentExtent.height;
        framebufferCreateInfo.layers = 1;
        framebufferCreateInfo.renderPass = _renderPass.get();
        framebufferCreateInfo.attachmentCount = std::size(frameBufferAttachments);
        framebufferCreateInfo.pAttachments = frameBufferAttachments;

        // ここで注意だが、初期化用構造体にレンダーパスの情報を入れてはいるものの、これでレンダーパスとイメージビューが結びついた訳ではない
        // ここで入れているレンダーパスの情報はあくまで「このフレームバッファはどのレンダーパスと結びつけることができるのか」を表しているに過ぎず、
        // フレームバッファを作成した時点で結びついた訳ではない
        // フレームバッファとレンダーパスを本当に結びつける処理はこの次に行う
        //
        // パイプラインの作成処理でもレンダーパスの情報を渡しているが、ここにも同じ事情がある
        // フレームバッファとパイプラインは特定のレンダーパスに依存して作られるものであり、互換性のない他のレンダーパスのために働こうと思ってもそのようなことはできない
        // 結びつけを行っている訳ではないのにレンダーパスの情報を渡さなければならないのはそのため
        _framebuffer[i] = _device.get().createFramebufferUnique(framebufferCreateInfo);
    }
}

void Renderer::createStagingVertexBuffer() {
    vk::BufferCreateInfo stagingBufferCreateInfo;
    stagingBufferCreateInfo.size = sizeof(Vertex) * _vertices.size();
    stagingBufferCreateInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
    stagingBufferCreateInfo.sharingMode = vk::SharingMode::eExclusive;

    _stagingVertexBuffer = _device.get().createBufferUnique(stagingBufferCreateInfo);



    vk::MemoryRequirements vertexBufferMemoryRequirements = _device.get().getBufferMemoryRequirements(_stagingVertexBuffer.get());

    vk::MemoryAllocateInfo vertexBufferMemAllocInfo;
    vertexBufferMemAllocInfo.allocationSize = vertexBufferMemoryRequirements.size;

    bool suitableMemoryTypeFound = false;
    for (uint32_t i = 0; i < _cachedPhysicalDeviceMemoryProperties.memoryTypeCount; i++)
    {
        if ((vertexBufferMemoryRequirements.memoryTypeBits & (1 << i)) &&
            (_cachedPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible))
        {
            vertexBufferMemAllocInfo.memoryTypeIndex = i;
            suitableMemoryTypeFound = true;
            break;
        }
    }
    if (!suitableMemoryTypeFound)
    {
        LOGERR("Suitable memory type not found. ");
        exit(EXIT_FAILURE);
    }

    _stagingVertexBufferMemory = _device.get().allocateMemoryUnique(vertexBufferMemAllocInfo);
    _device.get().bindBufferMemory(_stagingVertexBuffer.get(), _stagingVertexBufferMemory.get(), 0);



    // デバイスメモリに書き込むために、メモリマッピングというものをする
    // これは操作したい対象のデバイスメモリを仮想的にアプリケーションのメモリ空間に対応付けることで操作出来るようにするもの
    // 対象のデバイスメモリを直接操作するわけにはいかないのでこういう形になっている
    void* pStagingVertexBufferMem = _device.get().mapMemory(_stagingVertexBufferMemory.get(), 0, sizeof(Vertex) * _vertices.size());

    std::memcpy(pStagingVertexBufferMem, _vertices.data(), sizeof(Vertex) * _vertices.size());

    vk::MappedMemoryRange flushMemoryRange;
    flushMemoryRange.memory = _stagingVertexBufferMemory.get();
    flushMemoryRange.offset = 0;
    flushMemoryRange.size = sizeof(Vertex) * _vertices.size();

    // 書き込んだら flushMappedMemoryRangesメソッドを呼ぶことで書き込んだ内容がデバイスメモリに反映される
    // マッピングされたメモリはあくまで仮想的にデバイスメモリと対応付けられているだけ
    // 「同期しておけよ」と念をおさないとデータが同期されない可能性がある
    _device.get().flushMappedMemoryRanges({ flushMemoryRange });
    // 作業が終わった後はunmapMemoryできちんと後片付けをする
    _device.get().unmapMemory(_stagingVertexBufferMemory.get());
}

void Renderer::createVertexBuffer()
{
    // バッファというのはデバイスメモリ上のデータ列を表すオブジェクト
    // 何度も言うようにGPUから普通のメモリの内容は参照できない
    // シェーダで使いたいデータがある場合、まずデバイスメモリに移す必要がある
    // そしてそれはプログラムの上では「バッファに書き込む」「バッファに書き込んで転送する」という形になる
    // 頂点座標のデータをシェーダに送るためのバッファを作成する
    // いわゆる頂点バッファ
    // size はバッファの大きさをバイト数で示すもの
    // ここに例えば100という値を指定すれば、100バイトの大きさのバッファが作成できる
    // ここでは前節で定義した構造体のバイト数をsizeof演算子で取得し、それにデータの数をかけている

    // 次は実際に使われる頂点バッファの作成
    // 今までと違い、メモリの確保時にvk::MemoryPropertyFlagBits::eDeviceLocalフラグを持ったメモリを使うようにするE
    // 逆にeHostVisibleは要らない
    // また、usageにvk::BufferUsageFlagBits::eTransferDstを追加で指定する
    // データの転送先という意味
    // あとでステージングバッファからデータを転送してくる
    vk::BufferCreateInfo vertexBufferCreateInfo;
    vertexBufferCreateInfo.size = sizeof(Vertex) * _vertices.size();
    // usage は作成するバッファの使い道を示すためのもの
    // 今回のように頂点バッファを作る場合、上記のようにvk::BufferUsageFlagBits::eVertexBufferフラグを指定しなければならない
    // 他にも場合によって様々なフラグを指定する必要があり、複数のフラグを指定することもある
    vertexBufferCreateInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst; // eTransferDstを追加
    // sharingModeについては今のところは無視
    // ここではvk::SharingMode::eExclusiveを指定
    vertexBufferCreateInfo.sharingMode = vk::SharingMode::eExclusive;

    _vertexBuffer = _device.get().createBufferUnique(vertexBufferCreateInfo);



    // 実際に使われる頂点バッファのデバイスメモリを確保する

    vk::MemoryRequirements vertexBufferMemReq = _device.get().getBufferMemoryRequirements(_vertexBuffer.get());

    vk::MemoryAllocateInfo vertexBufferMemAllocateInfo;
    vertexBufferMemAllocateInfo.allocationSize = vertexBufferMemReq.size;

    bool suitableMemoryTypeFound = false;
    for (uint32_t i = 0; i < _cachedPhysicalDeviceMemoryProperties.memoryTypeCount; i++)
    {
        if ((vertexBufferMemReq.memoryTypeBits & (1 << i)) &&
            (_cachedPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal))
        {
            vertexBufferMemAllocateInfo.memoryTypeIndex = i;
            suitableMemoryTypeFound = true;
            break;
        }
    }
    if (!suitableMemoryTypeFound)
    {
        LOGERR("Suitable memory type not found. ");
        exit(EXIT_FAILURE);
    }

    _vertexBufferMemory = _device.get().allocateMemoryUnique(vertexBufferMemAllocateInfo);
    // デバイスメモリが確保出来たら bindBufferMemoryで結び付ける
    // 第1引数は結びつけるバッファ、第2引数は結びつけるデバイスメモリ
    // 第3引数は、確保したデバイスメモリのどこを(先頭から何バイト目以降を)使用するかを指定するもの
    _device.get().bindBufferMemory(_vertexBuffer.get(), _vertexBufferMemory.get(), 0);




    // デバイスメモリに書き込むために、メモリマッピングというものをする
    // これは操作したい対象のデバイスメモリを仮想的にアプリケーションのメモリ空間に対応付けることで操作出来るようにするもの
    // 対象のデバイスメモリを直接操作するわけにはいかないのでこういう形になっている
    void* pStagingVertexBufferMemory = _device.get().mapMemory(_stagingVertexBufferMemory.get(), 0, sizeof(Vertex) * _vertices.size());

    std::memcpy(pStagingVertexBufferMemory, _vertices.data(), sizeof(Vertex) * _vertices.size());

    vk::MappedMemoryRange flushMemoryRange;
    flushMemoryRange.memory = _stagingVertexBufferMemory.get();
    flushMemoryRange.offset = 0;
    flushMemoryRange.size = sizeof(Vertex) * _vertices.size();

    // 書き込んだら flushMappedMemoryRangesメソッドを呼ぶことで書き込んだ内容がデバイスメモリに反映される
    // マッピングされたメモリはあくまで仮想的にデバイスメモリと対応付けられているだけ
    // 「同期しておけよ」と念をおさないとデータが同期されない可能性がある
    _device.get().flushMappedMemoryRanges({ flushMemoryRange });
    // 作業が終わった後はunmapMemoryできちんと後片付けをする
    _device.get().unmapMemory(_stagingVertexBufferMemory.get());
}

void Renderer::sendVertexBuffer()
{
    // こちらはメモリマッピングではデータを入れられない ホスト可視でないため
    // ホスト可視でないメモリはCPUからは触れない
    // GPUにコピーさせる訳だが、GPUに命令するということは例によってコマンドバッファとキューを使う
    // データの転送用に、コマンドバッファを作成するコードを別途追加する
    vk::CommandPoolCreateInfo tmpCommandPoolCreateInfo;
    tmpCommandPoolCreateInfo.queueFamilyIndex = _queueFamilyIndex;
    // vk::CommandPoolCreateFlagBits::eTransientフラグを指定
    // これは比較的すぐに使ってすぐに役目を終えるコマンドバッファ用であることを意味するフラグ
    // 必須ではないが指定しておくと内部的に最適化が起きる可能性がある
    tmpCommandPoolCreateInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;
    vk::UniqueCommandPool tmpCmdPool = _device->createCommandPoolUnique(tmpCommandPoolCreateInfo);

    vk::CommandBufferAllocateInfo tmpCommandBufferAllocateInfo;
    tmpCommandBufferAllocateInfo.commandPool = tmpCmdPool.get();
    tmpCommandBufferAllocateInfo.commandBufferCount = 1;
    tmpCommandBufferAllocateInfo.level = vk::CommandBufferLevel::ePrimary;
    std::vector<vk::UniqueCommandBuffer> tmpCommandBuffers = _device->allocateCommandBuffersUnique(tmpCommandBufferAllocateInfo);

    // バッファ間でデータをコピーするには copyBuffer を使う
    // そしてコピーするバッファやコピーする領域を指定するために vk::BufferCopy 構造体を使う
    // srcOffsetは転送元バッファの先頭から何バイト目を読み込むというデータ位置、dstOffsetは転送先バッファの先頭から何バイト目に書き込むというデータ位置、sizeはデータサイズを表す
    // memcpyの引数と似たような感じだと理解すると分かりやすい
    // あとはキューに投げるだけ
    vk::BufferCopy bufferCopy;
    bufferCopy.srcOffset = 0;
    bufferCopy.dstOffset = 0;
    bufferCopy.size = sizeof(Vertex) * _vertices.size();

    vk::CommandBufferBeginInfo cmdBeginInfo;
    cmdBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    tmpCommandBuffers[0]->begin(cmdBeginInfo);
    tmpCommandBuffers[0]->copyBuffer(_stagingVertexBuffer.get(), _vertexBuffer.get(), { bufferCopy });
    tmpCommandBuffers[0]->end();

    vk::CommandBuffer submitCmdBuf[1] = {tmpCommandBuffers[0].get()};
    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = submitCmdBuf;

    _graphicsQueue.submit({submitInfo});
    _graphicsQueue.waitIdle();
}

void Renderer::createVertexBindingDescription()
{
    // このままではバッファがどういう形式で頂点のデータを持っているのか、Vulkanのシステム側には分からない
    // Vertex構造体というこちらが勝手に定義してみただけの構造体のことは知りようがない
    // このままではシェーダに読み込ませられない
    // そこで頂点入力デスクリプションというものを用意する
    // 適切な説明(デスクリプション)を与えることで「こういう形式でこういうデータが入っている」という情報を与えることができる
    // データだけでなく説明を与えて初めてシェーダからデータを読み込める

    // 頂点入力デスクリプションには2種類ある
    // アトリビュートデスクリプション(vk::VertexInputAttributeDescription)
    // バインディングデスクリプション(vk::VertexInputBindingDescription)
    // これはひとえに、シェーダに与えるデータを分ける単位が「バインディング」と「アトリビュート」の2段階あるため

    // バインディングが一番上の単位
    // 基本的に1つのバインディングに対して1つのバッファ(の範囲)が結び付けられる
    // あるバインディングに含まれるデータは一定の大きさごとに切り分けられ、各頂点のデータとして解釈される
    // アトリビュートはより細かい単位になる
    // 1つの頂点のデータは、1つまたは複数のアトリビュートとして分けられる
    // 一つの頂点データは「座標」とか「色」みたいな複数のデータを内包していますが、この1つ1つのデータが1つのアトリビュートに相当することになる
    // (注意点として、1つのアトリビュートは「多次元ベクトル」や「行列」だったりできるので、アトリビュートは「x座標」とか「y座標」みたいな単位よりはもう少し大きい単位になる)

    // バインディングデスクリプションは、1つのバインディングのデータをどうやって各頂点のデータに分割するかという情報を設定する。
    // 具体的には、1つの頂点データのバイト数などを設定する。
    // そしてアトリビュートデスクリプションは、1つの頂点データのどの位置からどういう形式でアトリビュートのデータを取り出すかという情報を設定する。
    // アトリビュートデスクリプションはアトリビュートの数だけ作成する。
    // 1つの頂点データが3種類のデータを含んでいるならば、3つのアトリビュートデスクリプションを作らなければならない。
    // 今回のデータだと含んでいるデータは「座標」だけなので1つだけになるが、複数のアトリビュートを含む場合はその数だけ作成する。

    // bindingは説明の対象となるバインディングの番号である。各頂点入力バインディングには0から始まる番号が振ってある。ここでは0番を使うことにする。
    // strideは1つのデータを取り出す際の刻み幅である。つまり上の図でいう所の「1つの頂点データ」の大きさである。ここではVertex構造体のバイト数を指定している。
    // 前節までで用意したデータはVertex構造体が並んでいるわけなので、各データを取り出すには当然Vertex構造体のバイト数ずつずらして読み取ることになる。
    // inputRateにはvk::VertexInputRate::eVertexを指定する。1頂点ごとにデータを読み込む、というだけの意味である。インスタンス化というものを行う場合に別の値を指定する。
    vk::VertexInputBindingDescription vertexBindingDescription;
    vertexBindingDescription.binding = 0;
    vertexBindingDescription.stride = sizeof(Vertex);
    vertexBindingDescription.inputRate = vk::VertexInputRate::eVertex;

    _vertexInputBindingDescriptions.push_back(vertexBindingDescription);


    // アトリビュートの読み込み方の説明情報
    // 今度は vk::VertexInputAttributeDescription を用意する
    // 今回用意した頂点データが含む情報は「座標」だけなので、用意するアトリビュートデスクリプションは1つだけ
    //
    // bindingはデータの読み込み元のバインディングの番号を指定。上で0番のバインディングを使うことにしたので、ここでも0にする。
    // locationはシェーダにデータを渡す際のデータ位置。
    //
    // 頂点データの用意に書いたシェーダのコードを見てみる
    // layout(location = 0) in vec2 inPos;
    // location = 0という部分がある。シェーダがアトリビュートのデータを受け取る際はこのようにデータ位置を指定する。
    // このlayout(location = xx)の指定とアトリビュートデスクリプションのlocationの位置は対応付けて書かなければならない。
    // locationの対応付けによって、シェーダで読み込む変数とVulkanが用意したアトリビュートの対応が付くのである。
    //
    // formatはデータ形式である。今回渡すのは32bitのfloat型が2つある2次元ベクトルなので、それを表すeR32G32Sfloatを指定する。
    // ここで使われているvk::Formatは本来ピクセルの色データのフォーマットを表すものなのでRとかGとか入っているが、それらはここでは無視してほしい。
    // ここでは 「32bit, 32bit, それぞれSigned(符号付)floatである」という意味を表すためだけにこれが指定されている。
    // ちなみにfloat型の3次元ベクトルを渡す際にはeR32G32B32Sfloatとか指定する。ここでもRGBの文字に深い意味はない。
    // 色のデータを渡すにしろ座標データを渡すにしろこういう名前の値を指定する。違和感があるかもしれないが、こういうものなので仕方ない。
    // offsetは頂点データのどの位置からデータを取り出すかを示す値。今回は1つしかアトリビュートが無いので0を指定しているが、複数のアトリビュートがある場合にはとても重要なものである。
    _vertexInputAttributeDescriptions.emplace_back();
    _vertexInputAttributeDescriptions.emplace_back();

    _vertexInputAttributeDescriptions[0].binding = 0;
    _vertexInputAttributeDescriptions[0].location = 0;
    _vertexInputAttributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
    _vertexInputAttributeDescriptions[0].offset = offsetof(Vertex, position);

    _vertexInputAttributeDescriptions[1].binding = 0;
    _vertexInputAttributeDescriptions[1].location = 1;
    _vertexInputAttributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
    _vertexInputAttributeDescriptions[1].offset = offsetof(Vertex, color);
}






void Renderer::createSubpassDescriptions()
{
    // vk::AttachmentReference構造体のattachmentメンバは「何番のアタッチメント」という形で
    // レンダーパスの中のアタッチメントを指定する
    // ここでは0を指定しているので0番のアタッチメントの意味
    _attachmentReference.emplace_back();
    _attachmentReference[0].attachment = 0;
    _attachmentReference[0].layout = vk::ImageLayout::eColorAttachmentOptimal;

    _subpassDescriptions.emplace_back();
    _subpassDescriptions[0].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    _subpassDescriptions[0].colorAttachmentCount = _attachmentReference.size();
    _subpassDescriptions[0].pColorAttachments = _attachmentReference.data();
}

void Renderer::createRenderPass()
{


    _attachmentDescriptions.emplace_back();
    _attachmentDescriptions.emplace_back();

    // flags: アタッチメントに関する追加のフラグを指定する。例えば、vk::AttachmentDescriptionFlagBits::eMayAlias は、異なるレンダーパスにおいて同じメモリ領域がエイリアスされる可能性があることを示す。
    _attachmentDescriptions[0].flags = vk::AttachmentDescriptionFlagBits::eMayAlias;

    // formatにはイメージのフォーマット情報を指定する必要がある
    // format: アタッチメントが使用するピクセルフォーマットを指定する。
    // これは、カラーバッファであれば vk::Format::eR8G8B8A8Unorm のようなカラーフォーマット、デプス/ステンシルバッファであれば vk::Format::eD32Sfloat のようなデプス/ステンシルフォーマットとなる。
    _attachmentDescriptions[0].format = _surfaceFormats[0].format;

    // samples: アタッチメントが使用するサンプリング数を指定する。
    // マルチサンプリングを行う場合は、vk::SampleCountFlagBits::e4 のように複数のサンプル数を指定する。単一サンプリングの場合は vk::SampleCountFlagBits::e1 を指定する。
    _attachmentDescriptions[0].samples = vk::SampleCountFlagBits::e1;

    // loadOp: レンダーパスの開始時にアタッチメントの内容をどのようにロードするかを指定する。
    // 選択肢としては、以前の内容を保持する vk::AttachmentLoadOp::eLoad、内容を破棄する vk::AttachmentLoadOp::eDiscard、内容を特定の値でクリアする vk::AttachmentLoadOp::eClear などが存在する。
    _attachmentDescriptions[0].loadOp = vk::AttachmentLoadOp::eClear;

    // storeOp: レンダーパスの終了時にアタッチメントの内容をどのように保存するかを指定する。選択肢としては、メモリに保存する
    // vk::AttachmentStoreOp::eStore、内容を破棄する vk::AttachmentStoreOp::eDiscard などが存在する。
    _attachmentDescriptions[0].storeOp = vk::AttachmentStoreOp::eStore;

    // stencilLoadOp: ステンシルバッファに対するレンダーパス開始時のロード操作を指定する。loadOp と同様の選択肢が存在する。
    _attachmentDescriptions[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;

    // stencilStoreOp: ステンシルバッファに対するレンダーパス終了時のストア操作を指定する。storeOp と同様の選択肢が存在する。
    _attachmentDescriptions[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;

    // initialLayout: レンダーパス開始前の、アタッチメントの初期レイアウトを指定する。レイアウトは、GPU がどのようにメモリにアクセスするかを示すものであり、適切なレイアウトを設定することで効率的なレンダリングが可能となる。
    // 例えば、カラーアタッチメントであれば vk::ImageLayout::eUndefined や vk::ImageLayout::eColorAttachmentOptimal などが考えられる。
    _attachmentDescriptions[0].initialLayout = vk::ImageLayout::eUndefined;

    // イメージレイアウト
    // イメージのメモリ上における配置方法・取扱い方に関する指定
    // 以前は特に説明せずvk::ImageLayout::eGeneralを指定していた
    // 実に色々な設定があり、必要に応じて最適なものを指定しなければならない
    // 公式ドキュメントなどの資料を参考に正しいものを探すしかない
    // 間違ったものを指定するとエラーが出る場合がある
    // レンダーパスの設定によって、レンダリング処理が終わった後でどのようなイメージレイアウトにするかを決めることができる
    // 今回はレンダリングが終わった後で表示(プレゼン)しなければならず、その場合はvk::ImageLayout::ePresentSrcKHRでなければならないという決まりなのでこれを指定
    // ちなみにeGeneral形式のイメージは(最適ではないものの)一応どんな扱いを受けても基本的にはOKらしく、eGeneralのままでも動くには動く でもやめておいた方が無難
    // finalLayout: レンダーパス終了後の、アタッチメントの最終レイアウトを指定する。
    // レンダーパスの後にどのようにアタッチメントを使用するかによって適切なレイアウトを選択する必要がある。
    // 例えば、描画結果をスワップチェーンに表示する場合は vk::ImageLayout::ePresentSrcKHR、次のレンダーパスで入力として使用する場合は vk::ImageLayout::eShaderReadOnlyOptimal などが考えられる。
    _attachmentDescriptions[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;

    // レンダーパスは描画処理の大まかな流れを表すオブジェクト
    // 今までは画像一枚を出力するだけだったが、深度バッファが関わる場合は少し設定を変える必要がある
    // 具体的には、深度バッファもアタッチメントの一種という扱いなのでその設定をする
    _attachmentDescriptions[1].format = vk::Format::eD32Sfloat;
    _attachmentDescriptions[1].samples = vk::SampleCountFlagBits::e1;
    _attachmentDescriptions[1].loadOp = vk::AttachmentLoadOp::eClear;
    // storeOpはeDontCareにする
    // 深度バッファの最終的な値はどうでもいいため
    _attachmentDescriptions[1].storeOp = vk::AttachmentStoreOp::eDontCare;
    _attachmentDescriptions[1].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    _attachmentDescriptions[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    _attachmentDescriptions[1].initialLayout = vk::ImageLayout::eUndefined;
    // finalLayoutはeDepthStencilAttachmentOptimalを指定
    // 深度バッファとして使うイメージはこのレイアウトになっていると良いとされている
    _attachmentDescriptions[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;




    vk::RenderPassCreateInfo renderPassCreateInfo = vk::RenderPassCreateInfo();
    renderPassCreateInfo.attachmentCount = _attachmentDescriptions.size();
    renderPassCreateInfo.pAttachments = _attachmentDescriptions.data();
    renderPassCreateInfo.subpassCount = _subpassDescriptions.size();
    renderPassCreateInfo.pSubpasses = _subpassDescriptions.data();
    renderPassCreateInfo.dependencyCount = 0;
    renderPassCreateInfo.pDependencies = nullptr;

    // レンダーパスを作成

    // これでレンダーパスが作成できたが、
    // レンダーパスはあくまで「この処理はこのデータを相手にする、あの処理はあのデータを～」
    // という関係性を表す”枠組み”に過ぎず、それぞれの処理(＝サブパス)が具体的にどのような処理を行うかは関知しない
    // 実際にはいろいろなコマンドを任意の回数呼ぶことができる
    _renderPass = _device.get().createRenderPassUnique(renderPassCreateInfo);
}





void Renderer::createPipeline()
{
    vk::PipelineLayoutCreateInfo layoutCreateInfo;
    layoutCreateInfo.setLayoutCount = _discriptorSetLayouts.size();
    std::shared_ptr<std::vector<vk::DescriptorSetLayout>> unwrapedDescSetLayouts = Vulkan_Test::unwrapHandles<vk::DescriptorSetLayout, vk::UniqueDescriptorSetLayout>(_discriptorSetLayouts);
    layoutCreateInfo.pSetLayouts = unwrapedDescSetLayouts.get()->data();
    layoutCreateInfo.pushConstantRangeCount = 0;

    _pipelineLayout = _device->createPipelineLayoutUnique(layoutCreateInfo);




    // 2種類の頂点入力デスクリプションを作成したら、それをパイプラインに設定する
    // 頂点入力デスクリプションはvk::PipelineVertexInputStateCreateInfo構造体に設定する
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    vertexInputInfo.vertexBindingDescriptionCount = _vertexInputBindingDescriptions.size();
    vertexInputInfo.pVertexBindingDescriptions = _vertexInputBindingDescriptions.data();
    vertexInputInfo.vertexAttributeDescriptionCount = _vertexInputAttributeDescriptions.size();
    vertexInputInfo.pVertexAttributeDescriptions = _vertexInputAttributeDescriptions.data();

    // 深度バッファを有効化するための設定を入れる構造体
    vk::PipelineDepthStencilStateCreateInfo depthstencil;
    depthstencil.depthTestEnable = false;
    depthstencil.stencilTestEnable = false;

    // パイプラインとは、3DCGの基本的な描画処理をひとつながりにまとめたもの
    // パイプラインは「点の集まりで出来た図形を色のついたピクセルの集合に変換するもの」
    // ほぼ全ての3DCGは三角形の集まりであり、私たちが最初に持っているものは三角形の各点の色や座標であるが、
    // 最終的に欲しいものは画面のどのピクセルがどんな色なのかという情報である
    // この間を繋ぐ演算処理は大体お決まりのパターンになっており、まとめてグラフィックスパイプラインとなっている

    // この処理は全ての部分が固定されているものではなく、プログラマ側で色々指定する部分があり、
    // それらの情報をまとめたものがパイプラインオブジェクト(vk::Pipeline)である
    // 実際に使用して描画処理を行う際はコマンドでパイプラインをバインドし、ドローコールを呼ぶ

    // Vulkanにおけるパイプラインには「グラフィックスパイプライン」と「コンピュートパイプライン」の2種類がある
    // コンピュートパイプラインはGPGPUなどに使うもの
    // 今回は普通に描画が目的なのでグラフィックスパイプラインを作成する
    // グラフィックスパイプラインはvk::DeviceのcreateGraphicsPipelineメソッドで作成できる

    vk::Viewport viewports[1];
    viewports[0].x = 0.0;
    viewports[0].y = 0.0;
    viewports[0].minDepth = 0.0;
    viewports[0].maxDepth = 1.0;
    viewports[0].width = _surfaceCapabilities.currentExtent.width;
    viewports[0].height = _surfaceCapabilities.currentExtent.height;

    vk::Rect2D scissors[1];
    scissors[0].offset = vk::Offset2D(0, 0);
    scissors[0].extent = _surfaceCapabilities.currentExtent;

    vk::PipelineViewportStateCreateInfo viewportState;
    viewportState.viewportCount = 1;
    viewportState.pViewports = viewports;
    viewportState.scissorCount = 1;
    viewportState.pScissors = scissors;

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
    inputAssembly.primitiveRestartEnable = false;

    vk::PipelineRasterizationStateCreateInfo rasterizer;
    rasterizer.depthClampEnable = false;
    rasterizer.rasterizerDiscardEnable = false;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eClockwise;
    rasterizer.depthBiasEnable = false;

    vk::PipelineMultisampleStateCreateInfo multisample;
    multisample.sampleShadingEnable = false;
    multisample.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineColorBlendAttachmentState blendattachment[1];
    blendattachment[0].colorWriteMask =
            vk::ColorComponentFlagBits::eA |
            vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB;
    blendattachment[0].blendEnable = false;

    vk::PipelineColorBlendStateCreateInfo blend;
    blend.logicOpEnable = false;
    blend.attachmentCount = 1;
    blend.pAttachments = blendattachment;

    vk::UniqueShaderModule vertShader;
    vk::UniqueShaderModule fragShader;
    {
        AAssetManager* assetManager = _pApp->activity->assetManager;

        // 頂点シェーダーを読み込む
        std::vector<char> vertSpvFileData;
        AAsset* vertSpvFile = AAssetManager_open(assetManager, "shader.vert.spv", AASSET_MODE_BUFFER);
        size_t vertSpvFileSz = AAsset_getLength(vertSpvFile);
        vertSpvFileData.resize(vertSpvFileSz);
        AAsset_read(vertSpvFile, vertSpvFileData.data(), vertSpvFileSz);
        AAsset_close(vertSpvFile);

        vk::ShaderModuleCreateInfo vertShaderCreateInfo;
        vertShaderCreateInfo.codeSize = vertSpvFileSz;
        vertShaderCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vertSpvFileData.data());
        vertShader = _device.get().createShaderModuleUnique(vertShaderCreateInfo);

        // フラグメントシェーダーを読み込む
        std::vector<char> fragSpvFileData;
        AAsset* fragSpvFile = AAssetManager_open(assetManager, "shader.frag.spv", AASSET_MODE_BUFFER);
        size_t fragSpvFileSz = AAsset_getLength(fragSpvFile);
        fragSpvFileData.resize(fragSpvFileSz);
        AAsset_read(fragSpvFile, fragSpvFileData.data(), fragSpvFileSz);
        AAsset_close(fragSpvFile);

        vk::ShaderModuleCreateInfo fragShaderCreateInfo;
        fragShaderCreateInfo.codeSize = fragSpvFileSz;
        fragShaderCreateInfo.pCode = reinterpret_cast<const uint32_t*>(fragSpvFileData.data());
        fragShader = _device.get().createShaderModuleUnique(fragShaderCreateInfo);
    }

    vk::PipelineShaderStageCreateInfo shaderStage[2];
    shaderStage[0].stage = vk::ShaderStageFlagBits::eVertex;
    shaderStage[0].module = vertShader.get();
    shaderStage[0].pName = "main";
    shaderStage[1].stage = vk::ShaderStageFlagBits::eFragment;
    shaderStage[1].module = fragShader.get();
    shaderStage[1].pName = "main";

    vk::GraphicsPipelineCreateInfo pipelineCreateInfo;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    pipelineCreateInfo.pRasterizationState = &rasterizer;
    pipelineCreateInfo.pMultisampleState = &multisample;
    pipelineCreateInfo.pColorBlendState = &blend;
    pipelineCreateInfo.pDepthStencilState = &depthstencil;
    pipelineCreateInfo.layout = _pipelineLayout.get();
    pipelineCreateInfo.renderPass = _renderPass.get();
    pipelineCreateInfo.subpass = 0;
    pipelineCreateInfo.stageCount = std::size(shaderStage);
    pipelineCreateInfo.pStages = shaderStage;

    _pipeline = _device.get().createGraphicsPipelineUnique(nullptr, pipelineCreateInfo).value;
}



void Renderer::createCommandBuffer()
{
    // コマンドバッファを作るには、その前段階として「コマンドプール」というまた別のオブジェクトを作る必要がある
    // コマンドバッファをコマンドの記録に使うオブジェクトとすれば、コマンドプールというのはコマンドを記録するためのメモリ実体
    // コマンドバッファを作る際には必ず必要
    // コマンドプールは論理デバイス(vk::Device)の createCommandPool メソッド、コマンドバッファは論理デバイス(vk::Device)の allocateCommandBuffersメソッドで作成することができる
    // コマンドプールの作成が「create」なのに対し、コマンドバッファの作成は「allocate」であるあたりから
    // 「コマンドバッファの記録能力は既に用意したコマンドプールから割り当てる」という気持ちが読み取れる

    // コマンドバッファとは、コマンドをため込んでおくバッファ
    // VulkanでGPUに仕事をさせる際は「コマンドバッファの中にコマンドを記録し、そのコマンドバッファをキューに送る」必要がある
    vk::CommandPoolCreateInfo cmdPoolCreateInfo;

    // CommandPoolCreateInfoのqueueFamilyIndexには、後でそのコマンドバッファを送信するときに対象とするキューを指定する
    // 結局送信するときにも「このコマンドバッファをこのキューに送信する」というのは指定するが、こんな二度手間が盛り沢山なのがVulkanである
    cmdPoolCreateInfo.queueFamilyIndex = _queueFamilyIndex;

    // もしも固定の内容のコマンドバッファを使ってしまうと、毎回同じフレームバッファが使われることになる
    // 毎回同じフレームバッファということは、毎回同じイメージに向けて描画することになってしまう
    // これではスワップチェーンによる表示処理がうまくいかない
    // そこで毎フレームコマンドバッファをリセットして、コマンドを記録し直す こうすることで毎回別のイメージに向けて描画できる
    // コマンドプール作成時 vk::CommandPoolCreateInfo::flags に vk::CommandPoolCreateFlagBits::eResetCommandBuffer を指定すると、
    // そのコマンドプールから作成したコマンドバッファはリセット可能になる
    cmdPoolCreateInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    _commandPool = _device.get().createCommandPoolUnique(cmdPoolCreateInfo);

    // コマンドバッファの作成
    // コマンドバッファを毎フレーム記録しなおせるようにするのは、描画対象のイメージを切り替えるという以外にも意味がある。
    // 今は単なる三角形の表示ですが、動くアニメーションになれば毎フレーム描画内容が変わる
    // そこでフレーム毎にコマンド(描画命令)を変えるのは自然なこと

    // 作るコマンドバッファの数はCommandBufferAllocateInfoの commandBufferCount で指定する
    // commandPoolにはコマンドバッファの作成に使うコマンドプールを指定する
    // このコードではUniqueCommandPoolを使っているので.get()を呼び出して生のCommandPoolを取得している
    vk::CommandBufferAllocateInfo cmdBufferAllocateInfo;
    cmdBufferAllocateInfo.commandPool = _commandPool.get();
    cmdBufferAllocateInfo.commandBufferCount = 1;
    cmdBufferAllocateInfo.level = vk::CommandBufferLevel::ePrimary;

    // allocateCommandBufferではなくallocateCommandBuffersである名前から分かる通り、一度にいくつも作れる仕様になっている
    _commandBuffers = _device.get().allocateCommandBuffersUnique(cmdBufferAllocateInfo);
}

////////////////// public functions //////////////////

void Renderer::render() {

    _device->resetFences({ _swapchainImgFence.get() });

    vk::ResultValue acquireImgResult = _device->acquireNextImageKHR(_swapchain.get(), 1'000'000'000, {}, _swapchainImgFence.get());

    // 再作成処理
    if(acquireImgResult.result == vk::Result::eSuboptimalKHR || acquireImgResult.result == vk::Result::eErrorOutOfDateKHR)
    {
        LOGERR("Recreate swapchain : " << to_string(acquireImgResult.result));
        return;
    }
    if (acquireImgResult.result != vk::Result::eSuccess)
    {
        LOGERR("Failed to get next frame");
        exit(EXIT_FAILURE);
    }



    uint32_t imgIndex = acquireImgResult.value;

    _commandBuffers[0]->reset();

    vk::CommandBufferBeginInfo cmdBeginInfo;
    _commandBuffers[0]->begin(cmdBeginInfo);

    vk::ClearValue clearVal[2];
    clearVal[0].color.float32[0] = 0.3f;
    clearVal[0].color.float32[1] = 0.3f;
    clearVal[0].color.float32[2] = 0.3f;
    clearVal[0].color.float32[3] = 1.0f;

    // 深度バッファの値は最初は1.0fにクリアされている必要がある
    // 手前かどうかを判定するためのものなので、初期値は何よりも遠くになっていなければならない
    // クリッピングにより1.0より遠くは描画されないので、1.0より大きい値でクリアする必要はない
    //clearVal[1].depthStencil.depth = 1.0f;

    vk::RenderPassBeginInfo renderpassBeginInfo;
    renderpassBeginInfo.renderPass = _renderPass.get();
    renderpassBeginInfo.framebuffer = _framebuffer[imgIndex].get();
    renderpassBeginInfo.renderArea = vk::Rect2D({ 0,0 }, _surfaceCapabilities.currentExtent);
    renderpassBeginInfo.clearValueCount = 1;
    renderpassBeginInfo.pClearValues = clearVal;

    _commandBuffers[0]->beginRenderPass(renderpassBeginInfo, vk::SubpassContents::eInline);

    _commandBuffers[0]->bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline.get());
//    _commandBuffers[0]->bindVertexBuffers(0, { _vertexBuffer.get() }, { 0 });
//    //_commandBuffers[0]->bindIndexBuffer(indexBuf->get(), 0, vk::IndexType::eUint16);
//    _commandBuffers[0]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _descpriptorPipelineLayout->get(), 0, { (*descSets)[0].get() }, {});
//
//    _commandBuffers[0]->pushConstants(descpriptorPipelineLayout->get(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(ObjectData), &objectData);
//    _commandBuffers[0]->drawIndexed(indices.size(), 1, 0, 0, 0);


    _commandBuffers[0]->draw(3, 1, 0, 0);

    _commandBuffers[0]->endRenderPass();

    _commandBuffers[0]->end();

    vk::CommandBuffer submitCmdBuf[1] = { _commandBuffers[0].get() };
    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = submitCmdBuf;
    _graphicsQueue.submit({ submitInfo }, nullptr);

    _graphicsQueue.waitIdle();

    vk::PresentInfoKHR presentInfo;

    auto presentSwapchains = { _swapchain.get() };
    auto imgIndices = { imgIndex };

    presentInfo.swapchainCount = presentSwapchains.size();
    presentInfo.pSwapchains = presentSwapchains.begin();
    presentInfo.pImageIndices = imgIndices.begin();

    _graphicsQueue.presentKHR(presentInfo);

    _graphicsQueue.waitIdle();
}

void Renderer::handleInput() 
{

}