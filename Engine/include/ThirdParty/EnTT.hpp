#pragma once

// =================================================================
//
//  EnTT.hpp -- header-unit shim for the EnTT ECS library.
//
// =================================================================
//
// WHY THIS FILE EXISTS:
//
//   MSVC's named-module + global-module-fragment include path breaks
//   ADL for EnTT's sparse_set_iterator operators (entt::internal::
//   operator== / operator!=). A re-exporting wrapper module doesn't
//   help either, because EnTT's template bodies instantiate in the
//   *consumer's* translation unit, where module-purview ADL is still
//   broken.
//
//   HEADER UNITS are the mechanism Microsoft recommends for legacy
//   headers. When this file is compiled as a header unit (/exportHeader)
//   and imported via `import "ThirdParty/EnTT.hpp";`, every declaration
//   -- including the internal operators -- stays attached to the
//   GLOBAL module and remains ADL-reachable at instantiation time.
//
//   This shim exists (rather than importing <entt/entt.hpp> directly)
//   so the header-unit build target points at a stable path in our
//   own include tree, independent of vcpkg's versioned layout.
//
// =================================================================

#include <entt/entt.hpp>