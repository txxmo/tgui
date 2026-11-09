// Vulkan renderer. Two ways to use it:
//
// 1) Inside your Vulkan app (you own the instance, device, swapchain and render pass):
//      tg::VulkanRenderer Vk;
//      Vk.Init( { Instance, Physical, Device, QueueFamily, Queue, RenderPass, FramesInFlight } );
//      ... every frame, after waiting for that frame slot's fence and inside your render pass:
//      Vk.BeginFrame( CommandBuffer, FrameSlot );
//      Ctx.Frame();  Ctx.Render( { Width, Height } );   // records tg's draws into CommandBuffer
//    Use a UNORM swapchain format (B8G8R8A8_UNORM / R8G8B8A8_UNORM) for the same colors as the other
//    renderers (tg's colors are sRGB values blended in sRGB space, like GL / D3D).
//
// 2) Self-contained, offscreen (tests, servers, screenshots):  Vk.InitHeadless();  Render() draws into
//    an internal image and fills Captured (top-left first) — the software renderer's job on a GPU.
//
// Supported: textures, clipping, render targets (CachedPanel), custom shaders (ShaderDesc::Glsl, compiled
// at runtime when tg was built with TG_VULKAN_SHADERC), frame capture (headless mode).
// Not supported: backdrop blur (it needs to read the frame mid-pass; Backdrop widgets fall back to a tint).
#pragma once
#include "tg/renderer.h"
#include <vulkan/vulkan.h>
#include <memory>

namespace tg
{
    class VulkanRenderer : public Renderer
    {
    public:
        struct InitInfo
        {
            VkInstance Instance = VK_NULL_HANDLE;
            VkPhysicalDevice Physical = VK_NULL_HANDLE;
            VkDevice Device = VK_NULL_HANDLE;
            uint32_t QueueFamily = 0;
            VkQueue Queue = VK_NULL_HANDLE; // graphics; used for texture uploads and render targets
            VkRenderPass RenderPass = VK_NULL_HANDLE; // the pass Render() records into (one color attachment)
            uint32_t FramesInFlight = 2;
        };

        VulkanRenderer();
        ~VulkanRenderer() override;

        bool Init( const InitInfo& Info );
        bool InitHeadless( bool PreferSoftware = false ); // own instance + device; PreferSoftware picks a CPU device (lavapipe)
        void Shutdown();
        const char* DeviceName() const;

        // Embedded mode: the command buffer Render() records into (inside your render pass) and the frame slot
        // (0..FramesInFlight-1) whose fence you already waited for.
        void BeginFrame( VkCommandBuffer Cb, uint32_t FrameSlot );

        TextureId CreateTexture( int W, int H, const void* Pixels ) override;
        void UpdateTexture( TextureId Tex, int W, int H, const void* Pixels ) override;
        void DestroyTexture( TextureId Tex ) override;
        void Render( const DrawList& List, Vec2 FramebufferSize, float Scale = 1.f ) override;
        bool CanCapture() const override;

        ShaderId CreateShader( const ShaderDesc& Desc ) override;
        void DestroyShader( ShaderId S ) override;
        bool CanShade() const override;
        TextureId CreateRenderTarget( int Width, int Height ) override;
        void RenderTo( TextureId Target, const DrawList& List, Vec2 Size, float Scale ) override;

        struct Impl;

    private:
        std::unique_ptr<Impl> P;
    };
}
