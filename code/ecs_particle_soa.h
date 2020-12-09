#pragma once

#define BLOCK_MEMORY_SIZE KiloBytes(64)
// TODO: This is a hack for now since the header size is 24
#define _BLOCK_MAX_NUM_PARTICLES ((BLOCK_MEMORY_SIZE - 24) / (2*sizeof(v3) + sizeof(f32)))
#define BLOCK_MAX_NUM_PARTICLES (_BLOCK_MAX_NUM_PARTICLES - (_BLOCK_MAX_NUM_PARTICLES % 4))
struct particle_block
{
    // NOTE: Data
    f32 PosX[BLOCK_MAX_NUM_PARTICLES];
    f32 PosY[BLOCK_MAX_NUM_PARTICLES];
    f32 PosZ[BLOCK_MAX_NUM_PARTICLES];

    f32 VelX[BLOCK_MAX_NUM_PARTICLES];
    f32 VelY[BLOCK_MAX_NUM_PARTICLES];
    f32 VelZ[BLOCK_MAX_NUM_PARTICLES];

    f32 LifeTime[BLOCK_MAX_NUM_PARTICLES];

    // NOTE: Header
    particle_block* Prev;
    particle_block* Next;
    u64 NumParticles;
};

struct particle_manager
{
    // TODO: This is allocating 16kb for no reason here, we probably wanna do the get data crap
    particle_block BlockSentinel;
    u64 TotalNumParticles;
    VkBuffer ParticleBuffer;
    VkBuffer UniformBuffer;
    VkDescriptorSet EntityDescriptor;
};
