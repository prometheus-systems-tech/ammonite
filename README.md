# Ammonite

Ammonite is a fork of GNOME Files (Nautilus), a file manager for GNOME. Its main difference is an optional folder tree in the sidebar (Preferences → General → "Folder Tree in Sidebar", off by default), which replaces the list of places.

Everything has been renamed from upstream, including the C identifiers (`ammonite_*`), the `org.ammonite.Ammonite` app ID and the GSettings schemas. The extension API is now `libammonite-extension`, so extensions written for Nautilus (such as `nautilus-python` ones) are not compatible. Merging upstream changes will need the rename applied to them.

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
