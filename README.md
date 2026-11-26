# oascpp

Static C++17 library with an early JSON Schema validation core for
OpenAPI-oriented applications. It does not yet validate complete OpenAPI
documents or request/response exchanges. Template-based bindings and optional
backend adapters remain header-based.

The goal is a library that validates the *document* and the *exchanges* an OpenAPI
description promises: request parameters and bodies, response payloads, content
negotiation. The JSON backend is an adapter concern, so the core never depends on
a specific parser.

## Status

Pre-alpha (library version 0.3.0). Available today:

- `oascpp/value.hpp` — arena value model (`ParsedDocument`, `ValueView`, `deep_equal`)
- `oascpp/document_builder.hpp` — arena builder used by adapters and tests
- `oascpp/json_pointer.hpp` — RFC 6901 escaping helpers
- `oascpp/error.hpp` — `ErrorCode`, `ErrorPath`, `ValidationError`, `ValidationResult`
- `oascpp/result.hpp` — `Result<T>` for cold, single-error entry points
- `oascpp/schema/schema.hpp` — JSON Schema compiler and validator, plus the
  owning `schema::Schema` wrapper; components live in
  `oascpp/schema/{dialect,compiled_schema,compiler,validator}.hpp`.
- `oascpp/adapters/nlohmann_json.hpp` — optional nlohmann/json adapter

Not implemented yet: `unevaluatedProperties` / `unevaluatedItems` (needs
annotation tracking), dynamic scope for `$dynamicRef`, external `$ref` loading,
`pattern` and `format` engines (both are injectable seams), the OpenAPI document
model, path matching, parameter serialization, request/response pipelines.

### Schema engine notes

- One interpreter serves every dialect; `DialectProfile` is data, not a second
  compiler. 2020-12 and OpenAPI 3.1 are supported today, other dialects report
  `unsupported_dialect` instead of silently mis-validating.
- `$ref` is resolved at compile time and cycles are legal. External references
  are a compile error until a loader seam lands.
- `unevaluatedProperties` / `unevaluatedItems` and `$dynamicRef` dynamic scope
  are not enforced yet; applicable validations are reported as incomplete.
- Integer constraints and integer equality preserve the stored 64-bit value.
  Real-number arithmetic, including `multipleOf`, still uses IEEE 754 `double`.
- `pattern` and `format` require callbacks in `schema::ValidationOptions`; when
  an applicable keyword has no callback, `ValidationResult::complete()` and
  `ok()` are false rather than reporting a successful validation. The core stays
  free of `<regex>`.
- `propertyNames` validates names through a scratch document so there is exactly
  one interpretation path.
- `ValidationOptions::max_errors` limits stored diagnostics without changing
  whether validation succeeds, and validation errors include a schema pointer.

## Requirements

- a C++17 compiler
- CMake 3.16 or newer to build the tests
- Git and network access on the first test configure; CMake FetchContent downloads
  GoogleTest 1.16.0
- link applications against the exported `oascpp::oascpp` static-library target
- optionally [nlohmann/json](https://github.com/nlohmann/json); without it the
  adapter header compiles to nothing

## Building the tests

```sh
cmake -S . -B build -DOASCPP_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The build produces a static `oascpp` library. Installed CMake consumers link
against `oascpp::oascpp`, which also provides the public include directory and
C++17 requirement.

## Usage

```cpp
#include <oascpp/oascpp.hpp>
#include <oascpp/adapters/nlohmann_json.hpp>

const oascpp::ParsedDocument document =
    oascpp::adapters::nlohmann_json::parse(R"({"info": {"title": "pets"}})");

const oascpp::ValueView title = document.root().find("info").find("title");
if (title.is_string()) {
  use(title.as_string());
}
```

Without a backend, documents are built directly:

```cpp
oascpp::DocumentBuilder builder;
const auto root = builder.make_object();
builder.append_member(root, "count", builder.make_integer(3));
const oascpp::ParsedDocument document = builder.finish();
```

### Binding validated JSON to C++ types

Define the types in an application header. Register their fields in one
`.cpp` file and call the registration function at startup.

```cpp
// user_bindings.cpp
#include <oascpp/binding.hpp>
#include "user.hpp"

void register_user_bindings(oascpp::Bindings& bindings) {
  using oascpp::field;
  OASCPP_BIND(bindings, User::Address, field("postalCode", &User::Address::postcode));
  OASCPP_BIND(bindings, User, field("home", &User::home), field("addresses", &User::addresses));
}
```

Here `User` defines `Address home` and `std::vector<Address> addresses`, and
`User::Address` defines `std::string postcode`. Share the populated registry
with the code that calls `bindings.decode<User>(document.root())` after schema
validation. The decoder returns a `User` or diagnostics with JSON Pointer paths.
Vectors decode recursively. Optional members map missing fields and JSON null
to `std::nullopt`. Bound types must be default-constructible and their mapped
members writable.

## Layout

| Header | Purpose |
| --- | --- |
| `oascpp/value.hpp` | arena model, `ValueView` |
| `oascpp/document_builder.hpp` | `DocumentBuilder` |
| `oascpp/json_pointer.hpp` | pointer token escaping |
| `oascpp/error.hpp` | error codes, paths, results |
| `oascpp/result.hpp` | `Result<T>` |
| `oascpp/adapters/*.hpp` | JSON backends |

## Conventions

- Types, templates and concepts: `PascalCase`
- Functions, member functions, variables, enumerators: `snake_case`
- Macros: `OASCPP_` prefix; file names: `snake_case`
- Namespaces: `oascpp`, implementation details in `oascpp::detail`
- No exceptions on validation hot paths: failures accumulate in `ValidationResult`
- Core headers stay free of `<iostream>`, `<sstream>`, `<regex>` and third-party
  dependencies; adapters are opt-in and self-detecting

## Roadmap

1. Complete JSON Schema 2020-12 conformance using the official test suite
2. Add OpenAPI dialect handling and document structural validation
3. Add path matching, parameter serialization, and request validation
4. Add response validation, content negotiation, and security requirements

## License

Not chosen yet.
