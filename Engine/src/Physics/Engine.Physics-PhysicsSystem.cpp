module;

#include "glm/glm.hpp"

module Engine.Physics:PhysicsSystem;

import :PhysicsSystem;
import :PhysicsWorld;
import :Events;
import :Math;
import Engine.Core;
import Engine.ECS;
import std;

namespace Engine::Physics
{
    namespace
    {
        // RigidBody2D::BodyType (Static, Dynamic, Kinematic) -> the
        // world's BodyType (Static, Kinematic, Dynamic). The two enums
        // order their values differently, so map explicitly.
        PhysicsWorld::BodyType toWorldType(ECS::RigidBody2D::BodyType t)
        {
            using S = ECS::RigidBody2D::BodyType;
            switch (t)
            {
                case S::Static:    return PhysicsWorld::BodyType::Static;
                case S::Kinematic: return PhysicsWorld::BodyType::Kinematic;
                case S::Dynamic:
                default:           return PhysicsWorld::BodyType::Dynamic;
            }
        }

        // Shared body params from Transform + RigidBody2D (degrees ->
        // radians for the angle).
        PhysicsWorld::BodyDef makeBodyDef(ECS::Transform const& tf, ECS::RigidBody2D const& rb)
        {
            PhysicsWorld::BodyDef bd;
            bd.type          = toWorldType(rb.type);
            bd.position      = tf.position;
            bd.angle         = glm::radians(tf.rotation);
            bd.gravityScale  = rb.gravityScale;
            bd.fixedRotation = rb.fixedRotation;
            return bd;
        }
    }

    PhysicsSystem::PhysicsSystem(ECS::Registry& registry, Core::EventBus& bus, glm::vec2 gravity)
        : m_registry { registry }
        , m_bus      { bus }
        , m_world    { gravity }
    {}

    void PhysicsSystem::onUpdate(float dt)
    {
        syncToSim();
        m_world.step(dt);
        syncFromSim();
        dispatchEvents();
    }

    // -- ECS -> simulation ----------------------------------------------

    void PhysicsSystem::syncToSim()
    {
        // 1. Create a body for every RigidBody2D + Transform that doesn't
        //    have one yet; keep kinematic bodies fed from Velocity2D.
        for (auto&& [e, rb, tf] : m_registry.view<ECS::RigidBody2D, ECS::Transform>().each())
        {
            auto const id = static_cast<PhysicsWorld::EntityId>(e);

            if (m_world.hasBody(id))
            {
                if (
                    rb.type == ECS::RigidBody2D::BodyType::Kinematic
                    && m_registry.hasComponent<ECS::Velocity2D>(e)
                    )
                    m_world.setLinearVelocity(
                        id, 
                        m_registry.getComponent<ECS::Velocity2D>(e).linear
                    );
                continue;
            }

            auto bd = makeBodyDef(tf, rb);
            if (m_registry.hasComponent<ECS::Velocity2D>(e))
                bd.linearVelocity = m_registry.getComponent<ECS::Velocity2D>(e).linear;

            if (m_registry.hasComponent<ECS::BoxCollider2D>(e))
            {
                auto const& c = m_registry.getComponent<ECS::BoxCollider2D>(e);
                m_world.createBoxBody(
                    id, 
                    bd, 
                    { c.density, c.friction, c.restitution, c.isTrigger },
                    c.size * 0.5f, 
                    c.offset
                );
            }
            else if (m_registry.hasComponent<ECS::CircleCollider2D>(e))
            {
                auto const& c = m_registry.getComponent<ECS::CircleCollider2D>(e);
                m_world.createCircleBody(
                    id, 
                    bd, 
                    { c.density, c.friction, c.restitution, c.isTrigger },
                    c.radius, 
                    c.offset
                );
            }
            else if (m_registry.hasComponent<ECS::PolygonCollider2D>(e))
            {
                auto const& c = m_registry.getComponent<ECS::PolygonCollider2D>(e);
                m_world.createPolygonBody(
                    id, 
                    bd, 
                    { c.density, c.friction, c.restitution, c.isTrigger },
                    c.points
                );
            }
            // else: no collider -> no body (a physics body needs a shape).
        }

        // 2. Destroy bodies whose entity or RigidBody2D disappeared
        //    (collect first -- can't mutate the body map mid-iteration).
        std::vector<PhysicsWorld::EntityId> stale;
        m_world.eachBody(
            [&](PhysicsWorld::EntityId id, glm::vec2, float)
            {
                auto ent = m_registry.entityFromId(id);
                if (!ent.valid() || !ent.has<ECS::RigidBody2D>())
                    stale.push_back(id);
            }
        );
        for (auto const id : stale)
            m_world.destroyBody(id);
    }

    // -- simulation -> ECS ----------------------------------------------

    void PhysicsSystem::syncFromSim()
    {
        m_world.eachBody(
            [&](PhysicsWorld::EntityId id, glm::vec2 position, float angle)
            {
                auto ent = m_registry.entityFromId(id);
                if (!ent.valid() || !ent.has<ECS::Transform>())
                    return;

                auto& tf    = ent.get<ECS::Transform>();
                tf.position = position;
                tf.rotation = glm::degrees(angle);
            }
        );
    }

    void PhysicsSystem::dispatchEvents()
    {
        for (auto const& c : m_world.contactBeginEvents())
            m_bus.publish(
                CollisionEnterEvent{ m_registry.entityFromId(c.a), m_registry.entityFromId(c.b), c.manifold }
            );

        for (auto const& c : m_world.contactEndEvents())
            m_bus.publish(
                CollisionExitEvent{ m_registry.entityFromId(c.a), m_registry.entityFromId(c.b) }
            );

        for (auto const& s : m_world.sensorBeginEvents())
            m_bus.publish(
                TriggerEnterEvent{ m_registry.entityFromId(s.sensor), m_registry.entityFromId(s.visitor) }
            );

        for (auto const& s : m_world.sensorEndEvents())
            m_bus.publish(
                TriggerExitEvent{ m_registry.entityFromId(s.sensor), m_registry.entityFromId(s.visitor) }
            );
    }

    // -- Queries --------------------------------------------------------

    RaycastHit PhysicsSystem::raycast(glm::vec2 origin, glm::vec2 direction, float maxDistance) const
    {
        auto const h = m_world.raycast(origin, direction, maxDistance);

        RaycastHit out;
        out.hit = h.hit;
        if (h.hit)
        {
            out.entity   = m_registry.entityFromId(h.entity);
            out.point    = h.point;
            out.normal   = h.normal;
            out.fraction = h.fraction;
        }
        return out;
    }

    void PhysicsSystem::overlapAABB(AABB const& region, std::function<void(ECS::Entity)> const& fn) const
    {
        m_world.overlapAABB(region, [&](PhysicsWorld::EntityId id) { fn(m_registry.entityFromId(id)); });
    }

} // namespace Physics