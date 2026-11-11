#pragma once
#include "qdcoop/weak_object_view.hpp"
#include <array>
#include <cstdint>

namespace qdcoop::enemy {
// Game-thread discovery result. The weak identity must be resolved before use.
struct NativeCandidate {
    ue427::WeakKey actor{};
    std::uint16_t class_id{},mesh_id{};
    std::array<float,3> location{};
    float hp{};
};
static_assert(sizeof(NativeCandidate)==28);

struct NativeBindingView {
    std::uint32_t generation{};
    std::uint32_t confirmed{};
    ue427::WeakKey actor{},display{};
};
static_assert(sizeof(NativeBindingView)==24);
}
