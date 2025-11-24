#include "lupine/components/StyleBox.hpp"
#include "lupine/logger/Logger.hpp"
#include <fstream>

namespace lupine {
namespace components {

using namespace math;

nlohmann::json StyleBox::Serialize() const {
    nlohmann::json json;
    json["type"] = GetTypeName();
    json["contentMarginLeft"] = m_ContentMarginLeft;
    json["contentMarginRight"] = m_ContentMarginRight;
    json["contentMarginTop"] = m_ContentMarginTop;
    json["contentMarginBottom"] = m_ContentMarginBottom;
    return json;
}

void StyleBox::Deserialize(const nlohmann::json& json) {
    if (json.contains("contentMarginLeft")) {
        m_ContentMarginLeft = json["contentMarginLeft"].get<float>();
    }
    if (json.contains("contentMarginRight")) {
        m_ContentMarginRight = json["contentMarginRight"].get<float>();
    }
    if (json.contains("contentMarginTop")) {
        m_ContentMarginTop = json["contentMarginTop"].get<float>();
    }
    if (json.contains("contentMarginBottom")) {
        m_ContentMarginBottom = json["contentMarginBottom"].get<float>();
    }
}

bool StyleBox::SaveToFile(const std::string& filepath) const {
    try {
        nlohmann::json json = Serialize();
        std::ofstream file(filepath);
        if (!file.is_open()) {

            return false;
        }
        file << json.dump(2);
        file.close();

        return true;
    } catch (const std::exception& e) {

        return false;
    }
}

std::shared_ptr<StyleBox> StyleBox::LoadFromFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {

            return nullptr;
        }

        nlohmann::json json;
        file >> json;
        file.close();

        if (!json.contains("type")) {

            return nullptr;
        }

        std::string typeName = json["type"].get<std::string>();
        std::shared_ptr<StyleBox> styleBox;

        if (typeName == "StyleBoxFlat") {
            styleBox = std::make_shared<StyleBoxFlat>();
        } else {

            return nullptr;
        }

        styleBox->Deserialize(json);

        return styleBox;

    } catch (const std::exception& e) {

        return nullptr;
    }
}

StyleBoxFlat::StyleBoxFlat()
    : StyleBox()
    , m_BackgroundColor(Color::White())
    , m_Opacity(1.0f)
    , m_BorderWidthLinked(true)
    , m_BorderWidthLeft(0.0f)
    , m_BorderWidthRight(0.0f)
    , m_BorderWidthTop(0.0f)
    , m_BorderWidthBottom(0.0f)
    , m_BorderColorLinked(true)
    , m_BorderColorLeft(Color::Black())
    , m_BorderColorRight(Color::Black())
    , m_BorderColorTop(Color::Black())
    , m_BorderColorBottom(Color::Black())
    , m_CornerRadiusLinked(true)
    , m_CornerRadiusTopLeft(0.0f)
    , m_CornerRadiusTopRight(0.0f)
    , m_CornerRadiusBottomLeft(0.0f)
    , m_CornerRadiusBottomRight(0.0f)
    , m_CornerDetail(8)
    , m_AntiAliasing(true)
    , m_AntiAliasingSize(1.0f)
    , m_ExpandMarginLeft(0.0f)
    , m_ExpandMarginRight(0.0f)
    , m_ExpandMarginTop(0.0f)
    , m_ExpandMarginBottom(0.0f)
    , m_ShadowEnabled(false)
    , m_ShadowColor(Color(0.0f, 0.0f, 0.0f, 0.6f))
    , m_ShadowSize(4.0f)
    , m_ShadowOffset(Vec2(0.0f, 0.0f))
{
}

std::shared_ptr<StyleBox> StyleBoxFlat::Clone() const {
    auto clone = std::make_shared<StyleBoxFlat>();

    clone->m_ContentMarginLeft = m_ContentMarginLeft;
    clone->m_ContentMarginRight = m_ContentMarginRight;
    clone->m_ContentMarginTop = m_ContentMarginTop;
    clone->m_ContentMarginBottom = m_ContentMarginBottom;

    clone->m_BackgroundColor = m_BackgroundColor;
    clone->m_Opacity = m_Opacity;

    clone->m_BorderWidthLinked = m_BorderWidthLinked;
    clone->m_BorderWidthLeft = m_BorderWidthLeft;
    clone->m_BorderWidthRight = m_BorderWidthRight;
    clone->m_BorderWidthTop = m_BorderWidthTop;
    clone->m_BorderWidthBottom = m_BorderWidthBottom;

    clone->m_BorderColorLinked = m_BorderColorLinked;
    clone->m_BorderColorLeft = m_BorderColorLeft;
    clone->m_BorderColorRight = m_BorderColorRight;
    clone->m_BorderColorTop = m_BorderColorTop;
    clone->m_BorderColorBottom = m_BorderColorBottom;

    clone->m_CornerRadiusLinked = m_CornerRadiusLinked;
    clone->m_CornerRadiusTopLeft = m_CornerRadiusTopLeft;
    clone->m_CornerRadiusTopRight = m_CornerRadiusTopRight;
    clone->m_CornerRadiusBottomLeft = m_CornerRadiusBottomLeft;
    clone->m_CornerRadiusBottomRight = m_CornerRadiusBottomRight;

    clone->m_CornerDetail = m_CornerDetail;
    clone->m_AntiAliasing = m_AntiAliasing;
    clone->m_AntiAliasingSize = m_AntiAliasingSize;

    clone->m_ExpandMarginLeft = m_ExpandMarginLeft;
    clone->m_ExpandMarginRight = m_ExpandMarginRight;
    clone->m_ExpandMarginTop = m_ExpandMarginTop;
    clone->m_ExpandMarginBottom = m_ExpandMarginBottom;

    clone->m_ShadowEnabled = m_ShadowEnabled;
    clone->m_ShadowColor = m_ShadowColor;
    clone->m_ShadowSize = m_ShadowSize;
    clone->m_ShadowOffset = m_ShadowOffset;

    clone->m_CustomShaderPath = m_CustomShaderPath;

    return clone;
}

nlohmann::json StyleBoxFlat::Serialize() const {
    nlohmann::json json = StyleBox::Serialize();

    json["backgroundColor"] = {
        {"r", m_BackgroundColor.r},
        {"g", m_BackgroundColor.g},
        {"b", m_BackgroundColor.b},
        {"a", m_BackgroundColor.a}
    };
    json["opacity"] = m_Opacity;

    json["borderWidthLinked"] = m_BorderWidthLinked;
    json["borderWidthLeft"] = m_BorderWidthLeft;
    json["borderWidthRight"] = m_BorderWidthRight;
    json["borderWidthTop"] = m_BorderWidthTop;
    json["borderWidthBottom"] = m_BorderWidthBottom;

    json["borderColorLinked"] = m_BorderColorLinked;
    json["borderColorLeft"] = {
        {"r", m_BorderColorLeft.r},
        {"g", m_BorderColorLeft.g},
        {"b", m_BorderColorLeft.b},
        {"a", m_BorderColorLeft.a}
    };
    json["borderColorRight"] = {
        {"r", m_BorderColorRight.r},
        {"g", m_BorderColorRight.g},
        {"b", m_BorderColorRight.b},
        {"a", m_BorderColorRight.a}
    };
    json["borderColorTop"] = {
        {"r", m_BorderColorTop.r},
        {"g", m_BorderColorTop.g},
        {"b", m_BorderColorTop.b},
        {"a", m_BorderColorTop.a}
    };
    json["borderColorBottom"] = {
        {"r", m_BorderColorBottom.r},
        {"g", m_BorderColorBottom.g},
        {"b", m_BorderColorBottom.b},
        {"a", m_BorderColorBottom.a}
    };

    json["cornerRadiusLinked"] = m_CornerRadiusLinked;
    json["cornerRadiusTopLeft"] = m_CornerRadiusTopLeft;
    json["cornerRadiusTopRight"] = m_CornerRadiusTopRight;
    json["cornerRadiusBottomLeft"] = m_CornerRadiusBottomLeft;
    json["cornerRadiusBottomRight"] = m_CornerRadiusBottomRight;

    json["cornerDetail"] = m_CornerDetail;
    json["antiAliasing"] = m_AntiAliasing;
    json["antiAliasingSize"] = m_AntiAliasingSize;

    json["expandMarginLeft"] = m_ExpandMarginLeft;
    json["expandMarginRight"] = m_ExpandMarginRight;
    json["expandMarginTop"] = m_ExpandMarginTop;
    json["expandMarginBottom"] = m_ExpandMarginBottom;

    json["shadowEnabled"] = m_ShadowEnabled;
    json["shadowColor"] = {
        {"r", m_ShadowColor.r},
        {"g", m_ShadowColor.g},
        {"b", m_ShadowColor.b},
        {"a", m_ShadowColor.a}
    };
    json["shadowSize"] = m_ShadowSize;
    json["shadowOffset"] = {
        {"x", m_ShadowOffset.x},
        {"y", m_ShadowOffset.y}
    };

    json["customShaderPath"] = m_CustomShaderPath;

    return json;
}

void StyleBoxFlat::Deserialize(const nlohmann::json& json) {
    StyleBox::Deserialize(json);

    if (json.contains("backgroundColor")) {
        auto bg = json["backgroundColor"];
        m_BackgroundColor = Color(
            bg["r"].get<float>(),
            bg["g"].get<float>(),
            bg["b"].get<float>(),
            bg["a"].get<float>()
        );
    }
    if (json.contains("opacity")) {
        m_Opacity = json["opacity"].get<float>();
    }

    if (json.contains("borderWidthLinked")) {
        m_BorderWidthLinked = json["borderWidthLinked"].get<bool>();
    }
    if (json.contains("borderWidthLeft")) {
        m_BorderWidthLeft = json["borderWidthLeft"].get<float>();
    }
    if (json.contains("borderWidthRight")) {
        m_BorderWidthRight = json["borderWidthRight"].get<float>();
    }
    if (json.contains("borderWidthTop")) {
        m_BorderWidthTop = json["borderWidthTop"].get<float>();
    }
    if (json.contains("borderWidthBottom")) {
        m_BorderWidthBottom = json["borderWidthBottom"].get<float>();
    }

    if (json.contains("borderColorLinked")) {
        m_BorderColorLinked = json["borderColorLinked"].get<bool>();
    }
    if (json.contains("borderColorLeft")) {
        auto bc = json["borderColorLeft"];
        m_BorderColorLeft = Color(bc["r"].get<float>(), bc["g"].get<float>(), bc["b"].get<float>(), bc["a"].get<float>());
    }
    if (json.contains("borderColorRight")) {
        auto bc = json["borderColorRight"];
        m_BorderColorRight = Color(bc["r"].get<float>(), bc["g"].get<float>(), bc["b"].get<float>(), bc["a"].get<float>());
    }
    if (json.contains("borderColorTop")) {
        auto bc = json["borderColorTop"];
        m_BorderColorTop = Color(bc["r"].get<float>(), bc["g"].get<float>(), bc["b"].get<float>(), bc["a"].get<float>());
    }
    if (json.contains("borderColorBottom")) {
        auto bc = json["borderColorBottom"];
        m_BorderColorBottom = Color(bc["r"].get<float>(), bc["g"].get<float>(), bc["b"].get<float>(), bc["a"].get<float>());
    }

    if (json.contains("cornerRadiusLinked")) {
        m_CornerRadiusLinked = json["cornerRadiusLinked"].get<bool>();
    }
    if (json.contains("cornerRadiusTopLeft")) {
        m_CornerRadiusTopLeft = json["cornerRadiusTopLeft"].get<float>();
    }
    if (json.contains("cornerRadiusTopRight")) {
        m_CornerRadiusTopRight = json["cornerRadiusTopRight"].get<float>();
    }
    if (json.contains("cornerRadiusBottomLeft")) {
        m_CornerRadiusBottomLeft = json["cornerRadiusBottomLeft"].get<float>();
    }
    if (json.contains("cornerRadiusBottomRight")) {
        m_CornerRadiusBottomRight = json["cornerRadiusBottomRight"].get<float>();
    }

    if (json.contains("cornerDetail")) {
        m_CornerDetail = json["cornerDetail"].get<int>();
    }
    if (json.contains("antiAliasing")) {
        m_AntiAliasing = json["antiAliasing"].get<bool>();
    }
    if (json.contains("antiAliasingSize")) {
        m_AntiAliasingSize = json["antiAliasingSize"].get<float>();
    }

    if (json.contains("expandMarginLeft")) {
        m_ExpandMarginLeft = json["expandMarginLeft"].get<float>();
    }
    if (json.contains("expandMarginRight")) {
        m_ExpandMarginRight = json["expandMarginRight"].get<float>();
    }
    if (json.contains("expandMarginTop")) {
        m_ExpandMarginTop = json["expandMarginTop"].get<float>();
    }
    if (json.contains("expandMarginBottom")) {
        m_ExpandMarginBottom = json["expandMarginBottom"].get<float>();
    }

    if (json.contains("shadowEnabled")) {
        m_ShadowEnabled = json["shadowEnabled"].get<bool>();
    }
    if (json.contains("shadowColor")) {
        auto sc = json["shadowColor"];
        m_ShadowColor = Color(sc["r"].get<float>(), sc["g"].get<float>(), sc["b"].get<float>(), sc["a"].get<float>());
    }
    if (json.contains("shadowSize")) {
        m_ShadowSize = json["shadowSize"].get<float>();
    }
    if (json.contains("shadowOffset")) {
        auto so = json["shadowOffset"];
        m_ShadowOffset = Vec2(so["x"].get<float>(), so["y"].get<float>());
    }

    if (json.contains("customShaderPath")) {
        m_CustomShaderPath = json["customShaderPath"].get<std::string>();
    }
}

}
}
