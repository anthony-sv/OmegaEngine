#version 460 core

// ── Varyings from the vertex shader ────────────────────────────────
// These arrive interpolated across each triangle's surface.
in vec4  v_Color;
in vec2  v_TexCoord;
in float v_TexIndex;
in float v_TilingFactor;

out vec4 o_Color;

// ── Texture sampler array ──────────────────────────────────────────
// One sampler per texture slot. The batch renderer binds up to 32
// different textures simultaneously, one per slot. Each vertex
// carries a v_TexIndex that says "sample from slot N."
//
// Slot 0 is always a 1x1 white pixel. Untextured (color-only) quads
// use slot 0, so texture() returns (1,1,1,1) and the final color is
// just v_Color. This avoids branching — every pixel takes the same
// code path regardless of whether it has a "real" texture.
//
// 32 is the OpenGL 4.6 minimum for GL_MAX_TEXTURE_IMAGE_UNITS.
uniform sampler2D u_Textures[32];

void main()
{
    // Convert the interpolated float index to an integer.
    // The GPU interpolates v_TexIndex across each triangle, but since
    // all 4 vertices of a quad have the same value, the result is
    // exact (or very close). The +0.5 guards against floating-point
    // truncation: if interpolation produces 2.9999 instead of 3.0,
    // int() would give 2 — adding 0.5 makes it 3.4999 -> 3. Correct.
    int index = int(v_TexIndex + 0.5);

    // Sample the texture, scaling UVs by the tiling factor.
    // A tiling factor of 2.0 makes the texture repeat twice in each
    // direction (only visible with GL_REPEAT wrapping, which is the
    // default set by Texture2D::initStorage).
    vec4 texColor = texture(u_Textures[index], v_TexCoord * v_TilingFactor);

    // Final color = texture sample * vertex color.
    // This enables three use cases with one multiplication:
    //   1. Color-only quad:   white tex (1,1,1,1) * color = color
    //   2. Texture-only quad: tex color * white (1,1,1,1) = tex
    //   3. Tinted texture:    tex color * tint color = tinted tex
    o_Color = texColor * v_Color;
}