export module Engine.Systems;

// Engine systems -- the LOGIC that operates on ECS components.
//
// Components (Engine.ECS) are pure data. Systems (here) are the
// behavior: they query the Registry for entities matching a set of
// components and act on them. This separation -- data in one module,
// logic in another -- is the heart of the ECS pattern and keeps each
// side independently testable and swappable.
//
// As the engine grows this module gains partitions:
//   :RenderSystem     -- draws sprites           (render phase)  [done]
//   :MovementSystem   -- integrates velocity     (update phase)  [done]
//   :AnimationSystem  -- advances sprite frames  (update phase)  [done]
//   :PhysicsSystem    -- collisions / dynamics   (fixed step)    [later]

export import :RenderSystem;
export import :MovementSystem;
export import :AnimationSystem;