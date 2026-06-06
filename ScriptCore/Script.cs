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

    // Physics callbacks. `other` is the entity this one touched. Collisions
    // fire for solid contacts; triggers fire for sensor overlaps. Both sides
    // of a contact are notified.
    public virtual void OnCollisionEnter(Entity other) { }
    public virtual void OnCollisionExit(Entity other) { }
    public virtual void OnTriggerEnter(Entity other) { }
    public virtual void OnTriggerExit(Entity other) { }
}