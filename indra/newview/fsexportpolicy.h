#ifndef FS_EXPORT_POLICY_H
#define FS_EXPORT_POLICY_H
namespace FSAssetExportPolicy
{
enum class Grid { second_life, opensim, unknown };
enum class Policy { allowed, undefined, denied };
// Matches the established grid-specific asset policy; missing rights never grant access.
constexpr bool allowed(Grid grid, Policy policy, bool creator, bool export_flag, bool full_permissions)
{
    if (grid == Grid::second_life) return creator;
    if (grid != Grid::opensim) return false;
    switch (policy)
    {
    case Policy::allowed: return export_flag;
    case Policy::undefined: return full_permissions;
    case Policy::denied: return creator;
    }
    return false;
}
}
#endif
