
/*
 * SPDX-FileCopyrightText: 1999, 2000 Free Software Foundaton
 * SPDX-FileCopyrightText: 2000 Eazel, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Rebecca Schulman <rebecka@eazel.com>
 */

#pragma once

#include <gtk/gtk.h>

typedef struct _AmmoniteClipboard AmmoniteClipboard;
#define AMMONITE_TYPE_CLIPBOARD (ammonite_clipboard_get_type())
GType              ammonite_clipboard_get_type     (void);

void ammonite_clipboard_clear_if_colliding_uris    (GtkWidget          *widget,
                                                    const GList        *item_uris);
GList             *ammonite_clipboard_peek_files   (AmmoniteClipboard *clip);
GList             *ammonite_clipboard_get_uri_list (AmmoniteClipboard *clip);
gboolean           ammonite_clipboard_is_cut       (AmmoniteClipboard *clip);

AmmoniteClipboard *ammonite_clipboard_copy         (AmmoniteClipboard *clip);
void               ammonite_clipboard_free         (AmmoniteClipboard *clip);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (AmmoniteClipboard, ammonite_clipboard_free)

void ammonite_clipboard_prepare_for_files (GdkClipboard *clipboard,
                                           GList        *files,
                                           gboolean      cut);

void               ammonite_clipboard_register     (void);
