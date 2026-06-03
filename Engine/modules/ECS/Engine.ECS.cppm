export module Engine.ECS;

// Component partitions -- pure data structs, grouped by domain.
export import :CoreComponents;
export import :TransformComponents;
export import :RenderComponents;
export import :PhysicsComponents;
export import :AnimationComponents;
export import :TilemapComponents;
export import :TagComponents;

// Entity handle + Registry wrapper (custom sparse-set ECS).
export import :Registry;