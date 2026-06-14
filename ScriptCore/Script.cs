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

    // Called when the instance is torn down for good: a scene switch, or the
    // editor's Stop. NOT called on hot reload (the instance is swapped, not
    // destroyed). Stop sounds / release anything global here.
    public virtual void OnDestroy() { }

    // Physics callbacks. Collisions fire for solid contacts (with the contact
    // normal); triggers fire for sensor overlaps (just the other entity). Both
    // sides of a contact are notified.
    public virtual void OnCollisionEnter(Collision collision) { }
    public virtual void OnCollisionExit(Collision collision) { }
    public virtual void OnTriggerEnter(Entity other) { }
    public virtual void OnTriggerExit(Entity other) { }
}