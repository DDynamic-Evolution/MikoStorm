#include "linden_common.h"
#include "../test/lltut.h"
#include "../llmcpvalidation.h"
#include <limits>
namespace tut
{
struct MCPValidation {};
using MCPGroup = test_group<MCPValidation>;
using MCPObject = MCPGroup::object;
MCPGroup mcpValidation("LLMCPValidation");
template<> template<> void MCPObject::test<1>()
{
    const LLSD schema = LLSDMap("type", "object")("additionalProperties", false)
        ("properties", LLSDMap("x", LLSDMap("type", "number")("minimum", 0)("maximum", 256))
            ("enabled", LLSDMap("type", "boolean")))("required", llsd::array("x"));
    std::string error;
    ensure("valid numeric coordinate", LLMCPValidation::validate(LLSDMap("x", 128), schema, error));
    ensure("required coordinate", !LLMCPValidation::validate(LLSD::emptyMap(), schema, error));
    ensure("no string coercion", !LLMCPValidation::validate(LLSDMap("x", "128"), schema, error));
    ensure("no boolean coercion", !LLMCPValidation::validate(LLSDMap("x", 1)("enabled", "false"), schema, error));
    ensure("unknown argument rejected", !LLMCPValidation::validate(LLSDMap("x", 1)("extra", 2), schema, error));
    ensure("negative coordinate rejected", !LLMCPValidation::validate(LLSDMap("x", -1), schema, error));
    ensure("region bound", !LLMCPValidation::validate(LLSDMap("x", 257), schema, error));
    ensure("NaN rejected", !LLMCPValidation::validate(LLSDMap("x", std::numeric_limits<double>::quiet_NaN()), schema, error));
    ensure("infinity rejected", !LLMCPValidation::validate(LLSDMap("x", std::numeric_limits<double>::infinity()), schema, error));
}
template<> template<> void MCPObject::test<2>()
{
    std::string error;
    const LLSD schema = LLSDMap("type", "integer")("minimum", 1)("maximum", 200);
    ensure("integer-valued real accepted", LLMCPValidation::validate(LLSD(2.0), schema, error));
    ensure("fractional limits rejected", !LLMCPValidation::validate(LLSD(2.5), schema, error));
    ensure("unbounded limit rejected", !LLMCPValidation::validate(LLSD(201), schema, error));
    ensure("zero limit rejected", !LLMCPValidation::validate(LLSD(0), schema, error));
}
template<> template<> void MCPObject::test<3>()
{
    std::string error;
    const LLSD schema = LLSDMap("type", "string")("minLength", 1)("maxLength", 2);
    ensure("unicode codepoints, not bytes", LLMCPValidation::validate(LLSD("\xc3\xa4\xc3\xb6"), schema, error));
    ensure("long string rejected", !LLMCPValidation::validate(LLSD("abc"), schema, error));
    ensure("empty string rejected", !LLMCPValidation::validate(LLSD(""), schema, error));
    ensure("embedded NUL rejected", !LLMCPValidation::validate(LLSD(std::string("a\0", 2)), schema, error));
}
template<> template<> void MCPObject::test<4>()
{
    std::string error;
    const LLSD schema = LLSDMap("type", "array")("maxItems", 2)("items", LLSDMap("type", "string"));
    ensure("valid array", LLMCPValidation::validate(llsd::array("a", "b"), schema, error));
    ensure("oversized array", !LLMCPValidation::validate(llsd::array("a", "b", "c"), schema, error));
    ensure("wrong element type", !LLMCPValidation::validate(llsd::array("a", 5), schema, error));
    ensure("error locates element", error.find("[1]") != std::string::npos);
    const LLSD enumeration = LLSDMap("type", "string")("enum", llsd::array("start", "stop"));
    ensure("valid enum", LLMCPValidation::validate(LLSD("stop"), enumeration, error));
    ensure("unknown enum", !LLMCPValidation::validate(LLSD("erase"), enumeration, error));
    const LLSD uuid = LLSDMap("type", "string")("format", "uuid");
    ensure("valid UUID", LLMCPValidation::validate(LLSD("12345678-1234-1234-1234-123456789abc"), uuid, error));
    ensure("invalid UUID", !LLMCPValidation::validate(LLSD("not-a-uuid"), uuid, error));
}
}
