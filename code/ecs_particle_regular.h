#pragma once

struct particle
{
    v3 Pos;
    v3 Vel;
    f32 LifeTime;
};

#define BLOCK_MEMORY_SIZE KiloBytes(64)
// TODO: This is a hack for now since the header size is 24
#define BLOCK_MAX_NUM_PARTICLES ((BLOCK_MEMORY_SIZE - 24) / sizeof(particle))
struct particle_block
{
    particle_block* Prev;
    particle_block* Next;
    u64 NumParticles;
    particle Particles[BLOCK_MAX_NUM_PARTICLES];
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
