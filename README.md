# Ammonite

Ammonite is a fork of GNOME Files (Nautilus), a file manager for GNOME. Its main difference is a folder tree in the sidebar (Preferences → General → "Folder Tree in Sidebar", on by default and can be switched off), which replaces the list of places.

Everything has been renamed from upstream, including the C identifiers (`ammonite_*`), the `org.ammonite.Ammonite` app ID and the GSettings schemas. The extension API is now `libammonite-extension`, so extensions written for Nautilus (such as `nautilus-python` ones) are not compatible. Merging upstream changes will need the rename applied to them.

## Versioning
Ammonite uses `MAJOR.MINOR.MICRO` versions, with GNOME-style pre-releases. Each cycle on `main` runs `X.Y.alpha`, `X.Y.beta`, `X.Y.rc` and then `X.Y.0`. Later fixes to a release are `X.Y.1`, `X.Y.2` and so on. The major version changes only for breaking changes, such as settings that are no longer compatible or a change to the `libammonite-extension` API. Releases are tagged `vX.Y.Z`; the first release is `v1.0.0`. It is based on Nautilus 51.0.1 (upstream version 52.alpha).

## Building the Flatpak
```
flatpak run org.flatpak.Builder --user --install --force-clean build-dir build-aux/flatpak/org.ammonite.Ammonite.json
flatpak run org.ammonite.Ammonite.Devel
```
The manifest builds from this checkout and needs the GNOME Nightly runtime (`org.gnome.Platform//master`).

## Runtime dependencies
- [Bubblewrap](https://github.com/containers/bubblewrap) installed. Used for security reasons.
- [LocalSearch](https://gitlab.gnome.org/GNOME/localsearch) properly set up and with all features enabled. Used for fast search and metadata extraction, starred files and batch renaming.
- [xdg-user-dirs-gtk](https://gitlab.gnome.org/GNOME/xdg-user-dirs-gtk) installed.  Used to create the default bookmarks and update localization.
