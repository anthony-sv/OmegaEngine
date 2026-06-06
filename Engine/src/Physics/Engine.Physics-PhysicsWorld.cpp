module;

#include "glm/glm.hpp"
#include "box2d/box2d.h"

module Engine.Physics:PhysicsWorld;

import :PhysicsWorld;
import :Math;
import std;

namespace Engine::Physics
{
    using EntityId = PhysicsWorld::EntityId;

    namespace
    {
        // ── glm <-> Box2D scalars/vectors ───────────────────────────
        b2Vec2    toB2 (glm::vec2 v) { return b2Vec2{ v.x, v.y }; }
        glm::vec2 toGlm(b2Vec2   v)  { return glm::vec2{ v.x, v.y }; }

        // Entity id <-> body userData. We stash the 32-bit entity id in
        // the body's void* userData so contact/sensor/ray results map
        // straight back to entities without a reverse lookup.
        void*    toUserData  (EntityId id) { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(id)); }
        EntityId fromUserData(void* p)     { return static_cast<EntityId>(reinterpret_cast<std::uintptr_t>(p)); }

        EntityId entityOfShape(b2ShapeId shape)
        {
            return fromUserData(b2Body_GetUserData(b2Shape_GetBody(shape)));
        }

        // Build a b2Body from our BodyDef. BodyType order matches
        // b2BodyType (Static=0, Kinematic=1, Dynamic=2), so the cast is
        // direct.
        b2BodyId createBodyRaw(b2WorldId world, EntityId id, PhysicsWorld::BodyDef const& bd)
        {
            b2BodyDef def       = b2DefaultBodyDef();
            def.type            = static_cast<b2BodyType>(static_cast<int>(bd.type));
            def.position        = toB2(bd.position);
            def.rotation        = b2MakeRot(bd.angle);
            def.linearVelocity  = toB2(bd.linearVelocity);
            def.gravityScale    = bd.gravityScale;
            def.fixedRotation   = bd.fixedRotation;
            def.userData        = toUserData(id);
            return b2CreateBody(world, &def);
        }

        b2ShapeDef makeShapeDef(PhysicsWorld::ShapeDef const& sd)
        {
            b2ShapeDef def           = b2DefaultShapeDef();
            def.density              = sd.density;
            def.material.friction    = sd.friction;
            def.material.restitution = sd.restitution;
            def.isSensor             = sd.isSensor;
            def.enableContactEvents  = true;    // we want begin/end touch
            def.enableSensorEvents   = true;    // ...and sensor overlaps
            return def;
        }
    }

    // =================================================================
    //  Impl -- the actual Box2D state (RAII over the b2World).
    // =================================================================

    struct PhysicsWorld::Impl
    {
        b2WorldId                              world {};   // zero == invalid
        std::unordered_map<EntityId, b2BodyId> bodies;

        std::vector<ContactPair> contactBegin, contactEnd;
        std::vector<SensorPair>  sensorBegin,  sensorEnd;

        ~Impl()
        {
            if (b2World_IsValid(world))
                b2DestroyWorld(world);
        }
    };

    // =================================================================
    //  Construction / lifetime
    // =================================================================

    PhysicsWorld::PhysicsWorld(glm::vec2 gravity)
        : m_impl { std::make_unique<Impl>() }
    {
        b2WorldDef def = b2DefaultWorldDef();
        def.gravity    = toB2(gravity);
        m_impl->world  = b2CreateWorld(&def);
    }

    PhysicsWorld::~PhysicsWorld()                                  = default;
    PhysicsWorld::PhysicsWorld(PhysicsWorld&&) noexcept            = default;
    PhysicsWorld& PhysicsWorld::operator=(PhysicsWorld&&) noexcept = default;

    void PhysicsWorld::setGravity(glm::vec2 gravity)
    {
        b2World_SetGravity(m_impl->world, toB2(gravity));
    }

    // =================================================================
    //  Body lifecycle
    // =================================================================

    void PhysicsWorld::createBoxBody(EntityId id, BodyDef const& bd, ShapeDef const& sd,
                                     glm::vec2 halfSize, glm::vec2 offset)
    {
        b2BodyId   body  = createBodyRaw(m_impl->world, id, bd);
        b2ShapeDef shape = makeShapeDef(sd);
        b2Polygon  box   = b2MakeOffsetBox(halfSize.x, halfSize.y, toB2(offset), b2MakeRot(0.0f));
        b2CreatePolygonShape(body, &shape, &box);
        m_impl->bodies[id] = body;
    }

    void PhysicsWorld::createStaticBoxesBody(EntityId id, glm::vec2 position, ShapeDef const& sd,
                                             std::span<BoxShape const> boxes)
    {
        if (boxes.empty())
            return;

        BodyDef bd;
        bd.type     = BodyType::Static;
        bd.position = position;

        b2BodyId   body  = createBodyRaw(m_impl->world, id, bd);
        b2ShapeDef shape = makeShapeDef(sd);

        for (auto const& b : boxes)
        {
            b2Polygon box = b2MakeOffsetBox(b.halfSize.x, b.halfSize.y, toB2(b.center), b2MakeRot(0.0f));
            b2CreatePolygonShape(body, &shape, &box);
        }
        m_impl->bodies[id] = body;
    }

    void PhysicsWorld::createCircleBody(
        EntityId id, 
        BodyDef const& bd, 
        ShapeDef const& sd,
        float radius, glm::vec2 offset
    )
    {
        b2BodyId   body   = createBodyRaw(m_impl->world, id, bd);
        b2ShapeDef shape  = makeShapeDef(sd);
        b2Circle   circle { toB2(offset), radius };
        b2CreateCircleShape(body, &shape, &circle);
        m_impl->bodies[id] = body;
    }

    void PhysicsWorld::createPolygonBody(
        EntityId id, 
        BodyDef const& bd, 
        ShapeDef const& sd,
        std::span<glm::vec2 const> points
    )
    {
        if (points.size() < 3)
            return;     // need at least a triangle

        std::vector<b2Vec2> pts;
        pts.reserve(points.size());
        for (auto const p : points)
            pts.push_back(toB2(p));

        b2Hull hull = b2ComputeHull(pts.data(), static_cast<int>(pts.size()));
        if (hull.count < 3)
            return;     // degenerate (collinear) -> skip

        b2Polygon  poly  = b2MakePolygon(&hull, 0.0f);
        b2BodyId   body  = createBodyRaw(m_impl->world, id, bd);
        b2ShapeDef shape = makeShapeDef(sd);
        b2CreatePolygonShape(body, &shape, &poly);
        m_impl->bodies[id] = body;
    }

    void PhysicsWorld::destroyBody(EntityId id)
    {
        if (auto it = m_impl->bodies.find(id); it != m_impl->bodies.end())
        {
            if (b2Body_IsValid(it->second))
                b2DestroyBody(it->second);
            m_impl->bodies.erase(it);
        }
    }

    bool PhysicsWorld::hasBody(EntityId id) const
    {
        return m_impl->bodies.contains(id);
    }

    void PhysicsWorld::setLinearVelocity(EntityId id, glm::vec2 velocity)
    {
        if (auto it = m_impl->bodies.find(id); it != m_impl->bodies.end())
            b2Body_SetLinearVelocity(it->second, toB2(velocity));
    }

    void PhysicsWorld::setTransform(EntityId id, glm::vec2 position, float angle)
    {
        if (auto it = m_impl->bodies.find(id); it != m_impl->bodies.end())
            b2Body_SetTransform(it->second, toB2(position), b2MakeRot(angle));
    }

    glm::vec2 PhysicsWorld::getLinearVelocity(EntityId id) const
    {
        if (auto it = m_impl->bodies.find(id); it != m_impl->bodies.end())
        {
            b2Vec2 const v = b2Body_GetLinearVelocity(it->second);
            return { v.x, v.y };
        }
        return { 0.0f, 0.0f };
    }

    void PhysicsWorld::applyLinearImpulse(EntityId id, glm::vec2 impulse)
    {
        if (auto it = m_impl->bodies.find(id); it != m_impl->bodies.end())
            b2Body_ApplyLinearImpulseToCenter(it->second, toB2(impulse), true);
    }

    void PhysicsWorld::applyForce(EntityId id, glm::vec2 force)
    {
        if (auto it = m_impl->bodies.find(id); it != m_impl->bodies.end())
            b2Body_ApplyForceToCenter(it->second, toB2(force), true);
    }

    // =================================================================
    //  Step + event buffering
    // =================================================================

    void PhysicsWorld::step(float dt, int subStepCount)
    {
        b2World_Step(m_impl->world, dt, subStepCount);

        auto& I = *m_impl;
        I.contactBegin.clear();
        I.contactEnd.clear();
        I.sensorBegin.clear();
        I.sensorEnd.clear();

        // ── Contacts (solid-vs-solid) ───────────────────────────────
        b2ContactEvents ce = b2World_GetContactEvents(m_impl->world);

        for (int i = 0; i < ce.beginCount; ++i)
        {
            auto const& e = ce.beginEvents[i];

            ContactPair pair;
            pair.a = entityOfShape(e.shapeIdA);
            pair.b = entityOfShape(e.shapeIdB);
            pair.manifold.normal = toGlm(e.manifold.normal);
            for (int p = 0; p < e.manifold.pointCount; ++p)
                pair.manifold.points.push_back(
                    { toGlm(e.manifold.points[p].point), -e.manifold.points[p].separation });

            I.contactBegin.push_back(std::move(pair));
        }

        for (int i = 0; i < ce.endCount; ++i)
        {
            auto const& e = ce.endEvents[i];     // shapes may be destroyed
            I.contactEnd.push_back(
                {
                    b2Shape_IsValid(e.shapeIdA) ? entityOfShape(e.shapeIdA) : EntityId { 0xFFFF'FFFF },
                    b2Shape_IsValid(e.shapeIdB) ? entityOfShape(e.shapeIdB) : EntityId { 0xFFFF'FFFF },
                    {}
                }
            );
        }

        // ── Sensors (trigger volumes) ───────────────────────────────
        b2SensorEvents se = b2World_GetSensorEvents(m_impl->world);

        for (int i = 0; i < se.beginCount; ++i)
        {
            auto const& e = se.beginEvents[i];
            I.sensorBegin.push_back({ entityOfShape(e.sensorShapeId), entityOfShape(e.visitorShapeId) });
        }

        for (int i = 0; i < se.endCount; ++i)
        {
            auto const& e = se.endEvents[i];
            I.sensorEnd.push_back(
                {
                    b2Shape_IsValid(e.sensorShapeId)  ? entityOfShape(e.sensorShapeId)  : EntityId { 0xFFFF'FFFF },
                    b2Shape_IsValid(e.visitorShapeId) ? entityOfShape(e.visitorShapeId) : EntityId { 0xFFFF'FFFF } 
                }
            );
        }
    }

    void PhysicsWorld::eachBody(std::function<void(EntityId, glm::vec2, float)> const& fn) const
    {
        for (auto const& [id, body] : m_impl->bodies)
        {
            b2Vec2 const p     = b2Body_GetPosition(body);
            float  const angle = b2Rot_GetAngle(b2Body_GetRotation(body));
            fn(id, toGlm(p), angle);
        }
    }

    // =================================================================
    //  Buffered event access
    // =================================================================

    std::span<PhysicsWorld::ContactPair const> PhysicsWorld::contactBeginEvents() const { return m_impl->contactBegin; }
    std::span<PhysicsWorld::ContactPair const> PhysicsWorld::contactEndEvents()   const { return m_impl->contactEnd; }
    std::span<PhysicsWorld::SensorPair  const> PhysicsWorld::sensorBeginEvents()  const { return m_impl->sensorBegin; }
    std::span<PhysicsWorld::SensorPair  const> PhysicsWorld::sensorEndEvents()    const { return m_impl->sensorEnd; }

    // =================================================================
    //  Queries
    // =================================================================

    PhysicsWorld::RayHit PhysicsWorld::raycast(glm::vec2 origin, glm::vec2 direction, float maxDistance) const
    {
        glm::vec2 const dir = (glm::length(direction) > 0.0f) ? glm::normalize(direction) : glm::vec2 { 0.0f, 0.0f };
        b2Vec2    const translation = toB2(dir * maxDistance);

        b2RayResult const r = b2World_CastRayClosest(m_impl->world, toB2(origin), translation, b2DefaultQueryFilter());

        RayHit hit;
        hit.hit = r.hit;
        if (r.hit)
        {
            hit.entity   = entityOfShape(r.shapeId);
            hit.point    = toGlm(r.point);
            hit.normal   = toGlm(r.normal);
            hit.fraction = r.fraction;
        }
        return hit;
    }

    void PhysicsWorld::overlapAABB(AABB const& region, std::function<void(EntityId)> const& fn) const
    {
        b2AABB aabb;
        aabb.lowerBound = toB2(region.min);
        aabb.upperBound = toB2(region.max);

        // Box2D's query takes a C function + void* context. Forward the
        // std::function through the context; the captureless lambda
        // decays to the required function pointer.
        auto const* fnPtr = &fn;
        auto callback = [](b2ShapeId shape, void* context) -> bool
        {
            auto const* f = *static_cast<std::function<void(EntityId)> const* const*>(context);
            (*f)(entityOfShape(shape));
            return true;    // keep visiting
        };
        b2World_OverlapAABB(m_impl->world, aabb, b2DefaultQueryFilter(), callback, &fnPtr);
    }

} // namespace Physics