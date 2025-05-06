#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>

using namespace std;

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const vector<const char*> requiredLayers =
{
    "VK_LAYER_KHRONOS_validation" // VK_LAYER_KHRONOS_validationで暗黙的にすべての検証レイヤーを有効化
};

#ifdef NDEBUG // リリース版ではパフォーマンス向上のため検証レイヤーを無効化
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

VkDebugUtilsMessengerEXT callback;

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