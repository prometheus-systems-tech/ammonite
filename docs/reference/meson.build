# SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>

toml_data = configuration_data()
toml_data.set('VERSION', meson.project_version())

ammonite_toml = configure_file(
  input: 'ammonite.toml.in',
  output: 'ammonite.toml',
  configuration: toml_data,
)

custom_target(
  'ammonite-doc',
  input: [
    ammonite_toml,
    ammonite_extension_gir[0],
  ],
  output: 'ammonite',
  command: [
    gi_docgen,
    'generate',
    '--quiet',
    '--add-include-path=@0@'.format(
      meson.project_source_root() / 'libammonite-extension',
    ),
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--no-namespace-dir',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT1@',
  ],
  build_by_default: true,
  install: true,
  install_dir: docdir,
)
