# SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>

test_utils = files('test-utilities.c')

libtestutils = static_library(
  'testutils',
  test_utils,
  dependencies: libammonite_dep,
  include_directories: include_directories('.')
)

libtestutils_dep = declare_dependency(
  link_with: libtestutils,
  include_directories: include_directories('.'),
  dependencies: libammonite_dep,
  sources: resources_header
)
