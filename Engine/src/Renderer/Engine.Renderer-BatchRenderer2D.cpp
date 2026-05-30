module;

#include "glad/glad.h"
#include "glm/glm.hpp"

module Engine.Renderer:BatchRenderer2D;

import :BatchRenderer2D;
import :Buffer;
import :VertexArray;
import :Shader;
import :Texture;
import :RenderCommand;
import :Camera2D;
import Engine.Core;
import std;

namespace Engine::Renderer
{
    // ── BatchVertex ─────────────────────────────────────────────────────
    // One vertex in the batch. The batch renderer writes 4 of these per
    // quad into a CPU staging buffer, then uploads them all to the GPU in
    // one shot at flush time.

    // Every field maps 1:1 to a vertex attribute in the batch shader:
    //   layout(location = 0) in vec2  a_Position;
    //   layout(location = 1) in vec4  a_Color;
    //   layout(location = 2) in vec2  a_TexCoord;
    //   layout(location = 3) in float a_TexIndex;
    //   layout(location = 4) in float a_TilingFactor;
    //
    // Total size: 2*4 + 4*4 + 2*4 + 4 + 4 = 40 bytes per vertex.
    // A quad is 4 vertices = 160 bytes. 10,000 quads = 1.6 MB.

    struct BatchVertex
    {
        glm::vec2 position;         // world-space XY (already transformed)
        glm::vec4 color;            // RGBA tint
        glm::vec2 texCoord;         // UV coordinates
        float     texIndex;         // which texture slot (0..31)
        float     tilingFactor;     // UV multiplier for tiling
    };

    // ── Capacity limits ─────────────────────────────────────────────────
    // These define the maximum batch size before an automatic flush.
    //
    // MaxQuads: how many quads fit in one batch. 10,000 is a good default
    //   for 2D games — most frames use far fewer, so the buffer is rarely
    //   full. Increasing it uses more memory but allows larger batches.
    //
    // MaxVertices / MaxIndices: derived from MaxQuads.
    //   Each quad = 4 vertices + 6 indices (two triangles sharing an edge).
    //
    // MaxTextureSlots: how many different textures can be bound at once.
    //   OpenGL 4.6 guarantees at least 16 texture image units per stage,
    //   but most desktop GPUs support 32. When all slots are full, the
    //   batch is flushed to free them up.

    static constexpr std::uint32_t MaxQuads        = 10'000;
    static constexpr std::uint32_t MaxVertices     = MaxQuads * 4;      // 40,000
    static constexpr std::uint32_t MaxIndices      = MaxQuads * 6;      // 60,000
    static constexpr std::uint32_t MaxTextureSlots = 32;

    // ── GPU resources ───────────────────────────────────────────────────
    // Created in init(), destroyed in shutdown(). Wrapped in std::optional
    // because RAII types (Shader, VertexBuffer, etc.) have no default
    // constructors — we construct them later when the GL context is ready.

    static Shader*                     s_batchShader { nullptr };  // owned by the AssetManager (non-owning here)
    static std::optional<VertexBuffer> s_vertexBuffer;    // dynamic VBO (re-filled every frame)
    static std::optional<IndexBuffer>  s_indexBuffer;     // static IBO (pre-computed quad pattern)
    static std::optional<VertexArray>  s_vertexArray;     // VAO wiring the VBO + IBO together
    static std::optional<Texture2D>    s_whiteTexture;    // 1x1 white pixel for color-only quads

    // ── CPU-side vertex staging buffer ──────────────────────────────────
    // This is where drawQuad() writes vertices. It's a flat contiguous
    // block in regular memory (not GPU memory). At flush time, the used
    // portion is uploaded to the GPU via VertexBuffer::setSubData().
    //
    // We use a std::vector instead of std::array because MSVC's module
    // compiler (C1060) runs out of heap space when instantiating a
    // std::array<BatchVertex, 40000> in a heavily-imported TU. The
    // vector is resized once in init() and never reallocated after that.

    // s_vertexPtr is a write cursor. It starts at the beginning of the
    // vector each batch. drawQuad() writes 4 vertices at the cursor and
    // advances it by 4. At flush time, the byte count is:
    //   (s_vertexPtr - s_vertexStorage.data()) * sizeof(BatchVertex)

    static std::vector<BatchVertex> s_vertexStorage;
    static BatchVertex* s_vertexPtr = nullptr;

    // ── Texture slot tracking ───────────────────────────────────────────
    // Maps slot indices (0..31) to OpenGL texture IDs. When drawQuad()
    // submits a textured quad, it looks up the texture's GL ID in this
    // array. If found, it reuses that slot. If not found, it assigns the
    // next free slot. If all slots are full, the batch is flushed.
    //
    // Slot 0 is ALWAYS the white texture (never reassigned). This means
    // color-only quads always use texIndex=0, and up to 31 real textures
    // can be bound simultaneously per batch.

    static std::array<std::uint32_t, MaxTextureSlots> s_textureSlots {};
    static std::uint32_t s_textureSlotIndex = 1;    // next free slot (0 = white)

    // ── Per-frame counters ──────────────────────────────────────────────
    static std::uint32_t       s_quadCount = 0;
    static Renderer2D::Stats   s_stats {};


    // ── Standard UV coordinates for a quad ──────────────────────────────
    // Bottom-left origin. Every quad uses these unless sub-rect UVs are
    // needed (future texture atlas support).

    static constexpr glm::vec2 QuadUVs[4] =
    {
        { 0.0f, 0.0f },    // bottom-left
        { 1.0f, 0.0f },    // bottom-right
        { 1.0f, 1.0f },    // top-right
        { 0.0f, 1.0f },    // top-left
    };

    // ── flushAndReset ───────────────────────────────────────────────────
    // The heart of the batch renderer. This function:
    //   1. Uploads the CPU vertex data to the GPU.
    //   2. Binds all active textures to their slots.
    //   3. Issues ONE draw call for all accumulated quads.
    //   4. Resets the batch state for the next group of quads.
    //
    // Called in two situations:
    //   - Explicitly by endBatch() at the end of the frame.
    //   - Implicitly when a drawQuad() call would overflow the batch
    //     (too many quads or too many textures). This "mid-frame flush"
    //     is transparent to the caller — they keep calling drawQuad()
    //     and the renderer handles the split automatically.

    static void flushAndReset()
    {
        // How many vertices did we actually write this batch?
        auto const vertexCount = static_cast<std::uint32_t>(
            s_vertexPtr - s_vertexStorage.data()
        );

        // Nothing to draw — skip the draw call entirely.
        if (vertexCount == 0) return;

        // ── Step 1: Upload vertex data to the GPU ───────────────
        // setSubData copies from CPU memory into the dynamic VBO.
        // Only the used portion is uploaded (not the full 1.6 MB).
        auto const byteCount = vertexCount *
            static_cast<std::uint32_t>(sizeof(BatchVertex));
        s_vertexBuffer->setSubData(s_vertexStorage.data(), byteCount);

        // ── Step 2: Bind textures to their slots ────────────────
        // Each slot index maps to a GL texture unit. The fragment
        // shader's sampler array u_Textures[i] reads from unit i.
        // We only bind the slots that were actually assigned this
        // batch (0 through s_textureSlotIndex - 1).
        for (std::uint32_t i = 0; i < s_textureSlotIndex; ++i)
            glBindTextureUnit(i, s_textureSlots[i]);

        // ── Step 3: Draw everything in one call ─────────────────
        // Bind the shader and VAO, then issue a single indexed draw.
        // The index count = quads * 6 (two triangles per quad,
        // three indices per triangle).
        s_batchShader->bind();
        s_vertexArray->bind();
        RenderCommand::drawIndexed(s_quadCount * 6);
        s_vertexArray->unbind();

        // ── Step 4: Update stats ────────────────────────────────
        s_stats.drawCalls++;
        s_stats.quadCount += s_quadCount;

        // ── Step 5: Reset for the next batch ────────────────────
        // The vertex cursor goes back to the start, the quad count
        // resets, and all texture slots (except 0 = white) are freed.
        s_vertexPtr        = s_vertexStorage.data();
        s_quadCount        = 0;
        s_textureSlotIndex = 1;     // slot 0 (white tex) stays
    }


    // ── findOrAssignTextureSlot ─────────────────────────────────────────
    // Given a GL texture ID, returns the slot index (as a float, since
    // that's what goes into the BatchVertex). If the texture is already
    // bound to a slot, reuses it. If not, assigns the next free slot.
    // If all 32 slots are full, flushes the batch to free them up.

    static float findOrAssignTextureSlot(std::uint32_t textureId)
    {
        // Check if this texture is already in a slot.
        // Start at 1 because slot 0 is always the white texture.
        for (std::uint32_t i = 1; i < s_textureSlotIndex; ++i)
        {
            if (s_textureSlots[i] == textureId)
                return static_cast<float>(i);
        }

        // Not found — need a new slot. If all slots are taken,
        // flush the current batch to free them up.
        if (s_textureSlotIndex >= MaxTextureSlots)
            flushAndReset();

        // Assign this texture to the next free slot.
        s_textureSlots[s_textureSlotIndex] = textureId;
        return static_cast<float>(s_textureSlotIndex++);
    }


    // ── emitQuadVertices ────────────────────────────────────────────────
    // Writes the 4 vertices for one quad into the staging buffer.
    // Shared by all drawQuad / drawRotatedQuad overloads.
    //
    // "positions" is an array of 4 world-space corner positions:
    //   [0] = bottom-left,  [1] = bottom-right,
    //   [2] = top-right,    [3] = top-left.
    //
    // After writing, advances the vertex pointer by 4 and increments
    // the quad count.

    static void emitQuadVertices(
        glm::vec2 const (&positions)[4],
        glm::vec4 const& color,
        float texIndex,
        float tilingFactor
    )
    {
        for (std::uint32_t i = 0; i < 4; ++i)
        {
            s_vertexPtr->position     = positions[i];
            s_vertexPtr->color        = color;
            s_vertexPtr->texCoord     = QuadUVs[i];
            s_vertexPtr->texIndex     = texIndex;
            s_vertexPtr->tilingFactor = tilingFactor;
            s_vertexPtr++;
        }

        s_quadCount++;
    }


    // ── emitQuadVertices (sub-region UVs) ──────────────────────────────
    // Same as above, but instead of using the full-texture QuadUVs
    // [0,0]-[1,1], uses a custom UV rectangle defined by uvMin / uvMax.
    // This is the key function for texture atlas / sprite sheet rendering.
    //
    // The 4 UV corners are derived from the min/max bounds, matching the
    // same vertex winding order as QuadUVs: BL → BR → TR → TL.
    //
    //   uvMin = bottom-left  corner of the sub-region (in UV space).
    //   uvMax = top-right    corner of the sub-region (in UV space).
    //
    //   TL (uvMin.x, uvMax.y) ──── TR (uvMax.x, uvMax.y)
    //          │                        │
    //          │    sub-region UVs      │
    //          │                        │
    //   BL (uvMin.x, uvMin.y) ──── BR (uvMax.x, uvMin.y)

    static void emitQuadVertices(
        glm::vec2 const (&positions)[4],
        glm::vec4 const& color,
        float texIndex,
        float tilingFactor,
        glm::vec2 const& uvMin,
        glm::vec2 const& uvMax
    )
    {
        glm::vec2 const uvs[4] =
        {
            { uvMin.x, uvMin.y },       // bottom-left
            { uvMax.x, uvMin.y },       // bottom-right
            { uvMax.x, uvMax.y },       // top-right
            { uvMin.x, uvMax.y },       // top-left
        };

        for (std::uint32_t i = 0; i < 4; ++i)
        {
            s_vertexPtr->position     = positions[i];
            s_vertexPtr->color        = color;
            s_vertexPtr->texCoord     = uvs[i];
            s_vertexPtr->texIndex     = texIndex;
            s_vertexPtr->tilingFactor = tilingFactor;
            s_vertexPtr++;
        }

        s_quadCount++;
    }

    // ── init ────────────────────────────────────────────────────────────
    // Creates all GPU resources the batch renderer needs. Call once
    // after the OpenGL context is ready (typically in onAttach).
    //
    // Resources created:
    //   1. Batch shader (vertex + fragment programs).
    //   2. White texture (1x1 pixel, always in texture slot 0).
    //   3. Dynamic vertex buffer (re-filled every frame).
    //   4. Static index buffer (pre-computed quad index pattern).
    //   5. Vertex array (wires VBO + IBO + attribute layout).

    Core::VoidResult Renderer2D::init()
    {
        // ── 0. CPU staging buffer ───────────────────────────────
        // Allocate the vertex staging buffer on the heap. This is
        // done once and never reallocated. 40,000 vertices × 40
        // bytes = 1.6 MB — perfectly fine on the heap.
        s_vertexStorage.resize(MaxVertices);

        // ── 1. Shader ───────────────────────────────────────────
        // Loaded through the app's AssetManager (owns + dedups). Two
        // files, so loadKeyed with an explicit key. Returns nullptr on
        // failure (the manager logs the specifics).
        s_batchShader = Core::Application::get().assets().loadKeyed<Shader>(
            "batch_quad",
            "assets/shaders/batch_quad/vertex.glsl",
            "assets/shaders/batch_quad/fragment.glsl"
        );
        if (!s_batchShader)
            return std::unexpected(
                Core::ErrorInfo::make(
                    Core::ErrorCode::ShaderCompileFailed,
                    "batch shader failed to load"
                )
            );

        // Upload the sampler array uniform once. This tells the
        // shader "u_Textures[0] reads from unit 0, u_Textures[1]
        // reads from unit 1, ..." — the mapping never changes.
        std::array<int, MaxTextureSlots> samplers {};
        for (std::uint32_t i = 0; i < MaxTextureSlots; ++i)
            samplers[i] = static_cast<int>(i);

        s_batchShader->setIntArray("u_Textures", std::span<int const> { samplers });

        // ── 2. White texture ────────────────────────────────────
        // A single white pixel. When a quad has no texture, it uses
        // this in slot 0. The shader does: white * color = color.
        constexpr std::uint32_t whitePixel = 0xFFFF'FFFF;
        s_whiteTexture.emplace(Texture2D::create(1, 1, &whitePixel));

        // Pre-fill slot 0 with the white texture's GL ID.
        s_textureSlots[0] = s_whiteTexture->id();

        // ── 3. Dynamic vertex buffer ────────────────────────────
        // Allocated empty (no initial data). Each frame, drawQuad()
        // fills the CPU staging buffer, and flush() uploads the used
        // portion via setSubData(). The DYNAMIC flag tells GL the
        // data will change frequently.
        auto vb = VertexBuffer::createDynamic(
            MaxVertices * static_cast<std::uint32_t>(sizeof(BatchVertex))
        );

        // Describe the vertex layout so the VAO knows how to
        // interpret each 40-byte vertex.
        vb.setLayout({
            { ShaderDataType::Float2, "a_Position" },
            { ShaderDataType::Float4, "a_Color" },
            { ShaderDataType::Float2, "a_TexCoord" },
            { ShaderDataType::Float,  "a_TexIndex" },
            { ShaderDataType::Float,  "a_TilingFactor" },
        });

        // ── 4. Static index buffer ──────────────────────────────
        // Pre-compute indices for all 10,000 possible quads.
        // Each quad is two triangles sharing vertices 0 and 2:
        //
        //   3 ─── 2        indices: 0, 1, 2  (bottom-right triangle)
        //   │   / │                 0, 2, 3  (top-left triangle)
        //   │ /   │
        //   0 ─── 1
        //
        // For quad N, the base vertex is N*4, so:
        //   { N*4+0, N*4+1, N*4+2, N*4+0, N*4+2, N*4+3 }
        //
        // This pattern is the same for every quad — only the base
        // offset changes. The IBO is static (uploaded once, never
        // modified) because the pattern never changes.

        std::vector<std::uint32_t> indices(MaxIndices);
        for (std::uint32_t i = 0; i < MaxQuads; ++i)
        {
            std::uint32_t const base = i * 4;
            std::uint32_t const idx  = i * 6;

            indices[idx + 0] = base + 0;    // first triangle
            indices[idx + 1] = base + 1;
            indices[idx + 2] = base + 2;

            indices[idx + 3] = base + 0;    // second triangle
            indices[idx + 4] = base + 2;
            indices[idx + 5] = base + 3;
        }

        auto ib = IndexBuffer::create(indices);

        // ── 5. Vertex array ─────────────────────────────────────
        // Wires the VBO and IBO together. The VAO remembers the
        // attribute layout (positions, colors, UVs, etc.) so we
        // only need to bind the VAO at draw time — no per-draw
        // attribute setup.
        auto va = VertexArray::create();
        va.addVertexBuffer(vb);
        va.setIndexBuffer(ib);

        s_vertexBuffer.emplace(std::move(vb));
        s_indexBuffer.emplace(std::move(ib));
        s_vertexArray.emplace(std::move(va));

        // ── Enable alpha blending ───────────────────────────────
        // Without this, transparent pixels in textures would show
        // as opaque black. The blend function says:
        //   finalColor = srcColor * srcAlpha + dstColor * (1 - srcAlpha)
        // So a pixel with alpha=0.5 blends 50/50 with whatever is
        // already in the framebuffer behind it.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        std::println("[Ω::Renderer2D] initialized (max {} quads/batch, {} texture slots)", MaxQuads, MaxTextureSlots);

        return std::monostate {};
    }

    // ── shutdown ────────────────────────────────────────────────────────

    void Renderer2D::shutdown()
    {
        // Destroy GPU resources in reverse creation order.
        s_vertexArray.reset();
        s_indexBuffer.reset();
        s_vertexBuffer.reset();
        s_whiteTexture.reset();
        s_batchShader = nullptr;   // owned by the AssetManager; just drop our pointer

        // Free the CPU staging buffer.
        s_vertexStorage.clear();
        s_vertexStorage.shrink_to_fit();
        s_vertexPtr = nullptr;

        std::println("[Ω::Renderer2D] shut down");
    }


    // ── beginBatch ──────────────────────────────────────────────────────
    // Starts a new batch. Must be paired with endBatch().
    // Resets the staging buffer and uploads the camera matrix.

    void Renderer2D::beginBatch(Camera2D const& camera)
    {
        // Reset the write cursor to the start of the staging buffer.
        s_vertexPtr        = s_vertexStorage.data();
        s_quadCount        = 0;
        s_textureSlotIndex = 1;     // slot 0 = white tex, always present

        // Upload the camera's View-Projection matrix. The shader uses
        // this to transform world-space vertex positions into clip space.
        // We upload it once per batch, not once per quad.
        s_batchShader->setMat4("u_ViewProjection", camera.viewProjection());
    }

    // ── endBatch ────────────────────────────────────────────────────────
    // Flushes whatever is left in the staging buffer.

    void Renderer2D::endBatch()
    {
        flushAndReset();
    }

    // ── drawQuad (color only) ───────────────────────────────────────────
    // Draws an axis-aligned colored quad with no texture.
    // Uses texture slot 0 (white pixel): white * color = color.
    //
    // The 4 corners are computed directly from position + size.
    // No rotation math needed — just offset the corners.
    //
    //   position + (0, size.y) ──── position + size
    //          │                        │
    //          │       the quad         │
    //          │                        │
    //       position ──────── position + (size.x, 0)

    void Renderer2D::drawQuad(
        glm::vec2 const& position,
        glm::vec2 const& size,
        glm::vec4 const& color
    )
    {
        // If the batch is full, flush it first to make room.
        if (s_quadCount >= MaxQuads)
            flushAndReset();

        // Compute the 4 corners (bottom-left, bottom-right,
        // top-right, top-left) in world space.
        glm::vec2 const corners[4] =
        {
            position,                                           // bottom-left
            { position.x + size.x, position.y },                // bottom-right
            { position.x + size.x, position.y + size.y },       // top-right
            { position.x,          position.y + size.y },       // top-left
        };

        // texIndex = 0 (white texture), tilingFactor = 1.0
        emitQuadVertices(
            corners, 
            color, 
            0.0f, 
            1.0f
        );
    }

    // ── drawQuad (textured) ─────────────────────────────────────────────
    // Draws an axis-aligned textured quad with an optional color tint.
    // The texture is looked up in the slot array and bound at flush time.

    void Renderer2D::drawQuad(
        glm::vec2 const& position,
        glm::vec2 const& size,
        Texture2D const& texture,
        glm::vec4 const& tintColor,
        float tilingFactor
    )
    {
        if (s_quadCount >= MaxQuads)
            flushAndReset();

        // Find (or assign) a texture slot for this texture.
        // This may trigger a flush if all 32 slots are full.
        float const texIndex = findOrAssignTextureSlot(texture.id());

        glm::vec2 const corners[4] =
        {
            position,
            { position.x + size.x, position.y },
            { position.x + size.x, position.y + size.y },
            { position.x,          position.y + size.y },
        };

        emitQuadVertices(
            corners, 
            tintColor, 
            texIndex, 
            tilingFactor
        );
    }

    // ── drawQuad (sub-texture / sprite sheet) ─────────────────────────
    // Draws an axis-aligned quad using a sub-region of a texture atlas.
    // The UV rectangle comes from SubTexture2D, and the GL texture ID
    // comes from SubTexture2D::texture(). Everything else is identical
    // to the Texture2D overload above.

    void Renderer2D::drawQuad(
        glm::vec2 const& position,
        glm::vec2 const& size,
        SubTexture2D const& subTexture,
        glm::vec4 const& tintColor,
        float tilingFactor
    )
    {
        if (s_quadCount >= MaxQuads)
            flushAndReset();

        // The atlas texture goes into a slot just like any Texture2D.
        // Multiple SubTexture2D instances sharing the same atlas will
        // reuse the same slot — that's the whole point of atlasing.
        float const texIndex = findOrAssignTextureSlot(subTexture.texture().id());

        glm::vec2 const corners[4] =
        {
            position,
            { position.x + size.x, position.y },
            { position.x + size.x, position.y + size.y },
            { position.x,          position.y + size.y },
        };

        // Use the sub-region UV overload instead of the full-texture one.
        emitQuadVertices(
            corners, 
            tintColor, 
            texIndex, 
            tilingFactor,
            subTexture.uvMin(),
            subTexture.uvMax()
        );
    }

    // ── drawRotatedQuad (color only) ────────────────────────────────────
    // Draws a rotated colored quad. The rotation is applied around the
    // center of the quad (not the bottom-left corner).
    //
    // How the rotation works:
    //   1. Compute the center of the quad: position + size/2.
    //   2. Define the 4 corner offsets relative to the center:
    //        (-halfW, -halfH), (+halfW, -halfH),
    //        (+halfW, +halfH), (-halfW, +halfH).
    //   3. Rotate each offset by the angle:
    //        x' = x * cos(angle) - y * sin(angle)
    //        y' = x * sin(angle) + y * cos(angle)
    //   4. Add the center back to get world-space positions.
    //
    // This is done on the CPU (not in the shader) because each quad
    // in the batch can have a different rotation. The shader only
    // applies the camera transform, which is the same for all quads.

    void Renderer2D::drawRotatedQuad(
        glm::vec2 const& position,
        glm::vec2 const& size,
        float rotationDegrees,
        glm::vec4 const& color
    )
    {
        if (s_quadCount >= MaxQuads)
            flushAndReset();

        // Pre-compute sin/cos once (expensive trig functions).
        float const rad = glm::radians(rotationDegrees);
        float const c   = std::cos(rad);
        float const s   = std::sin(rad);

        // Quad center and half-extents.
        glm::vec2 const center = position + size * 0.5f;
        glm::vec2 const half   = size * 0.5f;

        // The 4 corner offsets from center (before rotation).
        glm::vec2 const offsets[4] =
        {
            { -half.x, -half.y },   // bottom-left
            {  half.x, -half.y },   // bottom-right
            {  half.x,  half.y },   // top-right
            { -half.x,  half.y },   // top-left
        };

        // Rotate each offset and add center to get world position.
        glm::vec2 corners[4];
        for (std::uint32_t i = 0; i < 4; ++i)
        {
            corners[i] =
            {
                center.x + offsets[i].x * c - offsets[i].y * s,
                center.y + offsets[i].x * s + offsets[i].y * c,
            };
        }

        emitQuadVertices(
            corners, 
            color, 
            0.0f, 
            1.0f
        );
    }


    // ── drawRotatedQuad (textured) ──────────────────────────────────────
    // Same rotation logic but with a texture instead of a plain color.

    void Renderer2D::drawRotatedQuad(
        glm::vec2 const& position,
        glm::vec2 const& size,
        float rotationDegrees,
        Texture2D const& texture,
        glm::vec4 const& tintColor,
        float tilingFactor
    )
    {
        if (s_quadCount >= MaxQuads)
            flushAndReset();

        float const texIndex = findOrAssignTextureSlot(texture.id());

        float const rad = glm::radians(rotationDegrees);
        float const c   = std::cos(rad);
        float const s   = std::sin(rad);

        glm::vec2 const center = position + size * 0.5f;
        glm::vec2 const half   = size * 0.5f;

        glm::vec2 const offsets[4] =
        {
            { -half.x, -half.y },
            {  half.x, -half.y },
            {  half.x,  half.y },
            { -half.x,  half.y },
        };

        glm::vec2 corners[4];
        for (std::uint32_t i = 0; i < 4; ++i)
        {
            corners[i] =
            {
                center.x + offsets[i].x * c - offsets[i].y * s,
                center.y + offsets[i].x * s + offsets[i].y * c,
            };
        }

        emitQuadVertices(
            corners, 
            tintColor, 
            texIndex, 
            tilingFactor
        );
    }

    // ── drawRotatedQuad (sub-texture / sprite sheet) ──────────────────
    // Rotated quad using a sub-region of a texture atlas.
    // Same rotation math as the other drawRotatedQuad overloads,
    // but passes the sub-region UVs instead of the full [0,1] range.

    void Renderer2D::drawRotatedQuad(
        glm::vec2 const& position,
        glm::vec2 const& size,
        float rotationDegrees,
        SubTexture2D const& subTexture,
        glm::vec4 const& tintColor,
        float tilingFactor
    )
    {
        if (s_quadCount >= MaxQuads)
            flushAndReset();

        float const texIndex = findOrAssignTextureSlot(subTexture.texture().id());

        float const rad = glm::radians(rotationDegrees);
        float const c   = std::cos(rad);
        float const s   = std::sin(rad);

        glm::vec2 const center = position + size * 0.5f;
        glm::vec2 const half   = size * 0.5f;

        glm::vec2 const offsets[4] =
        {
            { -half.x, -half.y },
            {  half.x, -half.y },
            {  half.x,  half.y },
            { -half.x,  half.y },
        };

        glm::vec2 corners[4];
        for (std::uint32_t i = 0; i < 4; ++i)
        {
            corners[i] =
            {
                center.x + offsets[i].x * c - offsets[i].y * s,
                center.y + offsets[i].x * s + offsets[i].y * c,
            };
        }

        emitQuadVertices(
            corners, 
            tintColor, 
            texIndex, 
            tilingFactor,
            subTexture.uvMin(), 
            subTexture.uvMax()
        );
    }

    // ── Stats ───────────────────────────────────────────────────────────

    Renderer2D::Stats Renderer2D::stats()
    {
        return s_stats;
    }

    void Renderer2D::resetStats()
    {
        s_stats = {};
    }

} // namespace Renderer