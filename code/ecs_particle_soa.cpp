
inline void ParticleManagerCreate(particle_manager* Manager, v3 Color)
{
    LinkedListSentinelCreate(Manager->BlockSentinel);

    // NOTE: Create gpu resources
    // TODO: Can we do something more dynamic here?
    Manager->ParticleBuffer = VkBufferCreate(RenderState->Device, &RenderState->GpuArena,
                                             VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                             sizeof(v3)*MAX_TOTAL_NUM_PARTICLES);
    
    Manager->UniformBuffer = VkBufferCreate(RenderState->Device, &RenderState->HostArena,
                                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                            sizeof(entity_group_uniforms));

    entity_group_uniforms* Data = VkTransferPushWriteStruct(&RenderState->TransferManager, Manager->UniformBuffer, entity_group_uniforms,
                                                            BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT),
                                                            BarrierMask(VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT));
    *Data = {};
    Data->Color = Color;
        
    Manager->EntityDescriptor = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, DemoState->EntityDescLayout);
    VkDescriptorBufferWrite(&RenderState->DescriptorManager, Manager->EntityDescriptor, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            Manager->UniformBuffer);
}

inline void ParticleCreate(particle_manager* Manager, v3 Pos, v3 Vel, f32 LifeTime)
{
    particle_block* LastBlock = Manager->BlockSentinel.Prev;
    if (Manager->BlockSentinel.Next == &Manager->BlockSentinel ||
        Manager->BlockSentinel.Prev->NumParticles == BLOCK_MAX_NUM_PARTICLES)
    {
        // NOTE: Create a new particle block to store particles in
        LastBlock = (particle_block*)MemoryAllocate(BLOCK_MEMORY_SIZE);
        LinkedListSentinelAppend(Manager->BlockSentinel, LastBlock);
    }

    u64 ParticleId = LastBlock->NumParticles++;
    Manager->TotalNumParticles += 1;

    LastBlock->PosX[ParticleId] = Pos.x;
    LastBlock->PosY[ParticleId] = Pos.y;
    LastBlock->PosZ[ParticleId] = Pos.z;
    LastBlock->VelX[ParticleId] = Vel.x;
    LastBlock->VelY[ParticleId] = Vel.y;
    LastBlock->VelZ[ParticleId] = Vel.z;
    LastBlock->LifeTime[ParticleId] = LifeTime;
}

inline void ParticleDestroyBlock(particle_manager* Manager, particle_block* Block)
{
    Manager->TotalNumParticles -= Block->NumParticles;
    LinkedListSentinelRemove(Block);
    MemoryFree(Block);
}

inline void ParticleDestroyFromBlock(particle_manager* Manager, particle_block* Block, u32 ParticleId)
{
    particle_block* LastBlock = Manager->BlockSentinel.Prev;
    u64 SwapId = --LastBlock->NumParticles;
    // NOTE: Swap data
    Block->PosX[ParticleId] = LastBlock->PosX[SwapId];
    Block->PosY[ParticleId] = LastBlock->PosY[SwapId];
    Block->PosZ[ParticleId] = LastBlock->PosZ[SwapId];
    Block->VelX[ParticleId] = LastBlock->VelX[SwapId];
    Block->VelY[ParticleId] = LastBlock->VelY[SwapId];
    Block->VelZ[ParticleId] = LastBlock->VelZ[SwapId];
    Block->LifeTime[ParticleId] = LastBlock->LifeTime[SwapId];
    
    Manager->TotalNumParticles -= 1;

    if (LastBlock->NumParticles == 0)
    {
        ParticleDestroyBlock(Manager, LastBlock);
    }
}

inline void ParticleDestroy(particle_manager* Manager, particle_block* Block, u32 ParticleId)
{
    // IMPORTANT: This will delete a block so not safe in loops that iterate through that block
    ParticleDestroyFromBlock(Manager, Block, ParticleId);

    particle_block* LastBlock = Manager->BlockSentinel.Prev;
    if (LastBlock->NumParticles == 0)
    {
        ParticleDestroyBlock(Manager, Block);
    }
}

inline void ParticleManagerUpdate(particle_manager* Manager, f32 FrameTime, u32 NumCreatePerFrame)
{
    // NOTE: Particle Destroy
    {
        CPU_TIMED_BLOCK("Particle Destroy");

        u32 NumDeletedParticles = 0;
        for (particle_block* Block = Manager->BlockSentinel.Next;
             Block != &Manager->BlockSentinel;
             )
        {
            // NOTE: Do pre-emptively incase block gets deleted
            for (u32 ParticleId = 0; ParticleId < Block->NumParticles; ++ParticleId)
            {
                f32 LifeTime = Block->LifeTime[ParticleId];
                if (LifeTime <= 0.0f)
                {
                    ParticleDestroyFromBlock(Manager, Block, ParticleId);
                    ParticleId -= 1;
                    NumDeletedParticles += 1;
                }
            }

            particle_block* DeleteBlock = Block;
            Block = Block->Next;
            if (DeleteBlock->NumParticles == 0)
            {
                ParticleDestroyBlock(Manager, DeleteBlock);
            }
        }

        DebugPrintLog("NumDeleted: %i\n", NumDeletedParticles);
    }
        
    // NOTE: Particle Create
    {
        CPU_TIMED_BLOCK("Particle Create");
        for (u32 CreateId = 0; CreateId < NumCreatePerFrame; ++CreateId)
        {
            v3 Vel = V3((2.0f * (f32(rand()) / f32(RAND_MAX)) - 1.0f),
                        (2.0f * (f32(rand()) / f32(RAND_MAX)) - 1.0f),
                        0.0f);
            Vel *= 0.1f;
            
            ParticleCreate(Manager, V3(0.0f, 0.2f, 0.0f), Vel, 15.0f*f32(rand()) / f32(RAND_MAX));
        }
    }

    DebugPrintLog("ParticleNum: %llu\n", Manager->TotalNumParticles);
    
    // NOTE: Particle Update and prepare rendering
    Assert(Manager->TotalNumParticles < MAX_TOTAL_NUM_PARTICLES);
    u64 TotalNumParticlesPadded = Manager->TotalNumParticles + (4 - (Manager->TotalNumParticles % 4));
    f32* GpuVertexX = VkTransferPushWriteArray(&RenderState->TransferManager, Manager->ParticleBuffer,
                                               0*MAX_TOTAL_NUM_PARTICLES*sizeof(f32), f32, TotalNumParticlesPadded,
                                               BarrierMask(VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT),
                                               BarrierMask(VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT));
    f32* GpuVertexY = VkTransferPushWriteArray(&RenderState->TransferManager, Manager->ParticleBuffer,
                                               1*MAX_TOTAL_NUM_PARTICLES*sizeof(f32), f32, TotalNumParticlesPadded,
                                               BarrierMask(VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT),
                                               BarrierMask(VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT));
    f32* GpuVertexZ = VkTransferPushWriteArray(&RenderState->TransferManager, Manager->ParticleBuffer,
                                               2*MAX_TOTAL_NUM_PARTICLES*sizeof(f32), f32, TotalNumParticlesPadded,
                                               BarrierMask(VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT),
                                               BarrierMask(VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT));
    {
        CPU_TIMED_BLOCK("Particle Update");
        u64 WriteParticleId = 0;
        for (particle_block* Block = Manager->BlockSentinel.Next;
             Block != &Manager->BlockSentinel;
             Block = Block->Next)
        {
            u64 TempWriteParticleId = WriteParticleId;
            for (u32 ParticleId = 0; ParticleId < Block->NumParticles; ParticleId += 4, TempWriteParticleId += 4)
            {
                v3_x4 Vel = V3X4(Block->VelX + ParticleId, Block->VelY + ParticleId, Block->VelZ + ParticleId);
                v3_x4 Pos = V3X4(Block->PosX + ParticleId, Block->PosY + ParticleId, Block->PosZ + ParticleId);
                v1_x4 LifeTime = V1X4(Block->LifeTime + ParticleId);

                // NOTE: Update lifetimes
                LifeTime -= FrameTime;
                StoreAligned(LifeTime, Block->LifeTime + ParticleId);

                // NOTE: Update position
                Pos = Pos + Vel*FrameTime;
                Pos = Clamp(Pos, V3X4(-1, -1, -1), V3X4(1, 1, 1));
                
                StoreAligned(Pos.x, Block->PosX + ParticleId);
                StoreAligned(Pos.y, Block->PosY + ParticleId);
                StoreAligned(Pos.z, Block->PosZ + ParticleId);

                // NOTE: Write Vertex Data to GPU
                StoreAligned(Pos.x, GpuVertexX + TempWriteParticleId);
                StoreAligned(-Pos.y, GpuVertexY + TempWriteParticleId);
                StoreAligned(Pos.z, GpuVertexZ + TempWriteParticleId);
            }

            WriteParticleId += Block->NumParticles;
        }
    }
}
