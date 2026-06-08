module;

#include "glm/glm.hpp"

export module Engine.ECS:TextComponents;

import std;

namespace Engine::ECS
{

    // -----------------------------------------------------------------
    // TextComponent -- a string drawn in the world at the entity's
    // Transform, using a baked font. The RenderSystem resolves `fontPath`
    // to a Renderer::Font and lays out the glyphs as textured quads.
    //
    //   text     : the string to draw.
    //   fontPath : the .ttf asset (serializable); empty = a default font.
    //   size     : world height of one line of text.
    //   color    : RGBA tint (alpha blends the glyph coverage).
    //   align    : horizontal anchoring around the Transform position.
    // -----------------------------------------------------------------

    export struct TextComponent
    {
        enum class Align : std::uint8_t { Left, Center, Right };

        std::string text     {};
        std::string fontPath {};
        float       size     { 1.0f };
        glm::vec4   color    { 1.0f, 1.0f, 1.0f, 1.0f };
        Align       align    { Align::Center };
	}; // struct TextComponent

} // namespace ECS