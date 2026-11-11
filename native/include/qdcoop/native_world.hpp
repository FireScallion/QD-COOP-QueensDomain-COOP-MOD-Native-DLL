#pragma once
#include <cstdint>

namespace qdcoop::native_world {
enum class Phase : std::uint32_t {
    idle, waiting_player, preparing, active, suspended, world_changed, failed
};
}
