
blueprint_names = [
  'ammonite-app-chooser',
  'ammonite-app-chooser-widget',
  'ammonite-batch-rename-dialog',
  'ammonite-column-chooser',
  'ammonite-compress-dialog',
  'ammonite-date-entry-row',
  'ammonite-date-range-dialog',
  'ammonite-file-chooser',
  'ammonite-file-conflict-dialog',
  'ammonite-file-properties-change-permissions',
  'ammonite-files-view-select-items',
  'ammonite-files-view',
  'ammonite-grid-cell',
  'ammonite-grid-view-captions-dialog',
  'ammonite-history-controls',
  'ammonite-name-cell',
  'ammonite-network-address-bar',
  'ammonite-network-cell',
  'ammonite-new-folder-dialog',
  'ammonite-preferences-dialog',
  'ammonite-operations-ui-manager-request-passphrase',
  'ammonite-progress-indicator',
  'ammonite-progress-info-widget',
  'ammonite-properties-widget',
  'ammonite-properties-window',
  'ammonite-query-editor',
  'ammonite-rename-file-popover',
  'ammonite-search-popover',
  'ammonite-search-types-dialog',
  'ammonite-sidebar-row',
  'ammonite-toolbar',
  'ammonite-view-controls',
  'ammonite-window',
  'shortcuts-dialog',
]

blueprint_files = []
ui_files = []

foreach name : blueprint_names
  blueprint_files += [name + '.blp']
  ui_files += [name + '.ui']
endforeach

blueprints = custom_target(
  'blueprints',
  input: files(blueprint_files),
  # This is the reason this file exists separately. output doesn't accept path separators
  output: ui_files,
  command: [
    blueprint_cmd,
    'batch-compile',
    '--minify',
    meson.current_build_dir(),
    '@CURRENT_SOURCE_DIR@',
    '@INPUT@',
  ],
)
