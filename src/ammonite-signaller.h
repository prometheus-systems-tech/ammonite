
/*
 * SPDX-FileCopyrightText: 1999, 2000 Eazel, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: John Sullivan <sullivan@eazel.com>
 */

/* ammonite-signaller.h: Class to manage ammonite-wide signals that don't
 * correspond to any particular object.
 */

#pragma once

#include <glib-object.h>

/* AmmoniteSignaller is a class that manages signals between
   disconnected Ammonite code. Ammonite objects connect to these signals
   so that other objects can cause them to be emitted later, without
   the connecting and emit-causing objects needing to know about each
   other. It seems a shame to have to invent a subclass and a special
   object just for this purpose. Perhaps there's a better way to do 
   this kind of thing.
*/

/* Get the one and only AmmoniteSignaller to connect with or emit signals for */
GObject *ammonite_signaller_get_current (void);
