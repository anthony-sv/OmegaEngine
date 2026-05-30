module;

#include "glm/glm.hpp"
#include "nlohmann/json.hpp"

module Engine.Scene:Serializer;

import :Serializer;
import :World;
import Engine.ECS;
import Engine.Renderer;   // Renderer::Texture2D (resolve texturePath -> pointer)
import Engine.Core;
import std;

namespace Engine::Scene
{
    using json = nlohmann::json;
    using Core::ErrorInfo;
    using Core::ErrorCode;

    namespace
    {
        // ── glm <-> JSON helpers (vectors as arrays) ─────────────────
        json toJson(glm::vec2 const& v) { return json::array({ v.x, v.y }); }
        json toJson(glm::vec4 const& v) { return json::array({ v.x, v.y, v.z, v.w }); }

        glm::vec2 vec2From(json const& j)
        {
            return { j.at(0).get<float>(), j.at(1).get<float>() };
        }
        glm::vec4 vec4From(json const& j)
        {
            return { j.at(0).get<float>(), j.at(1).get<float>(),
                     j.at(2).get<float>(), j.at(3).get<float>() };
        }
    }

    Core::VoidResult SceneSerializer::save(World& world, std::filesystem::path const& file)
    {
        using namespace Engine::ECS;

        json root;
        root["name"] = world.name();

        json entities = json::array();

        world.registry().eachEntity([&](Entity e)
        {
            json je;

            if (e.has<NameComponent>())
                je["Name"] = e.get<NameComponent>().name;

            if (e.has<Transform>())
            {
                auto const& t = e.get<Transform>();
                je["Transform"] = {
                    { "position", toJson(t.position) },
                    { "rotation", t.rotation },
                    { "scale",    toJson(t.scale) },
                };
            }

            if (e.has<SpriteRenderer>())
            {
                auto const& s = e.get<SpriteRenderer>();
                je["SpriteRenderer"] = {
                    { "color",        toJson(s.color) },
                    { "texturePath",  s.texturePath },     // serializable asset id
                    { "uvMin",        toJson(s.uvMin) },
                    { "uvMax",        toJson(s.uvMax) },
                    { "tilingFactor", s.tilingFactor },
                };
            }

            if (e.has<Velocity2D>())
            {
                auto const& v = e.get<Velocity2D>();
                je["Velocity2D"] = {
                    { "linear",  toJson(v.linear) },
                    { "angular", v.angular },
                };
            }

            if (e.has<SpriteAnimation>())
            {
                auto const& a = e.get<SpriteAnimation>();
                json frames = json::array();
                for (auto const& f : a.frames)
                    frames.push_back({ { "uvMin", toJson(f.uvMin) },
                                       { "uvMax", toJson(f.uvMax) } });

                je["SpriteAnimation"] = {
                    { "frames",        frames },
                    { "frameDuration", a.frameDuration },
                    { "looping",       a.looping },
                    { "playing",       a.playing },
                };
            }

            entities.push_back(std::move(je));
        });

        root["entities"] = std::move(entities);

        std::ofstream out { file };
        if (!out)
            return std::unexpected(ErrorInfo::make(
                ErrorCode::FileReadFailed,
                std::format("cannot open '{}' for writing", file.string())));

        out << root.dump(2);

        std::println("[Ω::SceneSerializer] saved '{}' ({} entities) -> '{}'",
                     world.name(), world.registry().entityCount(), file.string());
        return {};
    }

    Core::VoidResult SceneSerializer::load(
        World& world,
        std::filesystem::path const& file,
        Core::AssetManager& assets)
    {
        using namespace Engine::ECS;

        std::ifstream in { file };
        if (!in)
            return std::unexpected(ErrorInfo::make(
                ErrorCode::FileReadFailed,
                std::format("cannot open scene file '{}'", file.string())));

        // Wrap parse AND build: nlohmann's .at()/get<> throw on malformed
        // data, so a single try keeps a bad file from crashing the engine.
        try
        {
            json root;
            in >> root;

            // Replace entities; keep the world's systems.
            world.clearEntities();
            if (root.contains("name"))
                world.setName(root["name"].get<std::string>());

            for (auto const& je : root.value("entities", json::array()))
            {
            auto e = world.createEntity(je.value("Name", std::string { "Entity" }));

            if (je.contains("Transform"))
            {
                auto const& jt = je["Transform"];
                e.add<Transform>(Transform{
                    .position = vec2From(jt.at("position")),
                    .rotation = jt.at("rotation").get<float>(),
                    .scale    = vec2From(jt.at("scale")),
                });
            }

            if (je.contains("SpriteRenderer"))
            {
                auto const& js = je["SpriteRenderer"];
                SpriteRenderer s;
                s.color        = vec4From(js.at("color"));
                s.texturePath  = js.value("texturePath", std::string {});
                s.uvMin        = vec2From(js.at("uvMin"));
                s.uvMax        = vec2From(js.at("uvMax"));
                s.tilingFactor = js.value("tilingFactor", 1.0f);

                // Resolve the asset path back to a live texture pointer.
                if (!s.texturePath.empty())
                    s.texture = assets.load<Renderer::Texture2D>(s.texturePath);

                e.add<SpriteRenderer>(std::move(s));
            }

            if (je.contains("Velocity2D"))
            {
                auto const& jv = je["Velocity2D"];
                e.add<Velocity2D>(Velocity2D{
                    .linear  = vec2From(jv.at("linear")),
                    .angular = jv.at("angular").get<float>(),
                });
            }

            if (je.contains("SpriteAnimation"))
            {
                auto const& ja = je["SpriteAnimation"];
                SpriteAnimation anim;
                for (auto const& jf : ja.value("frames", json::array()))
                    anim.frames.push_back({ vec2From(jf.at("uvMin")), vec2From(jf.at("uvMax")) });
                anim.frameDuration = ja.value("frameDuration", 0.1f);
                anim.looping       = ja.value("looping", true);
                anim.playing       = ja.value("playing", true);
                e.add<SpriteAnimation>(std::move(anim));
            }
            }
        }
        catch (std::exception const& ex)
        {
            return std::unexpected(ErrorInfo::make(
                ErrorCode::FileReadFailed,
                std::format("error loading '{}': {}", file.string(), ex.what())));
        }

        std::println("[Ω::SceneSerializer] loaded '{}' ({} entities) <- '{}'",
                     world.name(), world.registry().entityCount(), file.string());
        return {};
    }

} // namespace Scene