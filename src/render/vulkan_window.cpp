#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include "threepoint/preview.hpp"
#include "cube_demo.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <nvrhi/vulkan.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace threepoint {
namespace {

void check(VkResult result, const char* action) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string(action) + " failed (" +
                                 nvrhi::vulkan::resultToString(result) + ")");
    }
}

void glfw_error(int code, const char* message) {
    std::cerr << "GLFW " << code << ": " << message << '\n';
}

class Messages final : public nvrhi::IMessageCallback {
public:
    void message(nvrhi::MessageSeverity severity, const char* message_text) override {
        if (severity == nvrhi::MessageSeverity::Error) {
            error_ = true;
        }
        std::cerr << "nvrhi: " << message_text << '\n';
    }

    bool has_error() const { return error_; }

private:
    bool error_ = false;
};

nvrhi::Format texture_format(VkFormat format) {
    switch (format) {
    case VK_FORMAT_B8G8R8A8_UNORM: return nvrhi::Format::BGRA8_UNORM;
    case VK_FORMAT_R8G8B8A8_UNORM: return nvrhi::Format::RGBA8_UNORM;
    case VK_FORMAT_B8G8R8A8_SRGB: return nvrhi::Format::SBGRA8_UNORM;
    case VK_FORMAT_R8G8B8A8_SRGB: return nvrhi::Format::SRGBA8_UNORM;
    default: return nvrhi::Format::UNKNOWN;
    }
}

std::string key_name(int key) {
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) return std::string(1, char('a' + key - GLFW_KEY_A));
    if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) return std::string(1, char('0' + key - GLFW_KEY_0));
    switch (key) {
    case GLFW_KEY_LEFT: return "left";
    case GLFW_KEY_RIGHT: return "right";
    case GLFW_KEY_UP: return "up";
    case GLFW_KEY_DOWN: return "down";
    case GLFW_KEY_SPACE: return "space";
    case GLFW_KEY_ENTER: return "enter";
    case GLFW_KEY_ESCAPE: return "escape";
    case GLFW_KEY_TAB: return "tab";
    case GLFW_KEY_BACKSPACE: return "backspace";
    case GLFW_KEY_LEFT_SHIFT: return "left_shift";
    case GLFW_KEY_RIGHT_SHIFT: return "right_shift";
    case GLFW_KEY_LEFT_CONTROL: return "left_control";
    case GLFW_KEY_RIGHT_CONTROL: return "right_control";
    default: return {};
    }
}

const char* mouse_button_name(int button) {
    switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT: return "left";
    case GLFW_MOUSE_BUTTON_RIGHT: return "right";
    case GLFW_MOUSE_BUTTON_MIDDLE: return "middle";
    default: return nullptr;
    }
}

class VulkanWindow {
public:
    explicit VulkanWindow(const PreviewOptions& options) : options_(options) {}

    ~VulkanWindow() {
        if (device_ != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device_);
        }
        cube_.reset();
        swapchain_textures_.clear();
        staging_ = nullptr;
        command_list_ = nullptr;
        nvrhi_device_ = nullptr;
        if (image_available_ != VK_NULL_HANDLE) vkDestroySemaphore(device_, image_available_, nullptr);
        if (render_complete_ != VK_NULL_HANDLE) vkDestroySemaphore(device_, render_complete_, nullptr);
        if (swapchain_ != VK_NULL_HANDLE) vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        if (device_ != VK_NULL_HANDLE) vkDestroyDevice(device_, nullptr);
        if (surface_ != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance_, surface_, nullptr);
        if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);
        if (window_) glfwDestroyWindow(window_);
        if (glfw_started_) glfwTerminate();
    }

    PreviewResult run() {
        initialize();
        PreviewResult result;
        bool requested_resize = false;
        double previous_time = glfwGetTime();
        while (!glfwWindowShouldClose(window_) &&
               (options_.frame_limit == 0 || result.frames < options_.frame_limit)) {
            glfwPollEvents();
            if (pending_input_error_) std::rethrow_exception(pending_input_error_);
            poll_gamepad();
            if (glfwWindowShouldClose(window_)) break;
            const double current_time = glfwGetTime();
            if (options_.on_frame) {
                const double dt = options_.fixed_delta > 0.0
                    ? options_.fixed_delta : std::clamp(current_time - previous_time, 0.0, 0.25);
                scene_frame_ = options_.on_frame(dt);
                if (scene_frame_.quit) break;
            }
            previous_time = current_time;
            if (resized_) {
                recreate_swapchain();
                ++result.swapchain_rebuilds;
            }
            if (glfwWindowShouldClose(window_)) break;
            if (!draw_frame(result.frames + 1)) continue;
            ++result.frames;
            if (options_.resize_after != 0 && !requested_resize &&
                result.frames == options_.resize_after) {
                glfwSetWindowSize(window_, int(options_.width + 96), int(options_.height + 64));
                requested_resize = true;
            }
        }
        check(vkDeviceWaitIdle(device_), "wait for device shutdown");
        if (messages_.has_error()) {
            throw std::runtime_error("nvrhi reported a rendering error");
        }
        return result;
    }

private:
    void initialize() {
        glfwSetErrorCallback(glfw_error);
#ifdef GLFW_WAYLAND_LIBDECOR
        // Avoid libdecor's unstable rapid-resize shutdown path on the tested Wayland setup.
        glfwInitHint(GLFW_WAYLAND_LIBDECOR, GLFW_WAYLAND_DISABLE_LIBDECOR);
#endif
        if (!glfwInit()) {
            throw std::runtime_error("GLFW could not initialize a display");
        }
        glfw_started_ = true;
        if (!glfwVulkanSupported()) {
            throw std::runtime_error("Vulkan loader or driver is unavailable");
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window_ = glfwCreateWindow(int(options_.width), int(options_.height),
                                   options_.scene == PreviewOptions::Scene::Cube
                                       ? "ThreePoint 3D cube" : options_.scene == PreviewOptions::Scene::Project
                                           ? "ThreePoint game" : "ThreePoint Vulkan preview",
                                   nullptr, nullptr);
        if (!window_) {
            throw std::runtime_error("GLFW could not create the Vulkan window");
        }
        glfwSetWindowUserPointer(window_, this);
        glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* window, int, int) {
            static_cast<VulkanWindow*>(glfwGetWindowUserPointer(window))->resized_ = true;
        });
        glfwSetKeyCallback(window_, [](GLFWwindow* window, int key, int, int action, int) {
            auto* self = static_cast<VulkanWindow*>(glfwGetWindowUserPointer(window));
            if (!self->options_.on_input) return;
            try {
                const std::string name = key_name(key);
                if (!name.empty()) self->options_.on_input({InputDevice::Keyboard, name,
                    action == GLFW_RELEASE ? 0.0f : 1.0f});
            } catch (...) { self->pending_input_error_ = std::current_exception(); }
        });
        glfwSetMouseButtonCallback(window_, [](GLFWwindow* window, int button, int action, int) {
            auto* self = static_cast<VulkanWindow*>(glfwGetWindowUserPointer(window));
            if (!self->options_.on_input) return;
            try {
                const char* name = mouse_button_name(button);
                if (name) self->options_.on_input({InputDevice::MouseButton, name,
                    action == GLFW_RELEASE ? 0.0f : 1.0f});
            } catch (...) { self->pending_input_error_ = std::current_exception(); }
        });
        glfwSetCursorPosCallback(window_, [](GLFWwindow* window, double x, double y) {
            auto* self = static_cast<VulkanWindow*>(glfwGetWindowUserPointer(window));
            if (!self->options_.on_pointer) return;
            try { self->options_.on_pointer(x, y); }
            catch (...) { self->pending_input_error_ = std::current_exception(); }
        });
        if (options_.on_pointer) {
            double x = 0.0, y = 0.0;
            glfwGetCursorPos(window_, &x, &y);
            options_.on_pointer(x, y);
        }

        uint32_t extension_count = 0;
        const char** extensions = glfwGetRequiredInstanceExtensions(&extension_count);
        if (!extensions || extension_count == 0) {
            throw std::runtime_error("Vulkan driver or window-surface extensions are unavailable");
        }
        instance_extensions_.assign(extensions, extensions + extension_count);
        VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        application.pApplicationName = "ThreePoint";
        application.apiVersion = VK_API_VERSION_1_3;
        VkInstanceCreateInfo instance_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        instance_info.pApplicationInfo = &application;
        instance_info.enabledExtensionCount = extension_count;
        instance_info.ppEnabledExtensionNames = extensions;
        check(vkCreateInstance(&instance_info, nullptr, &instance_), "create Vulkan instance");
        check(glfwCreateWindowSurface(instance_, window_, nullptr, &surface_),
              "create Vulkan window surface");
        select_gpu();
        create_device();

        vk::detail::DynamicLoader loader;
        const auto get_instance_proc = loader.getProcAddress<PFN_vkGetInstanceProcAddr>(
            "vkGetInstanceProcAddr");
        if (!get_instance_proc) {
            throw std::runtime_error("Vulkan loader has no vkGetInstanceProcAddr");
        }
        VULKAN_HPP_DEFAULT_DISPATCHER.init(instance_, get_instance_proc, device_);

        nvrhi::vulkan::DeviceDesc nvrhi_desc{};
        nvrhi_desc.errorCB = &messages_;
        nvrhi_desc.instance = instance_;
        nvrhi_desc.physicalDevice = physical_device_;
        nvrhi_desc.device = device_;
        nvrhi_desc.graphicsQueue = queue_;
        nvrhi_desc.graphicsQueueIndex = int(queue_family_);
        nvrhi_desc.instanceExtensions = instance_extensions_.data();
        nvrhi_desc.numInstanceExtensions = instance_extensions_.size();
        nvrhi_desc.deviceExtensions = device_extensions_;
        nvrhi_desc.numDeviceExtensions = 1;
        nvrhi_device_ = nvrhi::vulkan::createDevice(nvrhi_desc);
        if (!nvrhi_device_) {
            throw std::runtime_error("nvrhi could not create its Vulkan device");
        }
        command_list_ = nvrhi_device_->createCommandList();
        if (!command_list_) {
            throw std::runtime_error("nvrhi could not create a command list");
        }
        if (options_.scene != PreviewOptions::Scene::Clear) {
            cube_ = std::make_unique<CubeDemo>(nvrhi_device_);
        }
        VkSemaphoreCreateInfo semaphore_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        check(vkCreateSemaphore(device_, &semaphore_info, nullptr, &image_available_),
              "create image-available semaphore");
        check(vkCreateSemaphore(device_, &semaphore_info, nullptr, &render_complete_),
              "create render-complete semaphore");
        recreate_swapchain();
    }

    void select_gpu() {
        uint32_t count = 0;
        check(vkEnumeratePhysicalDevices(instance_, &count, nullptr), "enumerate Vulkan devices");
        if (count == 0) {
            throw std::runtime_error("No Vulkan device is available");
        }
        std::vector<VkPhysicalDevice> devices(count);
        check(vkEnumeratePhysicalDevices(instance_, &count, devices.data()),
              "enumerate Vulkan devices");
        for (VkPhysicalDevice candidate : devices) {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(candidate, &properties);
            if (VK_API_VERSION_MAJOR(properties.apiVersion) < 1 ||
                (VK_API_VERSION_MAJOR(properties.apiVersion) == 1 &&
                 VK_API_VERSION_MINOR(properties.apiVersion) < 3)) continue;

            VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
            VkPhysicalDeviceVulkan12Features features12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
            features12.pNext = &features13;
            VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
            features.pNext = &features12;
            vkGetPhysicalDeviceFeatures2(candidate, &features);
            if (!features12.timelineSemaphore || !features13.synchronization2 ||
                !features13.dynamicRendering) continue;

            uint32_t extension_count = 0;
            check(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extension_count, nullptr),
                  "enumerate device extensions");
            std::vector<VkExtensionProperties> device_extensions(extension_count);
            check(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extension_count,
                                                       device_extensions.data()),
                  "enumerate device extensions");
            const bool has_swapchain = std::any_of(device_extensions.begin(), device_extensions.end(),
                [](const VkExtensionProperties& extension) {
                    return std::string(extension.extensionName) == VK_KHR_SWAPCHAIN_EXTENSION_NAME;
                });
            if (!has_swapchain) continue;

            uint32_t family_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, nullptr);
            std::vector<VkQueueFamilyProperties> families(family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, families.data());
            for (uint32_t family = 0; family < family_count; ++family) {
                VkBool32 present = VK_FALSE;
                check(vkGetPhysicalDeviceSurfaceSupportKHR(candidate, family, surface_, &present),
                      "query surface support");
                if ((families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
                    physical_device_ = candidate;
                    queue_family_ = family;
                    std::cout << "Vulkan device: " << properties.deviceName << '\n';
                    return;
                }
            }
        }
        throw std::runtime_error("No Vulkan 1.3 device supports presentation, timeline semaphores, synchronization2, and dynamic rendering");
    }

    void create_device() {
        float priority = 1.0f;
        VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue_info.queueFamilyIndex = queue_family_;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &priority;
        VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        features13.synchronization2 = VK_TRUE;
        features13.dynamicRendering = VK_TRUE;
        VkPhysicalDeviceVulkan12Features features12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        features12.pNext = &features13;
        features12.timelineSemaphore = VK_TRUE;
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        features.pNext = &features12;
        VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        device_info.pNext = &features;
        device_info.queueCreateInfoCount = 1;
        device_info.pQueueCreateInfos = &queue_info;
        device_info.enabledExtensionCount = 1;
        device_info.ppEnabledExtensionNames = device_extensions_;
        check(vkCreateDevice(physical_device_, &device_info, nullptr, &device_),
              "create Vulkan device");
        vkGetDeviceQueue(device_, queue_family_, 0, &queue_);
    }

    void poll_gamepad() {
        if (!options_.on_input) return;
        GLFWgamepadstate state{};
        const bool connected = glfwGetGamepadState(GLFW_JOYSTICK_1, &state) == GLFW_TRUE;
        constexpr const char* buttons[] = {
            "a", "b", "x", "y", "left_bumper", "right_bumper", "back", "start",
            "guide", "left_thumb", "right_thumb", "dpad_up", "dpad_right",
            "dpad_down", "dpad_left"};
        constexpr const char* axes[] = {
            "left_x", "left_y", "right_x", "right_y", "left_trigger", "right_trigger"};
        for (int i = 0; i <= GLFW_GAMEPAD_BUTTON_LAST; ++i) {
            options_.on_input({InputDevice::GamepadButton, buttons[i],
                connected && state.buttons[i] == GLFW_PRESS ? 1.0f : 0.0f});
        }
        for (int i = 0; i <= GLFW_GAMEPAD_AXIS_LAST; ++i) {
            float value = connected ? state.axes[i] : 0.0f;
            if (connected && i >= GLFW_GAMEPAD_AXIS_LEFT_TRIGGER) value = (value + 1.0f) * 0.5f;
            options_.on_input({InputDevice::GamepadAxis, axes[i], value});
        }
    }

    void recreate_swapchain() {
        int width = 0;
        int height = 0;
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window_, &width, &height);
            if (width == 0 || height == 0) glfwWaitEvents();
            if (glfwWindowShouldClose(window_)) return;
        }
        check(vkDeviceWaitIdle(device_), "wait before swapchain resize");
        if (cube_) cube_->release_size_dependent();
        swapchain_textures_.clear();
        if (swapchain_ != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device_, swapchain_, nullptr);
            swapchain_ = VK_NULL_HANDLE;
        }

        VkSurfaceCapabilitiesKHR capabilities{};
        check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &capabilities),
              "query surface capabilities");
        if ((capabilities.supportedUsageFlags &
             (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) !=
            (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) {
            throw std::runtime_error("Vulkan surface cannot accept a cleared swapchain image");
        }
        if (!options_.capture_path.empty() &&
            !(capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) {
            throw std::runtime_error("Vulkan surface cannot capture its swapchain image");
        }
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            extent_ = capabilities.currentExtent;
        } else {
            extent_.width = std::clamp(uint32_t(width), capabilities.minImageExtent.width,
                                       capabilities.maxImageExtent.width);
            extent_.height = std::clamp(uint32_t(height), capabilities.minImageExtent.height,
                                        capabilities.maxImageExtent.height);
        }
        uint32_t format_count = 0;
        check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, nullptr),
              "query surface formats");
        if (format_count == 0) throw std::runtime_error("Vulkan surface has no supported image formats");
        std::vector<VkSurfaceFormatKHR> formats(format_count);
        check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count,
                                                   formats.data()), "query surface formats");
        auto selected = std::find_if(formats.begin(), formats.end(), [](const auto& format) {
            return format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR &&
                   format.format == VK_FORMAT_B8G8R8A8_UNORM;
        });
        if (selected == formats.end()) {
            selected = std::find_if(formats.begin(), formats.end(), [](const auto& format) {
                return texture_format(format.format) != nvrhi::Format::UNKNOWN;
            });
        }
        if (selected == formats.end()) {
            throw std::runtime_error("Vulkan surface has no nvrhi-compatible RGBA swapchain format");
        }
        surface_format_ = *selected;
        uint32_t image_count = std::max(2u, capabilities.minImageCount);
        if (capabilities.maxImageCount != 0) image_count = std::min(image_count, capabilities.maxImageCount);
        VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        if (!(capabilities.supportedCompositeAlpha & alpha)) {
            bool found_alpha = false;
            for (VkCompositeAlphaFlagBitsKHR candidate :
                 {VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                  VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
                  VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR}) {
                if (capabilities.supportedCompositeAlpha & candidate) {
                    alpha = candidate;
                    found_alpha = true;
                    break;
                }
            }
            if (!found_alpha) throw std::runtime_error("Vulkan surface has no composite alpha mode");
        }
        VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        info.surface = surface_;
        info.minImageCount = image_count;
        info.imageFormat = surface_format_.format;
        info.imageColorSpace = surface_format_.colorSpace;
        info.imageExtent = extent_;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (!options_.capture_path.empty()) info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.preTransform = capabilities.currentTransform;
        info.compositeAlpha = alpha;
        info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        info.clipped = VK_TRUE;
        check(vkCreateSwapchainKHR(device_, &info, nullptr, &swapchain_), "create Vulkan swapchain");
        uint32_t actual_count = 0;
        check(vkGetSwapchainImagesKHR(device_, swapchain_, &actual_count, nullptr),
              "get swapchain images");
        std::vector<VkImage> images(actual_count);
        check(vkGetSwapchainImagesKHR(device_, swapchain_, &actual_count, images.data()),
              "get swapchain images");
        nvrhi::TextureDesc texture_desc{};
        texture_desc.width = extent_.width;
        texture_desc.height = extent_.height;
        texture_desc.format = texture_format(surface_format_.format);
        texture_desc.isShaderResource = false;
        texture_desc.isRenderTarget = true;
        texture_desc.initialState = nvrhi::ResourceStates::Unknown;
        for (VkImage image : images) {
            auto texture = nvrhi_device_->createHandleForNativeTexture(
                nvrhi::ObjectTypes::VK_Image, nvrhi::Object(uint64_t(image)), texture_desc);
            if (!texture) throw std::runtime_error("nvrhi could not wrap a swapchain image");
            swapchain_textures_.push_back(texture);
        }
        if (cube_) cube_->resize(swapchain_textures_, extent_.width, extent_.height);
        if (!options_.capture_path.empty()) {
            staging_ = nvrhi_device_->createStagingTexture(texture_desc,
                                                            nvrhi::CpuAccessMode::Read);
            if (!staging_) throw std::runtime_error("nvrhi could not create capture storage");
        }
        resized_ = false;
        if (options_.on_resize) {
            int logical_width = 0, logical_height = 0;
            glfwGetWindowSize(window_, &logical_width, &logical_height);
            options_.on_resize(uint32_t(logical_width), uint32_t(logical_height));
        }
        std::cout << "Swapchain: " << extent_.width << 'x' << extent_.height << '\n';
    }

    bool draw_frame(uint32_t next_frame) {
        if (swapchain_ == VK_NULL_HANDLE) return false;
        if (cube_) cube_->prepare(scene_frame_);
        uint32_t image_index = 0;
        VkResult acquire = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                                                 image_available_, VK_NULL_HANDLE, &image_index);
        if (acquire == VK_ERROR_OUT_OF_DATE_KHR) {
            recreate_swapchain();
            return false;
        }
        if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) {
            check(acquire, "acquire swapchain image");
        }
        if (image_index >= swapchain_textures_.size()) {
            throw std::runtime_error("Vulkan returned an invalid swapchain image index");
        }
        nvrhi_device_->queueWaitForSemaphore(nvrhi::CommandQueue::Graphics,
                                              image_available_, 0);
        nvrhi_device_->queueSignalSemaphore(nvrhi::CommandQueue::Graphics,
                                            render_complete_, 0);
        command_list_->open();
        // Each clear discards the previous swapchain contents, including after presentation.
        command_list_->beginTrackingTextureState(swapchain_textures_[image_index],
                                                 nvrhi::AllSubresources,
                                                 nvrhi::ResourceStates::Common);
        command_list_->clearTextureFloat(swapchain_textures_[image_index],
                                         nvrhi::AllSubresources,
                                         nvrhi::Color(0.08f, 0.18f, 0.38f, 1.0f));
        if (cube_) {
            if (options_.scene == PreviewOptions::Scene::Cube) {
                const float angle = options_.capture_path.empty()
                    ? options_.cube_angle + float(glfwGetTime()) * 0.8f
                    : options_.cube_angle;
                scene_frame_.cubes = {CubeInstance{1, glm::vec3(0.0f),
                    glm::vec3(0.30f, angle, 0.0f), glm::vec3(1.0f)}};
            }
            cube_->record(command_list_, image_index, scene_frame_);
        }
        const bool capture_this_frame = staging_ && !captured_ &&
                                        next_frame == options_.capture_frame;
        if (capture_this_frame) {
            command_list_->copyTexture(staging_, nvrhi::TextureSlice{},
                                       swapchain_textures_[image_index], nvrhi::TextureSlice{});
        }
        command_list_->setTextureState(swapchain_textures_[image_index],
                                       nvrhi::AllSubresources,
                                       nvrhi::ResourceStates::Present);
        command_list_->close();
        nvrhi_device_->executeCommandList(command_list_);
        VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &render_complete_;
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain_;
        present.pImageIndices = &image_index;
        VkResult status = vkQueuePresentKHR(queue_, &present);
        if (status == VK_ERROR_OUT_OF_DATE_KHR || status == VK_SUBOPTIMAL_KHR ||
            acquire == VK_SUBOPTIMAL_KHR || resized_) {
            check(vkQueueWaitIdle(queue_), "wait before swapchain rebuild");
            recreate_swapchain();
        } else {
            check(status, "present swapchain image");
            check(vkQueueWaitIdle(queue_), "wait for presented frame");
        }
        if (!nvrhi_device_->waitForIdle()) {
            throw std::runtime_error("Vulkan device lost while rendering the frame");
        }
        if (messages_.has_error()) throw std::runtime_error("nvrhi reported a rendering error");
        if (capture_this_frame) {
            save_capture();
            captured_ = true;
        }
        nvrhi_device_->runGarbageCollection();
        return true;
    }

    void save_capture() {
        size_t row_pitch = 0;
        const auto* pixels = static_cast<const uint8_t*>(nvrhi_device_->mapStagingTexture(
            staging_, nvrhi::TextureSlice{}, nvrhi::CpuAccessMode::Read, &row_pitch));
        if (!pixels) throw std::runtime_error("nvrhi could not map captured frame");
        std::ofstream output(options_.capture_path, std::ios::binary);
        if (!output) {
            nvrhi_device_->unmapStagingTexture(staging_);
            throw std::runtime_error("cannot write capture: " + options_.capture_path.string());
        }
        output << "P6\n" << extent_.width << ' ' << extent_.height << "\n255\n";
        const bool bgra = surface_format_.format == VK_FORMAT_B8G8R8A8_UNORM ||
                          surface_format_.format == VK_FORMAT_B8G8R8A8_SRGB;
        for (uint32_t y = 0; y < extent_.height; ++y) {
            const uint8_t* row = pixels + size_t(y) * row_pitch;
            for (uint32_t x = 0; x < extent_.width; ++x) {
                const uint8_t* pixel = row + size_t(x) * 4;
                const char rgb[3] = {char(pixel[bgra ? 2 : 0]), char(pixel[1]),
                                     char(pixel[bgra ? 0 : 2])};
                output.write(rgb, sizeof(rgb));
            }
        }
        nvrhi_device_->unmapStagingTexture(staging_);
        if (!output) throw std::runtime_error("failed to save capture: " + options_.capture_path.string());
        std::cout << "Captured "
                  << (options_.scene == PreviewOptions::Scene::Project ? "project" :
                      cube_ ? "cube" : "clear") << " frame: "
                  << options_.capture_path.string() << '\n';
    }

    PreviewOptions options_;
    bool glfw_started_ = false;
    bool resized_ = false;
    bool captured_ = false;
    std::exception_ptr pending_input_error_;
    GLFWwindow* window_ = nullptr;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue queue_ = VK_NULL_HANDLE;
    uint32_t queue_family_ = 0;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkExtent2D extent_{};
    VkSurfaceFormatKHR surface_format_{};
    VkSemaphore image_available_ = VK_NULL_HANDLE;
    VkSemaphore render_complete_ = VK_NULL_HANDLE;
    std::vector<const char*> instance_extensions_;
    const char* device_extensions_[1] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    Messages messages_;
    nvrhi::vulkan::DeviceHandle nvrhi_device_;
    nvrhi::CommandListHandle command_list_;
    nvrhi::StagingTextureHandle staging_;
    std::vector<nvrhi::TextureHandle> swapchain_textures_;
    std::unique_ptr<CubeDemo> cube_;
    SceneFrame scene_frame_;
};

}  // namespace

PreviewResult run_preview(const PreviewOptions& options) {
    if (options.width == 0 || options.height == 0 || options.width > 8192 || options.height > 8192) {
        throw std::invalid_argument("preview width and height must be between 1 and 8192");
    }
    if (options.scene == PreviewOptions::Scene::Cube && !std::isfinite(options.cube_angle)) {
        throw std::invalid_argument("cube angle must be finite");
    }
    if (options.capture_frame == 0 || !std::isfinite(options.fixed_delta) ||
        options.fixed_delta < 0.0 ||
        (options.frame_limit != 0 && !options.capture_path.empty() &&
         options.capture_frame > options.frame_limit)) {
        throw std::invalid_argument("invalid preview frame or timing options");
    }
    return VulkanWindow(options).run();
}

}  // namespace threepoint
