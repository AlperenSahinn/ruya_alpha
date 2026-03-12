#pragma once
#include <stdint.h>

#include <core/ry_id.h>

namespace ruya
{
    struct PBROpaqueMaterial
    {
        RyID albedoImageId;
        RyID normalImageId;
        RyID metallicRoughnessImageId;
        uint32_t descriptorIndex;
    };

    struct PBROpaqueMaterialGPU
    {
        uint32_t albedoImageDescriptorIndex;
        uint32_t normalImageDescriptorIndex;
        uint32_t metallicRoughnessImageDescriptorIndex;
        uint32_t pad;
    };
}