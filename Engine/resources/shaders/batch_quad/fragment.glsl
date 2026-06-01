#version 460 core

// ── Varyings from the vertex shader ────────────────────────────────
in vec4  v_Color;
in vec2  v_TexCoord;
flat in float v_TexIndex;       // flat: the slot is per-QUAD, never interpolated
in float v_TilingFactor;

out vec4 o_Color;

// ── Texture sampler array ──────────────────────────────────────────
// One sampler per texture slot (slot 0 is always a 1x1 white pixel, so
// colour-only quads sample white and the final colour is just v_Color).
// 32 is the OpenGL 4.6 minimum for GL_MAX_TEXTURE_IMAGE_UNITS.
uniform sampler2D u_Textures[32];

void main()
{
    int  index = int(v_TexIndex + 0.5);     // flat -> exact; +0.5 is belt-and-braces
    vec2 uv    = v_TexCoord * v_TilingFactor;

    // Sample through a CONSTANT-index switch.
    //
    // Indexing a sampler array with a non-"dynamically uniform" index
    // (here v_TexIndex, which is a per-fragment varying) is UNDEFINED
    // BEHAVIOUR in GLSL -- even when every vertex of the quad carries the
    // same value. On some GPUs/drivers a handful of fragments on a
    // colour-only quad would mis-sample a *real* texture, scattering
    // stray textured specks (worse under minification, and it varied by
    // display). Constant literal indices ARE dynamically uniform, so this
    // switch is spec-compliant and artefact-free.
    vec4 texColor = vec4(1.0);
    switch (index)
    {
        case  0: texColor = texture(u_Textures[ 0], uv); break;
        case  1: texColor = texture(u_Textures[ 1], uv); break;
        case  2: texColor = texture(u_Textures[ 2], uv); break;
        case  3: texColor = texture(u_Textures[ 3], uv); break;
        case  4: texColor = texture(u_Textures[ 4], uv); break;
        case  5: texColor = texture(u_Textures[ 5], uv); break;
        case  6: texColor = texture(u_Textures[ 6], uv); break;
        case  7: texColor = texture(u_Textures[ 7], uv); break;
        case  8: texColor = texture(u_Textures[ 8], uv); break;
        case  9: texColor = texture(u_Textures[ 9], uv); break;
        case 10: texColor = texture(u_Textures[10], uv); break;
        case 11: texColor = texture(u_Textures[11], uv); break;
        case 12: texColor = texture(u_Textures[12], uv); break;
        case 13: texColor = texture(u_Textures[13], uv); break;
        case 14: texColor = texture(u_Textures[14], uv); break;
        case 15: texColor = texture(u_Textures[15], uv); break;
        case 16: texColor = texture(u_Textures[16], uv); break;
        case 17: texColor = texture(u_Textures[17], uv); break;
        case 18: texColor = texture(u_Textures[18], uv); break;
        case 19: texColor = texture(u_Textures[19], uv); break;
        case 20: texColor = texture(u_Textures[20], uv); break;
        case 21: texColor = texture(u_Textures[21], uv); break;
        case 22: texColor = texture(u_Textures[22], uv); break;
        case 23: texColor = texture(u_Textures[23], uv); break;
        case 24: texColor = texture(u_Textures[24], uv); break;
        case 25: texColor = texture(u_Textures[25], uv); break;
        case 26: texColor = texture(u_Textures[26], uv); break;
        case 27: texColor = texture(u_Textures[27], uv); break;
        case 28: texColor = texture(u_Textures[28], uv); break;
        case 29: texColor = texture(u_Textures[29], uv); break;
        case 30: texColor = texture(u_Textures[30], uv); break;
        case 31: texColor = texture(u_Textures[31], uv); break;
    }

    // Final colour: texture * tint. White slot 0 -> colour-only; white
    // tint -> texture-only; both -> tinted texture.
    o_Color = texColor * v_Color;
}