// vulkan_sample.c — Direct X11 + Vulkan: draw one colored rectangle
// No GLFW, no Raylib, no middleware. Pure Vulkan + X11.
// 
// BUILD:
//   # First compile shaders (one-time):
//   glslangValidator -V gui/shaders/rect.vert -o gui/shaders/vert.spv
//   glslangValidator -V gui/shaders/rect.frag -o gui/shaders/frag.spv
//   # Then compile:
//   gcc -O2 -o vulkan_sample gui/vulkan_sample.c -lvulkan -lX11 -lm
//
// REQUIRES:
//   sudo apt install libvulkan-dev libx11-dev vulkan-tools mesa-vulkan-drivers glslang-tools

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>

// Vulkan
#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>

// X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>

//  Configuration
#define WIN_WIDTH  800
#define WIN_HEIGHT 600
#define WIN_TITLE  "Luna Vulkan Sample - One Rectangle"

#define MAX_FRAMES_IN_FLIGHT 2

//  Vertex Data
typedef struct {
    float pos[2];
    float color[3];
} Vertex;

// Rectangle at center, teal colored
static Vertex rectangle_vertices[] = {
    {{ -0.5f, -0.3f }, { 0.0f, 0.8f, 0.7f }},
    {{  0.5f, -0.3f }, { 0.0f, 0.8f, 0.7f }},
    {{  0.5f,  0.3f }, { 0.0f, 0.6f, 0.9f }},
    {{ -0.5f, -0.3f }, { 0.0f, 0.8f, 0.7f }},
    {{  0.5f,  0.3f }, { 0.0f, 0.6f, 0.9f }},
    {{ -0.5f,  0.3f }, { 0.0f, 0.6f, 0.9f }},
};

//  Globals
static Display *x_display;
static Window   x_window;
static Atom     wm_delete_msg;

static VkInstance               instance;
static VkPhysicalDevice         physical_device;
static VkDevice                 device;
static VkQueue                  graphics_queue;
static VkQueue                  present_queue;
static uint32_t                 graphics_family;
static uint32_t                 present_family;
static VkSurfaceKHR             surface;
static VkSwapchainKHR           swapchain;
static VkFormat                 swapchain_format;
static VkExtent2D               swapchain_extent;
static uint32_t                 image_count;
static VkImage                 *swapchain_images;
static VkImageView             *swapchain_views;
static VkRenderPass             render_pass;
static VkPipelineLayout         pipeline_layout;
static VkPipeline               graphics_pipeline;
static VkFramebuffer           *framebuffers;
static VkCommandPool            command_pool;
static VkCommandBuffer          command_buffers[MAX_FRAMES_IN_FLIGHT];
static VkSemaphore              image_available[MAX_FRAMES_IN_FLIGHT];
static VkSemaphore              render_finished[MAX_FRAMES_IN_FLIGHT];
static VkFence                  in_flight[MAX_FRAMES_IN_FLIGHT];
static VkBuffer                 vertex_buffer;
static VkDeviceMemory           vertex_memory;
static int                      current_frame = 0;

//  Helper
#define VK_CHECK(call) do { \
    VkResult _r = (call); \
    if (_r != VK_SUCCESS) { \
        fprintf(stderr, "Vulkan error %d at %s:%d\n", _r, __FILE__, __LINE__); \
        exit(1); \
    } \
} while(0)

//  Load SPIR-V file 
static uint32_t* load_spv(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open shader: %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint32_t *buf = (uint32_t*)malloc(len);
    if (fread(buf, 1, len, f) != (size_t)len) {
        fprintf(stderr, "Failed to read shader: %s\n", path);
        fclose(f);
        exit(1);
    }
    fclose(f);
    *out_size = (size_t)len;
    return buf;
}

//  1. Create X11 Window 
static void create_x11_window(void) {
    x_display = XOpenDisplay(NULL);
    if (!x_display) { fprintf(stderr, "Cannot open X display\n"); exit(1); }

    int screen = DefaultScreen(x_display);
    x_window = XCreateSimpleWindow(x_display, RootWindow(x_display, screen),
        0, 0, WIN_WIDTH, WIN_HEIGHT, 0,
        BlackPixel(x_display, screen), BlackPixel(x_display, screen));

    XStoreName(x_display, x_window, WIN_TITLE);
    XSelectInput(x_display, x_window, ExposureMask | KeyPressMask | StructureNotifyMask);
    
    // Handle window close button
    wm_delete_msg = XInternAtom(x_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(x_display, x_window, &wm_delete_msg, 1);
    
    XMapWindow(x_display, x_window);
    XFlush(x_display);
}

//  2. Create Vulkan Instance 
static void create_instance(void) {
    const char *extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
    };

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Luna Vulkan Sample",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Luna",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };

    VkInstanceCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = 2,
        .ppEnabledExtensionNames = extensions,
    };

    VK_CHECK(vkCreateInstance(&ci, NULL, &instance));
}

//  3. Create Xlib Surface 
static void create_surface(void) {
    VkXlibSurfaceCreateInfoKHR ci = {
        .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        .dpy = x_display,
        .window = x_window,
    };
    VK_CHECK(vkCreateXlibSurfaceKHR(instance, &ci, NULL, &surface));
}

//  4. Pick Physical Device 
static void pick_physical_device(void) {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, NULL);
    if (count == 0) { fprintf(stderr, "No Vulkan devices found\n"); exit(1); }
    
    VkPhysicalDevice *devices = malloc(sizeof(VkPhysicalDevice) * count);
    vkEnumeratePhysicalDevices(instance, &count, devices);

    for (uint32_t i = 0; i < count; i++) {
        uint32_t qcount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &qcount, NULL);
        VkQueueFamilyProperties *qprops = malloc(sizeof(VkQueueFamilyProperties) * qcount);
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &qcount, qprops);

        bool found_graphics = false, found_present = false;
        for (uint32_t q = 0; q < qcount; q++) {
            if (qprops[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphics_family = q;
                found_graphics = true;
            }
            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(devices[i], q, surface, &present_support);
            if (present_support) {
                present_family = q;
                found_present = true;
            }
            if (found_graphics && found_present) break;
        }
        free(qprops);

        if (found_graphics && found_present) {
            physical_device = devices[i];
            
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(physical_device, &props);
            printf("Using GPU: %s\n", props.deviceName);
            
            free(devices);
            return;
        }
    }
    fprintf(stderr, "No suitable Vulkan device found\n");
    free(devices);
    exit(1);
}

//  5. Create Logical Device 
static void create_device(void) {
    float priority = 1.0f;
    uint32_t unique_families[2] = { graphics_family, present_family };
    uint32_t family_count = (graphics_family == present_family) ? 1 : 2;

    VkDeviceQueueCreateInfo queue_cis[2];
    for (uint32_t i = 0; i < family_count; i++) {
        queue_cis[i] = (VkDeviceQueueCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = unique_families[i],
            .queueCount = 1,
            .pQueuePriorities = &priority,
        };
    }

    const char *device_exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = family_count,
        .pQueueCreateInfos = queue_cis,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = device_exts,
    };

    VK_CHECK(vkCreateDevice(physical_device, &ci, NULL, &device));
    vkGetDeviceQueue(device, graphics_family, 0, &graphics_queue);
    vkGetDeviceQueue(device, present_family, 0, &present_queue);
}

//  6. Create Swapchain 
static void create_swapchain(void) {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &caps);

    uint32_t fmt_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &fmt_count, NULL);
    VkSurfaceFormatKHR *formats = malloc(sizeof(VkSurfaceFormatKHR) * fmt_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &fmt_count, formats);

    swapchain_format = formats[0].format;
    VkColorSpaceKHR color_space = formats[0].colorSpace;
    for (uint32_t i = 0; i < fmt_count; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            swapchain_format = formats[i].format;
            color_space = formats[i].colorSpace;
            break;
        }
    }
    free(formats);

    swapchain_extent = caps.currentExtent;
    if (swapchain_extent.width == UINT32_MAX) {
        swapchain_extent.width = WIN_WIDTH;
        swapchain_extent.height = WIN_HEIGHT;
    }

    image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount)
        image_count = caps.maxImageCount;

    VkSwapchainCreateInfoKHR ci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = image_count,
        .imageFormat = swapchain_format,
        .imageColorSpace = color_space,
        .imageExtent = swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
    };

    if (graphics_family != present_family) {
        uint32_t indices[] = { graphics_family, present_family };
        ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices = indices;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VK_CHECK(vkCreateSwapchainKHR(device, &ci, NULL, &swapchain));

    vkGetSwapchainImagesKHR(device, swapchain, &image_count, NULL);
    swapchain_images = malloc(sizeof(VkImage) * image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, swapchain_images);
}

//  7. Create Image Views 
static void create_image_views(void) {
    swapchain_views = malloc(sizeof(VkImageView) * image_count);
    for (uint32_t i = 0; i < image_count; i++) {
        VkImageViewCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchain_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchain_format,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        };
        VK_CHECK(vkCreateImageView(device, &ci, NULL, &swapchain_views[i]));
    }
}

//  8. Create Render Pass 
static void create_render_pass(void) {
    VkAttachmentDescription color_attach = {
        .format = swapchain_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference color_ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
    };

    VkSubpassDependency dep = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attach,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dep,
    };

    VK_CHECK(vkCreateRenderPass(device, &ci, NULL, &render_pass));
}

//  9. Create Shader Module 
static VkShaderModule create_shader_module(const uint32_t *code, size_t size) {
    VkShaderModuleCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = code,
    };
    VkShaderModule mod;
    VK_CHECK(vkCreateShaderModule(device, &ci, NULL, &mod));
    return mod;
}

//  10. Create Graphics Pipeline 
static void create_pipeline(void) {
    size_t vert_size, frag_size;
    uint32_t *vert_code = load_spv("gui/shaders/vert.spv", &vert_size);
    uint32_t *frag_code = load_spv("gui/shaders/frag.spv", &frag_size);

    printf("Loaded vert.spv: %zu bytes\n", vert_size);
    printf("Loaded frag.spv: %zu bytes\n", frag_size);

    VkShaderModule vert_mod = create_shader_module(vert_code, vert_size);
    VkShaderModule frag_mod = create_shader_module(frag_code, frag_size);
    free(vert_code);
    free(frag_code);

    VkPipelineShaderStageCreateInfo stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_mod,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_mod,
            .pName = "main",
        },
    };

    VkVertexInputBindingDescription binding = {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkVertexInputAttributeDescription attrs[] = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,    .offset = offsetof(Vertex, pos) },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, color) },
    };

    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = attrs,
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkViewport viewport = { 0, 0, (float)swapchain_extent.width, (float)swapchain_extent.height, 0, 1 };
    VkRect2D scissor = { {0, 0}, swapchain_extent };

    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1, .pViewports = &viewport,
        .scissorCount = 1,  .pScissors = &scissor,
    };

    VkPipelineRasterizationStateCreateInfo raster = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_NONE,  // No culling for 2D
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
    };

    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineColorBlendAttachmentState blend_attach = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE,
    };

    VkPipelineColorBlendStateCreateInfo blend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blend_attach,
    };

    VkPipelineLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };
    VK_CHECK(vkCreatePipelineLayout(device, &layout_ci, NULL, &pipeline_layout));

    VkGraphicsPipelineCreateInfo pipeline_ci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &raster,
        .pMultisampleState = &multisample,
        .pColorBlendState = &blend,
        .layout = pipeline_layout,
        .renderPass = render_pass,
        .subpass = 0,
    };

    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_ci, NULL, &graphics_pipeline));

    vkDestroyShaderModule(device, vert_mod, NULL);
    vkDestroyShaderModule(device, frag_mod, NULL);
}

//  11. Create Framebuffers 
static void create_framebuffers(void) {
    framebuffers = malloc(sizeof(VkFramebuffer) * image_count);
    for (uint32_t i = 0; i < image_count; i++) {
        VkFramebufferCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass,
            .attachmentCount = 1,
            .pAttachments = &swapchain_views[i],
            .width = swapchain_extent.width,
            .height = swapchain_extent.height,
            .layers = 1,
        };
        VK_CHECK(vkCreateFramebuffer(device, &ci, NULL, &framebuffers[i]));
    }
}

//  12. Create Command Pool + Buffers 
static void create_command_pool(void) {
    VkCommandPoolCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphics_family,
    };
    VK_CHECK(vkCreateCommandPool(device, &ci, NULL, &command_pool));

    VkCommandBufferAllocateInfo ai = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
    };
    VK_CHECK(vkAllocateCommandBuffers(device, &ai, command_buffers));
}

//  13. Find Memory Type 
static uint32_t find_memory_type(uint32_t filter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    fprintf(stderr, "Failed to find suitable memory type\n");
    exit(1);
}

//  14. Create Vertex Buffer 
static void create_vertex_buffer(void) {
    VkDeviceSize size = sizeof(rectangle_vertices);

    VkBufferCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VK_CHECK(vkCreateBuffer(device, &ci, NULL, &vertex_buffer));

    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(device, vertex_buffer, &mem_req);

    VkMemoryAllocateInfo ai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = find_memory_type(mem_req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
    };
    VK_CHECK(vkAllocateMemory(device, &ai, NULL, &vertex_memory));
    vkBindBufferMemory(device, vertex_buffer, vertex_memory, 0);

    void *data;
    vkMapMemory(device, vertex_memory, 0, size, 0, &data);
    memcpy(data, rectangle_vertices, size);
    vkUnmapMemory(device, vertex_memory);
}

//  15. Create Sync Objects 
static void create_sync_objects(void) {
    VkSemaphoreCreateInfo sem_ci = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fence_ci = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateSemaphore(device, &sem_ci, NULL, &image_available[i]));
        VK_CHECK(vkCreateSemaphore(device, &sem_ci, NULL, &render_finished[i]));
        VK_CHECK(vkCreateFence(device, &fence_ci, NULL, &in_flight[i]));
    }
}

//  16. Record Command Buffer 
static void record_command_buffer(VkCommandBuffer cmd, uint32_t image_index) {
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin));

    VkClearValue clear = { .color = {{ 0.05f, 0.05f, 0.08f, 1.0f }} };

    VkRenderPassBeginInfo rp_begin = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = render_pass,
        .framebuffer = framebuffers[image_index],
        .renderArea = { {0, 0}, swapchain_extent },
        .clearValueCount = 1,
        .pClearValues = &clear,
    };

    vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer, &offset);
    vkCmdDraw(cmd, 6, 1, 0, 0);

    vkCmdEndRenderPass(cmd);
    VK_CHECK(vkEndCommandBuffer(cmd));
}

//  17. Draw Frame 
static void draw_frame(void) {
    VK_CHECK(vkWaitForFences(device, 1, &in_flight[current_frame], VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(device, 1, &in_flight[current_frame]));

    uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, 
                                             image_available[current_frame], VK_NULL_HANDLE, &image_index);
    if (result != VK_SUCCESS) return;

    vkResetCommandBuffer(command_buffers[current_frame], 0);
    record_command_buffer(command_buffers[current_frame], image_index);

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &image_available[current_frame],
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffers[current_frame],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &render_finished[current_frame],
    };
    VK_CHECK(vkQueueSubmit(graphics_queue, 1, &submit, in_flight[current_frame]));

    VkPresentInfoKHR present = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &render_finished[current_frame],
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &image_index,
    };
    vkQueuePresentKHR(present_queue, &present);

    current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

//  18. Cleanup
static void cleanup(void) {
    vkDeviceWaitIdle(device);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, image_available[i], NULL);
        vkDestroySemaphore(device, render_finished[i], NULL);
        vkDestroyFence(device, in_flight[i], NULL);
    }
    vkDestroyCommandPool(device, command_pool, NULL);
    for (uint32_t i = 0; i < image_count; i++)
        vkDestroyFramebuffer(device, framebuffers[i], NULL);
    vkDestroyPipeline(device, graphics_pipeline, NULL);
    vkDestroyPipelineLayout(device, pipeline_layout, NULL);
    vkDestroyRenderPass(device, render_pass, NULL);
    for (uint32_t i = 0; i < image_count; i++)
        vkDestroyImageView(device, swapchain_views[i], NULL);
    vkDestroySwapchainKHR(device, swapchain, NULL);
    vkDestroyBuffer(device, vertex_buffer, NULL);
    vkFreeMemory(device, vertex_memory, NULL);
    vkDestroyDevice(device, NULL);
    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroyInstance(instance, NULL);

    free(swapchain_images);
    free(swapchain_views);
    free(framebuffers);

    XDestroyWindow(x_display, x_window);
    XCloseDisplay(x_display);
}

//  MAIN
int main(void) {
    printf("=== Luna Vulkan Sample ===\n");
    printf("Drawing ONE colored rectangle with direct X11 + Vulkan.\n");
    printf("Close the window or press any key to exit.\n\n");

    create_x11_window();
    create_instance();
    create_surface();
    pick_physical_device();
    create_device();
    create_swapchain();
    create_image_views();
    create_render_pass();
    create_pipeline();
    create_framebuffers();
    create_command_pool();
    create_vertex_buffer();
    create_sync_objects();

    printf("Vulkan initialized! (15 setup steps for 1 rectangle)\n\n");

    bool running = true;
    while (running) {
        while (XPending(x_display)) {
            XEvent event;
            XNextEvent(x_display, &event);
            if (event.type == KeyPress)
                running = false;
            if (event.type == ClientMessage && 
                (Atom)event.xclient.data.l[0] == wm_delete_msg)
                running = false;
        }
        draw_frame();
    }

    cleanup();
    printf("Cleaned up. Goodbye!\n");
    return 0;
}
