namespace OmegaEngine;

// Exposes a NON-public field in the editor inspector and saves it in the scene
// (public fields are exposed automatically). Mirrors Unity's [SerializeField].
[AttributeUsage(AttributeTargets.Field)]
public sealed class SerializeFieldAttribute : Attribute;