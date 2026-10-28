#ifndef LL_STREAM3D_NUMERIC_H
#define LL_STREAM3D_NUMERIC_H
#include <charconv>
#include <cmath>
#include <string_view>
namespace LLStream3DNumeric
{
inline bool parseFloat(std::string_view text, float& value)
{
    if (text.empty()) return false;
    // Keep the previously accepted explicit positive sign.
    if (text.front() == '+') text.remove_prefix(1);
    float parsed = 0.f;
    auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc() || result.ptr != text.data() + text.size() || !std::isfinite(parsed)) return false;
    value = parsed;
    return true;
}
}
#endif
