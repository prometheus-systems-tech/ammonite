/*
 * SPDX-FileCopyrightText: 2024 Peter Eisenmann <p3732@getgoogleoff.me>
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ammonite-scheme.h"

#include <glib.h>

gboolean
ammonite_scheme_is_internal (const char *scheme)
{
    return g_str_equal (scheme, SCHEME_BURN) ||
           g_str_equal (scheme, SCHEME_NETWORK) ||
           g_str_equal (scheme, SCHEME_NETWORK_VIEW) ||
           g_str_equal (scheme, SCHEME_RECENT) ||
           g_str_equal (scheme, SCHEME_SEARCH) ||
           g_str_equal (scheme, SCHEME_STARRED) ||
           g_str_equal (scheme, SCHEME_TRASH);
}
