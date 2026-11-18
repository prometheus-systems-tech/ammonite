# SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>

libm = cc.find_library('m')

shared_module(
  'ammonite-audio-video-properties',
  sources: [
    'audio-video-properties-module.c',
    'audio-video-properties-model.c',
    'audio-video-properties-model.h',
  ],
  dependencies: [libm, gst_tag_dep, gst_pbutils_dep, ammonite_extension],
  c_args: ['-DG_LOG_DOMAIN="av-properties"'],
  install: true,
  install_dir: extensiondir,
)
