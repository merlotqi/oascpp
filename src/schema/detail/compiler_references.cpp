#include <oascpp/schema/compiler.hpp>

namespace oascpp {
namespace jsonschema {

[[nodiscard]] SchemaId SchemaCompiler::resolve_reference(std::string_view ref) {
  if (ref.empty()) {
    fail(ErrorCode::unresolved_reference, "empty $ref");
    return no_schema;
  }
  if (ref.front() != '#') {
    fail(ErrorCode::unresolved_reference, "external references are not supported yet: " + std::string(ref));
    return no_schema;
  }
  if (ref.size() == 1) {
    return compile_schema(document_.root());
  }

  const std::string_view fragment = ref.substr(1);
  if (fragment.front() == '/') {
    ValueView target = document_.root();
    std::size_t position = 0;
    while (position < fragment.size()) {
      const std::size_t start = position + 1;
      std::size_t end = fragment.find('/', start);
      if (end == std::string_view::npos) {
        end = fragment.size();
      }
      const std::string token = unescape_pointer_token(fragment.substr(start, end - start));
      target = descend(target, token);
      if (!target.valid()) {
        fail(ErrorCode::unresolved_reference, "cannot resolve reference: " + std::string(ref));
        return no_schema;
      }
      position = end;
    }
    return compile_schema(target);
  }

  build_anchor_index();
  const auto found = anchors_.find(std::string(fragment));
  if (found == anchors_.end()) {
    fail(ErrorCode::unresolved_reference, "unknown anchor: " + std::string(ref));
    return no_schema;
  }
  return compile_schema(ValueView(&document_, found->second));
}

[[nodiscard]] ValueView SchemaCompiler::descend(ValueView container, const std::string& token) {
  if (container.is_object()) {
    return container.find(token);
  }
  if (container.is_array()) {
    if (token.empty()) {
      return ValueView();
    }
    std::size_t index = 0;
    for (const char character : token) {
      if (character < '0' || character > '9') {
        return ValueView();
      }
      index = index * 10 + static_cast<std::size_t>(character - '0');
    }
    return container.at(index);
  }
  return ValueView();
}

void SchemaCompiler::build_anchor_index() {
  if (anchors_built_) {
    return;
  }
  anchors_built_ = true;
  std::vector<ValueView> pending;
  pending.push_back(document_.root());
  while (!pending.empty()) {
    const ValueView current = pending.back();
    pending.pop_back();
    if (!current.valid()) {
      continue;
    }
    if (current.is_object()) {
      const ValueView anchor = current.find("$anchor");
      if (anchor.valid() && anchor.is_string()) {
        anchors_.emplace(std::string(anchor.as_string()), current.index());
      }
    }
    if (current.is_container()) {
      for (const ValueView child : current) {
        pending.push_back(child);
      }
    }
  }
}

}  // namespace jsonschema
}  // namespace oascpp
