# SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>

subdir('ui')

gresource_file = configure_file(
  input: 'ammonite.gresource.xml.in',
  output: 'ammonite.gresource.xml',
  configuration: file_conf,
)

resources = gnome.compile_resources(
  'ammonite-resources',
  gresource_file,
  dependencies: [appdata, blueprints, gresource_file],
  source_dir: 'resources',
  c_name: 'ammonite',
  extra_args: '--manual-register',
)

resources_header = []

foreach res : resources
  filename = res.full_path().split('/')[-1]
  if filename == 'ammonite-resources.h'
    resources_header += res
  endif
endforeach
