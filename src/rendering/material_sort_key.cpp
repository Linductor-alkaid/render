#include "render/material_sort_key.h"
#include "render/material.h"
#include "render/shader.h"
#include <tuple>

namespace Render {

bool MaterialSortKey::operator==(const MaterialSortKey& other) const noexcept {
    return materialID == other.materialID &&
           shaderID == other.shaderID &&
           blendMode == other.blendMode &&
           cullFace == other.cullFace &&
           depthTest == other.depthTest &&
           depthWrite == other.depthWrite &&
           overrideHash == other.overrideHash &&
           pipelineFlags == other.pipelineFlags;
}

MaterialSortKey BuildMaterialSortKey(const Material* material,
                                     uint32_t overrideHash,
                                     uint32_t pipelineFlags) noexcept {
    MaterialSortKey key{};

    if (!material) {
        key.overrideHash = overrideHash;
        key.pipelineFlags = pipelineFlags;
        return key;
    }

    key.materialID = material->GetStableID();

    if (auto shader = material->GetShader()) {
        key.shaderID = shader->GetProgramID();
    }

    key.blendMode = material->GetBlendMode();
    key.cullFace = material->GetCullFace();
    key.depthTest = material->GetDepthTest();
    key.depthWrite = material->GetDepthWrite();
    key.overrideHash = overrideHash;
    key.pipelineFlags = pipelineFlags;

    return key;
}

std::size_t MaterialSortKeyHasher::operator()(const MaterialSortKey& key) const noexcept {
    const uint64_t parts[] = {
        (static_cast<uint64_t>(key.materialID) << 32) | key.shaderID,
        (static_cast<uint64_t>(key.overrideHash) << 32) | key.pipelineFlags,
        static_cast<uint64_t>(static_cast<uint32_t>(key.blendMode)) << 32 |
        static_cast<uint32_t>(key.cullFace),
        static_cast<uint64_t>(key.depthTest) << 1 | static_cast<uint64_t>(key.depthWrite)
    };

    std::size_t seed = 0xcbf29ce484222325ULL;
    const std::size_t prime = 0x100000001b3ULL;

    for (uint64_t part : parts) {
        seed ^= part;
        seed *= prime;
    }

    return seed;
}

bool MaterialSortKeyLess::operator()(const MaterialSortKey& lhs, const MaterialSortKey& rhs) const noexcept {
    return std::tie(lhs.materialID,
                    lhs.shaderID,
                    lhs.blendMode,
                    lhs.cullFace,
                    lhs.depthTest,
                    lhs.depthWrite,
                    lhs.overrideHash,
                    lhs.pipelineFlags) <
           std::tie(rhs.materialID,
                    rhs.shaderID,
                    rhs.blendMode,
                    rhs.cullFace,
                    rhs.depthTest,
                    rhs.depthWrite,
                    rhs.overrideHash,
                    rhs.pipelineFlags);
}

} // namespace Render

