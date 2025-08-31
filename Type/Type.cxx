#include "Type.hxx"

#include "../Interp/Interpreter.hxx"


namespace type {

    std::string LiteralType::text(const size_t indent) const {
        // return t.empty() ? "Any" : t;
        const auto& type = stringify(*cls, indent);
        return type.empty() ? "Any" : type;
    }

} // namespace type