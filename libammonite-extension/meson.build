# SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>

libammonite_extension_headers = [
  'ammonite-column-provider.h',
  'ammonite-column.h',
  'ammonite-file-info.h',
  'ammonite-info-provider.h',
  'ammonite-menu-provider.h',
  'ammonite-properties-model-provider.h',
  'ammonite-properties-model.h',
  'ammonite-properties-item.h',
  'ammonite-menu.h',
]

install_headers('ammonite-extension.h', subdir: 'ammonite')
install_headers(
  libammonite_extension_headers,
  subdir: join_paths('ammonite', 'libammonite-extension'),
)

libammonite_extension_enums = gnome.mkenums_simple(
  'ammonite-extension-enum-types',
  install_header: true,
  install_dir: join_paths(includedir, 'ammonite', 'libammonite-extension'),
  sources: ['ammonite-info-provider.h'],
)

libammonite_extension_sources = [
  libammonite_extension_enums,
  libammonite_extension_headers,
  'ammonite-column-provider.c',
  'ammonite-column.c',
  'ammonite-extension.h',
  'ammonite-extension-private.h',
  'ammonite-file-info.c',
  'ammonite-info-provider.c',
  'ammonite-menu-item.c',
  'ammonite-menu-provider.c',
  'ammonite-properties-model-provider.c',
  'ammonite-properties-model.c',
  'ammonite-properties-item.c',
  'ammonite-menu.c',
]

libammonite_extension_deps = [config_h, glib, gio]

libammonite_extension = shared_library(
  'ammonite-extension',
  [
    libammonite_extension_sources,
  ],
  c_args: ['-DAMMONITE_COMPILATION'],
  dependencies: libammonite_extension_deps,
  include_directories: ammonite_include_dirs,
  version: ammonite_extension_version,
  install: true,
)

if get_option('introspection')
  ammonite_extension_gir = gnome.generate_gir(
    libammonite_extension,
    export_packages: ['libammonite-extension'],
    extra_args: ['-DAMMONITE_COMPILATION'],
    sources: libammonite_extension_sources,
    nsversion: ammonite_extension_version,
    namespace: 'Ammonite',
    includes: ['Gio-2.0', 'GLib-2.0'],
    header: 'ammonite-extension.h',
    install: true,
  )
endif

ammonite_extension = declare_dependency(
  sources: libammonite_extension_enums[1],
  link_with: libammonite_extension,
  dependencies: libammonite_extension_deps,
  include_directories: ammonite_include_dirs,
)

pkgconfig.generate(
  description: 'A library to create Ammonite view extensions',
  filebase: 'libammonite-extension-' + ammonite_extension_major_version,
  libraries: [libammonite_extension],
  name: 'libammonite-extension',
  subdirs: ['ammonite'],
  requires: [
    'gio-2.0',
    'glib-2.0',
  ],
  variables: [
    'exec_prefix=${prefix}',
    'extensiondir=${libdir}/ammonite/extensions-' + ammonite_extension_major_version,
    'extensions_api_version=' + ammonite_extension_major_version,
  ],
  version: meson.project_version(),
)
