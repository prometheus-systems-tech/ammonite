/*
 * ammonite-progress-info.h: file operation progress info.
 *
 * SPDX-FileCopyrightText: 2007 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

#define AMMONITE_TYPE_PROGRESS_INFO (ammonite_progress_info_get_type ())

G_DECLARE_FINAL_TYPE (AmmoniteProgressInfo, ammonite_progress_info, AMMONITE, PROGRESS_INFO, GObject)

/* Signals:
   "changed" - status or details changed
   "progress-changed" - the percentage progress changed (or we pulsed if in activity_mode
   "started" - emited on job start
   "finished" - emitted when job is done
   
   All signals are emitted from idles in main loop.
   All methods are threadsafe.
 */

AmmoniteProgressInfo *ammonite_progress_info_new (void);

char *        ammonite_progress_info_get_short_status (AmmoniteProgressInfo *info);
char *        ammonite_progress_info_get_status      (AmmoniteProgressInfo *info);
char *        ammonite_progress_info_get_details     (AmmoniteProgressInfo *info);
double        ammonite_progress_info_get_progress    (AmmoniteProgressInfo *info);
GCancellable *ammonite_progress_info_get_cancellable (AmmoniteProgressInfo *info);
void          ammonite_progress_info_cancel          (AmmoniteProgressInfo *info);
gboolean      ammonite_progress_info_get_is_finished (AmmoniteProgressInfo *info);
gboolean      ammonite_progress_info_get_is_paused   (AmmoniteProgressInfo *info);
gboolean      ammonite_progress_info_get_is_cancelled (AmmoniteProgressInfo *info);

void          ammonite_progress_info_start           (AmmoniteProgressInfo *info);
void          ammonite_progress_info_finish          (AmmoniteProgressInfo *info);
void          ammonite_progress_info_pause           (AmmoniteProgressInfo *info);
void          ammonite_progress_info_resume          (AmmoniteProgressInfo *info);
void          ammonite_progress_info_set_status      (AmmoniteProgressInfo *info,
						      const char           *status,
                                                      const char           *short_status);
void          ammonite_progress_info_take_status     (AmmoniteProgressInfo *info,
                                                      char                 *status,
						      char                 *short_status);
void          ammonite_progress_info_set_details     (AmmoniteProgressInfo *info,
						      const char           *details);
void          ammonite_progress_info_take_details    (AmmoniteProgressInfo *info,
						      char                 *details);
void          ammonite_progress_info_set_progress    (AmmoniteProgressInfo *info,
						      double                current,
						      double                total);
void          ammonite_progress_info_pulse_progress  (AmmoniteProgressInfo *info);

void          ammonite_progress_info_set_remaining_time (AmmoniteProgressInfo *info,
                                                         gdouble               time);
gdouble       ammonite_progress_info_get_remaining_time (AmmoniteProgressInfo *info);
void          ammonite_progress_info_set_elapsed_time (AmmoniteProgressInfo *info,
                                                       gdouble               time);
gdouble       ammonite_progress_info_get_total_elapsed_time (AmmoniteProgressInfo *info);

void ammonite_progress_info_set_destination (AmmoniteProgressInfo *info,
                                             GFile                *file);
GFile *ammonite_progress_info_get_destination (AmmoniteProgressInfo *info);
