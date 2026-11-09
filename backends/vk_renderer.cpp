#include "vk_renderer.h"
#include "tg/draw.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#if defined( TG_VULKAN_SHADERC )
#include <shaderc/shaderc.h>
#endif

namespace
{
#include "vk_shaders.inc"

    struct PushConst
    {
        float Proj[ 4 ]; // scale.xy, offset.zw: UI units -> clip space
        float Scale, Time;
        float Res[ 2 ];
    };

    struct Buffer
    {
        VkBuffer B = VK_NULL_HANDLE;
        VkDeviceMemory M = VK_NULL_HANDLE;
        void* Map = nullptr;
        VkDeviceSize Size = 0;
    };

    struct Frame
    {
        Buffer Vb, Ib, Ub;
        VkDescriptorSet UboSet = VK_NULL_HANDLE;
    };

    struct Tex
    {
        VkImage Img = VK_NULL_HANDLE;
        VkDeviceMemory Mem = VK_NULL_HANDLE;
        VkImageView View = VK_NULL_HANDLE;
        VkDescriptorSet Set = VK_NULL_HANDLE;
        VkFramebuffer Fb = VK_NULL_HANDLE; // render targets
        int W = 0, H = 0;
        bool Target = false;
    };

#if defined( TG_VULKAN_SHADERC )
    const char* CustomPrefix = R"(#version 450
layout( location = 0 ) in vec2 vUV;
layout( location = 1 ) in vec4 vCol;
layout( location = 2 ) in vec2 vPos;
layout( set = 0, binding = 0 ) uniform sampler2D uTex;
layout( set = 1, binding = 0 ) uniform TgParams { vec4 Params[ 8 ]; };
layout( push_constant ) uniform PC { vec4 uProj; float uScale; float Time; vec2 Resolution; };
layout( location = 0 ) out vec4 oCol;
vec4 tex( vec2 uv ) { return texture( uTex, uv ); }
#line 1
)";
#endif
}

namespace tg
{
    struct VulkanRenderer::Impl
    {
        InitInfo I;
        bool Ready = false, OwnDevice = false;
        VkPhysicalDeviceMemoryProperties MemProps{};
        VkPhysicalDeviceProperties Props{};
        VkCommandPool Pool = VK_NULL_HANDLE;
        VkSampler Sampler = VK_NULL_HANDLE;
        VkDescriptorSetLayout TexLayout = VK_NULL_HANDLE, UboLayout = VK_NULL_HANDLE;
        VkPipelineLayout Layout = VK_NULL_HANDLE;
        VkDescriptorPool DescPool = VK_NULL_HANDLE;
        VkShaderModule Vs = VK_NULL_HANDLE, Fs = VK_NULL_HANDLE;
        std::unordered_map<ShaderId, VkShaderModule> Custom;
        ShaderId NextShader = 1;
        std::map<std::pair<VkRenderPass, ShaderId>, VkPipeline> Pipelines;
        std::unordered_map<TextureId, Tex> Textures;
        TextureId NextTex = 1, White = 0;
        VkRenderPass TargetPass = VK_NULL_HANDLE;
        std::vector<Frame> Frames;
        Frame Aux; // render targets, headless frames (synchronous)
        VkCommandBuffer CurCb = VK_NULL_HANDLE;
        uint32_t CurSlot = 0;
        // headless
        VkRenderPass HeadPass = VK_NULL_HANDLE;
        Tex Head;
        Buffer Readback;
        VkFence Fence = VK_NULL_HANDLE;

        VkDevice Dev() const { return I.Device; }

        uint32_t MemType( uint32_t Bits, VkMemoryPropertyFlags Want )
        {
            for ( uint32_t i = 0; i < MemProps.memoryTypeCount; i++ )
                if ( ( Bits & ( 1u << i ) ) && ( MemProps.memoryTypes[ i ].propertyFlags & Want ) == Want ) return i;
            return UINT32_MAX;
        }

        bool MakeBuffer( Buffer& B, VkDeviceSize Size, VkBufferUsageFlags Usage )
        {
            FreeBuffer( B );
            VkBufferCreateInfo Bi{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
            Bi.size = Size;
            Bi.usage = Usage;
            Bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            if ( vkCreateBuffer( Dev(), &Bi, nullptr, &B.B ) != VK_SUCCESS ) return false;
            VkMemoryRequirements Req;
            vkGetBufferMemoryRequirements( Dev(), B.B, &Req );
            VkMemoryAllocateInfo Ai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
            Ai.allocationSize = Req.size;
            Ai.memoryTypeIndex = MemType( Req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
            if ( Ai.memoryTypeIndex == UINT32_MAX || vkAllocateMemory( Dev(), &Ai, nullptr, &B.M ) != VK_SUCCESS ) return false;
            vkBindBufferMemory( Dev(), B.B, B.M, 0 );
            vkMapMemory( Dev(), B.M, 0, VK_WHOLE_SIZE, 0, &B.Map );
            B.Size = Size;
            return true;
        }

        void FreeBuffer( Buffer& B )
        {
            if ( B.Map ) vkUnmapMemory( Dev(), B.M );
            if ( B.B ) vkDestroyBuffer( Dev(), B.B, nullptr );
            if ( B.M ) vkFreeMemory( Dev(), B.M, nullptr );
            B = Buffer{};
        }

        bool Grow( Buffer& B, VkDeviceSize Need, VkBufferUsageFlags Usage )
        {
            if ( B.Size >= Need ) return true;
            VkDeviceSize S = std::max<VkDeviceSize>( 64 * 1024, B.Size );
            while ( S < Need ) S *= 2;
            return MakeBuffer( B, S, Usage );
        }

        VkShaderModule Module( const uint32_t* Code, size_t Bytes )
        {
            VkShaderModuleCreateInfo Ci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
            Ci.codeSize = Bytes;
            Ci.pCode = Code;
            VkShaderModule M = VK_NULL_HANDLE;
            vkCreateShaderModule( Dev(), &Ci, nullptr, &M );
            return M;
        }

        VkCommandBuffer BeginOnce()
        {
            VkCommandBufferAllocateInfo Ai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
            Ai.commandPool = Pool;
            Ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            Ai.commandBufferCount = 1;
            VkCommandBuffer Cb = VK_NULL_HANDLE;
            vkAllocateCommandBuffers( Dev(), &Ai, &Cb );
            VkCommandBufferBeginInfo Bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            Bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer( Cb, &Bi );
            return Cb;
        }

        void EndOnce( VkCommandBuffer Cb )
        {
            vkEndCommandBuffer( Cb );
            VkSubmitInfo Si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
            Si.commandBufferCount = 1;
            Si.pCommandBuffers = &Cb;
            vkResetFences( Dev(), 1, &Fence );
            vkQueueSubmit( I.Queue, 1, &Si, Fence );
            vkWaitForFences( Dev(), 1, &Fence, VK_TRUE, UINT64_MAX );
            vkFreeCommandBuffers( Dev(), Pool, 1, &Cb );
        }

        static void Barrier( VkCommandBuffer Cb, VkImage Img, VkImageLayout From, VkImageLayout To, VkAccessFlags SrcA, VkAccessFlags DstA,
                             VkPipelineStageFlags SrcS, VkPipelineStageFlags DstS )
        {
            VkImageMemoryBarrier B{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            B.oldLayout = From;
            B.newLayout = To;
            B.srcAccessMask = SrcA;
            B.dstAccessMask = DstA;
            B.srcQueueFamilyIndex = B.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            B.image = Img;
            B.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            vkCmdPipelineBarrier( Cb, SrcS, DstS, 0, 0, nullptr, 0, nullptr, 1, &B );
        }

        bool MakeImage( Tex& T, int W, int H, VkImageUsageFlags Usage )
        {
            VkImageCreateInfo Ii{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
            Ii.imageType = VK_IMAGE_TYPE_2D;
            Ii.format = VK_FORMAT_R8G8B8A8_UNORM;
            Ii.extent = { uint32_t( W ), uint32_t( H ), 1 };
            Ii.mipLevels = 1;
            Ii.arrayLayers = 1;
            Ii.samples = VK_SAMPLE_COUNT_1_BIT;
            Ii.tiling = VK_IMAGE_TILING_OPTIMAL;
            Ii.usage = Usage;
            Ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            Ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            if ( vkCreateImage( Dev(), &Ii, nullptr, &T.Img ) != VK_SUCCESS ) return false;
            VkMemoryRequirements Req;
            vkGetImageMemoryRequirements( Dev(), T.Img, &Req );
            VkMemoryAllocateInfo Ai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
            Ai.allocationSize = Req.size;
            Ai.memoryTypeIndex = MemType( Req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
            if ( Ai.memoryTypeIndex == UINT32_MAX ) Ai.memoryTypeIndex = MemType( Req.memoryTypeBits, 0 );
            if ( vkAllocateMemory( Dev(), &Ai, nullptr, &T.Mem ) != VK_SUCCESS ) return false;
            vkBindImageMemory( Dev(), T.Img, T.Mem, 0 );
            VkImageViewCreateInfo Vi{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            Vi.image = T.Img;
            Vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
            Vi.format = VK_FORMAT_R8G8B8A8_UNORM;
            Vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            if ( vkCreateImageView( Dev(), &Vi, nullptr, &T.View ) != VK_SUCCESS ) return false;
            T.W = W;
            T.H = H;
            return true;
        }

        void FreeImage( Tex& T )
        {
            if ( T.Fb ) vkDestroyFramebuffer( Dev(), T.Fb, nullptr );
            if ( T.View ) vkDestroyImageView( Dev(), T.View, nullptr );
            if ( T.Img ) vkDestroyImage( Dev(), T.Img, nullptr );
            if ( T.Mem ) vkFreeMemory( Dev(), T.Mem, nullptr );
            T.Fb = VK_NULL_HANDLE;
            T.View = VK_NULL_HANDLE;
            T.Img = VK_NULL_HANDLE;
            T.Mem = VK_NULL_HANDLE;
        }

        void WriteTexSet( Tex& T )
        {
            if ( !T.Set )
            {
                VkDescriptorSetAllocateInfo Ai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
                Ai.descriptorPool = DescPool;
                Ai.descriptorSetCount = 1;
                Ai.pSetLayouts = &TexLayout;
                vkAllocateDescriptorSets( Dev(), &Ai, &T.Set );
            }
            VkDescriptorImageInfo Img{ Sampler, T.View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
            VkWriteDescriptorSet W{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            W.dstSet = T.Set;
            W.descriptorCount = 1;
            W.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            W.pImageInfo = &Img;
            vkUpdateDescriptorSets( Dev(), 1, &W, 0, nullptr );
        }

        void Upload( Tex& T, const void* Pixels )
        {
            size_t Bytes = size_t( T.W ) * T.H * 4;
            Buffer Stage;
            if ( !MakeBuffer( Stage, Bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT ) ) return;
            if ( Pixels ) std::memcpy( Stage.Map, Pixels, Bytes );
            else std::memset( Stage.Map, 0, Bytes );
            VkCommandBuffer Cb = BeginOnce();
            Barrier( Cb, T.Img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );
            VkBufferImageCopy R{};
            R.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            R.imageExtent = { uint32_t( T.W ), uint32_t( T.H ), 1 };
            vkCmdCopyBufferToImage( Cb, Stage.B, T.Img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &R );
            Barrier( Cb, T.Img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
                     VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
            EndOnce( Cb );
            FreeBuffer( Stage );
        }

        VkRenderPass MakePass( VkImageLayout Final )
        {
            VkAttachmentDescription A{};
            A.format = VK_FORMAT_R8G8B8A8_UNORM;
            A.samples = VK_SAMPLE_COUNT_1_BIT;
            A.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            A.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            A.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            A.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            A.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            A.finalLayout = Final;
            VkAttachmentReference Ref{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
            VkSubpassDescription Sp{};
            Sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            Sp.colorAttachmentCount = 1;
            Sp.pColorAttachments = &Ref;
            VkSubpassDependency Dep[ 2 ] = {};
            Dep[ 0 ] = { VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT,
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0 };
            Dep[ 1 ] = { 0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                         VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT, 0 };
            VkRenderPassCreateInfo Ci{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
            Ci.attachmentCount = 1;
            Ci.pAttachments = &A;
            Ci.subpassCount = 1;
            Ci.pSubpasses = &Sp;
            Ci.dependencyCount = 2;
            Ci.pDependencies = Dep;
            VkRenderPass P = VK_NULL_HANDLE;
            vkCreateRenderPass( Dev(), &Ci, nullptr, &P );
            return P;
        }

        bool MakeFramebuffer( Tex& T, VkRenderPass Pass )
        {
            VkFramebufferCreateInfo Fi{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
            Fi.renderPass = Pass;
            Fi.attachmentCount = 1;
            Fi.pAttachments = &T.View;
            Fi.width = uint32_t( T.W );
            Fi.height = uint32_t( T.H );
            Fi.layers = 1;
            return vkCreateFramebuffer( Dev(), &Fi, nullptr, &T.Fb ) == VK_SUCCESS;
        }

        VkPipeline Pipeline( VkRenderPass Pass, ShaderId S )
        {
            auto Key = std::make_pair( Pass, S );
            auto It = Pipelines.find( Key );
            if ( It != Pipelines.end() ) return It->second;
            VkShaderModule Frag = Fs;
            if ( S )
            {
                auto C = Custom.find( S );
                if ( C == Custom.end() ) return Pipeline( Pass, 0 );
                Frag = C->second;
            }
            VkPipelineShaderStageCreateInfo St[ 2 ] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
                                                        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
            St[ 0 ].stage = VK_SHADER_STAGE_VERTEX_BIT;
            St[ 0 ].module = Vs;
            St[ 0 ].pName = "main";
            St[ 1 ].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            St[ 1 ].module = Frag;
            St[ 1 ].pName = "main";
            VkVertexInputBindingDescription Bind{ 0, sizeof( Vertex ), VK_VERTEX_INPUT_RATE_VERTEX };
            VkVertexInputAttributeDescription Attr[ 3 ] = { { 0, 0, VK_FORMAT_R32G32_SFLOAT, uint32_t( offsetof( Vertex, Pos ) ) },
                                                            { 1, 0, VK_FORMAT_R32G32_SFLOAT, uint32_t( offsetof( Vertex, UV ) ) },
                                                            { 2, 0, VK_FORMAT_R8G8B8A8_UNORM, uint32_t( offsetof( Vertex, Col ) ) } };
            VkPipelineVertexInputStateCreateInfo Vi{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
            Vi.vertexBindingDescriptionCount = 1;
            Vi.pVertexBindingDescriptions = &Bind;
            Vi.vertexAttributeDescriptionCount = 3;
            Vi.pVertexAttributeDescriptions = Attr;
            VkPipelineInputAssemblyStateCreateInfo Ia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
            Ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineViewportStateCreateInfo Vp{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
            Vp.viewportCount = 1;
            Vp.scissorCount = 1;
            VkPipelineRasterizationStateCreateInfo Rs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
            Rs.polygonMode = VK_POLYGON_MODE_FILL;
            Rs.cullMode = VK_CULL_MODE_NONE;
            Rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            Rs.lineWidth = 1;
            VkPipelineMultisampleStateCreateInfo Ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
            Ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            VkPipelineColorBlendAttachmentState Blend{};
            Blend.blendEnable = VK_TRUE;
            Blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; // same as the GL / D3D renderers
            Blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            Blend.colorBlendOp = VK_BLEND_OP_ADD;
            Blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            Blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            Blend.alphaBlendOp = VK_BLEND_OP_ADD;
            Blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            VkPipelineColorBlendStateCreateInfo Cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
            Cb.attachmentCount = 1;
            Cb.pAttachments = &Blend;
            VkDynamicState Dyn[ 2 ] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
            VkPipelineDynamicStateCreateInfo Ds{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
            Ds.dynamicStateCount = 2;
            Ds.pDynamicStates = Dyn;
            VkGraphicsPipelineCreateInfo Pi{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
            Pi.stageCount = 2;
            Pi.pStages = St;
            Pi.pVertexInputState = &Vi;
            Pi.pInputAssemblyState = &Ia;
            Pi.pViewportState = &Vp;
            Pi.pRasterizationState = &Rs;
            Pi.pMultisampleState = &Ms;
            Pi.pColorBlendState = &Cb;
            Pi.pDynamicState = &Ds;
            Pi.layout = Layout;
            Pi.renderPass = Pass;
            VkPipeline Pl = VK_NULL_HANDLE;
            if ( vkCreateGraphicsPipelines( Dev(), VK_NULL_HANDLE, 1, &Pi, nullptr, &Pl ) != VK_SUCCESS )
            {
                std::fprintf( stderr, "tg vulkan: pipeline creation failed\n" );
                return S ? Pipeline( Pass, 0 ) : VK_NULL_HANDLE;
            }
            Pipelines[ Key ] = Pl;
            return Pl;
        }

        bool Setup()
        {
            vkGetPhysicalDeviceMemoryProperties( I.Physical, &MemProps );
            vkGetPhysicalDeviceProperties( I.Physical, &Props );
            VkCommandPoolCreateInfo Pc{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
            Pc.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            Pc.queueFamilyIndex = I.QueueFamily;
            if ( vkCreateCommandPool( Dev(), &Pc, nullptr, &Pool ) != VK_SUCCESS ) return false;
            VkFenceCreateInfo Fc{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
            vkCreateFence( Dev(), &Fc, nullptr, &Fence );
            VkSamplerCreateInfo Sc{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
            Sc.magFilter = Sc.minFilter = VK_FILTER_LINEAR;
            Sc.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            Sc.addressModeU = Sc.addressModeV = Sc.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            Sc.maxLod = 0.25f;
            if ( vkCreateSampler( Dev(), &Sc, nullptr, &Sampler ) != VK_SUCCESS ) return false;

            VkDescriptorSetLayoutBinding Tb{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
            VkDescriptorSetLayoutCreateInfo Lc{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
            Lc.bindingCount = 1;
            Lc.pBindings = &Tb;
            vkCreateDescriptorSetLayout( Dev(), &Lc, nullptr, &TexLayout );
            VkDescriptorSetLayoutBinding Ub{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
            Lc.pBindings = &Ub;
            vkCreateDescriptorSetLayout( Dev(), &Lc, nullptr, &UboLayout );
            VkDescriptorSetLayout Sets[ 2 ] = { TexLayout, UboLayout };
            VkPushConstantRange Pr{ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( PushConst ) };
            VkPipelineLayoutCreateInfo Pl{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
            Pl.setLayoutCount = 2;
            Pl.pSetLayouts = Sets;
            Pl.pushConstantRangeCount = 1;
            Pl.pPushConstantRanges = &Pr;
            if ( vkCreatePipelineLayout( Dev(), &Pl, nullptr, &Layout ) != VK_SUCCESS ) return false;
            VkDescriptorPoolSize Sizes[ 2 ] = { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096 }, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 64 } };
            VkDescriptorPoolCreateInfo Dp{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
            Dp.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            Dp.maxSets = 4096 + 64;
            Dp.poolSizeCount = 2;
            Dp.pPoolSizes = Sizes;
            if ( vkCreateDescriptorPool( Dev(), &Dp, nullptr, &DescPool ) != VK_SUCCESS ) return false;
            Vs = Module( Spv_tg_vert, sizeof( Spv_tg_vert ) );
            Fs = Module( Spv_tg_frag, sizeof( Spv_tg_frag ) );
            if ( !Vs || !Fs ) return false;
            TargetPass = MakePass( VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
            Frames.resize( std::max( 1u, I.FramesInFlight ) );
            for ( Frame& F : Frames ) MakeUboSet( F );
            MakeUboSet( Aux );
            Ready = true;
            const uint32_t W = 0xFFFFFFFFu;
            Tex T;
            if ( MakeImage( T, 1, 1, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT ) ) // for draws without a texture
            {
                Upload( T, &W );
                WriteTexSet( T );
                White = NextTex++;
                Textures[ White ] = T;
            }
            return true;
        }

        void MakeUboSet( Frame& F )
        {
            Grow( F.Ub, 64 * 1024, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT );
            VkDescriptorSetAllocateInfo Ai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
            Ai.descriptorPool = DescPool;
            Ai.descriptorSetCount = 1;
            Ai.pSetLayouts = &UboLayout;
            vkAllocateDescriptorSets( Dev(), &Ai, &F.UboSet );
            PointUbo( F );
        }

        void PointUbo( Frame& F )
        {
            VkDescriptorBufferInfo Bi{ F.Ub.B, 0, 32 * sizeof( float ) };
            VkWriteDescriptorSet W{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            W.dstSet = F.UboSet;
            W.descriptorCount = 1;
            W.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            W.pBufferInfo = &Bi;
            vkUpdateDescriptorSets( Dev(), 1, &W, 0, nullptr );
        }

        // Record a DrawList into Cb, inside a begun render pass Pass of size Fb (pixels).
        void Record( VkCommandBuffer Cb, const DrawList& L, Vec2 Fb, float Scale, VkRenderPass Pass, Frame& F )
        {
            if ( L.Idx.empty() || L.Vtx.empty() || Fb.x <= 0 || Fb.y <= 0 ) return;
            Grow( F.Vb, L.Vtx.size() * sizeof( Vertex ), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT );
            Grow( F.Ib, L.Idx.size() * sizeof( uint32_t ), VK_BUFFER_USAGE_INDEX_BUFFER_BIT );
            std::memcpy( F.Vb.Map, L.Vtx.data(), L.Vtx.size() * sizeof( Vertex ) );
            std::memcpy( F.Ib.Map, L.Idx.data(), L.Idx.size() * sizeof( uint32_t ) );
            // shader params: one 128-byte block per custom-shaded draw, at the device's UBO alignment
            VkDeviceSize Align = std::max<VkDeviceSize>( Props.limits.minUniformBufferOffsetAlignment, 16 );
            VkDeviceSize Block = ( 128 + Align - 1 ) / Align * Align;
            size_t Shaded = 0;
            for ( const DrawCmd& C : L.Cmds ) Shaded += C.Shader != 0;
            if ( Shaded && F.Ub.Size < Shaded * Block )
            {
                Grow( F.Ub, Shaded * Block, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT );
                PointUbo( F );
            }

            VkViewport Vp{ 0, 0, Fb.x, Fb.y, 0, 1 };
            vkCmdSetViewport( Cb, 0, 1, &Vp );
            VkDeviceSize Zero = 0;
            vkCmdBindVertexBuffers( Cb, 0, 1, &F.Vb.B, &Zero );
            vkCmdBindIndexBuffer( Cb, F.Ib.B, 0, VK_INDEX_TYPE_UINT32 );
            float W = Fb.x / Scale, H = Fb.y / Scale;
            PushConst Pc{ { 2.f / W, 2.f / H, -1.f, -1.f }, Scale, L.Time, { Fb.x, Fb.y } };
            vkCmdPushConstants( Cb, Layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( Pc ), &Pc );

            VkPipeline Bound = VK_NULL_HANDLE;
            VkDescriptorSet BoundTex = VK_NULL_HANDLE;
            size_t Slot = 0;
            for ( const DrawCmd& C : L.Cmds )
            {
                if ( C.Kind != DrawCmd::Draw || !C.IdxCount ) continue; // (backdrop blur isn't supported here)
                int X0 = std::max( 0, int( C.Clip.x * Scale ) ), Y0 = std::max( 0, int( C.Clip.y * Scale ) );
                int X1 = std::min( int( Fb.x ), int( C.Clip.x * Scale + C.Clip.w * Scale + 0.5f ) );
                int Y1 = std::min( int( Fb.y ), int( C.Clip.y * Scale + C.Clip.h * Scale + 0.5f ) );
                if ( X1 <= X0 || Y1 <= Y0 ) continue;
                VkRect2D Sc{ { X0, Y0 }, { uint32_t( X1 - X0 ), uint32_t( Y1 - Y0 ) } };
                vkCmdSetScissor( Cb, 0, 1, &Sc );
                ShaderId S = C.Shader && Custom.count( C.Shader ) && C.ParamOffset + 32 <= L.Params.size() ? C.Shader : 0;
                VkPipeline Pl = Pipeline( Pass, S );
                if ( !Pl ) continue;
                if ( Pl != Bound )
                {
                    vkCmdBindPipeline( Cb, VK_PIPELINE_BIND_POINT_GRAPHICS, Pl );
                    Bound = Pl;
                }
                auto Tx = Textures.find( C.Texture );
                if ( Tx == Textures.end() ) Tx = Textures.find( White );
                if ( Tx != Textures.end() && Tx->second.Set != BoundTex )
                {
                    vkCmdBindDescriptorSets( Cb, VK_PIPELINE_BIND_POINT_GRAPHICS, Layout, 0, 1, &Tx->second.Set, 0, nullptr );
                    BoundTex = Tx->second.Set;
                }
                if ( S )
                {
                    uint32_t Off = uint32_t( Slot++ * Block );
                    std::memcpy( (char*)F.Ub.Map + Off, L.Params.data() + C.ParamOffset, 32 * sizeof( float ) );
                    vkCmdBindDescriptorSets( Cb, VK_PIPELINE_BIND_POINT_GRAPHICS, Layout, 1, 1, &F.UboSet, 1, &Off );
                }
                vkCmdDrawIndexed( Cb, C.IdxCount, 1, C.IdxOffset, 0, 0 );
            }
        }

        void Destroy()
        {
            if ( !Dev() ) return;
            vkDeviceWaitIdle( Dev() );
            for ( auto& [ K, Pl ] : Pipelines ) vkDestroyPipeline( Dev(), Pl, nullptr );
            Pipelines.clear();
            for ( auto& [ K, M ] : Custom ) vkDestroyShaderModule( Dev(), M, nullptr );
            Custom.clear();
            for ( auto& [ K, T ] : Textures ) FreeImage( T );
            Textures.clear();
            FreeImage( Head );
            FreeBuffer( Readback );
            for ( Frame& F : Frames ) { FreeBuffer( F.Vb ); FreeBuffer( F.Ib ); FreeBuffer( F.Ub ); }
            Frames.clear();
            FreeBuffer( Aux.Vb );
            FreeBuffer( Aux.Ib );
            FreeBuffer( Aux.Ub );
            if ( Vs ) vkDestroyShaderModule( Dev(), Vs, nullptr );
            if ( Fs ) vkDestroyShaderModule( Dev(), Fs, nullptr );
            if ( TargetPass ) vkDestroyRenderPass( Dev(), TargetPass, nullptr );
            if ( HeadPass ) vkDestroyRenderPass( Dev(), HeadPass, nullptr );
            if ( DescPool ) vkDestroyDescriptorPool( Dev(), DescPool, nullptr );
            if ( Layout ) vkDestroyPipelineLayout( Dev(), Layout, nullptr );
            if ( TexLayout ) vkDestroyDescriptorSetLayout( Dev(), TexLayout, nullptr );
            if ( UboLayout ) vkDestroyDescriptorSetLayout( Dev(), UboLayout, nullptr );
            if ( Sampler ) vkDestroySampler( Dev(), Sampler, nullptr );
            if ( Fence ) vkDestroyFence( Dev(), Fence, nullptr );
            if ( Pool ) vkDestroyCommandPool( Dev(), Pool, nullptr );
            if ( OwnDevice )
            {
                vkDestroyDevice( I.Device, nullptr );
                vkDestroyInstance( I.Instance, nullptr );
            }
            *this = Impl{};
        }
    };

    VulkanRenderer::VulkanRenderer() : P( std::make_unique<Impl>() ) {}
    VulkanRenderer::~VulkanRenderer() { Shutdown(); }

    bool VulkanRenderer::Init( const InitInfo& Info )
    {
        Shutdown();
        P->I = Info;
        if ( !Info.Device || !Info.Physical || !Info.Queue ) return false;
        return P->Setup();
    }

    bool VulkanRenderer::InitHeadless( bool PreferSoftware )
    {
        Shutdown();
        VkApplicationInfo App{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
        App.pApplicationName = "tg";
        App.apiVersion = VK_API_VERSION_1_0;
        VkInstanceCreateInfo Ic{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        Ic.pApplicationInfo = &App;
        VkInstance Inst = VK_NULL_HANDLE;
        if ( vkCreateInstance( &Ic, nullptr, &Inst ) != VK_SUCCESS ) return false;
        uint32_t N = 0;
        vkEnumeratePhysicalDevices( Inst, &N, nullptr );
        std::vector<VkPhysicalDevice> Devs( N );
        vkEnumeratePhysicalDevices( Inst, &N, Devs.data() );
        VkPhysicalDevice Best = VK_NULL_HANDLE;
        uint32_t BestFamily = 0;
        int BestScore = -1;
        for ( VkPhysicalDevice D : Devs )
        {
            uint32_t Qn = 0;
            vkGetPhysicalDeviceQueueFamilyProperties( D, &Qn, nullptr );
            std::vector<VkQueueFamilyProperties> Qs( Qn );
            vkGetPhysicalDeviceQueueFamilyProperties( D, &Qn, Qs.data() );
            for ( uint32_t q = 0; q < Qn; q++ )
            {
                if ( !( Qs[ q ].queueFlags & VK_QUEUE_GRAPHICS_BIT ) ) continue;
                VkPhysicalDeviceProperties Pr;
                vkGetPhysicalDeviceProperties( D, &Pr );
                int Score = Pr.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 3 : Pr.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2 : 1;
                if ( PreferSoftware ) Score = Pr.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU ? 4 : Score;
                if ( Score > BestScore ) { BestScore = Score; Best = D; BestFamily = q; }
                break;
            }
        }
        if ( !Best ) { vkDestroyInstance( Inst, nullptr ); return false; }
        float Prio = 1;
        VkDeviceQueueCreateInfo Qc{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        Qc.queueFamilyIndex = BestFamily;
        Qc.queueCount = 1;
        Qc.pQueuePriorities = &Prio;
        VkDeviceCreateInfo Dc{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        Dc.queueCreateInfoCount = 1;
        Dc.pQueueCreateInfos = &Qc;
        VkDevice Dev = VK_NULL_HANDLE;
        if ( vkCreateDevice( Best, &Dc, nullptr, &Dev ) != VK_SUCCESS ) { vkDestroyInstance( Inst, nullptr ); return false; }
        InitInfo I;
        I.Instance = Inst;
        I.Physical = Best;
        I.Device = Dev;
        I.QueueFamily = BestFamily;
        vkGetDeviceQueue( Dev, BestFamily, 0, &I.Queue );
        I.FramesInFlight = 1;
        P->I = I;
        P->OwnDevice = true;
        if ( !P->Setup() ) { Shutdown(); return false; }
        P->HeadPass = P->MakePass( VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL );
        return P->HeadPass != VK_NULL_HANDLE;
    }

    void VulkanRenderer::Shutdown() { P->Destroy(); }

    const char* VulkanRenderer::DeviceName() const { return P->Ready ? P->Props.deviceName : ""; }

    void VulkanRenderer::BeginFrame( VkCommandBuffer Cb, uint32_t Slot )
    {
        P->CurCb = Cb;
        P->CurSlot = P->Frames.empty() ? 0 : Slot % uint32_t( P->Frames.size() );
    }

    bool VulkanRenderer::CanCapture() const { return P->HeadPass != VK_NULL_HANDLE; }

    bool VulkanRenderer::CanShade() const
    {
#if defined( TG_VULKAN_SHADERC )
        return P->Ready;
#else
        return false;
#endif
    }

    TextureId VulkanRenderer::CreateTexture( int W, int H, const void* Pixels )
    {
        if ( !P->Ready || W <= 0 || H <= 0 ) return 0;
        Tex T;
        if ( !P->MakeImage( T, W, H, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT ) ) { P->FreeImage( T ); return 0; }
        P->Upload( T, Pixels );
        P->WriteTexSet( T );
        TextureId Id = P->NextTex++;
        P->Textures[ Id ] = T;
        return Id;
    }

    void VulkanRenderer::UpdateTexture( TextureId Id, int W, int H, const void* Pixels )
    {
        auto It = P->Textures.find( Id );
        if ( It == P->Textures.end() ) return;
        Tex& T = It->second;
        vkQueueWaitIdle( P->I.Queue ); // frames in flight may still sample it
        if ( T.W != W || T.H != H )
        {
            P->FreeImage( T );
            if ( !P->MakeImage( T, W, H, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT ) ) return;
            P->WriteTexSet( T );
        }
        P->Upload( T, Pixels );
    }

    void VulkanRenderer::DestroyTexture( TextureId Id )
    {
        auto It = P->Textures.find( Id );
        if ( It == P->Textures.end() || Id == P->White ) return;
        vkQueueWaitIdle( P->I.Queue );
        if ( It->second.Set ) vkFreeDescriptorSets( P->Dev(), P->DescPool, 1, &It->second.Set );
        P->FreeImage( It->second );
        P->Textures.erase( It );
    }

    ShaderId VulkanRenderer::CreateShader( const ShaderDesc& D )
    {
#if defined( TG_VULKAN_SHADERC )
        if ( !P->Ready || D.Glsl.empty() ) return 0;
        std::string Src = std::string( CustomPrefix ) + D.Glsl + "\nvoid main() { oCol = effect( vCol, vUV, vPos ); }\n";
        shaderc_compiler_t Comp = shaderc_compiler_initialize();
        shaderc_compilation_result_t R = shaderc_compile_into_spv( Comp, Src.c_str(), Src.size(), shaderc_fragment_shader, "tg_effect.frag", "main", nullptr );
        ShaderId Id = 0;
        if ( shaderc_result_get_compilation_status( R ) == shaderc_compilation_status_success )
        {
            VkShaderModule M = P->Module( (const uint32_t*)shaderc_result_get_bytes( R ), shaderc_result_get_length( R ) );
            if ( M )
            {
                Id = P->NextShader++;
                P->Custom[ Id ] = M;
            }
        }
        else std::fprintf( stderr, "tg vulkan: shader error: %s\n", shaderc_result_get_error_message( R ) );
        shaderc_result_release( R );
        shaderc_compiler_release( Comp );
        return Id;
#else
        (void)D;
        return 0;
#endif
    }

    void VulkanRenderer::DestroyShader( ShaderId S )
    {
        auto It = P->Custom.find( S );
        if ( It == P->Custom.end() ) return;
        vkDeviceWaitIdle( P->Dev() );
        for ( auto Pl = P->Pipelines.begin(); Pl != P->Pipelines.end(); )
        {
            if ( Pl->first.second == S ) { vkDestroyPipeline( P->Dev(), Pl->second, nullptr ); Pl = P->Pipelines.erase( Pl ); }
            else ++Pl;
        }
        vkDestroyShaderModule( P->Dev(), It->second, nullptr );
        P->Custom.erase( It );
    }

    TextureId VulkanRenderer::CreateRenderTarget( int W, int H )
    {
        if ( !P->Ready || W <= 0 || H <= 0 ) return 0;
        Tex T;
        T.Target = true;
        if ( !P->MakeImage( T, W, H, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT ) ||
             !P->MakeFramebuffer( T, P->TargetPass ) )
        {
            P->FreeImage( T );
            return 0;
        }
        P->Upload( T, nullptr ); // cleared and in SHADER_READ_ONLY layout until something renders into it
        P->WriteTexSet( T );
        TextureId Id = P->NextTex++;
        P->Textures[ Id ] = T;
        return Id;
    }

    static void RunPass( VulkanRenderer::Impl& I, VkCommandBuffer Cb, VkRenderPass Pass, VkFramebuffer Fb, int W, int H )
    {
        VkClearValue Clear{};
        VkRenderPassBeginInfo Rb{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        Rb.renderPass = Pass;
        Rb.framebuffer = Fb;
        Rb.renderArea = { { 0, 0 }, { uint32_t( W ), uint32_t( H ) } };
        Rb.clearValueCount = 1;
        Rb.pClearValues = &Clear;
        vkCmdBeginRenderPass( Cb, &Rb, VK_SUBPASS_CONTENTS_INLINE );
        (void)I;
    }

    void VulkanRenderer::RenderTo( TextureId Id, const DrawList& L, Vec2 Size, float Scale )
    {
        auto It = P->Textures.find( Id );
        if ( It == P->Textures.end() || !It->second.Target ) return;
        Tex& T = It->second;
        int W = int( Size.x ), H = int( Size.y );
        if ( W <= 0 || H <= 0 ) return;
        vkQueueWaitIdle( P->I.Queue );
        if ( T.W != W || T.H != H )
        {
            P->FreeImage( T );
            if ( !P->MakeImage( T, W, H, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT ) ||
                 !P->MakeFramebuffer( T, P->TargetPass ) )
                return;
            P->WriteTexSet( T );
        }
        VkCommandBuffer Cb = P->BeginOnce();
        RunPass( *P, Cb, P->TargetPass, T.Fb, W, H );
        P->Record( Cb, L, Size, Scale, P->TargetPass, P->Aux );
        vkCmdEndRenderPass( Cb );
        P->EndOnce( Cb );
    }

    void VulkanRenderer::Render( const DrawList& L, Vec2 Fb, float Scale )
    {
        if ( !P->Ready || Fb.x <= 0 || Fb.y <= 0 ) return;
        if ( !P->HeadPass ) // embedded: record into the app's command buffer / render pass
        {
            if ( P->CurCb ) P->Record( P->CurCb, L, Fb, Scale, P->I.RenderPass, P->Frames[ P->CurSlot ] );
            return;
        }
        // headless: our own image, rendered and read back synchronously
        int W = int( Fb.x ), H = int( Fb.y );
        Tex& Hd = P->Head;
        if ( Hd.W != W || Hd.H != H || !Hd.Img )
        {
            vkQueueWaitIdle( P->I.Queue );
            P->FreeImage( Hd );
            if ( !P->MakeImage( Hd, W, H, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT ) || !P->MakeFramebuffer( Hd, P->HeadPass ) )
                return;
            P->MakeBuffer( P->Readback, VkDeviceSize( W ) * H * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT );
        }
        VkCommandBuffer Cb = P->BeginOnce();
        RunPass( *P, Cb, P->HeadPass, Hd.Fb, W, H );
        P->Record( Cb, L, Fb, Scale, P->HeadPass, P->Frames[ 0 ] );
        vkCmdEndRenderPass( Cb );
        VkBufferImageCopy R{};
        R.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        R.imageExtent = { uint32_t( W ), uint32_t( H ), 1 };
        vkCmdCopyImageToBuffer( Cb, Hd.Img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, P->Readback.B, 1, &R );
        VkBufferMemoryBarrier Bb{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
        Bb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        Bb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        Bb.srcQueueFamilyIndex = Bb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        Bb.buffer = P->Readback.B;
        Bb.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier( Cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &Bb, 0, nullptr );
        P->EndOnce( Cb );
        CaptureNext = false;
        Captured.W = W;
        Captured.H = H;
        Captured.Px.resize( size_t( W ) * H );
        std::memcpy( Captured.Px.data(), P->Readback.Map, Captured.Px.size() * 4 );
    }
}
