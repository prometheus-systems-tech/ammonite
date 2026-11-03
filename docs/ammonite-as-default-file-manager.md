# Using Ammonite as the default file manager (flatpak)

The flatpak (`build-aux/flatpak/org.ammonite.Ammonite.json`) is allowed to
own `org.freedesktop.FileManager1` and to talk to `org.freedesktop.Flatpak`
so that it can replace Nautilus as the desktop's file manager.

Two user-level changes make the replacement take effect. Neither is part of
the build; they are made on the machine.

## Enabling

1. Start Ammonite on demand for "Show in Folder" requests by creating
   `~/.local/share/dbus-1/services/org.freedesktop.FileManager1.service`:

   ```ini
   [D-BUS Service]
   Name=org.freedesktop.FileManager1
   Exec=/usr/bin/flatpak run org.ammonite.Ammonite.Devel --gapplication-service
   ```

2. Make Ammonite the default folder handler:

   ```sh
   xdg-mime default org.ammonite.Ammonite.Devel.desktop inode/directory
   ```

3. Quit Nautilus so it releases the service name: `nautilus -q`.

## Undoing

1. Remove the D-Bus service file:

   ```sh
   rm ~/.local/share/dbus-1/services/org.freedesktop.FileManager1.service
   ```

2. Restore Nautilus as the default folder handler:

   ```sh
   xdg-mime default org.gnome.Nautilus.desktop inode/directory
   ```

3. Stop Ammonite so it releases the service name (it will otherwise keep
   `org.freedesktop.FileManager1` until it exits):

   ```sh
   flatpak kill org.ammonite.Ammonite.Devel
   ```

   Nautilus will then take the name again the next time something asks for it.

Optionally, to also drop the extra sandbox permissions, remove
`--own-name=org.freedesktop.FileManager1` and `--talk-name=org.freedesktop.Flatpak`
from the flatpak manifest and rebuild.

## Not covered

The portal file chooser backend and the GNOME Shell search provider cannot be
replaced from inside a flatpak sandbox.
