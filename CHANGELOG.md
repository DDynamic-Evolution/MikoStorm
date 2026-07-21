# Changelog

## Unreleased

### Bug Fixes

- **Restart Avoider:** Added maximum retry limit (10 attempts) for returning to the original region. Previously the viewer would retry indefinitely every 60 seconds if it could not return, wasting resources. A notification is now shown when the limit is reached. ([fsrestartavoid.h](indra/newview/fsrestartavoid.h), [fsrestartavoid.cpp](indra/newview/fsrestartavoid.cpp))

- **Per-Contact IM Sounds:** Fixed custom IM sounds not playing and not being removable. The `FSPerAccountIMSounds` setting was never declared in `settings_per_account.xml`, causing all `setLLSD`/`getLLSD` calls to silently fail. ([settings_per_account.xml](indra/newview/app_settings/settings_per_account.xml))
