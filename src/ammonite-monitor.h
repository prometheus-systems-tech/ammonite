/*
 * ammonite-monitor.h: file and directory change monitoring for ammonite
 *
 * SPDX-FileCopyrightText: 2000, 2001 Eazel, Inc.
 * SPDX-FileCopyrightText: 2016 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Seth Nickell <seth@eazel.com>
 *          Darin Adler <darin@bentspoon.com>
 *          Carlos Soriano <csoriano@gnome.org>
 */

#pragma once

#include <glib.h>
#include <gio/gio.h>

typedef struct AmmoniteMonitor AmmoniteMonitor;

AmmoniteMonitor *ammonite_monitor_directory (GFile *location);
void             ammonite_monitor_cancel    (AmmoniteMonitor *monitor);
