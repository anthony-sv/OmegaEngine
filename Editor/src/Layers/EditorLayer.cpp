module;

#include "imgui.h"
#include "ImGuizmo.h"                          // after imgui.h -- depends on it
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"        // translate / rotate / scale
#include "glm/gtc/type_ptr.hpp"                // value_ptr (mat4 -> float*)
#include "IconsFontAwesome6.h"
#ifdef _WIN32
#   include "TitlebarState.hpp"
#endif

module EditorLayer;

import Engine.Core;
import Engine.Renderer;
import Engine.ECS;
import Engine.Scene;
import Engine.Systems;
import Engine.Scripting;
import NodeEditor;            // visual scripting node-graph panel

namespace Editor
{

    namespace
    {
        // ── Duplicate: copy every known component ─────────────
        // There is no C++ reflection, so a clone mirrors the inspector's
        // component set by hand. Add new components here when the engine
        // grows them. add<T>(const T&) copy-constructs into the new entity.
        void cloneInto(Engine::ECS::Entity src, Engine::ECS::Entity dst)
        {
            using namespace Engine::ECS;
            if (src.has<NameComponent>())     dst.add<NameComponent>(src.get<NameComponent>());
            if (src.has<Transform>())         dst.add<Transform>(src.get<Transform>());
            if (src.has<SpriteRenderer>())    dst.add<SpriteRenderer>(src.get<SpriteRenderer>());
            if (src.has<Velocity2D>())        dst.add<Velocity2D>(src.get<Velocity2D>());
            if (src.has<SpriteAnimation>())   dst.add<SpriteAnimation>(src.get<SpriteAnimation>());
            if (src.has<TextureAnimation>())  dst.add<TextureAnimation>(src.get<TextureAnimation>());
            if (src.has<RigidBody2D>())       dst.add<RigidBody2D>(src.get<RigidBody2D>());
            if (src.has<BoxCollider2D>())     dst.add<BoxCollider2D>(src.get<BoxCollider2D>());
            if (src.has<CircleCollider2D>())  dst.add<CircleCollider2D>(src.get<CircleCollider2D>());
            if (src.has<PolygonCollider2D>()) dst.add<PolygonCollider2D>(src.get<PolygonCollider2D>());
            if (src.has<MarkerComponent>())   dst.add<MarkerComponent>(src.get<MarkerComponent>());
            if (src.has<ScriptComponent>())   dst.add<ScriptComponent>(src.get<ScriptComponent>());
            if (src.has<GraphComponent>())    dst.add<GraphComponent>(src.get<GraphComponent>());
            if (src.has<ParticleEmitterComponent>()) dst.add<ParticleEmitterComponent>(src.get<ParticleEmitterComponent>());
        }

        // ── Visual-scripting node panel ────────────────────────
        // File-scope (not function-local) so BOTH onImGuiRender (drawing) and
        // the inspector's "Edit Graph" button reach the same panel. Its
        // imgui-node-editor context is deliberately leaked at exit (ImGui is
        // torn down before static dtors run).
        Nodes::NodeEditorPanel g_nodePanel;
        bool                   g_showNodes = true;

        // ── Viewport math: screen -> world + picking  ──────────
        // The viewport image occupies the rect [mn, mx] in ImGui SCREEN
        // coordinates. A click maps back through the camera's view-projection
        // (vp) to world space. Y is flipped: screen grows DOWN, but the FBO is
        // drawn flipped so world-up is screen-up. (The gizmo itself is handled
        // by ImGuizmo, which takes the camera's separate view/projection.)

        glm::vec2 screenToWorld(ImVec2 s, glm::mat4 const& vp, ImVec2 mn, ImVec2 mx)
        {
            float const u = (s.x - mn.x) / (mx.x - mn.x);
            float const v = (s.y - mn.y) / (mx.y - mn.y);
            glm::vec4 const world =
                glm::inverse(vp) * glm::vec4 { u * 2.0f - 1.0f, 1.0f - v * 2.0f, 0.0f, 1.0f };
            return { world.x, world.y };
        }

        // World -> screen (the inverse of the above), for drawing overlays like
        // the painting brush highlight onto the viewport.
        ImVec2 worldToScreen(glm::vec2 world, glm::mat4 const& vp, ImVec2 mn, ImVec2 mx)
        {
            glm::vec4 const clip = vp * glm::vec4 { world.x, world.y, 0.0f, 1.0f };
            glm::vec2 const ndc  { clip.x / clip.w, clip.y / clip.w };
            return {
                mn.x + (ndc.x * 0.5f + 0.5f) * (mx.x - mn.x),
                mn.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * (mx.y - mn.y),
            };
        }

        // Point-in-oriented-box test for picking. Transform is center +
        // rotation (deg) + full-size scale. Rotate the point into the box's
        // local frame, then compare against half-extents.
        bool hitTest(Engine::ECS::Transform const& t, glm::vec2 p)
        {
            glm::vec2 const d   = p - t.position;
            float     const rad = glm::radians(-t.rotation);
            float     const c   = std::cos(rad);
            float     const s   = std::sin(rad);
            glm::vec2 const local { d.x * c - d.y * s, d.x * s + d.y * c };
            glm::vec2 const half = t.scale * 0.5f;
            return std::abs(local.x) <= half.x && std::abs(local.y) <= half.y;
        }

        // Draw editable ImGui fields for each component the entity has.
        // There is no C++ reflection, so this is the standard hand-written
        // dispatch: check has<T>(), then draw fields bound to get<T>().
        // get<T>() returns a reference, so the widgets edit the component
        // in place -- changes are live in the viewport next frame.
        void drawInspector(Engine::ECS::Entity entity)
        {
            using namespace Engine::ECS;

            if (entity.has<NameComponent>())
            {
                auto& name = entity.get<NameComponent>();
                char buf[128] = {};
                name.name.copy(buf, sizeof(buf) - 1);
                if (ImGui::InputText("Name", buf, sizeof(buf)))
                    name.name = buf;
                ImGui::Separator();
            }

            if (entity.has<Transform>())
            {
                auto& t = entity.get<Transform>();
                if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::DragFloat2("Position", &t.position.x, 0.05f);
                    ImGui::DragFloat ("Rotation", &t.rotation,   1.0f);
                    ImGui::DragFloat2("Scale",    &t.scale.x,    0.05f, 0.01f, 100.0f);
                }
            }

            if (entity.has<SpriteRenderer>())
            {
                auto& s = entity.get<SpriteRenderer>();
                if (ImGui::CollapsingHeader("Sprite Renderer", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::ColorEdit4("Color",  &s.color.x);
                    ImGui::DragFloat ("Tiling", &s.tilingFactor, 0.1f, 0.0f, 100.0f);
                    ImGui::TextDisabled(s.texture ? "Texture: set" : "Texture: none");
                }
            }

            if (entity.has<Velocity2D>())
            {
                auto& v = entity.get<Velocity2D>();
                if (ImGui::CollapsingHeader("Velocity2D", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::DragFloat2("Linear",  &v.linear.x, 0.05f);
                    ImGui::DragFloat ("Angular", &v.angular,  1.0f);
                }
            }

            if (entity.has<SpriteAnimation>())
            {
                auto& a = entity.get<SpriteAnimation>();
                if (ImGui::CollapsingHeader("Sprite Animation", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::Text("Frame %u / %zu", a.currentFrame + 1u, a.frames.size());
                    ImGui::DragFloat("Frame Duration", &a.frameDuration, 0.01f, 0.0f, 10.0f);
                    ImGui::Checkbox("Looping", &a.looping);
                    ImGui::Checkbox("Playing", &a.playing);
                }
            }

            if (entity.has<TextureAnimation>())
            {
                auto& a = entity.get<TextureAnimation>();
                if (ImGui::CollapsingHeader("Texture Animation", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& assets = Engine::Core::Application::get().assets();

                    // Frame list: each row is a whole-texture path + remove.
                    int removeAt = -1;
                    for (std::size_t i = 0; i < a.framePaths.size(); ++i)
                    {
                        ImGui::PushID(static_cast<int>(i));
                        ImGui::Text("%zu: %s", i, a.framePaths[i].c_str());
                        ImGui::SameLine();
                        if (ImGui::SmallButton("X")) removeAt = static_cast<int>(i);
                        ImGui::PopID();
                    }
                    if (removeAt >= 0)
                    {
                        a.framePaths.erase(a.framePaths.begin() + removeAt);
                        if (static_cast<std::size_t>(removeAt) < a.frames.size())
                            a.frames.erase(a.frames.begin() + removeAt);
                    }

                    static char addPath[260] = {};
                    ImGui::InputText("New frame path", addPath, sizeof(addPath));
                    if (ImGui::Button("Add Frame") && addPath[0] != '\0')
                    {
                        a.framePaths.emplace_back(addPath);
                        a.frames.push_back(assets.load<Engine::Renderer::Texture2D>(addPath));
                        addPath[0] = '\0';
                    }

                    ImGui::DragFloat("Frame Duration", &a.frameDuration, 0.01f, 0.0f, 10.0f);
                    ImGui::Checkbox("Looping", &a.looping);
                    ImGui::Checkbox("Playing", &a.playing);
                }
            }

            // ── Physics ─────────────────────────────────────────────────

            if (entity.has<RigidBody2D>())
            {
                auto& rb = entity.get<RigidBody2D>();
                if (ImGui::CollapsingHeader("Rigid Body 2D", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    char const* const kinds[] = { "Static", "Dynamic", "Kinematic" };
                    int kind = static_cast<int>(rb.type);
                    if (ImGui::Combo("Body Type", &kind, kinds, 3))
                        rb.type = static_cast<RigidBody2D::BodyType>(kind);

                    ImGui::DragFloat ("Mass",          &rb.mass,         0.05f, 0.0f, 1000.0f);
                    ImGui::DragFloat ("Gravity Scale", &rb.gravityScale, 0.05f, -10.0f, 10.0f);
                    ImGui::Checkbox  ("Fixed Rotation", &rb.fixedRotation);
                }
            }

            // Shared material/trigger editor for any collider.
            auto material = [](float& density, float& friction, float& restitution, bool& isTrigger)
            {
                ImGui::DragFloat("Density",     &density,     0.05f, 0.0f, 100.0f);
                ImGui::DragFloat("Friction",    &friction,    0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Restitution", &restitution, 0.01f, 0.0f, 1.0f);
                ImGui::Checkbox ("Is Trigger",  &isTrigger);
            };

            if (entity.has<BoxCollider2D>())
            {
                auto& c = entity.get<BoxCollider2D>();
                if (ImGui::CollapsingHeader("Box Collider 2D", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::DragFloat2("Size##box",   &c.size.x,   0.05f, 0.01f, 100.0f);
                    ImGui::DragFloat2("Offset##box", &c.offset.x, 0.05f);
                    material(c.density, c.friction, c.restitution, c.isTrigger);
                }
            }

            if (entity.has<CircleCollider2D>())
            {
                auto& c = entity.get<CircleCollider2D>();
                if (ImGui::CollapsingHeader("Circle Collider 2D", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::DragFloat ("Radius##cir", &c.radius,   0.05f, 0.01f, 100.0f);
                    ImGui::DragFloat2("Offset##cir", &c.offset.x, 0.05f);
                    material(c.density, c.friction, c.restitution, c.isTrigger);
                }
            }

            if (entity.has<PolygonCollider2D>())
            {
                auto& c = entity.get<PolygonCollider2D>();
                if (ImGui::CollapsingHeader("Polygon Collider 2D", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    // Box2D convex polygons take 3..8 points. Each row edits a
                    // point; X removes it (kept at >= 3); Add Point appends one.
                    int removeAt = -1;
                    for (std::size_t i = 0; i < c.points.size(); ++i)
                    {
                        ImGui::PushID(static_cast<int>(i));
                        ImGui::DragFloat2("Point", &c.points[i].x, 0.05f);
                        if (c.points.size() > 3)
                        {
                            ImGui::SameLine();
                            if (ImGui::SmallButton("X")) removeAt = static_cast<int>(i);
                        }
                        ImGui::PopID();
                    }
                    if (removeAt >= 0)
                        c.points.erase(c.points.begin() + removeAt);

                    if (c.points.size() < 8 && ImGui::SmallButton("+ Add Point"))
                        c.points.push_back(c.points.empty() ? glm::vec2 { 0.0f, 0.0f }
                                                            : c.points.back());

                    material(c.density, c.friction, c.restitution, c.isTrigger);
                }
            }

            // ── Add Component ───────────────────────────────────────────

            if (entity.has<TilemapComponent>())
            {
                auto& m = entity.get<TilemapComponent>();
                if (ImGui::CollapsingHeader("Tilemap", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto& assets = Engine::Core::Application::get().assets();

                    char const* const modes[] = { "Atlas", "Collection" };
                    int mode = static_cast<int>(m.source);
                    if (ImGui::Combo("Source", &mode, modes, 2))
                        m.source = static_cast<TilemapComponent::Source>(mode);

                    ImGui::DragFloat("Tile World Size", &m.tileWorldSize, 0.01f, 0.05f, 10.0f);

                    // Resize the grid, preserving the cells that still fit.
                    int dims[2] = { m.dimensions.x, m.dimensions.y };
                    if (ImGui::DragInt2("Dimensions", dims, 0.2f, 1, 1024))
                    {
                        int const nw = std::max(1, dims[0]);
                        int const nh = std::max(1, dims[1]);
                        std::vector<int> grid(static_cast<std::size_t>(nw) * nh, -1);
                        for (int y = 0; y < std::min(nh, m.dimensions.y); ++y)
                            for (int x = 0; x < std::min(nw, m.dimensions.x); ++x)
                                grid[static_cast<std::size_t>(y) * nw + x] = m.at(x, y);
                        m.tiles      = std::move(grid);
                        m.dimensions = { nw, nh };
                    }

                    if (m.source == TilemapComponent::Source::Atlas)
                    {
                        char path[260] = {};
                        m.atlasPath.copy(path, sizeof(path) - 1);
                        if (ImGui::InputText("Atlas Path", path, sizeof(path)))
                            m.atlasPath = path;

                        // Resolve the texture when the field is COMMITTED (Enter
                        // or click-away) or via the Reload button -- not per
                        // keystroke, so a half-typed path doesn't spam failures.
                        // The AssetManager caches, so a valid path loads once.
                        bool const committed = ImGui::IsItemDeactivatedAfterEdit();
                        ImGui::SameLine();
                        bool const reload = ImGui::SmallButton("Reload");
                        if (committed || reload)
                            m.atlas = m.atlasPath.empty()
                                ? nullptr
                                : assets.load<Engine::Renderer::Texture2D>(m.atlasPath);

                        int cell[2] = { m.tilePixelSize.x, m.tilePixelSize.y };
                        if (ImGui::DragInt2("Cell Size (px)", cell, 1, 1, 1024))
                            m.tilePixelSize = { std::max(1, cell[0]), std::max(1, cell[1]) };

                        if (m.atlas == nullptr && !m.atlasPath.empty())
                            ImGui::TextColored({ 0.9f, 0.5f, 0.3f, 1.0f }, "texture not loaded (check path)");
                    }
                    else // Collection: editable list of tile defs
                    {
                        ImGui::TextDisabled("Tiles (%zu)", m.tileDefs.size());
                        int removeAt = -1;
                        for (std::size_t i = 0; i < m.tileDefs.size(); ++i)
                        {
                            ImGui::PushID(static_cast<int>(i));
                            auto& d = m.tileDefs[i];
                            ImGui::Text("%zu: %s", i, d.texturePath.c_str());
                            int fp[2] = { d.footprint.x, d.footprint.y };
                            if (ImGui::DragInt2("Footprint", fp, 0.1f, 1, 16))
                                d.footprint = { std::max(1, fp[0]), std::max(1, fp[1]) };
                            ImGui::SameLine();
                            if (ImGui::SmallButton("X")) removeAt = static_cast<int>(i);
                            ImGui::PopID();
                        }
                        if (removeAt >= 0)
                            m.tileDefs.erase(m.tileDefs.begin() + removeAt);

                        static char addPath[260] = {};
                        static int  addFp[2] = { 1, 1 };
                        ImGui::InputText("New tile path", addPath, sizeof(addPath));
                        ImGui::DragInt2("New footprint", addFp, 0.1f, 1, 16);
                        if (ImGui::Button("Add Tile") && addPath[0] != '\0')
                        {
                            TilemapComponent::TileDef d;
                            d.texturePath = addPath;
                            d.footprint   = { std::max(1, addFp[0]), std::max(1, addFp[1]) };
                            d.texture     = assets.load<Engine::Renderer::Texture2D>(d.texturePath);
                            m.tileDefs.push_back(std::move(d));
                            addPath[0] = '\0';
                        }
                    }

                    // Solid tiles (which ids generate collision).
                    static int solidId = 0;
                    ImGui::SetNextItemWidth(120.0f);
                    ImGui::InputInt("##solidid", &solidId);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Toggle solid"))
                    {
                        if (auto it = std::ranges::find(m.solidTiles, solidId); it == m.solidTiles.end())
                            m.solidTiles.push_back(solidId);
                        else
                            m.solidTiles.erase(it);
                    }
                    if (!m.solidTiles.empty())
                    {
                        std::string s;
                        for (int id : m.solidTiles) s += std::to_string(id) + ' ';
                        ImGui::TextWrapped("Solid ids: %s", s.c_str());
                    }
                }
            }

            if (entity.has<MarkerComponent>())
            {
                auto& mk = entity.get<MarkerComponent>();
                if (ImGui::CollapsingHeader("Marker", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    char const* const types[] = { "SpawnPoint", "Trigger", "NPC", "Item", "CameraBound" };
                    int kind = static_cast<int>(mk.type);
                    if (ImGui::Combo("Type", &kind, types, 5))
                        mk.type = static_cast<MarkerType>(kind);

                    char tag[128] = {};
                    mk.tag.copy(tag, sizeof(tag) - 1);
                    if (ImGui::InputText("Tag", tag, sizeof(tag)))
                        mk.tag = tag;
                }
            }

            if (entity.has<ScriptComponent>())
            {
                auto& sc = entity.get<ScriptComponent>();
                if (ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    // Pick from the Script subclasses discovered in the project's
                    // Game.dll. The current value still shows even if it isn't in
                    // the list yet (so it round-trips through the scene file).
                    auto const classes = Engine::Scripting::ScriptHost::instance().scriptClasses();
                    std::string const preview = sc.className.empty() ? "(none)" : sc.className;
                    if (ImGui::BeginCombo("Class", preview.c_str()))
                    {
                        if (ImGui::Selectable("(none)", sc.className.empty()))
                            sc.className.clear();
                        for (auto const& cls : classes)
                        {
                            bool const selected = (cls == sc.className);
                            if (ImGui::Selectable(cls.c_str(), selected))
                                sc.className = cls;
                            if (selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    if (classes.empty())
                        ImGui::TextDisabled("No scripts compiled yet — save a .cs in the project's scripts/.");

                    // Per-field editors for the chosen class's exposed fields
                    // (public / [SerializeField]). Edits are stored as strings on
                    // the ScriptComponent (saved in the scene; the C# side converts
                    // to the real type). A field left untouched keeps its default.
                    using FieldType = Engine::Scripting::ScriptHost::ScriptFieldType;
                    auto const toF = [](std::string const& s) { float v = 0.0f; std::from_chars(s.data(), s.data() + s.size(), v); return v; };
                    auto const toI = [](std::string const& s) { int   v = 0;    std::from_chars(s.data(), s.data() + s.size(), v); return v; };

                    for (auto const& f : Engine::Scripting::ScriptHost::instance().describeFields(sc.className))
                    {
                        auto const it     = sc.fields.find(f.name);
                        std::string value = (it != sc.fields.end()) ? it->second : f.value;   // override or default
                        bool changed      = false;

                        switch (f.type)
                        {
                            case FieldType::Float:
                            {
                                float v = toF(value);
                                if (ImGui::DragFloat(f.name.c_str(), &v, 0.01f)) { value = std::format("{}", v); changed = true; }
                                break;
                            }
                            case FieldType::Int:
                            {
                                int v = toI(value);
                                if (ImGui::DragInt(f.name.c_str(), &v)) { value = std::format("{}", v); changed = true; }
                                break;
                            }
                            case FieldType::Bool:
                            {
                                bool v = (value == "true");
                                if (ImGui::Checkbox(f.name.c_str(), &v)) { value = v ? "true" : "false"; changed = true; }
                                break;
                            }
                            case FieldType::String:
                            {
                                char buf[256] = {};
                                value.copy(buf, sizeof(buf) - 1);
                                if (ImGui::InputText(f.name.c_str(), buf, sizeof(buf))) { value = buf; changed = true; }
                                break;
                            }
                            case FieldType::Vec2:
                            {
                                float v[2] = { 0.0f, 0.0f };
                                if (auto const sp = value.find(' '); sp != std::string::npos)
                                {
                                    std::from_chars(value.data(), value.data() + sp, v[0]);
                                    std::from_chars(value.data() + sp + 1, value.data() + value.size(), v[1]);
                                }
                                if (ImGui::DragFloat2(f.name.c_str(), v, 0.01f)) { value = std::format("{} {}", v[0], v[1]); changed = true; }
                                break;
                            }
                        }

                        if (changed)
                            sc.fields[f.name] = value;
                    }
                }
            }

            if (entity.has<GraphComponent>())
            {
                auto& gc = entity.get<GraphComponent>();
                if (ImGui::CollapsingHeader("Graph", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    // The name is the whole binding: graphs/<name>.ngraph in the
                    // editor, class "Game.<name>" at runtime -- so it must be a
                    // valid C# identifier.
                    char buf[128] = {};
                    gc.graphName.copy(buf, sizeof(buf) - 1);
                    if (ImGui::InputText("Graph##name", buf, sizeof(buf)))
                        gc.graphName = buf;

                    if (gc.graphName.empty())
                        ImGui::TextDisabled("Name the graph to bind it (becomes class Game.<name>).");
                    else if (ImGui::Button("Edit Graph"))
                    {
                        g_nodePanel.open(gc.graphName);
                        g_showNodes = true;
                    }
                }
            }

            if (entity.has<ParticleEmitterComponent>())
            {
                auto& pe = entity.get<ParticleEmitterComponent>();
                if (ImGui::CollapsingHeader("Particle Emitter", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::Checkbox("Emitting", &pe.emitting);
                    ImGui::DragFloat("Rate", &pe.rate, 0.5f, 0.0f, 500.0f);

                    float life[2] = { pe.lifetimeMin, pe.lifetimeMax };
                    if (ImGui::DragFloat2("Lifetime min/max", life, 0.02f, 0.01f, 30.0f))
                    {
                        pe.lifetimeMin = life[0];
                        pe.lifetimeMax = std::max(life[0], life[1]);
                    }
                    float speed[2] = { pe.speedMin, pe.speedMax };
                    if (ImGui::DragFloat2("Speed min/max", speed, 0.02f))
                    {
                        pe.speedMin = speed[0];
                        pe.speedMax = std::max(speed[0], speed[1]);
                    }

                    ImGui::DragFloat("Direction", &pe.direction, 1.0f, -360.0f, 360.0f);
                    ImGui::DragFloat("Spread", &pe.spread, 1.0f, 0.0f, 180.0f);
                    ImGui::DragFloat2("Gravity", &pe.gravity.x, 0.05f);
                    ImGui::ColorEdit4("Start color", &pe.startColor.x);
                    ImGui::ColorEdit4("End color", &pe.endColor.x);
                    ImGui::DragFloat("Start size", &pe.startSize, 0.005f, 0.0f, 10.0f);
                    ImGui::DragFloat("End size", &pe.endSize, 0.005f, 0.0f, 10.0f);
                    ImGui::DragInt("Max particles", &pe.maxParticles, 1.0f, 1, 10000);

                    // Texture path: empty = a soft colored quad. Changing it
                    // drops the cached pointer so the system re-resolves.
                    char buf[256] = {};
                    pe.texturePath.copy(buf, sizeof(buf) - 1);
                    if (ImGui::InputText("Texture", buf, sizeof(buf)))
                    {
                        pe.texturePath = buf;
                        pe.texture     = nullptr;
                    }
                }
            }

            // ── Add Component ───────────────────────────────────────────

            ImGui::Separator();
            if (ImGui::Button("Add Component"))
                ImGui::OpenPopup("AddComponent");

            if (ImGui::BeginPopup("AddComponent"))
            {
                if (!entity.has<Transform>()         && ImGui::MenuItem("Transform"))           entity.add<Transform>();
                if (!entity.has<SpriteRenderer>()    && ImGui::MenuItem("Sprite Renderer"))     entity.add<SpriteRenderer>();
                if (!entity.has<SpriteAnimation>()   && ImGui::MenuItem("Sprite Animation"))    entity.add<SpriteAnimation>();
                if (!entity.has<TextureAnimation>()  && ImGui::MenuItem("Texture Animation"))   entity.add<TextureAnimation>();
                if (!entity.has<Velocity2D>()        && ImGui::MenuItem("Velocity2D"))          entity.add<Velocity2D>();
                if (!entity.has<TilemapComponent>()  && ImGui::MenuItem("Tilemap"))             entity.add<TilemapComponent>();
                if (!entity.has<MarkerComponent>()   && ImGui::MenuItem("Marker"))              entity.add<MarkerComponent>();
                if (!entity.has<ScriptComponent>()   && ImGui::MenuItem("Script"))              entity.add<ScriptComponent>();
                if (!entity.has<GraphComponent>()    && ImGui::MenuItem("Graph"))               entity.add<GraphComponent>();
                if (!entity.has<ParticleEmitterComponent>() && ImGui::MenuItem("Particle Emitter")) entity.add<ParticleEmitterComponent>();
                if (!entity.has<RigidBody2D>()       && ImGui::MenuItem("Rigid Body 2D"))       entity.add<RigidBody2D>();

                // A physics body uses ONE collider (the PhysicsSystem picks
                // box, else circle, else polygon), so only offer to add one if
                // the entity has none yet.
                bool const hasCollider = entity.has<BoxCollider2D>()
                                      || entity.has<CircleCollider2D>()
                                      || entity.has<PolygonCollider2D>();
                if (!hasCollider && ImGui::MenuItem("Box Collider 2D"))     entity.add<BoxCollider2D>();
                if (!hasCollider && ImGui::MenuItem("Circle Collider 2D"))  entity.add<CircleCollider2D>();
                if (!hasCollider && ImGui::MenuItem("Polygon Collider 2D"))
                {
                    // Seed the minimal valid convex polygon -- a triangle (Box2D
                    // needs >= 3 points). Edit / add points in the inspector.
                    auto& poly = entity.add<PolygonCollider2D>();
                    poly.points = { { 0.0f, 0.5f }, { -0.5f, -0.5f }, { 0.5f, -0.5f } };
                }
                ImGui::EndPopup();
            }
        }
    }

    EditorLayer::EditorLayer(Engine::Scene::Project project)
        : ILayer    { "Ω::EditorLayer" }
        , m_project { std::move(project) }
    {}

    void EditorLayer::onAttach()
    {
        std::println("[Ω::EditorLayer] attached");

        // ── Batch renderer ─────────────────────────────────────────
        // Required GPU infrastructure (shader, white pixel, VBO/IBO/VAO).
        // NOT a texture asset -- this must run before any draw call.
        auto initResult = Engine::Renderer::Renderer2D::init();
        if (!initResult)
        {
            std::println(std::cerr, "[Ω::EditorLayer] Renderer2D init failed: {}", initResult.error().message);
            return;
        }

        // ── Framebuffer (FBO) ──────────────────────────────────────
        auto fbResult = Engine::Renderer::Framebuffer::create(1280, 720);
        if (fbResult)
            m_framebuffer.emplace(std::move(*fbResult));
        else
            std::println(std::cerr, "[Ω::EditorLayer] framebuffer failed: {}", fbResult.error().message);

        // ── Camera ─────────────────────────────────────────────────
        // size=2.0 means the view spans [-2,+2] vertically — enough
        // to see the test quads spread around the origin.
        m_camera.emplace(16.0f / 9.0f, 2.0f);

        // Console feedback when physics bodies collide during Play.
        {
            auto nameOf = [](Engine::ECS::Entity e) -> std::string
            {
                return (e.valid() && e.has<Engine::ECS::NameComponent>())
                     ? e.get<Engine::ECS::NameComponent>().name : std::string { "?" };
            };
            Engine::Core::Application::get().eventBus().subscribe<Engine::Physics::CollisionEnterEvent>(
                [nameOf](Engine::Physics::CollisionEnterEvent const& e)
                {
                    std::println("[Ω::Physics] collision  {} <-> {}", nameOf(e.a), nameOf(e.b));
                });
        }

        // ── ECS world ──────────────────────────────────────
        // The editor edits a scene from the OPEN PROJECT. We open its startup
        // scene; its entities are pure DATA loaded from the project's
        // scenes/<name>.json in the onEnter hook below. (Switching to other
        // scenes in the project is the next step, P3.)
        // One world per scene the project declares (Scene menu switches them).
        // The setup is IDEMPOTENT: it only loads + wires systems the FIRST
        // time a world is entered (systemCount == 0). Switching away does NOT
        // clear the world, so returning to a scene preserves your in-editor
        // edits (re-entry skips the reload).
        for (auto const& name : m_project.scenes())
            m_sceneManager.create(name).setOnEnter(
                [this](Engine::Scene::World& w) { setupWorld(w); });

        m_sceneManager.switchTo(m_project.startupScene());

        logConsole("opened project '" + m_project.name() + "'  (" +
                   std::to_string(m_project.scenes().size()) + " scenes)");
        logConsole(ICON_FA_LIST " scene -> " + m_project.startupScene());
    }

    void EditorLayer::onDetach()
    {
        m_framebuffer.reset();
        m_camera.reset();

        // Shutdown the batch renderer (frees its internal GPU resources).
        Engine::Renderer::Renderer2D::shutdown();

        std::println("[Ω::EditorLayer] detached");
    }

    void EditorLayer::onUpdate(float dt)
    {
        using Engine::Core::Key;
        using Input = Engine::Core::Input;

        // A follow-cam script (if any) steers this camera in Play mode; bind it
        // before ticking. (In Edit mode systems don't tick, so it stays put.)
        // NOTE: the scene manager is intentionally NOT bound here -- a script's
        // Scene.Load would hijack the editor's active scene and break Play/Stop.
        if (m_camera)
            Engine::Scripting::ScriptHost::instance().bindCamera(&*m_camera);

        // Always pump the manager so the scene LOADS (the deferred initial
        // switch runs enter()) and actions route -- but only TICK the world's
        // systems in Play mode, so Edit mode stays static for authoring.
        m_sceneManager.onUpdate(dt, m_playing);

        // Apply a deferred scene-world removal once the switch-away has taken
        // effect (delete/rename). We never free the world that was
        // active THIS frame -- only after the SceneManager has activated the
        // replacement scene.
        if (m_pendingSceneRemoval)
        {
            auto const* active = m_sceneManager.active();
            if (active && active->name() != *m_pendingSceneRemoval)
            {
                m_sceneManager.remove(*m_pendingSceneRemoval);
                m_pendingSceneRemoval.reset();
            }
        }

        // NOTE: edge-triggered shortcuts (Ctrl+S/O/P/D, Del, 1/2/3) are handled
        // in handleShortcuts() (per render frame), NOT here -- onUpdate runs on
        // the fixed-timestep tick, which is skipped on fast frames, so taps were
        // being missed. Only the CONTINUOUS camera controls below stay here,
        // where integrating over fixedDt is correct.
        bool const ctrl = Input::isKeyDown(Key::LeftControl);

        // In Play mode the GAME owns the keyboard (a script may drive WASD and the
        // camera), so the editor's own camera controls stand down.
        if (!m_camera || !m_viewportHovered || m_playing) return;

        // ── Camera controls ─────────────────────────────────────────
        // WASD = pan, Q/E = rotate, mouse wheel = zoom. Driven by the
        // engine's Input service. Gated on m_viewportHovered above so typing
        // in other panels doesn't move the camera.

        constexpr float panSpeed    = 2.0f;     // world units per second
        constexpr float rotateSpeed = 90.0f;    // degrees per second

        auto position = m_camera->position();
        auto rotation = m_camera->rotation();

        // Pan — move in camera-local axes so WASD feels correct
        // even when the camera is rotated.
        float const rad = glm::radians(rotation);
        float const c   = std::cos(rad);
        float const s   = std::sin(rad);

        float const step = panSpeed * dt / m_camera->zoom();    // zoom-compensated

        // Skip WASD pan while Ctrl is held so Ctrl+S / Ctrl+D (save / duplicate)
        // don't also drag the camera.
        if (!ctrl)
        {
            if (Input::isKeyDown(Key::W))  { position.x -= s * step; position.y += c * step; }
            if (Input::isKeyDown(Key::S))  { position.x += s * step; position.y -= c * step; }
            if (Input::isKeyDown(Key::A))  { position.x -= c * step; position.y -= s * step; }
            if (Input::isKeyDown(Key::D))  { position.x += c * step; position.y += s * step; }
        }

        if (Input::isKeyDown(Key::Q))  rotation += rotateSpeed * dt;
        if (Input::isKeyDown(Key::E))  rotation -= rotateSpeed * dt;

        // Scroll wheel zoom is handled in onImGuiRender() because
        // ImGui::GetIO().MouseWheel isn't populated until NewFrame(),
        // which runs after onUpdate.

        m_camera->setPosition(position);
        m_camera->setRotation(rotation);
    }

    void EditorLayer::onRender(float alpha)
    {
        if (!m_framebuffer || !m_camera) return;

        // Reset per-frame stats so the Console panel shows this frame's numbers.
        Engine::Renderer::Renderer2D::resetStats();

        // Bind the FBO — all draw calls go into its color texture.
        m_framebuffer->bind();
        Engine::Renderer::RenderCommand::setClearColor(0.12f, 0.12f, 0.15f, 1.0f);
        Engine::Renderer::RenderCommand::clear();

        // ── ECS render pass ─────────────────────────────────────────
        // Hand the ACTIVE world's registry to the RenderSystem. It opens
        // its own batch, iterates every entity with Transform +
        // SpriteRenderer, draws each, and flushes. The EditorLayer no
        // longer owns a Registry — it asks the SceneManager for the active
        // world. The FBO is still bound here, so these quads land in the
        // same off-screen target as the demo batch.
        if (auto* world = m_sceneManager.active())
        {
            // Tilemaps are the background layer -- drawn first, sprites on top.
            Engine::Systems::RenderSystem::renderTilemaps(world->registry(), *m_camera);

            // Cell-grid overlay (authoring aid), over the tiles, under sprites.
            if (m_showGrid)
                Engine::Systems::RenderSystem::renderTilemapGrid(world->registry(), *m_camera);

            Engine::Systems::RenderSystem::render(world->registry(), *m_camera, alpha);

            // Particles -- above sprites, below text. (They only MOVE in Play,
            // when the systems tick; in edit mode the pools just sit still.)
            Engine::Systems::ParticleSystem::render(world->registry(), *m_camera);

            // Text (HUD / menus) -- foreground, on top of sprites.
            Engine::Systems::RenderSystem::renderText(world->registry(), *m_camera);

            // Collider wireframe overlay (authoring aid). Drawn after the
            // sprites, into the same FBO, so it sits on top.
            if (m_showColliders)
                Engine::Systems::RenderSystem::renderColliders(world->registry(), *m_camera, alpha);

            // Marker icons (spawn / trigger / etc.) so invisible markers are
            // visible to author.
            if (m_showMarkers)
                Engine::Systems::RenderSystem::renderMarkers(world->registry(), *m_camera);
        }

        // F2 -> screenshot the GAME VIEW (the viewport FBO, no editor chrome),
        // captured while the FBO is still bound.
        if (Engine::Core::Input::wasKeyPressed(Engine::Core::Key::F2))
        {
            auto const path = Engine::Renderer::Screenshot::timestamped();
            if (auto r = Engine::Renderer::Screenshot::capture(
                    path, 0, 0,
                    static_cast<int>(m_framebuffer->width()),
                    static_cast<int>(m_framebuffer->height())); !r)
                std::println(std::cerr, "[Ω::EditorLayer] screenshot failed: {}", r.error().message);
            else
                logConsole(ICON_FA_CAMERA " screenshot -> " + path.string());
        }

        m_framebuffer->unbind();
    }

    void EditorLayer::onImGuiRender()
    {
        // ImGuizmo is stateful per-frame: reset it right after ImGui::NewFrame
        // (which the ImGuiLayer ran before this) and before any Manipulate call.
        ImGuizmo::BeginFrame();

        // Edge-triggered keyboard shortcuts, polled once per render frame here
        // (see handleShortcuts() for why not in onUpdate).
        handleShortcuts();

        // Ω::Fullscreen DockSpace host ────────────────────────────────
        auto const* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        constexpr ImGuiWindowFlags hostFlags =
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_MenuBar;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
        ImGui::Begin("##Ω_DockSpaceHost", nullptr, hostFlags);
        ImGui::PopStyleVar(3);

        ImGui::DockSpace(ImGui::GetID("ΩmegaEngineDockSpace"));

        // Ω::Menu bar ─────────────────────────────────────────────────
        if(ImGui::BeginMenuBar())
        {
            if(ImGui::BeginMenu("File"))
            {
                if(ImGui::MenuItem("Save Scene", "Ctrl+S")) saveScene();
                if(ImGui::MenuItem("Load Scene", "Ctrl+O")) loadScene();
                ImGui::Separator();
                if(ImGui::MenuItem("Exit", "Alt+F4"))
                    Engine::Core::Application::get().quit();
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Viewport", nullptr, &m_showViewport);
                ImGui::MenuItem("Inspector", nullptr, &m_showInspector);
                ImGui::MenuItem("Hierarchy", nullptr, &m_showHierarchy);
                ImGui::MenuItem("Console", nullptr, &m_showConsole);
                ImGui::MenuItem("Tile Palette", nullptr, &m_showPalette);
                ImGui::MenuItem("Tilemap Layers", nullptr, &m_showLayers);
                ImGui::MenuItem("Objects", nullptr, &m_showObjects);
                ImGui::Separator();
                ImGui::MenuItem("Colliders", nullptr, &m_showColliders);
                ImGui::MenuItem("Tilemap Grid", nullptr, &m_showGrid);
                ImGui::MenuItem("Markers", nullptr, &m_showMarkers);
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("Scene"))
            {
                // One entry per scene in the open project; the active scene is
                // ticked. Switching is deferred to the frame boundary by the
                // SceneManager (safe mid-frame).
                auto const* active = m_sceneManager.active();
                for(auto const& name : m_project.scenes())
                {
                    bool const isActive = active && active->name() == name;
                    if(ImGui::MenuItem(name.c_str(), nullptr, isActive) && !isActive)
                        switchScene(name);
                }

                // ── Scene authoring  ────────────────────────────
                // Popups can't be opened from inside a menu (ImGui closes the
                // menu first), so these only RAISE a request flag; the modal
                // is opened + drawn after the dockspace below.
                ImGui::Separator();
                if(ImGui::MenuItem(ICON_FA_PLUS " New Scene..."))
                    m_openNewScenePopup = true;

                if(active)
                {
                    if(ImGui::MenuItem(ICON_FA_PEN " Rename Scene..."))
                    {
                        m_renameSceneFrom = active->name();
                        std::snprintf(m_sceneNameBuf, sizeof(m_sceneNameBuf), "%s",
                                      active->name().c_str());
                        m_openRenameScenePopup = true;
                    }

                    bool const isStartup = active->name() == m_project.startupScene();
                    if(ImGui::MenuItem(ICON_FA_STAR " Set as Startup", nullptr, isStartup, !isStartup))
                        setStartupScene(active->name());

                    bool const canDelete = m_project.scenes().size() > 1;
                    if(ImGui::MenuItem(ICON_FA_TRASH " Delete Scene", nullptr, false, canDelete))
                        deleteScene(active->name());
                }

                ImGui::EndMenu();
            }

            // Play / Stop toggle (edit vs play-in-editor). Green = will play,
            // red = currently playing (click to stop + restore).
            ImGui::PushStyleColor(ImGuiCol_Button, m_playing ? ImVec4 { 0.70f, 0.20f, 0.20f, 1.0f }
                                                             : ImVec4 { 0.20f, 0.55f, 0.30f, 1.0f });
            if(ImGui::Button(m_playing ? ICON_FA_STOP " Stop##play" : ICON_FA_PLAY " Play##play", { 78.0f, 0.0f }))
                togglePlay();
            ImGui::PopStyleColor();
            if(m_playing)
            {
                ImGui::SameLine();
                ImGui::TextColored({ 0.9f, 0.6f, 0.3f, 1.0f }, "PLAYING");
            }

            ImVec2 const tl = ImGui::GetWindowPos();
            ImVec2 const br = { tl.x + ImGui::GetWindowWidth(), tl.y + g_titlebarHeight };
            auto* dl = ImGui::GetWindowDrawList();

            std::string const title = "ΩmegaEngine Editor — " + m_project.name();
            ImVec2 const textSize = ImGui::CalcTextSize(title.c_str());
            float  const textX = tl.x + (br.x - tl.x - textSize.x) * 0.5f;
            float  const textY = tl.y + (g_titlebarHeight - textSize.y) * 0.5f;
            dl->AddText({ textX, textY }, IM_COL32(180, 178, 200, 255), title.c_str());

#ifdef _WIN32
        // Ω::Window control buttons (right-aligned) ──────────────
            float frameH = ImGui::GetFrameHeight();
            float btnW = frameH * 1.5f;
            float windowW = ImGui::GetWindowWidth();

            ImGui::SameLine(windowW - 3 * btnW);

            ImGui::PushStyleColor(ImGuiCol_Button, { 0.0f, 0.0f, 0.0f, 0.0f });
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.3f, 0.3f, 0.3f, 1.0f });
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.15f, 0.15f, 0.15f, 1.0f });
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, 0.0f });
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

            auto& win = Engine::Core::Application::get().window();
            if(ImGui::Button(ICON_FA_MINUS "##ΩMin", { btnW, frameH }))
                win.minimize();

            ImGui::SameLine(0, 0);

            if(ImGui::Button(win.isMaximized() ? ICON_FA_WINDOW_RESTORE "##ΩMax" : ICON_FA_WINDOW_MAXIMIZE "##ΩMax", { btnW, frameH }))
            {
                if(win.isMaximized())  win.restore();
                else                   win.maximize();
            }

            ImGui::SameLine(0, 0);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.86f, 0.2f, 0.2f, 1.0f });
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.70f, 0.1f, 0.1f, 1.0f });
            if(ImGui::Button(ICON_FA_XMARK "##ΩClose", { btnW, frameH }))
                win.close();
            ImGui::PopStyleColor(2);

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);
#endif

            ImGui::EndMenuBar();
        }

        // Ω::Panels ───────────────────────────────────────────────────
        if(m_showViewport)
        {
            // Zero padding so the rendered image fills the panel edge-to-edge.
            // NoScrollWithMouse prevents ImGui from eating the scroll wheel —
            // we use it for camera zoom instead.
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::Begin("Viewport", &m_showViewport,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::PopStyleVar();

            // The viewport image's on-screen rect -- needed to map mouse <->
            // world for picking + the gizmo. Captured right after
            // the ImGui::Image() draw below.
            ImVec2 imageMin {};
            ImVec2 imageMax {};
            bool   haveImage { false };

            if (m_framebuffer)
            {
                auto const size = ImGui::GetContentRegionAvail();

                if (size.x > 0 && size.y > 0)
                {
                    auto const w = static_cast<std::uint32_t>(size.x);
                    auto const h = static_cast<std::uint32_t>(size.y);

                    if (w != m_framebuffer->width() || h != m_framebuffer->height())
                    {
                        m_framebuffer->resize(w, h);

                        if (m_camera)
                            m_camera->setAspectRatio(
                                static_cast<float>(w) / static_cast<float>(h)
                            );
                    }

                    auto const texId = static_cast<ImTextureID>(m_framebuffer->colorAttachment());
                    ImGui::Image(texId, size, ImVec2(0, 1), ImVec2(1, 0));

                    imageMin  = ImGui::GetItemRectMin();
                    imageMax  = ImGui::GetItemRectMax();
                    haveImage = true;
                }
            }

            m_viewportHovered = ImGui::IsWindowHovered();

            // Scroll wheel zoom — lives here (not in onUpdate) because
            // ImGui::GetIO().MouseWheel is only valid after NewFrame(). Stands
            // down in Play mode so a follow-cam script owns the camera.
            if (m_viewportHovered && m_camera && !m_playing)
            {
                constexpr float zoomSpeed = 0.15f;
                float const wheel = ImGui::GetIO().MouseWheel;
                if (wheel != 0.0f)
                {
                    float const zoom = std::clamp(
                        m_camera->zoom() * (1.0f + wheel * zoomSpeed),
                        0.1f, 50.0f
                    );
                    m_camera->setZoom(zoom);
                }
            }

            // ── Viewport gizmo (ImGuizmo) + picking  ───────
            if (haveImage && m_camera)
            if (auto* world = m_sceneManager.active())
            {
                auto& reg = world->registry();

                // Tool toolbar overlay (top-left of the viewport). Move/Rotate/
                // Scale drive the gizmo (1/2/3 hotkeys); Paint switches to tile
                // painting (suppresses the gizmo). Drawn ON TOP of the image.
                ImGui::SetCursorScreenPos({ imageMin.x + 8.0f, imageMin.y + 8.0f });
                for (auto const& [label, mode] : { std::pair { "Move (1)", 0 },
                                                   std::pair { "Rotate (2)", 1 },
                                                   std::pair { "Scale (3)", 2 } })
                {
                    bool const on = (!m_paintMode && m_gizmoOp == mode);
                    if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4 { 0.20f, 0.45f, 0.75f, 1.0f });
                    if (ImGui::Button(label)) { m_gizmoOp = mode; m_paintMode = false; }
                    if (on) ImGui::PopStyleColor();
                    ImGui::SameLine();
                }
                {
                    bool const on = m_paintMode;
                    if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4 { 0.20f, 0.55f, 0.30f, 1.0f });
                    if (ImGui::Button("Paint (B)")) m_paintMode = true;
                    if (on) ImGui::PopStyleColor();
                }
                ImGui::NewLine();

                if (m_paintMode)
                {
                    // Tile painting takes over the viewport input.
                    paintViewport(imageMin.x, imageMin.y, imageMax.x, imageMax.y);
                }
                else
                {
                    // ImGuizmo draws + manipulates the selected entity's transform.
                    // 2D via orthographic mode: feed it the camera's separate
                    // view/projection + a 4x4 model from Transform, then decompose
                    // the manipulated matrix back to our 2D fields.
                    bool overGizmo = false;
                    if (m_selected.valid() && m_selected.has<Engine::ECS::Transform>())
                    {
                        auto& t = m_selected.get<Engine::ECS::Transform>();

                        ImGuizmo::SetOrthographic(true);
                        ImGuizmo::SetDrawlist();
                        ImGuizmo::SetRect(imageMin.x, imageMin.y,
                                          imageMax.x - imageMin.x, imageMax.y - imageMin.y);

                        glm::mat4 model =
                            glm::translate(glm::mat4 { 1.0f }, glm::vec3 { t.position.x, t.position.y, 0.0f })
                          * glm::rotate   (glm::mat4 { 1.0f }, glm::radians(t.rotation), glm::vec3 { 0.0f, 0.0f, 1.0f })
                          * glm::scale    (glm::mat4 { 1.0f }, glm::vec3 { t.scale.x, t.scale.y, 1.0f });

                        ImGuizmo::OPERATION const op =
                            (m_gizmoOp == 1) ? ImGuizmo::ROTATE :
                            (m_gizmoOp == 2) ? ImGuizmo::SCALE  : ImGuizmo::TRANSLATE;

                        if (ImGuizmo::Manipulate(
                                glm::value_ptr(m_camera->view()),
                                glm::value_ptr(m_camera->projection()),
                                op, ImGuizmo::LOCAL,
                                glm::value_ptr(model)))
                        {
                            float translation[3], rotation[3], scale[3];
                            ImGuizmo::DecomposeMatrixToComponents(
                                glm::value_ptr(model), translation, rotation, scale);
                            t.position = { translation[0], translation[1] };
                            t.rotation = rotation[2];
                            t.scale    = { scale[0], scale[1] };
                        }

                        overGizmo = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
                    }

                    // Picking: a click NOT on the gizmo selects the front-most
                    // sprite under the cursor (or deselects on empty space).
                    if (m_viewportHovered && !overGizmo
                        && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        glm::vec2 const wp = screenToWorld(
                            ImGui::GetMousePos(), m_camera->viewProjection(), imageMin, imageMax);
                        Engine::ECS::Entity hit;
                        reg.eachEntity([&](Engine::ECS::Entity e)
                        {
                            if (e.has<Engine::ECS::Transform>() && e.has<Engine::ECS::SpriteRenderer>()
                                && hitTest(e.get<Engine::ECS::Transform>(), wp))
                                hit = e;   // last match = front-most in draw order
                        });
                        m_selected = hit;  // empty handle -> deselect
                    }
                }
            }

            ImGui::End();
        }

        if(m_showHierarchy)
        {
            ImGui::Begin("Scene Hierarchy", &m_showHierarchy);

            if (auto* world = m_sceneManager.active())
            {
                ImGui::TextDisabled("%s", world->name().c_str());

                // Create buttons. The right-click context menu on the
                // panel background offers the same, plus Del / Ctrl+D shortcuts.
                if (ImGui::SmallButton(ICON_FA_PLUS " Empty"))   createEntity("Entity");
                ImGui::SameLine();
                if (ImGui::SmallButton(ICON_FA_PLUS " Sprite"))  createSpriteEntity();
                ImGui::SameLine();
                if (ImGui::SmallButton(ICON_FA_PLUS " Tilemap")) createTilemapEntity();
                ImGui::Separator();

                // Snapshot the entity handles BEFORE iterating the UI, so a
                // create/delete/duplicate triggered by a row's context menu
                // doesn't mutate the registry storage we're walking.
                std::vector<Engine::ECS::Entity> entities;
                world->registry().eachEntity([&](Engine::ECS::Entity e) { entities.push_back(e); });

                int index = 0;
                for (auto e : entities)
                {
                    ImGui::PushID(index++);

                    std::string const label = e.has<Engine::ECS::NameComponent>()
                        ? e.get<Engine::ECS::NameComponent>().name
                        : std::string { "Entity" };

                    if (ImGui::Selectable(label.c_str(), e == m_selected))
                        m_selected = e;

                    // Right-click a row -> per-entity actions (selects it first).
                    if (ImGui::BeginPopupContextItem())
                    {
                        m_selected = e;
                        if (ImGui::MenuItem(ICON_FA_CLONE " Duplicate", "Ctrl+D")) duplicateSelected();
                        if (ImGui::MenuItem(ICON_FA_TRASH " Delete",    "Del"))    deleteSelected();
                        ImGui::EndPopup();
                    }

                    ImGui::PopID();
                }

                // Right-click empty panel background -> create menu.
                if (ImGui::BeginPopupContextWindow("##hierctx",
                        ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
                {
                    if (ImGui::MenuItem(ICON_FA_CUBE " Create Empty"))   createEntity("Entity");
                    if (ImGui::MenuItem(ICON_FA_CUBE " Create Sprite"))  createSpriteEntity();
                    if (ImGui::MenuItem(ICON_FA_CUBE " Create Tilemap")) createTilemapEntity();
                    ImGui::EndPopup();
                }

                // Left-click empty space in the panel to deselect.
                if (ImGui::IsWindowHovered()
                    && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
                    && !ImGui::IsAnyItemHovered())
                {
                    m_selected = {};
                }
            }
            else
            {
                ImGui::TextDisabled("(no active scene)");
            }

            ImGui::End();
        }

        if(m_showInspector)
        {
            ImGui::Begin("Inspector", &m_showInspector);

            // Guard with valid(): a scene rebuild destroys entities and
            // leaves m_selected dangling -- valid() catches that.
            if (m_selected.valid())
                drawInspector(m_selected);
            else
                ImGui::TextDisabled("No entity selected");

            ImGui::End();
        }

        if(m_showConsole)
        {
            ImGui::Begin("Console", &m_showConsole);

            // ── Header: status + hotkey reference + per-frame stats ──
            ImGui::TextColored({ 0.3f, 0.9f, 0.5f, 1.0f },
                               "[Ω] OmegaEngine started successfully");
            ImGui::TextColored({ 0.55f, 0.55f, 0.65f, 1.0f },
                               ICON_FA_PLAY " Ctrl+P play/stop   "
                               ICON_FA_FLOPPY_DISK " Ctrl+S save   "
                               ICON_FA_FOLDER_OPEN " Ctrl+O reload   "
                               ICON_FA_CAMERA " F2 screenshot");
            ImGui::TextColored({ 0.55f, 0.55f, 0.65f, 1.0f },
                               ICON_FA_CLONE " Ctrl+D duplicate   "
                               ICON_FA_TRASH " Del delete   "
                               "1/2/3 gizmo move/rotate/scale");

            auto const stats = Engine::Renderer::Renderer2D::stats();
            ImGui::TextColored({ 0.5f, 0.7f, 0.9f, 1.0f },
                               "[Ω] Draw calls: %u  |  Quads: %u   ·   60Hz (%.2fms)",
                               stats.drawCalls, stats.quadCount, 1000.0f / 60.0f);

            // ── Scrolling action log ────────────────────────────────
            ImGui::Separator();
            ImGui::BeginChild("##consolelog", { 0.0f, 0.0f }, false,
                              ImGuiWindowFlags_HorizontalScrollbar);
            for(auto const& line : m_consoleLog)
                ImGui::TextUnformatted(line.c_str());
            if(m_consoleScrollToBottom)
            {
                ImGui::SetScrollHereY(1.0f);
                m_consoleScrollToBottom = false;
            }
            ImGui::EndChild();

            ImGui::End();
        }

        if (m_showPalette)
            drawTilePalette();

        if (m_showLayers)
            drawLayersPanel();

        if (m_showObjects)
            drawObjectsPanel();

        // Visual scripting node editor (file-scope panel -- see the top of
        // this file; the inspector's "Edit Graph" button opens graphs in it).
        if (g_showNodes)
            g_nodePanel.draw(&g_showNodes);

        // ── Scene name modals (New / Rename) ───────────────────
        // Opened here (outside the menu) from the request flags set above.
        if (m_openNewScenePopup)    { ImGui::OpenPopup("New Scene");    m_sceneNameBuf[0] = '\0'; m_openNewScenePopup    = false; }
        if (m_openRenameScenePopup) { ImGui::OpenPopup("Rename Scene");                            m_openRenameScenePopup = false; }

        if (ImGui::BeginPopupModal("New Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("New scene name:");
            ImGui::SetNextItemWidth(260.0f);
            bool const enter = ImGui::InputText("##newscene", m_sceneNameBuf, sizeof(m_sceneNameBuf),
                                                ImGuiInputTextFlags_EnterReturnsTrue);
            bool const ok     = ImGui::Button("Create") || enter;
            ImGui::SameLine();
            bool const cancel = ImGui::Button("Cancel");

            if (ok && m_sceneNameBuf[0] != '\0')
            {
                newScene(m_sceneNameBuf);
                ImGui::CloseCurrentPopup();
            }
            if (cancel) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Rename Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Rename '%s' to:", m_renameSceneFrom.c_str());
            ImGui::SetNextItemWidth(260.0f);
            bool const enter = ImGui::InputText("##renamescene", m_sceneNameBuf, sizeof(m_sceneNameBuf),
                                                ImGuiInputTextFlags_EnterReturnsTrue);
            bool const ok     = ImGui::Button("Rename") || enter;
            ImGui::SameLine();
            bool const cancel = ImGui::Button("Cancel");

            if (ok && m_sceneNameBuf[0] != '\0' && m_renameSceneFrom != m_sceneNameBuf)
            {
                renameScene(m_renameSceneFrom, m_sceneNameBuf);
                ImGui::CloseCurrentPopup();
            }
            if (cancel) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::End(); // DockSpaceHost

        // Ω::Update titlebar state for WndProc ───────────────────────
#ifdef _WIN32
        g_imguiWantsInput = ImGui::IsAnyItemHovered()
            || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopup);
        g_titlebarHeight = static_cast<int>(
            ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.y * 2
            );
#endif
    }

    void EditorLayer::saveScene()
    {
        auto* world = m_sceneManager.active();
        if (!world) return;

        // Save to "scenes/<world>.json" -- the same file onEnter loads from.
        auto const result = Engine::Scene::SceneSerializer::save(*world, "scenes/" + world->name() + ".json");
        if (!result)   // the serializer logs success; we only flag failures
            std::println(std::cerr, "[Ω::EditorLayer] save failed: {}", result.error().message);
        else
            logConsole(ICON_FA_FLOPPY_DISK " saved scene '" + world->name() + "'");
    }

    void EditorLayer::loadScene()
    {
        auto* world = m_sceneManager.active();
        if (!world) return;

        m_selected = {};   // old entities are about to be destroyed

        auto const result = Engine::Scene::SceneSerializer::load(
            *world, "scenes/" + world->name() + ".json", Engine::Core::Application::get().assets());
        if (!result)
            std::println(std::cerr, "[Ω::EditorLayer] load failed: {}", result.error().message);
        else
            logConsole(ICON_FA_FOLDER_OPEN " reloaded scene '" + world->name() + "'");
    }

    void EditorLayer::togglePlay()
    {
        auto* world = m_sceneManager.active();
        if (!world) return;

        // A throwaway snapshot of the PRE-play (possibly edited) state. We
        // reuse the scene serializer rather than the committed scene file, so
        // Play captures unsaved edits and Stop restores exactly them.
        auto const snapshot = std::filesystem::temp_directory_path() / "omega_editor_play_snapshot.json";

        if (!m_playing)
        {
            // Edit -> Play: snapshot, then let the systems run.
            if (auto const r = Engine::Scene::SceneSerializer::save(*world, snapshot); !r)
            {
                std::println(std::cerr, "[Ω::EditorLayer] play snapshot failed: {}", r.error().message);
                return;     // don't enter Play if we couldn't capture a restore point
            }
            m_playing = true;
            logConsole(ICON_FA_PLAY " play  (scene '" + world->name() + "')");
        }
        else
        {
            // Play -> Edit: restore the snapshot, discarding the simulation.
            m_playing  = false;
            m_selected = {};    // entities are recreated by load(); old handle is stale
            if (auto const r = Engine::Scene::SceneSerializer::load(
                    *world, snapshot, Engine::Core::Application::get().assets()); !r)
                std::println(std::cerr, "[Ω::EditorLayer] restore failed: {}", r.error().message);
            logConsole(ICON_FA_STOP " stop  (restored)");
        }

        // Play/Stop restores into the SAME world, so the registry pointer never
        // changes and the host's automatic clear doesn't fire -- drop the previous
        // session's script instances here so they don't accumulate.
        Engine::Scripting::ScriptHost::instance().clearInstances();

        // Script-burst particles live in a world pool outside the snapshot --
        // drop them too so effects don't linger into edit mode.
        Engine::Systems::ParticleSystem::clearWorld();
    }

    void EditorLayer::switchScene(std::string name)
    {
        if (m_playing)          // leaving Play; restore first so we don't carry sim state
            togglePlay();

        m_selected = {};        // old scene's entities go away
        m_sceneManager.switchTo(name);
        logConsole(ICON_FA_LIST " scene -> " + name);
    }

    void EditorLayer::handleShortcuts()
    {
        using Engine::Core::Key;
        using Input = Engine::Core::Input;

        // Polled once per RENDER frame (from onImGuiRender). Input::update() ran
        // at the top of this frame, so wasKeyPressed() edges are valid here and
        // never dropped by skipped fixed-timestep ticks.

        // Drive the script build-watch + hot-reload every render frame. The world's
        // ScriptSystem only ticks in Play, but a saved script must rebuild (and be
        // ready, or hot-swap live) in EDIT mode too -- so pump the host here rather
        // than from the play-gated system tick.
        if (auto& host = Engine::Scripting::ScriptHost::instance(); host.ready())
        {
            host.pollBuild();
            host.beginFrame();
        }

        bool const ctrl = Input::isKeyDown(Key::LeftControl);

        if (ctrl && Input::wasKeyPressed(Key::P)) togglePlay();
        if (ctrl)
        {
            if (Input::wasKeyPressed(Key::S)) saveScene();
            if (Input::wasKeyPressed(Key::O)) loadScene();
            if (Input::wasKeyPressed(Key::D)) duplicateSelected();
        }

        // Undo / redo of tile painting (Ctrl+Z / Ctrl+Y). Suppressed while a
        // text field is focused so ImGui's own field-undo isn't hijacked.
        if (ctrl && !ImGui::GetIO().WantTextInput)
        {
            if (Input::wasKeyPressed(Key::Z)) undoTilePaint();
            if (Input::wasKeyPressed(Key::Y)) redoTilePaint();
        }

        // Entity/gizmo/tool keys are suppressed while a text field is focused,
        // so typing a name doesn't delete the entity or flip the tool.
        if (!ImGui::GetIO().WantTextInput)
        {
            if (Input::wasKeyPressed(Key::Delete) && m_selected.valid())
                deleteSelected();

            // Tools: 1/2/3 = gizmo move/rotate/scale (and exit paint), B = paint.
            if (Input::wasKeyPressed(Key::D1)) { m_gizmoOp = 0; m_paintMode = false; }
            if (Input::wasKeyPressed(Key::D2)) { m_gizmoOp = 1; m_paintMode = false; }
            if (Input::wasKeyPressed(Key::D3)) { m_gizmoOp = 2; m_paintMode = false; }
            if (Input::wasKeyPressed(Key::B))  m_paintMode = !m_paintMode;
        }
    }

    void EditorLayer::logConsole(std::string message)
    {
        m_consoleLog.push_back(std::move(message));

        constexpr std::size_t maxLines = 200;
        if (m_consoleLog.size() > maxLines)
            m_consoleLog.erase(m_consoleLog.begin(),
                               m_consoleLog.begin() + (m_consoleLog.size() - maxLines));

        m_consoleScrollToBottom = true;
    }

    // ─────────────────────────────────────────────────────────────────────
    //  World wiring + entity authoring 
    // ─────────────────────────────────────────────────────────────────────

    void EditorLayer::setupWorld(Engine::Scene::World& world)
    {
        if (world.systemCount() > 0)
            return;     // already wired -- keep in-editor edits on re-entry

        // Systems are engine CODE (not serialized). They only TICK in Play
        // mode (see onUpdate); Edit mode stays static for authoring.
        world.addSystem<Engine::Systems::InterpolationSystem>();   // FIRST (snapshot before movers)
        world.addSystem<Engine::Systems::MovementSystem>();
        world.addSystem<Engine::Systems::AnimationSystem>();
        world.addSystem<Engine::Systems::ParticleSystem>();

        // C# scripts -- gameplay logic from the project's managed assembly.
        // managedDir = exe dir (OmegaEngine.dll + BuildScripts.cs, deployed
        // post-build); scriptsDir = the project's script SOURCE folder (cwd =
        // project root). The engine compiles it and hot-reloads on edits.
        auto& scripts = world.addSystem<Engine::Scripting::ScriptSystem>(
            Engine::Core::Paths::executableDir(),
            "scripts");

        auto& bus     = Engine::Core::Application::get().eventBus();
        auto& physics = world.addSystem<Engine::Physics::PhysicsSystem>(bus);

        // Give scripts the body API (velocity/impulse/force) + collision and
        // trigger callbacks. Done after BOTH systems exist.
        scripts.usePhysics(physics.world(), bus);

        // Entities are pure DATA from scenes/<name>.json. A brand-new scene
        // (just created in the editor) has no file yet -- that's fine, it
        // starts empty and the file is written on the first save.
        auto const file = "scenes/" + world.name() + ".json";
        if (std::filesystem::exists(file))
        {
            auto& assets = Engine::Core::Application::get().assets();
            if (auto r = Engine::Scene::SceneSerializer::load(world, file, assets); !r)
                std::println(std::cerr, "[Ω::EditorLayer] could not load '{}': {}",
                             file, r.error().message);
        }
    }

    Engine::ECS::Entity EditorLayer::createEntity(std::string name)
    {
        auto* world = m_sceneManager.active();
        if (!world)
            return {};

        auto e = world->registry().create(name);     // adds NameComponent
        e.add<Engine::ECS::Transform>();             // every entity needs a place
        m_selected = e;
        logConsole(ICON_FA_CUBE " + entity '" + name + "'");
        return e;
    }

    Engine::ECS::Entity EditorLayer::createSpriteEntity()
    {
        auto e = createEntity("Sprite");
        if (e.valid())
            e.add<Engine::ECS::SpriteRenderer>();    // white quad (color-only)
        return e;
    }

    Engine::ECS::Entity EditorLayer::createTilemapEntity()
    {
        auto* world = m_sceneManager.active();
        if (!world)
            return {};

        auto e = world->registry().create("Tilemap");
        e.add<Engine::ECS::Transform>();

        // A blank atlas tilemap: assign a tileset + paint in the Inspector/Palette.
        Engine::ECS::TilemapComponent map;
        map.tileWorldSize = 0.5f;
        map.dimensions    = { 16, 16 };
        map.tiles.assign(map.cellCount(), -1);
        e.add<Engine::ECS::TilemapComponent>(std::move(map));

        m_selected = e;
        logConsole(ICON_FA_CUBE " + tilemap");
        return e;
    }

    void EditorLayer::drawTilePalette()
    {
        ImGui::Begin("Tile Palette", &m_showPalette);

        auto* world = m_sceneManager.active();
        if (!world || !m_selected.valid() || !m_selected.has<Engine::ECS::TilemapComponent>())
        {
            ImGui::TextDisabled("Select a tilemap to pick a brush.");
            ImGui::End();
            return;
        }

        auto& map = m_selected.get<Engine::ECS::TilemapComponent>();

        // ── Brush size + erase ──────────────────────────────────────────
        ImGui::TextUnformatted("Brush");
        for (int s : { 1, 2, 3 })
        {
            ImGui::SameLine();
            char label[8];
            std::snprintf(label, sizeof(label), "%dx%d", s, s);
            bool const on = (m_brushSize == s);
            if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4 { 0.20f, 0.45f, 0.75f, 1.0f });
            if (ImGui::Button(label)) m_brushSize = s;
            if (on) ImGui::PopStyleColor();
        }
        ImGui::SameLine();
        {
            bool const on = (m_brushTile < 0);
            if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4 { 0.70f, 0.30f, 0.30f, 1.0f });
            if (ImGui::Button("Erase")) m_brushTile = -1;
            if (on) ImGui::PopStyleColor();
        }
        ImGui::Separator();

        constexpr float thumb  = 40.0f;
        float const     availX = ImGui::GetContentRegionAvail().x;
        int const       perRow = std::max(1, static_cast<int>(availX / (thumb + 8.0f)));

        auto thumbButton = [&](int id, ImTextureID tex, ImVec2 uv0, ImVec2 uv1)
        {
            bool const   sel = (m_brushTile == id);
            ImVec4 const bg  = sel ? ImVec4 { 0.30f, 0.60f, 1.0f, 0.65f } : ImVec4 { 0.0f, 0.0f, 0.0f, 0.0f };
            ImGui::PushID(id);
            if (tex != 0)
            {
                if (ImGui::ImageButton("t", tex, { thumb, thumb }, uv0, uv1, bg, { 1, 1, 1, 1 }))
                    m_brushTile = id;
            }
            else if (ImGui::Button("?", { thumb, thumb }))
            {
                m_brushTile = id;
            }
            ImGui::PopID();
        };

        using Source = Engine::ECS::TilemapComponent::Source;

        if (map.source == Source::Atlas)
        {
            if (map.atlas == nullptr)
            {
                ImGui::TextDisabled("No atlas set (Inspector -> Atlas Path).");
                ImGui::End();
                return;
            }

            float const texW  = static_cast<float>(map.atlas->width());
            float const texH  = static_cast<float>(map.atlas->height());
            float const cw    = static_cast<float>(std::max(1, map.tilePixelSize.x));
            float const ch    = static_cast<float>(std::max(1, map.tilePixelSize.y));
            int const   cols  = std::max(1, static_cast<int>(texW / cw));
            int const   rows  = std::max(1, static_cast<int>(texH / ch));

            auto const tex = static_cast<ImTextureID>(map.atlas->id());
            int shown = 0;
            for (int id = 0; id < cols * rows; ++id)
            {
                int const col = id % cols;
                int const row = id / cols;
                // ImGui uv: top-left = (left, 1 - rowTop), bottom-right = (right, 1 - rowBottom).
                ImVec2 const uv0 { (col * cw) / texW, 1.0f - (row * ch) / texH };
                ImVec2 const uv1 { ((col + 1) * cw) / texW, 1.0f - ((row + 1) * ch) / texH };
                thumbButton(id, tex, uv0, uv1);
                if (++shown % perRow != 0) ImGui::SameLine();
            }
        }
        else // Collection
        {
            if (map.tileDefs.empty())
                ImGui::TextDisabled("No tiles (Inspector -> Add Tile).");

            int shown = 0;
            for (int id = 0; id < static_cast<int>(map.tileDefs.size()); ++id)
            {
                auto const& d   = map.tileDefs[id];
                auto const  tex = d.texture ? static_cast<ImTextureID>(d.texture->id()) : ImTextureID { 0 };
                ImVec2 const uv0 { d.uvMin.x, d.uvMax.y };
                ImVec2 const uv1 { d.uvMax.x, d.uvMin.y };
                thumbButton(id, tex, uv0, uv1);
                if (++shown % perRow != 0) ImGui::SameLine();
            }
        }

        ImGui::NewLine();
        ImGui::TextDisabled("Brush tile: %d   size: %dx%d", m_brushTile, m_brushSize, m_brushSize);

        ImGui::End();
    }

    void EditorLayer::paintViewport(float imgMinX, float imgMinY, float imgMaxX, float imgMaxY)
    {
        using Engine::ECS::TilemapComponent;
        using Engine::ECS::Transform;

        if (!m_camera || !m_selected.valid()
            || !m_selected.has<TilemapComponent>() || !m_selected.has<Transform>())
        {
            ImGui::SetCursorScreenPos({ imgMinX + 8.0f, imgMaxY - 26.0f });
            ImGui::TextColored({ 0.9f, 0.7f, 0.4f, 1.0f }, "Select a tilemap to paint.");
            return;
        }

        auto& map = m_selected.get<TilemapComponent>();
        auto& t   = m_selected.get<Transform>();

        ImVec2 const imgMin { imgMinX, imgMinY };
        ImVec2 const imgMax { imgMaxX, imgMaxY };
        auto const&  vp     = m_camera->viewProjection();
        ImVec2 const mouse  = ImGui::GetMousePos();
        auto*  const draw   = ImGui::GetWindowDrawList();

        float     const ts     = map.tileWorldSize;
        glm::vec2 const origin = t.position;

        // Cell under the cursor.
        glm::vec2 const wp    = screenToWorld(mouse, vp, imgMin, imgMax);
        int       const cellX = static_cast<int>(std::floor((wp.x - origin.x) / ts));
        int       const cellY = static_cast<int>(std::floor((wp.y - origin.y) / ts));

        // Brush footprint (odd sizes centred on the cursor cell).
        int const half = (m_brushSize - 1) / 2;
        int const bx0  = cellX - half;
        int const by0  = cellY - half;

        // Don't paint when the cursor is over a UI widget (e.g. the tool
        // toolbar drawn on top of the viewport).
        bool const hovered = m_viewportHovered && !ImGui::IsAnyItemHovered();
        bool const alt     = ImGui::GetIO().KeyAlt;
        bool const shift   = ImGui::GetIO().KeyShift;
        bool const lDown   = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        bool const rDown   = ImGui::IsMouseDown(ImGuiMouseButton_Right);

        // Screen rect spanning cell range [x0,x1) x [y0,y1).
        auto cellRectScreen = [&](int x0, int y0, int x1, int y1) -> std::pair<ImVec2, ImVec2>
        {
            glm::vec2 const a  = origin + glm::vec2 { static_cast<float>(x0), static_cast<float>(y0) } * ts;
            glm::vec2 const b  = origin + glm::vec2 { static_cast<float>(x1), static_cast<float>(y1) } * ts;
            ImVec2    const sa = worldToScreen(a, vp, imgMin, imgMax);
            ImVec2    const sb = worldToScreen(b, vp, imgMin, imgMax);
            return { 
                { std::min(sa.x, sb.x), std::min(sa.y, sb.y) },
                { std::max(sa.x, sb.x), std::max(sa.y, sb.y) } 
            };
        };

        // Alt = eyedropper: pick the tile under the cursor, don't paint.
        if (hovered && alt && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            if (int const picked = map.at(cellX, cellY); picked >= 0)
                m_brushTile = picked;
            return;
        }

        // Begin a stroke (snapshot the grid for undo).
        if (hovered && (lDown || rDown) && !m_painting && !alt)
        {
            m_painting     = true;
            m_strokeTarget = m_selected;
            if (map.tiles.size() != map.cellCount())
                map.tiles.assign(map.cellCount(), -1);
            m_strokeBefore = map.tiles;
            m_rectDrag     = shift && lDown;
            if (m_rectDrag) { m_rectAnchorX = cellX; m_rectAnchorY = cellY; }
        }

        // Freehand paint (left) / erase (right) while dragging.
        if (m_painting && !m_rectDrag)
        {
            int const value = rDown ? -1 : m_brushTile;
            for (int y = by0; y < by0 + m_brushSize; ++y)
                for (int x = bx0; x < bx0 + m_brushSize; ++x)
                    map.set(x, y, value);
        }

        // End the stroke -> apply a rect fill (if dragging) + push undo.
        if (m_painting && !lDown && !rDown)
        {
            if (m_rectDrag)
            {
                int const x0 = std::min(m_rectAnchorX, cellX), x1 = std::max(m_rectAnchorX, cellX);
                int const y0 = std::min(m_rectAnchorY, cellY), y1 = std::max(m_rectAnchorY, cellY);
                for (int y = y0; y <= y1; ++y)
                    for (int x = x0; x <= x1; ++x)
                        map.set(x, y, m_brushTile);
                m_rectDrag = false;
            }

            m_painting = false;
            if (m_strokeTarget.valid() && m_strokeBefore != map.tiles)
            {
                m_undoStack.push_back({ m_strokeTarget, std::move(m_strokeBefore) });
                m_redoStack.clear();
                constexpr std::size_t cap = 64;
                if (m_undoStack.size() > cap)
                    m_undoStack.erase(m_undoStack.begin());
            }
            m_strokeBefore.clear();
        }

        // Overlay: rectangle preview (Shift-drag) or brush-footprint outline.
        if (hovered || m_painting)
        {
            if (m_rectDrag)
            {
                auto const [a, b] = cellRectScreen(
                    std::min(m_rectAnchorX, cellX),
                    std::min(m_rectAnchorY, cellY),
                    std::max(m_rectAnchorX, cellX) + 1,
                    std::max(m_rectAnchorY, cellY) + 1
                );
                draw->AddRectFilled(a, b, IM_COL32(80, 160, 255, 50));
                draw->AddRect(a, b, IM_COL32(120, 190, 255, 230), 0.0f, 0, 2.0f);
            }
            else
            {
                auto const [a, b] = cellRectScreen(bx0, by0, bx0 + m_brushSize, by0 + m_brushSize);
                ImU32 const col = (m_brushTile < 0) ? IM_COL32(255, 120, 120, 220)
                                                    : IM_COL32(255, 240, 140, 220);
                draw->AddRect(a, b, col, 0.0f, 0, 2.0f);
            }
        }
    }

    void EditorLayer::undoTilePaint()
    {
        if (m_undoStack.empty())
            return;

        auto edit = std::move(m_undoStack.back());
        m_undoStack.pop_back();
        if (edit.target.valid() && edit.target.has<Engine::ECS::TilemapComponent>())
        {
            auto& map = edit.target.get<Engine::ECS::TilemapComponent>();
            m_redoStack.push_back({ edit.target, map.tiles });   // current state -> redo
            map.tiles = std::move(edit.tiles);
            logConsole("undo tile edit");
        }
    }

    void EditorLayer::redoTilePaint()
    {
        if (m_redoStack.empty())
            return;

        auto edit = std::move(m_redoStack.back());
        m_redoStack.pop_back();
        if (edit.target.valid() && edit.target.has<Engine::ECS::TilemapComponent>())
        {
            auto& map = edit.target.get<Engine::ECS::TilemapComponent>();
            m_undoStack.push_back({ edit.target, map.tiles });
            map.tiles = std::move(edit.tiles);
            logConsole("redo tile edit");
        }
    }

    void EditorLayer::drawLayersPanel()
    {
        using Engine::ECS::TilemapComponent;
        using Engine::ECS::NameComponent;

        ImGui::Begin("Tilemap Layers", &m_showLayers);

        auto* world = m_sceneManager.active();
        if (!world)
        {
            ImGui::TextDisabled("(no active scene)");
            ImGui::End();
            return;
        }

        if (ImGui::SmallButton(ICON_FA_PLUS " Layer"))
            createTilemapEntity();
        ImGui::Separator();

        // Collect tilemap entities and list them TOP-first (descending z), so
        // the panel order matches the visual stack (foreground on top).
        std::vector<Engine::ECS::Entity> layers;
        world->registry().eachEntity([&](Engine::ECS::Entity e)
        {
            if (e.has<TilemapComponent>())
                layers.push_back(e);
        });
        std::ranges::sort(layers, [](Engine::ECS::Entity a, Engine::ECS::Entity b)
        {
            return a.get<TilemapComponent>().zIndex > b.get<TilemapComponent>().zIndex;
        });

        if (layers.empty())
            ImGui::TextDisabled("No tilemaps yet.");

        int index = 0;
        for (auto e : layers)
        {
            ImGui::PushID(index++);
            auto& map = e.get<TilemapComponent>();

            // Visibility (hides the layer from rendering without deleting it).
            ImGui::Checkbox("##vis", &map.visible);
            ImGui::SameLine();

            // Name -> selects the layer (so the palette/inspector target it).
            std::string const name = e.has<NameComponent>()
                ? e.get<NameComponent>().name : std::string { "Tilemap" };
            if (ImGui::Selectable(name.c_str(), e == m_selected, 0, { 120.0f, 0.0f }))
                m_selected = e;

            // z reorder + value.
            ImGui::SameLine();
            if (ImGui::SmallButton("^")) ++map.zIndex;
            ImGui::SameLine();
            if (ImGui::SmallButton("v")) --map.zIndex;
            ImGui::SameLine();
            ImGui::Text("z%d", map.zIndex);

            // Opacity.
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderFloat("##op", &map.opacity, 0.0f, 1.0f, "opacity %.2f");

            ImGui::PopID();
        }

        ImGui::End();
    }

    Engine::ECS::Entity EditorLayer::createMarker(Engine::ECS::MarkerType type)
    {
        using namespace Engine::ECS;

        auto* world = m_sceneManager.active();
        if (!world)
            return {};

        char const* const names[] = { "SpawnPoint", "Trigger", "NPC", "Item", "CameraBound" };
        auto e = world->registry().create(names[static_cast<int>(type)]);

        Transform tf;
        tf.position = m_camera ? m_camera->position() : glm::vec2 { 0.0f, 0.0f };
        if (type == MarkerType::CameraBound)
            tf.scale = { 6.0f, 4.0f };           // a default camera region
        e.add<Transform>(tf);
        e.add<MarkerComponent>(MarkerComponent { type, {} });

        if (type == MarkerType::Trigger)
        {
            // A static sensor so a body entering it fires Trigger events.
            e.add<RigidBody2D>(RigidBody2D { .type = RigidBody2D::BodyType::Static });
            BoxCollider2D box;
            box.size      = { 1.0f, 1.0f };
            box.isTrigger = true;
            e.add<BoxCollider2D>(box);
        }
        else if (type == MarkerType::NPC || type == MarkerType::Item)
        {
            // A placeholder coloured quad so it's visible until art is assigned.
            SpriteRenderer sr;
            sr.color = (type == MarkerType::NPC) ? glm::vec4 { 0.30f, 0.80f, 0.95f, 1.0f }
                                                 : glm::vec4 { 0.90f, 0.40f, 0.90f, 1.0f };
            e.add<SpriteRenderer>(sr);
        }

        m_selected = e;
        logConsole(std::string { ICON_FA_CUBE " + " } + names[static_cast<int>(type)]);
        return e;
    }

    void EditorLayer::drawObjectsPanel()
    {
        using MT = Engine::ECS::MarkerType;

        ImGui::Begin("Objects", &m_showObjects);
        ImGui::TextDisabled("Place a marker, then move it with the gizmo:");

        if (ImGui::Button("Spawn point",  { -1.0f, 0.0f })) createMarker(MT::SpawnPoint);
        if (ImGui::Button("Trigger",      { -1.0f, 0.0f })) createMarker(MT::Trigger);
        if (ImGui::Button("NPC",          { -1.0f, 0.0f })) createMarker(MT::NPC);
        if (ImGui::Button("Item",         { -1.0f, 0.0f })) createMarker(MT::Item);
        if (ImGui::Button("Camera bound", { -1.0f, 0.0f })) createMarker(MT::CameraBound);

        ImGui::End();
    }

    void EditorLayer::deleteSelected()
    {
        auto* world = m_sceneManager.active();
        if (!world || !m_selected.valid())
            return;

        std::string const name = m_selected.has<Engine::ECS::NameComponent>()
            ? m_selected.get<Engine::ECS::NameComponent>().name
            : std::string { "entity" };

        world->registry().destroy(m_selected);
        m_selected = {};
        logConsole(ICON_FA_TRASH " - entity '" + name + "'");
    }

    void EditorLayer::duplicateSelected()
    {
        auto* world = m_sceneManager.active();
        if (!world || !m_selected.valid())
            return;

        auto dst = world->registry().create();
        cloneInto(m_selected, dst);

        if (dst.has<Engine::ECS::NameComponent>())
            dst.get<Engine::ECS::NameComponent>().name += " (copy)";

        m_selected = dst;
        logConsole(ICON_FA_CLONE " duplicated entity");
    }

    // ─────────────────────────────────────────────────────────────────────
    //  Scene authoring -- mutate the Project + persist project.json
    // ─────────────────────────────────────────────────────────────────────

    void EditorLayer::newScene(std::string name)
    {
        if (m_sceneManager.has(name))
        {
            logConsole("scene '" + name + "' already exists");
            return;
        }

        // Register in the manifest + persist project.json.
        m_project.addScene(name);
        if (auto r = m_project.save(); !r)
            std::println(std::cerr, "[Ω::EditorLayer] project save failed: {}", r.error().message);

        // Write an empty scene file up front so the scene persists immediately
        // (a throwaway empty World gives the serializer something to emit).
        {
            Engine::Scene::World empty { name };
            if (auto r = Engine::Scene::SceneSerializer::save(empty, m_project.scenePath(name)); !r)
                std::println(std::cerr, "[Ω::EditorLayer] new-scene file failed: {}", r.error().message);
        }

        // Create + wire the world, then switch to it (deferred).
        m_sceneManager.create(name).setOnEnter(
            [this](Engine::Scene::World& w) { setupWorld(w); });

        logConsole(ICON_FA_PLUS " new scene '" + name + "'");
        switchScene(name);
    }

    void EditorLayer::renameScene(std::string from, std::string to)
    {
        if (m_sceneManager.has(to))
        {
            logConsole("scene '" + to + "' already exists");
            return;
        }

        bool const wasActive =
            m_sceneManager.active() && m_sceneManager.active()->name() == from;

        // Persist current edits to the OLD file first, so the rename carries
        // them (the new world reloads from the moved file).
        if (wasActive)
            saveScene();

        // Move the scene file on disk.
        std::error_code ec;
        std::filesystem::rename(m_project.scenePath(from), m_project.scenePath(to), ec);
        if (ec)
        {
            std::println(std::cerr, "[Ω::EditorLayer] rename file failed: {}", ec.message());
            return;
        }

        // Update + persist the manifest.
        m_project.renameScene(from, to);
        if (auto r = m_project.save(); !r)
            std::println(std::cerr, "[Ω::EditorLayer] project save failed: {}", r.error().message);

        // Swap the world. The new one loads from the moved file via setupWorld.
        m_sceneManager.create(to).setOnEnter(
            [this](Engine::Scene::World& w) { setupWorld(w); });

        if (wasActive)
        {
            // Switch to the new world; remove the old one only AFTER the
            // deferred switch applies (handled in onUpdate).
            m_selected = {};
            m_sceneManager.switchTo(to);
            m_pendingSceneRemoval = from;
        }
        else
        {
            m_sceneManager.remove(from);
        }

        logConsole(ICON_FA_PEN " renamed '" + from + "' -> '" + to + "'");
    }

    void EditorLayer::deleteScene(std::string name)
    {
        if (m_project.scenes().size() <= 1)
        {
            logConsole("can't delete the last scene");
            return;
        }

        bool const isActive =
            m_sceneManager.active() && m_sceneManager.active()->name() == name;

        // Drop from the manifest + disk.
        m_project.removeScene(name);
        if (auto r = m_project.save(); !r)
            std::println(std::cerr, "[Ω::EditorLayer] project save failed: {}", r.error().message);

        std::error_code ec;
        std::filesystem::remove(m_project.scenePath(name), ec);

        if (isActive)
        {
            // Switch away first; defer the world removal until that applies so
            // we never free the world we're still rendering this frame.
            m_selected = {};
            m_sceneManager.switchTo(m_project.scenes().front());
            m_pendingSceneRemoval = name;
        }
        else
        {
            m_sceneManager.remove(name);
        }

        logConsole(ICON_FA_TRASH " deleted scene '" + name + "'");
    }

    void EditorLayer::setStartupScene(std::string name)
    {
        m_project.setStartupScene(std::move(name));
        if (auto r = m_project.save(); !r)
            std::println(std::cerr, "[Ω::EditorLayer] project save failed: {}", r.error().message);
        logConsole(ICON_FA_STAR " startup scene -> " + m_project.startupScene());
    }
} // namespace Editor