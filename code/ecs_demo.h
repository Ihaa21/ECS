#pragma once

//#define VALIDATION
#include "framework_vulkan\framework_vulkan.h"

#define CPU_PROFILING
#define WIN32_PROFILING
#define X86_PROFILING
#include "profiling\profiling.h"

#define MAX_TOTAL_NUM_PARTICLES 1024*1024
//#include "ecs_particle_regular.h"
#include "ecs_particle_soa.h"

struct entity_group_uniforms
{
    v3 Color;
};

struct demo_state
{
    linear_arena Arena;
    linear_arena TempArena;

    f32 TotalProgramTime;
    
    // NOTE: Samplers
    VkSampler PointSampler;
    VkSampler LinearSampler;
    
    camera Camera;

    // NOTE: Render Target Entries
    render_target_entry SwapChainEntry;
    render_target RenderTarget;

    vk_pipeline* EntityPipeline;
    VkDescriptorSetLayout EntityDescLayout;

    // NOTE: Particle Data
    particle_manager ParticleManager;
};

global demo_state* DemoState;

