# Contributing to Ammonite

Thank you for your interest in contributing to our project!

To build the development version of Ammonite and hack on the code,
see the Flatpak instructions in the README.

Commit messages should follow the expected format [detailed here](https://handbook.gnome.org/development/commit-messages.html).

Your commit message should look something like this:

```
tag: Short explanation of the commit

Longer explanation explaining exactly what's changed and why, whether
any external or private interfaces changed, what issue were fixed (with
issue number if applicable) and so forth. Be concise but not too brief.

Closes #1234
```

## File Chooser

Developing for the ammonite file chooser may be complicated because of the
portal implementation.  In order to call the development version of ammonite
(rather than the system installed version) use the below commands, while making
sure that you have the development version already running.  Note that the
below commands are somewhat redundant and include default keys in order to make
updating them easier. For example, `OpenFile` defaults to `directory = false`,
but you can easily open a directory file chooser by changing `false` to `true`.

##### Open a file or directory

```bash
gdbus call --session --dest org.ammonite.Ammonite.Devel --object-path /org/freedesktop/portal/desktop --method org.freedesktop.impl.portal.FileChooser.OpenFile '/org/ammonite/Ammonite/Devel' 'org.ammonite.Ammonite.Devel' '' 'Open a File' "{'filters': <[('All Files', [(uint32 0, '*')]), ('Images', [(uint32 0, '*.ico'), (uint32 1, 'image/png')]), ('Text', [(uint32 0, '*.txt')])]>, 'current_filter': <('All Files', [(uint32 0, '*')])>, 'choices': <[('encoding', 'Encoding', [('utf8', 'Unicode (UTF-8)'), ('latin15', 'Western')], 'latin15'), ('reencode', 'Reencode', [], 'false')]>, 'directory': <false>, 'multiple': <false>, 'modal': <true>, 'accept_label': <'Open'>, 'current_folder': <'/tmp'>}"
```

##### Save a file

```bash
gdbus call --session --dest org.ammonite.Ammonite.Devel --object-path /org/freedesktop/portal/desktop --method org.freedesktop.impl.portal.FileChooser.SaveFile '/org/ammonite/Ammonite/Devel' 'org.ammonite.Ammonite.Devel' '' 'Save a File' "{'filters': <[('All Files', [(uint32 0, '*')]),('Images', [(uint32 0, '*.ico'), (uint32 1, 'image/png')]), ('Text', [(uint32 0, '*.txt')])]>, 'current_filter': <('All Files', [(uint32 0, '*')])>, 'choices': <[('encoding', 'Encoding', [('utf8', 'Unicode (UTF-8)'), ('latin15', 'Western')], 'latin15'), ('reencode', 'Reencode', [], 'false')]>,  'modal': <true>, 'accept_label': <'Save'>, 'current_name': <'test_file'>}"
```

##### Save multiple files

```bash
gdbus call --session --dest org.ammonite.Ammonite.Devel --object-path /org/freedesktop/portal/desktop --method org.freedesktop.impl.portal.FileChooser.SaveFiles '/org/ammonite/Ammonite/Devel' 'org.ammonite.Ammonite.Devel' '' 'Save Files' "{'choices': <[('encoding', 'Encoding', [('utf8', 'Unicode (UTF-8)'), ('latin15', 'Western')], 'latin15'), ('reencode', 'Reencode', [], 'false')]>,  'modal': <true>, 'accept_label': <'Save'>, 'current_folder': <'/tmp'>}"
```
