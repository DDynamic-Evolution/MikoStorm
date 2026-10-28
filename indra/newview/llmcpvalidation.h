#ifndef LL_MCP_VALIDATION_H
#define LL_MCP_VALIDATION_H
#include "llsd.h"
#include "lluuid.h"
#include "llstring.h"
#include "llsdutil.h"
#include <cmath>
#include <string>

namespace LLMCPValidation
{
inline bool validate(const LLSD& value, const LLSD& schema, std::string& error,
                     const std::string& path = "arguments", unsigned depth = 0)
{
    if (depth > 8) { error = "Arguments are nested too deeply"; return false; }
    const std::string type = schema["type"].asString();
    auto fail = [&](const std::string& message) { error = path + ": " + message; return false; };
    if (type == "object")
    {
        if (!value.isMap()) return fail("expected an object");
        for (const LLSD& key : llsd::inArray(schema["required"]))
            if (!value.has(key.asString())) return fail("missing required property '" + key.asString() + "'");
        const LLSD& properties = schema["properties"];
        for (auto it = value.beginMap(); it != value.endMap(); ++it)
        {
            if (!properties.has(it->first))
            {
                if (!schema["additionalProperties"].asBoolean()) return fail("unknown property '" + it->first + "'");
                continue;
            }
            if (!validate(it->second, properties[it->first], error, path + "." + it->first, depth + 1)) return false;
        }
    }
    else if (type == "string")
    {
        if (!value.isString()) return fail("expected a string");
        const auto text = value.asString();
        const auto length = utf8str_to_wstring(text).size();
        if (schema.has("minLength") && length < static_cast<std::size_t>(schema["minLength"].asInteger())) return fail("string is too short");
        if (length > static_cast<std::size_t>(schema.has("maxLength") ? schema["maxLength"].asInteger() : 8192)) return fail("string is too long");
        if (text.find('\0') != std::string::npos) return fail("embedded NUL is not allowed");
        if (schema["format"].asString() == "uuid" && !LLUUID::validate(text)) return fail("invalid UUID");
    }
    else if (type == "number" || type == "integer")
    {
        if (!value.isInteger() && !value.isReal()) return fail("expected a number");
        const double number = value.asReal();
        if (!std::isfinite(number)) return fail("number must be finite");
        if (type == "integer" && std::floor(number) != number) return fail("expected an integer");
        if (schema.has("minimum") && number < schema["minimum"].asReal()) return fail("number is below minimum");
        if (schema.has("maximum") && number > schema["maximum"].asReal()) return fail("number exceeds maximum");
    }
    else if (type == "boolean")
    {
        if (!value.isBoolean()) return fail("expected a boolean");
    }
    else if (type == "array")
    {
        if (!value.isArray()) return fail("expected an array");
        if (value.size() > (schema.has("maxItems") ? schema["maxItems"].asInteger() : 128)) return fail("too many items");
        for (S32 i = 0; i < value.size(); ++i)
            if (!validate(value[i], schema["items"], error, path + "[" + std::to_string(i) + "]", depth + 1)) return false;
    }
    if (schema.has("enum"))
    {
        bool found = false;
        for (const LLSD& allowed : llsd::inArray(schema["enum"]))
            if (allowed.asString() == value.asString()) found = true;
        if (!found) return fail("unsupported value");
    }
    return true;
}
}
#endif
