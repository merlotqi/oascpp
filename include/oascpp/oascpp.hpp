#pragma once

// Umbrella header for the core layer: value model, JSON Pointer, error contract
// and Result. JSON adapters are opt-in and live in <oascpp/adapters/...>; the
// JSON Schema engine has its own umbrella at <oascpp/schema/jsonschema.hpp> so
// programs that only need the value model do not pay for it.

#include "oascpp/document_builder.hpp"
#include "oascpp/error.hpp"
#include "oascpp/json_pointer.hpp"
#include "oascpp/result.hpp"
#include "oascpp/value.hpp"
#include "oascpp/version.hpp"
