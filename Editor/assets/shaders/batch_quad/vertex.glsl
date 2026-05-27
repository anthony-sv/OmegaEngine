#version 460 core

// ── Per-vertex attributes ──────────────────────────────────────────
// Each field matches a member of the BatchVertex struct on the CPU.
// The batch renderer fills a dynamic VBO with thousands of these
// vertices per frame, then draws them all in one glDrawElements call.

layout(location = 0) in vec2  a_Position;        // world-space XY (pre-transformed on CPU)
layout(location = 1) in vec4  a_Color;           // RGBA tint (alpha for transparency)
layout(location = 2) in vec2  a_TexCoord;        // UV coordinates
layout(location = 3) in float a_TexIndex;        // which texture slot (0..31)
layout(location = 4) in float a_TilingFactor;    // UV multiplier for repeating textures

// ── Camera uniform ─────────────────────────────────────────────────
// Uploaded once per beginBatch(). Transforms world-space positions
// into clip space (NDC) via the camera's orthographic projection.
uniform mat4 u_ViewProjection;

// ── Varyings ───────────────────────────────────────────────────────
// Passed to the fragment shader. The GPU interpolates these values
// across each triangle (barycentric interpolation), so every pixel
// gets a smooth blend of its triangle's three vertex values.
out vec4  v_Color;
out vec2  v_TexCoord;
out float v_TexIndex;
out float v_TilingFactor;

void main()
{
    v_Color        = a_Color;
    v_TexCoord     = a_TexCoord;
    v_TexIndex     = a_TexIndex;
    v_TilingFactor = a_TilingFactor;

    // Transform from world space to clip space.
    // The position was already transformed to world space on the CPU
    // (the batch renderer bakes position/rotation/scale into the
    // vertex positions), so only the camera VP is needed here.
    gl_Position = u_ViewProjection * vec4(a_Position, 0.0, 1.0);
}