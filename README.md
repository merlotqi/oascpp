# oascpp

Header-only, template-friendly OpenAPI 3.0 / 3.1 validation for modern C++17.

The goal is a library that validates the *document* and the *exchanges* an OpenAPI
description promises: request parameters and bodies, response payloads, content
negotiation. The JSON backend is an adapter concern, so the core never depends on
a specific parser.

## Status

Pre-alpha (v0.2). Available today:

- `oascpp/value.hpp` — arena value model (`ParsedDocument`, `ValueView`, `deep_equal`)
- `oascpp/document_builder.hpp` — arena builder used by adapters and tests
- `oascpp/json_pointer.hpp` — RFC 6901 escaping helpers
- `oascpp/error.hpp` — `ErrorCode`, `ErrorPath`, `ValidationError`, `ValidationResult`
- `oascpp/result.hpp` — `Result<T>` for cold, single-error entry points
- `oascpp/jsonschema.hpp` — JSON Schema 2020-12 compiler and validator, plus the
  owning `jsonschema::Schema` wrapper; individual pieces live in
  `oascpp/jsonschema/{dialect,schema,compiler,validator}.hpp`
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
- `pattern` and `format` are only interpreted when a matcher is installed in
  `jsonschema::ValidationOptions`; the core stays free of `<regex>`.
- `propertyNames` validates names through a scratch document so there is exactly
  one interpretation path.

## Requirements

- a C++17 compiler
- CMake 3.16 or newer to build the tests
- optionally [nlohmann/json](https://github.com/nlohmann/json); without it the
  adapter header compiles to nothing

## Building the tests

```sh
cmake -S . -B build -DOASCPP_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

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

1. `v0.2` — JSON Schema 2020-12 core with the official test suite as conformance
2. `v0.3` — OpenAPI 3.0 dialect lowering and document structural validation
3. `v0.4` — path matching, parameter serialization, request pipeline
4. `v0.5` — response validation, content negotiation, security requirements

## License

Not chosen yet.
