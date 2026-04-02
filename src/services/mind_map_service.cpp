#include "mind_map/services/mind_map_service.hpp"
#include "mind_map/services/detail/mind_map_context.hpp"

namespace mind_map {
    MindMapService::MindMapService() : ctx_(std::make_unique<MindMapContext>()) {
    }

    MindMapService::~MindMapService() = default;
}
