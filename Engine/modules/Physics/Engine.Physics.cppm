export module Engine.Physics;

// Physics layer -- a thin, swappable wrapper over Box2D 3.x.
//
//   Engine.ECS      = data (RigidBody2D, Box/Circle/Polygon colliders)
//   Engine.Physics  = simulation (PhysicsWorld wraps b2World; the
//                                 PhysicsSystem drives it on the fixed
//                                 timestep and bridges it to the ECS)
//
// Box2D appears ONLY inside Engine.Physics-PhysicsWorld.cpp -- never in
// an interface -- so consumers import clean glm/ECS types and the
// backend can be replaced without touching them.

export import :Math;            // AABB, ContactPoint, ContactManifold
export import :Events;          // Collision/Trigger events, RaycastHit
export import :PhysicsWorld;    // the b2World wrapper (PIMPL)
export import :PhysicsSystem;   // the ECS <-> world bridge (ISystem)