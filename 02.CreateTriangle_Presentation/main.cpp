#define NOMINMAX   // limits.hのmax()定義がminwindef.hに上書きされるのを防ぐ

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>


#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <map>
#include <set>
#include <limits>
#include <algorithm>

using namespace std;

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const vector<const char*> requiredLayers =
{
    "VK_LAYER_KHRONOS_validation" // VK_LAYER_KHRONOS_validationで暗黙的にすべての検証レイヤーを有効化
};

const vector<const char*> requiredDeviceExtension =
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

struct SwapChainSupportDetails 
{
    VkSurfaceCapabilitiesKHR capabilities;
    vector<VkSurfaceFormatKHR> formats;
    vector<VkPresentModeKHR> presentModes;
};

#ifdef NDEBUG // リリース版ではパフォーマンス向上のため検証レイヤーを無効化
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

VkDebugUtilsMessengerEXT callback;

struct QueueFamilyIndices
{
    // グラフィックスコマンド用のキュー族
    int graphicsFamily = -1;

    // 表示用のコマンドキュー族
    int presentFamily = -1;

    // 必要なキュー族が全てサポートされているか
    bool isComplete()
    {
        return graphicsFamily >= 0 && presentFamily >= 0;
    }
};

class HelloTriangleApplication
{
public:
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT callback;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE; // physicalDevice は instance に紐づくため、instance 解放時に自動解放される（明示的解放不要）
    VkDevice device = VK_NULL_HANDLE; // 論理デバイスの作成時にcreateinfoを使用するため、明示的に解放する必要があります
    VkQueue graphicsQueue; // コマンドキューは論理デバイスの作成時に生成され、deviceの解放時に自動的に解放されます（明示的な解放不要）
    VkQueue presentQueue;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapChain;
    vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    vector<VkImageView> swapChainImageViews;

    void initWindow()
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // GLFWが自動的にOpenGLコンテキストを作成しないように設定する
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void initVulkan()
    {
        createInstance();
        setDebugCallback();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
    }

    void setDebugCallback()
    {
        if (!enableValidationLayers)
            return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
        createInfo.pUserData = nullptr; // オプションパラメータ

        if (VK_SUCCESS != CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &callback))
        {
            throw runtime_error("failed to set debug callback!");
        }
    }

    void mainLoop()
    {
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
        }
    }

    void cleanup()
    {
        // リソースの破棄と作成の順序は正確に逆にする必要がある
        for (auto imageView : swapChainImageViews) 
        {
            vkDestroyImageView(device, imageView, nullptr);
        }

        vkDestroySwapchainKHR(device, swapChain, nullptr);

        vkDestroyDevice(device, nullptr);

        vkDestroySurfaceKHR(instance, surface, nullptr);

        if (enableValidationLayers)
            DestroyDebugUtilsMessengerEXT(instance, callback, nullptr);

        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);

        glfwTerminate();
    }

    void createInstance()
    {
        // VkApplicationInfo は必須ではないが、ドライバの最適化に利用される可能性がある
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        // VkInstanceCreateInfo は必須入力項目
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        // グローバル拡張情報
        vector<const char*> glfwExtension = getExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(glfwExtension.size());
        createInfo.ppEnabledExtensionNames = glfwExtension.data(); // const char** → const char* const* への暗黙的型変換
        // 検証レイヤー情報
        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(requiredLayers.size());
            createInfo.ppEnabledLayerNames = requiredLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        // 検証レイヤーを有効にする場合、必要な検証レイヤーがサポートされているか確認
        if (enableValidationLayers && !checkValidationLayerSupport())
        {
            throw runtime_error("required validation layers not supported!");
        }

        // インスタンスの作成
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        {
            throw runtime_error("failed to create instance!");
        }
    }

    bool checkValidationLayerSupport()
    {
        // 現在のランタイムがサポートする検証レイヤーを取得
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        cout << "Available Validation Layers:" << endl;
        for (const auto& layer : availableLayers)
        {
            cout << "\t" << layer.layerName << endl;
        }
        cout << "Required Validation Layers:" << endl;
        for (const auto& layer : requiredLayers)
        {
            cout << "\t" << layer << endl;
        }

        //  必要な検証レイヤーのサポート状態をチェック
        for (const auto& requiredLayer : requiredLayers)
        {
            bool isSupported = false;

            for (const auto& availableLayer : availableLayers)
            {
                if (strcmp(requiredLayer, availableLayer.layerName) == 0)
                {
                    isSupported = true;
                    break;
                }
            }

            if (!isSupported)
                return false;
        }

        return true;

    }

    vector<const char*> getExtensions()
    {
        // 必要な拡張機能を取得
        // GLFWは必須
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtension = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        vector<const char*> extensions(glfwExtension, glfwExtension + glfwExtensionCount);

        // 検証レイヤーが有効な場合、デバッグ情報取得のためVK_EXT_debug_utils拡張を有効化
        // 検証レイヤーの利用可能性がVK_EXT_debug_utilsのサポートを暗黙的に保証する為、明示的なチェックは不要
        if (enableValidationLayers)
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        // 現在のruntimeでサポートされている拡張
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

        cout << "Available Extension:" << endl;
        for (const auto& extension : availableExtensions) // auto& の使用は auto より効率的 (コピーコスト削減)
        {
            cout << "\t" << extension.extensionName << endl;
        }

        cout << "Required Extension:" << endl;
        for (int i = 0; i < extensions.size(); i++)
        {
            cout << "\t" << extensions.data()[i] << endl;
        }

        // 拡張機能のサポート状態を確認
        for (int i = 0; i < extensions.size(); i++)
        {
            bool isSupported = false;

            for (const auto& extension : availableExtensions)
            {
                if (strcmp(extensions.data()[i], extension.extensionName) == 0)
                {
                    isSupported = true;
                    break;
                }
            }

            if (!isSupported)
                throw runtime_error("required extensions not supported!");
        }
        return extensions;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) 
    {
        cerr << "validation layer: " << pCallbackData->pMessage << endl;
        return VK_FALSE;
    }

    // vkCreateDebugUtilsMessengerEXTは拡張関数のため、vkGetInstanceProcAddrで動的にロードする必要がある
    VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance, 
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, 
        const VkAllocationCallbacks* pAllocator, 
        VkDebugUtilsMessengerEXT* pCallback)
    {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) 
        {
            return func(instance, pCreateInfo, pAllocator, pCallback);
        }
        else 
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    void DestroyDebugUtilsMessengerEXT(
        VkInstance instance, 
        VkDebugUtilsMessengerEXT pCallback,
        const VkAllocationCallbacks* pAllocator) 
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) 
        {
            func(instance, pCallback, pAllocator);
        }
    }

    void pickPhysicalDevice()
    {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance, &count, nullptr);
        if (count == 0)
            throw runtime_error("failed to get physical device with Vulkan support!");
        vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance, &count, devices.data());


        multimap<int, const VkPhysicalDevice&> candidates;

        for (const auto& device : devices)
        {
            candidates.insert(make_pair(ratePhyicalDevice(device), device));
        }

        // multimap はデフォルトでキーの昇順ソート（最終要素のキー値が最大）
        // ※ end() は使用不可
        if (candidates.rbegin()->first > 0)
        {
            physicalDevice = candidates.rbegin()->second;
        }
        else
        {
            throw runtime_error("failed to get suitable physical device!");
        }
    }

    int ratePhyicalDevice(const VkPhysicalDevice& device)
    {
        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceProperties(device, &properties);
        vkGetPhysicalDeviceFeatures(device, &features);

        multimap<int, const VkPhysicalDevice&> candidates;
        int score = 0;

        // ディスクリートGPU 
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            score += 1000;

        // 2Dテクスチャ品質
        score += properties.limits.maxImageDimension2D;

        // ジオメトリシェーダ　必須
        if (features.geometryShader == false)
            return 0;

        // 必要なコマンドキュー族 必須
        auto queueFamily = findQueueFamilyIndices(device);
        if (queueFamily.isComplete() == false)
            return 0;

        // 設備拡張　必須
        if (!checkDeviceExtensionSupport(device))
            return 0;

        // スワップチェーンのサポート（formatsとpresentModes）は空であってはならない
        auto swapChainSupport = getSupportedSwapChain(device);

        if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty())
        {
            return 0;
        }

    }

    QueueFamilyIndices findQueueFamilyIndices(const VkPhysicalDevice& device)
    {
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
        vector<VkQueueFamilyProperties> properties(count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties.data());

        //  必要なキュー族の検出 
        QueueFamilyIndices indices;
        int i = 0;
        for (const auto& property : properties)
        {
            if (property.queueCount > 0 && property.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                indices.graphicsFamily = i;

            VkBool32 presentSupported = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupported);
            if (property.queueCount > 0 && presentSupported)
                indices.presentFamily = i;

            if (indices.isComplete())
                break;

            i++;
        }

        return indices;
    }

    void createLogicalDevice()
    {
        // キュー族情報
        QueueFamilyIndices indices = findQueueFamilyIndices(physicalDevice);

        vector<VkDeviceQueueCreateInfo> queueInfos = {};

        // 赤黒木セットを使用すると重複要素を自動的に除外できます（例：graphicsQueueとprensentQueueが同一キュー族の場合、1つのキューのみ走査すれば良い）
        set<int> uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily };

        for (auto index : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueInfo = {};
            queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueInfo.queueFamilyIndex = index;
            queueInfo.queueCount = 1;  // キューの数。一般的に1つのキュー族で1つ以上のキューを使用することはない
            float queuePrirorty = 1.0f; // 単一のキューでも優先度の明示的指定が必須
            queueInfo.pQueuePriorities = &queuePrirorty;
            queueInfos.push_back(queueInfo);
        }

        // デバイス機能
        VkPhysicalDeviceFeatures deviceFeature = {}; // 後回しにする

        // 論理デバイスの作成
        VkDeviceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
        createInfo.pQueueCreateInfos = queueInfos.data();
        createInfo.pEnabledFeatures = &deviceFeature;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size());
        createInfo.ppEnabledExtensionNames = requiredDeviceExtension.data();

        // インスタンスに設定した検証レイヤーは、そのまま論理デバイスにも適用可能
        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(requiredLayers.size());
            createInfo.ppEnabledLayerNames = requiredLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        if (VK_SUCCESS != vkCreateDevice(physicalDevice, &createInfo, nullptr, &device))
        {
            throw runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);
    }

    void createSurface()
    {
        VkWin32SurfaceCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.hwnd = glfwGetWin32Window(window);
        createInfo.hinstance = GetModuleHandle(nullptr);  //注：ここのインスタンスはVkインスタンスではなく、現在のプロセスのインスタンスハンドルです

        auto CreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR");

        if (CreateWin32SurfaceKHR != nullptr && 
            CreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS)
        {
            throw runtime_error("failed to create window surface!");
        }
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice device) 
    {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        set<string> requiredExtensions(requiredDeviceExtension.begin(), requiredDeviceExtension.end());
        for (const auto& extension : availableExtensions) 
        {
            requiredExtensions.erase(extension.extensionName);
        }

        // 空でない場合、サポートされていないデバイス拡張が存在する
        return requiredExtensions.empty();

        return true;
    }

    SwapChainSupportDetails getSupportedSwapChain(VkPhysicalDevice device) 
    {
        SwapChainSupportDetails details;

        // capabilities
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        // format
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

        if (formatCount != 0) 
        {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        // presentMode
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

        if (presentModeCount != 0) 
        {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }
    
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) 
    {
        // 現在のExtentが最大値でない場合、現在の数値を使用
        if (capabilities.currentExtent.width != numeric_limits<uint32_t>::max()) 
        {
            return capabilities.currentExtent;
        }
        // 現在のExtentが最大値の場合、独自に設定可能
        else 
        {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            VkExtent2D actualExtent = 
            {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const vector<VkSurfaceFormatKHR>& availableFormats) 
    {
        for (const auto& availableFormat : availableFormats) 
        {
            //  SRGB非線形空間を使用するのはガンマ補正のため
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) 
            {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    // PresentModeは垂直同期の仕組みを提供し、画面ティアリングを改善
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) 
    {
        for (const auto& availablePresentMode : availablePresentModes) 
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) 
            {
                return VK_PRESENT_MODE_MAILBOX_KHR;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    void createSwapChain() 
    {
        SwapChainSupportDetails swapChainSupport = getSupportedSwapChain(physicalDevice);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        // バッファを1つ追加し、レンダリングブロッキングを低減
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) 
        {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1; // VR関連、ここでは1に設定
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // カラーアタッチメントとして使用

        QueueFamilyIndices indices = findQueueFamilyIndices(physicalDevice);
        uint32_t queueFamilyIndices[] = { indices.graphicsFamily, indices.presentFamily };

        if (indices.graphicsFamily != indices.presentFamily) 
        {
            // 複数のコマンドファミリーを並行処理する
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else 
        {
            // 単一のコマンド族処理で最高のパフォーマンス
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;  // オプション
            createInfo.pQueueFamilyIndices = nullptr; // オプション
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform; // 現在のTransformを維持
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // ウィンドウ間のブレンド
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE; // Ⅴulkanには、他のウィンドウに隠れたピクセルを最適化可能
        createInfo.oldSwapchain = VK_NULL_HANDLE; // スワップチェーンのリビルド用、暫定VK_NULL_HANDLE


        // スワップチェーン作成
        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) 
        {
            throw runtime_error("failed to create swap chain!");
        }

        // スワップチェーンイメージ取得
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }

    void createImageViews()
    {
        swapChainImageViews.resize(swapChainImages.size());

        for (size_t i = 0; i < swapChainImages.size(); i++) 
        {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapChainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapChainImageFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY; // チャネルはそのままにする
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; 
            createInfo.subresourceRange.baseMipLevel = 0; // mipmapなし
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0; // VR関連、ここでは0に設定
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) 
            {
                throw runtime_error("failed to create image views!");
            }

        }
    }

};


int main() 
{
    HelloTriangleApplication app;

    try 
    {
        app.run();
    }
    catch (const std::exception& e) 
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}