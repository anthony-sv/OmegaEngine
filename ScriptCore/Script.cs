namespace OmegaEngine;

// Base class for all game scripts. Override the lifecycle hooks.
//
// `Entity` is a field (not a property) so `Entity.Position = v` is a valid
// lvalue assignment -- through a struct *property* it would only mutate a
// temporary copy.
public abstract class Script
{
    public Entity Entity;

    public virtual void OnCreate() { }
    public virtual void OnUpdate(float dt) { }
}