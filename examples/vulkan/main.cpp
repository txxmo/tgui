// tg on Vulkan: GLFW window + swapchain you own, tg records its draws into your render pass.
// This is the "embedded" mode of tg::VulkanRenderer — the same thing you'd do inside a game engine.
//
//   tg_vulkan_demo            (needs a Vulkan driver; any GPU, or Mesa's lavapipe on the CPU)
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../common/demo_ui.h"
#include "glfw_platform.h"
#include "vk_renderer.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
    struct Swap
    {
        VkSwapchainKHR Chain = VK_NULL_HANDLE;
        VkFormat Format = VK_FORMAT_B8G8R8A8_UNORM;
        VkExtent2D Size{};
        std::vector<VkImage> Images;
        std::vector<VkImageView> Views;
        std::vector<VkFramebuffer> Fbs;
    };

    constexpr uint32_t InFlight = 2;
}

int main( int Argc, char** Argv )
{
    if ( !glfwInit() || !glfwVulkanSupported() ) { std::fprintf( stderr, "no Vulkan loader / GLFW\n" ); return 1; }
    glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
    glfwWindowHint( GLFW_SCALE_TO_MONITOR, 1 );
    GLFWwindow* Win = glfwCreateWindow( 1280, 820, "tg on Vulkan", nullptr, nullptr );

    // ---- instance, surface, device
    uint32_t ExtN = 0;
    const char** Ext = glfwGetRequiredInstanceExtensions( &ExtN );
    VkApplicationInfo App{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    App.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo Ic{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    Ic.pApplicationInfo = &App;
    Ic.enabledExtensionCount = ExtN;
    Ic.ppEnabledExtensionNames = Ext;
    VkInstance Inst;
    if ( vkCreateInstance( &Ic, nullptr, &Inst ) != VK_SUCCESS ) { std::fprintf( stderr, "vkCreateInstance failed\n" ); return 1; }
    VkSurfaceKHR Surf;
    if ( glfwCreateWindowSurface( Inst, Win, nullptr, &Surf ) != VK_SUCCESS ) { std::fprintf( stderr, "no surface\n" ); return 1; }

    uint32_t N = 0;
    vkEnumeratePhysicalDevices( Inst, &N, nullptr );
    std::vector<VkPhysicalDevice> Devs( N );
    vkEnumeratePhysicalDevices( Inst, &N, Devs.data() );
    VkPhysicalDevice Phys = VK_NULL_HANDLE;
    uint32_t Family = 0;
    for ( VkPhysicalDevice D : Devs )
    {
        uint32_t Qn = 0;
        vkGetPhysicalDeviceQueueFamilyProperties( D, &Qn, nullptr );
        std::vector<VkQueueFamilyProperties> Qs( Qn );
        vkGetPhysicalDeviceQueueFamilyProperties( D, &Qn, Qs.data() );
        for ( uint32_t q = 0; q < Qn && !Phys; q++ )
        {
            VkBool32 Present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR( D, q, Surf, &Present );
            if ( ( Qs[ q ].queueFlags & VK_QUEUE_GRAPHICS_BIT ) && Present ) { Phys = D; Family = q; }
        }
        if ( Phys ) break;
    }
    if ( !Phys ) { std::fprintf( stderr, "no device can present to this window\n" ); return 1; }
    float Prio = 1;
    VkDeviceQueueCreateInfo Qc{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    Qc.queueFamilyIndex = Family;
    Qc.queueCount = 1;
    Qc.pQueuePriorities = &Prio;
    const char* DevExt[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo Dc{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    Dc.queueCreateInfoCount = 1;
    Dc.pQueueCreateInfos = &Qc;
    Dc.enabledExtensionCount = 1;
    Dc.ppEnabledExtensionNames = DevExt;
    VkDevice Dev;
    if ( vkCreateDevice( Phys, &Dc, nullptr, &Dev ) != VK_SUCCESS ) { std::fprintf( stderr, "vkCreateDevice failed\n" ); return 1; }
    VkQueue Queue;
    vkGetDeviceQueue( Dev, Family, 0, &Queue );

    // ---- a UNORM surface format (tg blends sRGB values like the other renderers)
    uint32_t Fn = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR( Phys, Surf, &Fn, nullptr );
    std::vector<VkSurfaceFormatKHR> Fmts( Fn );
    vkGetPhysicalDeviceSurfaceFormatsKHR( Phys, Surf, &Fn, Fmts.data() );
    VkSurfaceFormatKHR Fmt = Fmts[ 0 ];
    for ( auto& F : Fmts )
        if ( F.format == VK_FORMAT_B8G8R8A8_UNORM || F.format == VK_FORMAT_R8G8B8A8_UNORM ) { Fmt = F; break; }

    // ---- render pass: clear + one color attachment, presented afterwards
    VkAttachmentDescription Att{};
    Att.format = Fmt.format;
    Att.samples = VK_SAMPLE_COUNT_1_BIT;
    Att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    Att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    Att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    Att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    Att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    Att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference Ref{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription Sp{};
    Sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    Sp.colorAttachmentCount = 1;
    Sp.pColorAttachments = &Ref;
    VkSubpassDependency Dep{ VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0 };
    VkRenderPassCreateInfo Rp{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    Rp.attachmentCount = 1;
    Rp.pAttachments = &Att;
    Rp.subpassCount = 1;
    Rp.pSubpasses = &Sp;
    Rp.dependencyCount = 1;
    Rp.pDependencies = &Dep;
    VkRenderPass Pass;
    vkCreateRenderPass( Dev, &Rp, nullptr, &Pass );

    Swap S;
    auto MakeSwap = [ & ] {
        vkDeviceWaitIdle( Dev );
        for ( auto F : S.Fbs ) vkDestroyFramebuffer( Dev, F, nullptr );
        for ( auto V : S.Views ) vkDestroyImageView( Dev, V, nullptr );
        S.Fbs.clear();
        S.Views.clear();
        VkSurfaceCapabilitiesKHR Caps;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR( Phys, Surf, &Caps );
        int Fw, Fh;
        glfwGetFramebufferSize( Win, &Fw, &Fh );
        S.Size = Caps.currentExtent.width != UINT32_MAX ? Caps.currentExtent : VkExtent2D{ uint32_t( Fw ), uint32_t( Fh ) };
        if ( S.Size.width == 0 || S.Size.height == 0 ) return false;
        VkSwapchainCreateInfoKHR Sc{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        Sc.surface = Surf;
        Sc.minImageCount = std::max( Caps.minImageCount, 2u );
        if ( Caps.maxImageCount ) Sc.minImageCount = std::min( Sc.minImageCount, Caps.maxImageCount );
        Sc.imageFormat = Fmt.format;
        Sc.imageColorSpace = Fmt.colorSpace;
        Sc.imageExtent = S.Size;
        Sc.imageArrayLayers = 1;
        Sc.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        Sc.preTransform = Caps.currentTransform;
        Sc.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        Sc.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        Sc.clipped = VK_TRUE;
        Sc.oldSwapchain = S.Chain;
        VkSwapchainKHR New;
        if ( vkCreateSwapchainKHR( Dev, &Sc, nullptr, &New ) != VK_SUCCESS ) return false;
        if ( S.Chain ) vkDestroySwapchainKHR( Dev, S.Chain, nullptr );
        S.Chain = New;
        uint32_t In = 0;
        vkGetSwapchainImagesKHR( Dev, S.Chain, &In, nullptr );
        S.Images.resize( In );
        vkGetSwapchainImagesKHR( Dev, S.Chain, &In, S.Images.data() );
        for ( VkImage Img : S.Images )
        {
            VkImageViewCreateInfo Vi{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            Vi.image = Img;
            Vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
            Vi.format = Fmt.format;
            Vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            VkImageView V;
            vkCreateImageView( Dev, &Vi, nullptr, &V );
            S.Views.push_back( V );
            VkFramebufferCreateInfo Fi{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
            Fi.renderPass = Pass;
            Fi.attachmentCount = 1;
            Fi.pAttachments = &V;
            Fi.width = S.Size.width;
            Fi.height = S.Size.height;
            Fi.layers = 1;
            VkFramebuffer Fb;
            vkCreateFramebuffer( Dev, &Fi, nullptr, &Fb );
            S.Fbs.push_back( Fb );
        }
        return true;
    };
    MakeSwap();

    // ---- per-frame command buffers + sync
    VkCommandPoolCreateInfo Pc{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    Pc.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    Pc.queueFamilyIndex = Family;
    VkCommandPool Pool;
    vkCreateCommandPool( Dev, &Pc, nullptr, &Pool );
    VkCommandBuffer Cbs[ InFlight ];
    VkCommandBufferAllocateInfo Ca{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    Ca.commandPool = Pool;
    Ca.commandBufferCount = InFlight;
    vkAllocateCommandBuffers( Dev, &Ca, Cbs );
    VkSemaphore Acquired[ InFlight ], Done[ InFlight ];
    VkFence Fences[ InFlight ];
    for ( uint32_t i = 0; i < InFlight; i++ )
    {
        VkSemaphoreCreateInfo Si{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        vkCreateSemaphore( Dev, &Si, nullptr, &Acquired[ i ] );
        vkCreateSemaphore( Dev, &Si, nullptr, &Done[ i ] );
        VkFenceCreateInfo Fi{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        Fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCreateFence( Dev, &Fi, nullptr, &Fences[ i ] );
    }

    // ---- tg
    {
        tg::VulkanRenderer Vk;
        if ( !Vk.Init( { Inst, Phys, Dev, Family, Queue, Pass, InFlight } ) ) { std::fprintf( stderr, "tg vulkan init failed\n" ); return 1; }
        std::printf( "tg on Vulkan: %s\n", Vk.DeviceName() );
        tg::Context Ctx( Vk );
        float Scale = 1, Dummy;
        glfwGetWindowContentScale( Win, &Scale, &Dummy );
        Ctx.LoadDefaultFonts( Scale > 0 ? Scale : 1 );
        tg::glfw::Install( Win, Ctx );
        DemoApp Demo;
        Demo.Build( Ctx, Ctx.Root() );
        for ( int i = 1; i + 1 < Argc; i++ )
            if ( !std::strcmp( Argv[ i ], "--tab" ) ) Demo.Tabs->Select( std::atoi( Argv[ i + 1 ] ) );

        uint32_t Slot = 0;
        bool Dirty = false;
        while ( !glfwWindowShouldClose( Win ) )
        {
            tg::glfw::WaitForWork( Ctx );
            tg::glfw::Sync( Win, Ctx );
            if ( !Ctx.NeedsFrame() && !Dirty ) continue;
            if ( Dirty && !MakeSwap() ) continue; // minimized
            Dirty = false;
            vkWaitForFences( Dev, 1, &Fences[ Slot ], VK_TRUE, UINT64_MAX );
            uint32_t Img = 0;
            VkResult R = vkAcquireNextImageKHR( Dev, S.Chain, UINT64_MAX, Acquired[ Slot ], VK_NULL_HANDLE, &Img );
            if ( R == VK_ERROR_OUT_OF_DATE_KHR ) { Dirty = true; continue; }
            vkResetFences( Dev, 1, &Fences[ Slot ] );

            Ctx.Frame();
            VkCommandBuffer Cb = Cbs[ Slot ];
            vkResetCommandBuffer( Cb, 0 );
            VkCommandBufferBeginInfo Bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            vkBeginCommandBuffer( Cb, &Bi );
            VkClearValue Clear{};
            VkRenderPassBeginInfo Rb{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
            Rb.renderPass = Pass;
            Rb.framebuffer = S.Fbs[ Img ];
            Rb.renderArea = { { 0, 0 }, S.Size };
            Rb.clearValueCount = 1;
            Rb.pClearValues = &Clear;
            // (Ctx.Render also runs render-to-texture passes for cached panels; they submit on their own)
            Vk.BeginFrame( Cb, Slot );
            vkCmdBeginRenderPass( Cb, &Rb, VK_SUBPASS_CONTENTS_INLINE );
            Ctx.Render( { float( S.Size.width ), float( S.Size.height ) } );
            vkCmdEndRenderPass( Cb );
            vkEndCommandBuffer( Cb );

            VkPipelineStageFlags Wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            VkSubmitInfo Si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
            Si.waitSemaphoreCount = 1;
            Si.pWaitSemaphores = &Acquired[ Slot ];
            Si.pWaitDstStageMask = &Wait;
            Si.commandBufferCount = 1;
            Si.pCommandBuffers = &Cb;
            Si.signalSemaphoreCount = 1;
            Si.pSignalSemaphores = &Done[ Slot ];
            vkQueueSubmit( Queue, 1, &Si, Fences[ Slot ] );
            VkPresentInfoKHR Pi{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
            Pi.waitSemaphoreCount = 1;
            Pi.pWaitSemaphores = &Done[ Slot ];
            Pi.swapchainCount = 1;
            Pi.pSwapchains = &S.Chain;
            Pi.pImageIndices = &Img;
            R = vkQueuePresentKHR( Queue, &Pi );
            if ( R == VK_ERROR_OUT_OF_DATE_KHR || R == VK_SUBOPTIMAL_KHR ) Dirty = true;
            Slot = ( Slot + 1 ) % InFlight;
        }
        vkDeviceWaitIdle( Dev );
    } // tg (Context + renderer) goes before the device

    for ( uint32_t i = 0; i < InFlight; i++ )
    {
        vkDestroySemaphore( Dev, Acquired[ i ], nullptr );
        vkDestroySemaphore( Dev, Done[ i ], nullptr );
        vkDestroyFence( Dev, Fences[ i ], nullptr );
    }
    vkDestroyCommandPool( Dev, Pool, nullptr );
    for ( auto F : S.Fbs ) vkDestroyFramebuffer( Dev, F, nullptr );
    for ( auto V : S.Views ) vkDestroyImageView( Dev, V, nullptr );
    vkDestroySwapchainKHR( Dev, S.Chain, nullptr );
    vkDestroyRenderPass( Dev, Pass, nullptr );
    vkDestroyDevice( Dev, nullptr );
    vkDestroySurfaceKHR( Inst, Surf, nullptr );
    vkDestroyInstance( Inst, nullptr );
    glfwDestroyWindow( Win );
    glfwTerminate();
    return 0;
}
