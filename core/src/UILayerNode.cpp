#include "lupine/core/UILayerNode.hpp"
#include "lupine/core/Serialization.hpp"

namespace lupine {
namespace core {

UILayer::UILayer()
    : Node(),
      m_Layer(0) {
}

UILayer::UILayer(const std::string& name)
    : Node(name),
      m_Layer(0) {
}

void UILayer::RegisterProperties() {
    Node::RegisterProperties();

    RegisterProperty<int>("layer", PropertyType::Int,
        [this]() { return m_Layer; },
        [this](const int& value) { m_Layer = value; });
}

} // namespace core
} // namespace lupine
