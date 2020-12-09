
#include "ecs_demo.h"
//#include "ecs_particle_regular.cpp"
#include "ecs_particle_soa.cpp"

//
// NOTE: Main Demo Code
//

inline void DemoAllocGlobals(linear_arena* Arena)
{
    // IMPORTANT: These are always the top of the program memory
    // TODO: We might just want all these systems to handle this on their own?
    DemoState = PushStruct(Arena, demo_state);
    RenderState = PushStruct(Arena, render_state);
    ProfilerState = PushStruct(Arena, profiler_state);
}

DEMO_INIT(Init)
{
    // NOTE: Init Memory
    {
        // TODO: Add support for dynamic arenas
        linear_arena Arena = LinearArenaCreate(ProgramMemory, ProgramMemorySize);
        DemoAllocGlobals(&Arena);
        *DemoState = {};
        *RenderState = {};
        *ProfilerState = {};
        DemoState->Arena = Arena;
        DemoState->TempArena = LinearSubArena(&DemoState->Arena, MegaBytes(10));
    }

    ProfilerStateCreate(ProfilerFlag_OutputCsv | ProfilerFlag_AutoSetEndOfFrame);
        
    // NOTE: Init Vulkan
    {
        {
            render_init_params InitParams = {};
            InitParams.ValidationEnabled = true;
            InitParams.WindowWidth = WindowWidth;
            InitParams.WindowHeight = WindowHeight;
            // TODO: Dynamic staging buffer stuff, do a demo measuring speeds on this
            InitParams.StagingBufferSize = MegaBytes(400);
            // TODO: Add support for dynamic arenas, we probably want most things to use those anyways
            VkInit(VulkanLib, hInstance, WindowHandle, &DemoState->Arena, &DemoState->TempArena, InitParams);
        }
        
        // NOTE: Init descriptor pool
        {
            VkDescriptorPoolSize Pools[5] = {};
            Pools[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            Pools[0].descriptorCount = 1000;
            Pools[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            Pools[1].descriptorCount = 1000;
            Pools[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            Pools[2].descriptorCount = 1000;
            Pools[3].type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            Pools[3].descriptorCount = 1000;
            Pools[4].type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
            Pools[4].descriptorCount = 1000;
            
            VkDescriptorPoolCreateInfo CreateInfo = {};
            CreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            CreateInfo.maxSets = 1000;
            CreateInfo.poolSizeCount = ArrayCount(Pools);
            CreateInfo.pPoolSizes = Pools;
            VkCheckResult(vkCreateDescriptorPool(RenderState->Device, &CreateInfo, 0, &RenderState->DescriptorPool));
        }
    }
    
    // NOTE: Create samplers
    {
        VkSamplerCreateInfo CreateInfo = {};
        CreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        CreateInfo.magFilter = VK_FILTER_NEAREST;
        CreateInfo.minFilter = VK_FILTER_NEAREST;
        CreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        CreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        CreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        CreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        CreateInfo.mipLodBias = 0.0f;
        CreateInfo.anisotropyEnable = VK_FALSE;
        CreateInfo.compareEnable = VK_FALSE;
        CreateInfo.minLod = 0.0f;
        CreateInfo.maxLod = 0.0f;
        CreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        CreateInfo.unnormalizedCoordinates = VK_FALSE;
        VkCheckResult(vkCreateSampler(RenderState->Device, &CreateInfo, 0, &DemoState->PointSampler));

        CreateInfo.magFilter = VK_FILTER_LINEAR;
        CreateInfo.minFilter = VK_FILTER_LINEAR;
        CreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        VkCheckResult(vkCreateSampler(RenderState->Device, &CreateInfo, 0, &DemoState->LinearSampler));
    }
    
    DemoState->Camera = CameraFpsCreate(V3(0, 0, 0), V3(0, 0, 1), f32(RenderState->WindowWidth / RenderState->WindowHeight),
                                       0.001f, 1000.0f, 90.0f, 1.0f, 0.01f);
        
    // NOTE: Init render target entries
    DemoState->SwapChainEntry = RenderTargetSwapChainEntryCreate(RenderState->WindowWidth, RenderState->WindowHeight,
                                                                RenderState->SwapChainFormat);

    // NOTE: Demo RT
    {
        render_target_builder Builder = RenderTargetBuilderBegin(&DemoState->Arena, &DemoState->TempArena, WindowWidth, WindowHeight);
        RenderTargetAddTarget(&Builder, &DemoState->SwapChainEntry, VkClearColorCreate(0, 0, 0, 1));

        vk_render_pass_builder RpBuilder = VkRenderPassBuilderBegin(&DemoState->TempArena);

        u32 ColorId = VkRenderPassAttachmentAdd(&RpBuilder, DemoState->SwapChainEntry.Format, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED,
                                                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        VkRenderPassSubPassBegin(&RpBuilder, VK_PIPELINE_BIND_POINT_GRAPHICS);
        VkRenderPassColorRefAdd(&RpBuilder, ColorId, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderPassSubPassEnd(&RpBuilder);

        DemoState->RenderTarget = RenderTargetBuilderEnd(&Builder, VkRenderPassBuilderEnd(&RpBuilder, RenderState->Device));
    }
        
    // NOTE: Create Render Entity Data
    {
        // NOTE: Create descriptor set layout
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&DemoState->EntityDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        // NOTE: Create Pipeline
        {
            vk_pipeline_builder Builder = VkPipelineBuilderBegin(&DemoState->TempArena);

            // NOTE: Shaders
            VkPipelineVertexShaderAdd(&Builder, "shader_entity_vert.spv", "main");
            VkPipelineFragmentShaderAdd(&Builder, "shader_entity_frag.spv", "main");
                
            // NOTE: Specify input vertex data format
            VkPipelineVertexBindingBegin(&Builder);
            VkPipelineVertexAttributeAdd(&Builder, VK_FORMAT_R32_SFLOAT, sizeof(f32));
            VkPipelineVertexBindingEnd(&Builder);
            VkPipelineVertexBindingBegin(&Builder);
            VkPipelineVertexAttributeAdd(&Builder, VK_FORMAT_R32_SFLOAT, sizeof(f32));
            VkPipelineVertexBindingEnd(&Builder);
            VkPipelineVertexBindingBegin(&Builder);
            VkPipelineVertexAttributeAdd(&Builder, VK_FORMAT_R32_SFLOAT, sizeof(f32));
            VkPipelineVertexBindingEnd(&Builder);

            VkPipelineInputAssemblyAdd(&Builder, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_FALSE);
            
            // NOTE: Set the blending state
            VkPipelineColorAttachmentAdd(&Builder, VK_FALSE, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO,
                                         VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO);

            VkDescriptorSetLayout Layouts[] =
                {
                    DemoState->EntityDescLayout,
                };
            DemoState->EntityPipeline = VkPipelineBuilderEnd(&Builder, RenderState->Device, &RenderState->PipelineManager,
                                                             DemoState->RenderTarget.RenderPass, 0, Layouts, ArrayCount(Layouts));
        }        
    }

    vk_commands Commands = RenderState->Commands;
    VkCommandsBegin(RenderState->Device, Commands);

    ParticleManagerCreate(&DemoState->ParticleManager, V3(1.0f, 0.0f, 0.5f));

    VkDescriptorManagerFlush(RenderState->Device, &RenderState->DescriptorManager);
    VkTransferManagerFlush(&RenderState->TransferManager, RenderState->Device, RenderState->Commands.Buffer, &RenderState->BarrierManager);
    
    VkCommandsSubmit(RenderState->GraphicsQueue, Commands);
}

DEMO_DESTROY(Destroy)
{
    // TODO: Remove if we can verify that this is auto destroyed (check recompiling if it calls the destructor)
    ProfilerStateDestroy();
}

DEMO_CODE_RELOAD(CodeReload)
{
#if 0
    linear_arena Arena = LinearArenaCreate(ProgramMemory, ProgramMemorySize);
    // IMPORTANT: We are relying on the memory being the same here since we have the same base ptr with the VirtualAlloc so we just need
    // to patch our global pointers here
    DemoAllocGlobals(&Arena);

    VkGetGlobalFunctionPointers(VulkanLib);
    VkGetInstanceFunctionPointers();
    VkGetDeviceFunctionPointers();
#endif
}

DEMO_MAIN_LOOP(MainLoop)
{
    u32 ImageIndex;
    VkCheckResult(vkAcquireNextImageKHR(RenderState->Device, RenderState->SwapChain,
                                        UINT64_MAX, RenderState->ImageAvailableSemaphore,
                                        VK_NULL_HANDLE, &ImageIndex));
    DemoState->SwapChainEntry.View = RenderState->SwapChainViews[ImageIndex];

    CameraUpdate(&DemoState->Camera, CurrInput, PrevInput);

    vk_commands Commands = RenderState->Commands;
    VkCommandsBegin(RenderState->Device, Commands);

    // NOTE: Update pipelines
    VkPipelineUpdateShaders(RenderState->Device, &RenderState->CpuArena, &RenderState->PipelineManager);

    RenderTargetUpdateEntries(&DemoState->Arena, &DemoState->RenderTarget);
    ParticleManagerUpdate(&DemoState->ParticleManager, FrameTime, 1000);
    
    // NOTE: Upload uniforms
    {
        // NOTE: Upload camera
        {
            camera_input* Data = VkTransferPushWriteStruct(&RenderState->TransferManager, DemoState->Camera.GpuBuffer, camera_input,
                                                           BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT),
                                                           BarrierMask(VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT));
            *Data = {};
            Data->CameraPos = DemoState->Camera.Pos;
        }
    }

    //
    // NOTE: Sync on GPU transfers
    //
    VkTransferManagerFlush(&RenderState->TransferManager, RenderState->Device, RenderState->Commands.Buffer, &RenderState->BarrierManager);

    //
    // NOTE: Entity Rendering
    //
    {
        RenderTargetRenderPassBegin(&DemoState->RenderTarget, Commands, RenderTargetRenderPass_SetViewPort | RenderTargetRenderPass_SetScissor);

        // NOTE: Particles
        {
            particle_manager* Manager = &DemoState->ParticleManager;
            vkCmdBindPipeline(Commands.Buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, DemoState->EntityPipeline->Handle);
            vkCmdBindDescriptorSets(Commands.Buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, DemoState->EntityPipeline->Layout, 0,
                                    1, &Manager->EntityDescriptor, 0, 0);

            VkBuffer Buffers[] =
            {
                Manager->ParticleBuffer,
                Manager->ParticleBuffer,
                Manager->ParticleBuffer,
            };
            
            VkDeviceSize Offsets[] =
            {
                0*MAX_TOTAL_NUM_PARTICLES*sizeof(f32),
                1*MAX_TOTAL_NUM_PARTICLES*sizeof(f32),
                2*MAX_TOTAL_NUM_PARTICLES*sizeof(f32),
            };
            vkCmdBindVertexBuffers(Commands.Buffer, 0, 3, Buffers, Offsets);

            // TODO: If we want more particles, we need more draw calls
            Assert(u64(u32(Manager->TotalNumParticles)) == Manager->TotalNumParticles);
            vkCmdDraw(Commands.Buffer, u32(Manager->TotalNumParticles), 1, 0, 0);
        }

        RenderTargetRenderPassEnd(Commands);        
    }

    VkCheckResult(vkEndCommandBuffer(Commands.Buffer));
                    
    // NOTE: Render to our window surface
    // NOTE: Tell queue where we render to surface to wait
    VkPipelineStageFlags WaitDstMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo SubmitInfo = {};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.waitSemaphoreCount = 1;
    SubmitInfo.pWaitSemaphores = &RenderState->ImageAvailableSemaphore;
    SubmitInfo.pWaitDstStageMask = &WaitDstMask;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = &Commands.Buffer;
    SubmitInfo.signalSemaphoreCount = 1;
    SubmitInfo.pSignalSemaphores = &RenderState->FinishedRenderingSemaphore;
    VkCheckResult(vkQueueSubmit(RenderState->GraphicsQueue, 1, &SubmitInfo, Commands.Fence));
            
    VkPresentInfoKHR PresentInfo = {};
    PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    PresentInfo.waitSemaphoreCount = 1;
    PresentInfo.pWaitSemaphores = &RenderState->FinishedRenderingSemaphore;
    PresentInfo.swapchainCount = 1;
    PresentInfo.pSwapchains = &RenderState->SwapChain;
    PresentInfo.pImageIndices = &ImageIndex;
    VkResult Result = vkQueuePresentKHR(RenderState->PresentQueue, &PresentInfo);

    switch (Result)
    {
        case VK_SUCCESS:
        {
        } break;

        case VK_ERROR_OUT_OF_DATE_KHR:
        case VK_SUBOPTIMAL_KHR:
        {
            // NOTE: Window size changed
            InvalidCodePath;
        } break;

        default:
        {
            InvalidCodePath;
        } break;
    }
    
    DemoState->TotalProgramTime += FrameTime;

    ProfilerProcessData();
    //DebugPrintTimeStamps();
    //DebugStateClearData();
}
