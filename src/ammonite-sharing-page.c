/*
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* Network sharing (SMB/CIFS and NFS) for a local folder.
 *
 * SMB:  Samba "usershares" (`net usershare`). Tried unprivileged first and
 *       retried through sudo when that is refused.
 * NFS:  a snippet in /etc/exports.d/ammonite.exports, written through sudo,
 *       followed by `exportfs -ra`.
 *
 * Inside the flatpak sandbox every command runs on the host through
 * `flatpak-spawn --host`. The sudo password is only ever sent on stdin.
 */

#include "ammonite-sharing-page.h"

#include <glib/gi18n.h>
#include <string.h>

#define NFS_EXPORTS_FILE "/etc/exports.d/ammonite.exports"

typedef enum
{
    APPLY_OK,
    APPLY_ERROR,
    APPLY_NEED_PASSWORD,
    APPLY_BAD_PASSWORD
} ApplyResult;

typedef struct
{
    /* inputs / outputs */
    char *path;
    char *password;
    gboolean smb_available;
    gboolean nfs_available;

    gboolean smb_enabled;
    char *smb_name;
    char *smb_comment;
    gboolean smb_write;
    gboolean smb_guest;
    char *smb_old_name;     /* name of an existing share for this path */

    gboolean nfs_enabled;
    char *nfs_clients;
    gboolean nfs_write;

    ApplyResult result;
    char *error;
} SharingState;

static void
sharing_state_free (SharingState *s)
{
    g_free (s->path);
    if (s->password != NULL)
    {
        memset (s->password, 0, strlen (s->password));
    }
    g_free (s->password);
    g_free (s->smb_name);
    g_free (s->smb_comment);
    g_free (s->smb_old_name);
    g_free (s->nfs_clients);
    g_free (s->error);
    g_free (s);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (SharingState, sharing_state_free)

/* ---- running commands on the host ------------------------------------ */

static gboolean
host_run (const char * const *argv,
          const char         *stdin_text,
          char              **out,
          char              **err,
          int                *status)
{
    g_autoptr (GPtrArray) full = g_ptr_array_new ();
    g_autoptr (GSubprocessLauncher) launcher = NULL;
    g_autoptr (GSubprocess) proc = NULL;
    g_autoptr (GError) error = NULL;
    g_autofree char *o = NULL;
    g_autofree char *e = NULL;

    if (g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS))
    {
        g_ptr_array_add (full, (gpointer) "flatpak-spawn");
        g_ptr_array_add (full, (gpointer) "--host");
    }

    for (int i = 0; argv[i] != NULL; i++)
    {
        g_ptr_array_add (full, (gpointer) argv[i]);
    }
    g_ptr_array_add (full, NULL);

    launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                          G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                          G_SUBPROCESS_FLAGS_STDERR_PIPE);
    g_subprocess_launcher_setenv (launcher, "LC_ALL", "C", TRUE);
    proc = g_subprocess_launcher_spawnv (launcher, (const char * const *) full->pdata, &error);

    if (proc == NULL)
    {
        if (err != NULL)
        {
            *err = g_strdup (error->message);
        }
        if (status != NULL)
        {
            *status = 127;
        }
        return FALSE;
    }

    if (!g_subprocess_communicate_utf8 (proc, stdin_text, NULL, &o, &e, &error))
    {
        if (err != NULL)
        {
            *err = g_strdup (error->message);
        }
        if (status != NULL)
        {
            *status = -1;
        }
        return FALSE;
    }

    int st = g_subprocess_get_if_exited (proc) ? g_subprocess_get_exit_status (proc) : -1;

    if (out != NULL)
    {
        *out = g_steal_pointer (&o);
    }
    if (err != NULL)
    {
        *err = g_steal_pointer (&e);
    }
    if (status != NULL)
    {
        *status = st;
    }

    return st == 0;
}

static gboolean
host_has_command (const char *name)
{
    const char *argv[] = { "sh", "-c", "command -v \"$1\" >/dev/null 2>&1", "sh", name, NULL };

    return host_run (argv, NULL, NULL, NULL, NULL);
}

/* Run a command as root. With a NULL password this only works if sudo needs
 * none; otherwise the password is fed to `sudo -S` on stdin. */
static ApplyResult
sudo_run (const char * const *argv,
          const char         *password,
          char              **out,
          char              **err)
{
    g_autoptr (GPtrArray) full = g_ptr_array_new ();
    g_autofree char *stdin_text = NULL;
    int status = 0;

    g_ptr_array_add (full, (gpointer) "sudo");
    if (password != NULL)
    {
        g_ptr_array_add (full, (gpointer) "-S");
        g_ptr_array_add (full, (gpointer) "-p");
        g_ptr_array_add (full, (gpointer) "");
        stdin_text = g_strdup_printf ("%s\n", password);
    }
    else
    {
        g_ptr_array_add (full, (gpointer) "-n");
    }
    g_ptr_array_add (full, (gpointer) "--");
    for (int i = 0; argv[i] != NULL; i++)
    {
        g_ptr_array_add (full, (gpointer) argv[i]);
    }
    g_ptr_array_add (full, NULL);

    if (host_run ((const char * const *) full->pdata, stdin_text, out, err, &status))
    {
        return APPLY_OK;
    }

    if (err != NULL && *err != NULL)
    {
        if (strstr (*err, "a password is required") != NULL)
        {
            return APPLY_NEED_PASSWORD;
        }
        if (strstr (*err, "try again") != NULL ||
            strstr (*err, "incorrect password") != NULL)
        {
            return APPLY_BAD_PASSWORD;
        }
    }

    return APPLY_ERROR;
}

/* ---- SMB (Samba usershares) ------------------------------------------ */

static void
smb_query (SharingState *s)
{
    g_autofree char *out = NULL;
    const char *argv[] = { "net", "usershare", "info", NULL };
    g_autoptr (GKeyFile) kf = g_key_file_new ();
    g_auto (GStrv) groups = NULL;

    s->smb_available = host_has_command ("net");
    if (!s->smb_available)
    {
        return;
    }

    if (!host_run (argv, NULL, &out, NULL, NULL))
    {
        /* Not allowed to read usershares as this user; use passwordless
         * sudo if available so existing shares are still shown. */
        g_clear_pointer (&out, g_free);
        if (sudo_run (argv, NULL, &out, NULL) != APPLY_OK)
        {
            return;
        }
    }

    if (out == NULL ||
        !g_key_file_load_from_data (kf, out, -1, G_KEY_FILE_NONE, NULL))
    {
        return;
    }

    groups = g_key_file_get_groups (kf, NULL);
    for (int i = 0; groups[i] != NULL; i++)
    {
        g_autofree char *path = g_key_file_get_string (kf, groups[i], "path", NULL);

        if (g_strcmp0 (path, s->path) == 0)
        {
            g_autofree char *acl = g_key_file_get_string (kf, groups[i], "usershare_acl", NULL);
            g_autofree char *guest = g_key_file_get_string (kf, groups[i], "guest_ok", NULL);

            s->smb_enabled = TRUE;
            s->smb_old_name = g_strdup (groups[i]);
            s->smb_name = g_strdup (groups[i]);
            s->smb_comment = g_key_file_get_string (kf, groups[i], "comment", NULL);
            s->smb_write = acl != NULL && strstr (acl, ":F") != NULL;
            s->smb_guest = g_strcmp0 (guest, "y") == 0;
            return;
        }
    }
}

static gboolean
smb_name_is_valid (const char *name)
{
    if (name == NULL || *name == '\0')
    {
        return FALSE;
    }

    return strpbrk (name, "%<>*?|/\\+=;:\",") == NULL;
}

/* Runs `net usershare <args>` unprivileged first, then through sudo. */
static ApplyResult
smb_net (SharingState *s,
         const char * const *args)
{
    g_autoptr (GPtrArray) argv = g_ptr_array_new ();
    g_autofree char *err = NULL;
    g_autofree char *err2 = NULL;
    ApplyResult r;

    g_ptr_array_add (argv, (gpointer) "net");
    g_ptr_array_add (argv, (gpointer) "usershare");
    for (int i = 0; args[i] != NULL; i++)
    {
        g_ptr_array_add (argv, (gpointer) args[i]);
    }
    g_ptr_array_add (argv, NULL);

    if (s->password == NULL &&
        host_run ((const char * const *) argv->pdata, NULL, NULL, &err, NULL))
    {
        return APPLY_OK;
    }

    r = sudo_run ((const char * const *) argv->pdata, s->password, NULL, &err2);
    if (r == APPLY_ERROR)
    {
        g_free (s->error);
        s->error = g_strdup (err2 != NULL && *err2 != '\0' ? err2 : err);
    }

    return r;
}

static ApplyResult
smb_apply (SharingState *s)
{
    ApplyResult r;

    if (!s->smb_available)
    {
        return APPLY_OK;
    }

    if (!s->smb_enabled)
    {
        if (s->smb_old_name != NULL)
        {
            const char *args[] = { "delete", s->smb_old_name, NULL };

            return smb_net (s, args);
        }
        return APPLY_OK;
    }

    if (!smb_name_is_valid (s->smb_name))
    {
        g_free (s->error);
        s->error = g_strdup (_("The SMB share name is empty or contains invalid characters."));
        return APPLY_ERROR;
    }

    if (s->smb_old_name != NULL && g_strcmp0 (s->smb_old_name, s->smb_name) != 0)
    {
        const char *args[] = { "delete", s->smb_old_name, NULL };

        r = smb_net (s, args);
        if (r != APPLY_OK)
        {
            return r;
        }
    }

    {
        const char *args[] = {
            "add", s->smb_name, s->path,
            s->smb_comment != NULL ? s->smb_comment : "",
            s->smb_write ? "S-1-1-0:F" : "S-1-1-0:R",
            s->smb_guest ? "guest_ok=y" : "guest_ok=n",
            NULL
        };

        return smb_net (s, args);
    }
}

/* ---- NFS (/etc/exports.d) -------------------------------------------- */

static char *
exports_escape (const char *path)
{
    GString *str = g_string_new (NULL);

    for (const char *p = path; *p != '\0'; p++)
    {
        switch (*p)
        {
            case ' ':  g_string_append (str, "\\040"); break;
            case '\t': g_string_append (str, "\\011"); break;
            case '\n': g_string_append (str, "\\012"); break;
            case '\\': g_string_append (str, "\\134"); break;
            default:   g_string_append_c (str, *p);   break;
        }
    }

    return g_string_free (str, FALSE);
}

static char *
exports_unescape (const char *text,
                  gsize       len)
{
    GString *str = g_string_new (NULL);

    for (gsize i = 0; i < len; i++)
    {
        if (text[i] == '\\' && i + 3 < len && g_ascii_isdigit (text[i + 1]) &&
            g_ascii_isdigit (text[i + 2]) && g_ascii_isdigit (text[i + 3]))
        {
            g_string_append_c (str, (char) ((text[i + 1] - '0') * 64 +
                                            (text[i + 2] - '0') * 8 +
                                            (text[i + 3] - '0')));
            i += 3;
        }
        else
        {
            g_string_append_c (str, text[i]);
        }
    }

    return g_string_free (str, FALSE);
}

/* Splits "path clients(opts)" into the unescaped path and the remainder.
 * Returns FALSE for comments, blanks and anything unparsable. */
static gboolean
exports_parse_line (const char  *line,
                    char       **path,
                    const char **rest)
{
    const char *p = line;
    const char *end;

    while (g_ascii_isspace (*p))
    {
        p++;
    }
    if (*p == '\0' || *p == '#')
    {
        return FALSE;
    }

    end = p;
    while (*end != '\0' && !g_ascii_isspace (*end))
    {
        end++;
    }

    *path = exports_unescape (p, end - p);
    while (g_ascii_isspace (*end))
    {
        end++;
    }
    *rest = end;

    return TRUE;
}

static char *
nfs_read_file (void)
{
    const char *argv[] = { "cat", NFS_EXPORTS_FILE, NULL };
    char *out = NULL;

    if (!host_run (argv, NULL, &out, NULL, NULL))
    {
        g_free (out);
        return g_strdup ("");
    }

    return out != NULL ? out : g_strdup ("");
}

static void
nfs_query (SharingState *s)
{
    g_autofree char *content = NULL;
    g_auto (GStrv) lines = NULL;

    s->nfs_available = host_has_command ("exportfs");
    if (!s->nfs_available)
    {
        return;
    }

    content = nfs_read_file ();
    lines = g_strsplit (content, "\n", -1);

    for (int i = 0; lines[i] != NULL; i++)
    {
        g_autofree char *path = NULL;
        const char *rest = NULL;

        if (exports_parse_line (lines[i], &path, &rest) && g_strcmp0 (path, s->path) == 0)
        {
            const char *paren = strchr (rest, '(');

            s->nfs_enabled = TRUE;
            s->nfs_clients = paren != NULL ? g_strndup (rest, paren - rest) : g_strdup (rest);
            s->nfs_write = paren != NULL && strstr (paren, "rw") != NULL;
            return;
        }
    }
}

static ApplyResult
nfs_apply (SharingState *s)
{
    g_autofree char *content = NULL;
    g_autoptr (GString) updated = g_string_new (NULL);
    g_auto (GStrv) lines = NULL;
    g_autofree char *escaped = NULL;
    g_autofree char *encoded = NULL;
    g_autofree char *script = NULL;
    g_autofree char *err = NULL;
    const char *clients = s->nfs_clients != NULL ? s->nfs_clients : "";
    gboolean had_line = FALSE;
    ApplyResult r;

    if (!s->nfs_available)
    {
        return APPLY_OK;
    }

    if (s->nfs_enabled)
    {
        if (*clients == '\0' || strpbrk (clients, " \t\r\n()") != NULL)
        {
            g_free (s->error);
            s->error = g_strdup (_("NFS clients must be a host, network or * with no spaces or parentheses."));
            return APPLY_ERROR;
        }
    }

    content = nfs_read_file ();
    lines = g_strsplit (content, "\n", -1);

    for (int i = 0; lines[i] != NULL; i++)
    {
        g_autofree char *path = NULL;
        const char *rest = NULL;

        if (exports_parse_line (lines[i], &path, &rest) && g_strcmp0 (path, s->path) == 0)
        {
            had_line = TRUE;
            continue;
        }

        if (lines[i + 1] == NULL && *lines[i] == '\0')
        {
            continue;
        }
        g_string_append_printf (updated, "%s\n", lines[i]);
    }

    if (!s->nfs_enabled && !had_line)
    {
        return APPLY_OK;
    }

    if (s->nfs_enabled)
    {
        escaped = exports_escape (s->path);
        g_string_append_printf (updated, "%s %s(%s,sync,no_subtree_check)\n",
                                escaped, clients, s->nfs_write ? "rw" : "ro");
    }

    if (g_strcmp0 (updated->str, content) == 0)
    {
        return APPLY_OK;
    }

    /* The content travels as an argument (base64), leaving stdin for sudo's
     * password. */
    encoded = g_base64_encode ((const guchar *) updated->str, updated->len);
    script = g_strdup_printf ("umask 022; mkdir -p /etc/exports.d && "
                              "printf %%s '%s' | base64 -d > %s && exportfs -ra",
                              encoded, NFS_EXPORTS_FILE);
    {
        const char *argv[] = { "sh", "-c", script, NULL };

        r = sudo_run (argv, s->password, NULL, &err);
    }

    if (r == APPLY_ERROR)
    {
        g_free (s->error);
        s->error = g_strdup (err != NULL && *err != '\0' ? err : _("Failed to update the NFS exports."));
    }

    return r;
}

/* ---- the page --------------------------------------------------------- */

struct _AmmoniteSharingPage
{
    AdwNavigationPage parent_instance;

    char *path;
    gboolean loading;

    GtkWidget *toast_overlay;
    GtkWidget *banner;
    GtkWidget *content;

    GtkWidget *smb_group;
    GtkWidget *smb_switch;
    GtkWidget *smb_name;
    GtkWidget *smb_comment;
    GtkWidget *smb_write;
    GtkWidget *smb_guest;

    GtkWidget *nfs_group;
    GtkWidget *nfs_switch;
    GtkWidget *nfs_clients;
    GtkWidget *nfs_write;

    GtkWidget *apply_button;
};

G_DEFINE_FINAL_TYPE (AmmoniteSharingPage, ammonite_sharing_page, ADW_TYPE_NAVIGATION_PAGE)

static void request_apply (AmmoniteSharingPage *self,
                           const char          *password);

static void
update_sensitivity (AmmoniteSharingPage *self)
{
    gboolean smb_on = adw_switch_row_get_active (ADW_SWITCH_ROW (self->smb_switch));
    gboolean nfs_on = adw_switch_row_get_active (ADW_SWITCH_ROW (self->nfs_switch));

    gtk_widget_set_sensitive (self->content, !self->loading);
    gtk_widget_set_sensitive (self->smb_name, smb_on);
    gtk_widget_set_sensitive (self->smb_comment, smb_on);
    gtk_widget_set_sensitive (self->smb_write, smb_on);
    gtk_widget_set_sensitive (self->smb_guest, smb_on);
    gtk_widget_set_sensitive (self->nfs_clients, nfs_on);
    gtk_widget_set_sensitive (self->nfs_write, nfs_on);
}

static void
set_banner (AmmoniteSharingPage *self,
            const char          *text)
{
    adw_banner_set_title (ADW_BANNER (self->banner), text != NULL ? text : "");
    adw_banner_set_revealed (ADW_BANNER (self->banner), text != NULL);
}

static void
state_to_ui (AmmoniteSharingPage *self,
             SharingState        *s)
{
    g_autofree char *base = g_path_get_basename (self->path);

    gtk_widget_set_sensitive (self->smb_group, s->smb_available);
    gtk_widget_set_sensitive (self->nfs_group, s->nfs_available);
    adw_preferences_group_set_description (
        ADW_PREFERENCES_GROUP (self->smb_group),
        s->smb_available ? _("Share this folder with Windows, macOS and Linux computers.")
                         : _("Samba is not installed on this computer."));
    adw_preferences_group_set_description (
        ADW_PREFERENCES_GROUP (self->nfs_group),
        s->nfs_available ? _("Share this folder with Linux and Unix computers.")
                         : _("NFS server tools are not installed on this computer."));

    g_object_set_data_full (G_OBJECT (self), "smb-old-name", g_strdup (s->smb_old_name), g_free);
    adw_switch_row_set_active (ADW_SWITCH_ROW (self->smb_switch), s->smb_enabled);
    gtk_editable_set_text (GTK_EDITABLE (self->smb_name),
                           s->smb_name != NULL ? s->smb_name : base);
    gtk_editable_set_text (GTK_EDITABLE (self->smb_comment),
                           s->smb_comment != NULL ? s->smb_comment : "");
    adw_switch_row_set_active (ADW_SWITCH_ROW (self->smb_write), s->smb_write);
    adw_switch_row_set_active (ADW_SWITCH_ROW (self->smb_guest), s->smb_guest);

    adw_switch_row_set_active (ADW_SWITCH_ROW (self->nfs_switch), s->nfs_enabled);
    gtk_editable_set_text (GTK_EDITABLE (self->nfs_clients),
                           s->nfs_clients != NULL ? s->nfs_clients : "*");
    adw_switch_row_set_active (ADW_SWITCH_ROW (self->nfs_write), s->nfs_write);
}

static void
query_thread (GTask        *task,
              gpointer      source,
              gpointer      task_data,
              GCancellable *cancellable)
{
    SharingState *s = task_data;

    smb_query (s);
    nfs_query (s);
    g_task_return_boolean (task, TRUE);
}

static void
query_done (GObject      *source,
            GAsyncResult *result,
            gpointer      user_data)
{
    AmmoniteSharingPage *self = AMMONITE_SHARING_PAGE (source);
    SharingState *s = g_task_get_task_data (G_TASK (result));

    self->loading = FALSE;
    state_to_ui (self, s);
    update_sensitivity (self);
    set_banner (self, NULL);
    g_object_unref (self);
}

static void
refresh (AmmoniteSharingPage *self)
{
    SharingState *s = g_new0 (SharingState, 1);
    g_autoptr (GTask) task = NULL;

    s->path = g_strdup (self->path);
    self->loading = TRUE;
    update_sensitivity (self);

    /* the task keeps its own reference to the page for the thread's duration */
    task = g_task_new (g_object_ref (self), NULL, query_done, NULL);
    g_task_set_task_data (task, s, (GDestroyNotify) sharing_state_free);
    g_task_run_in_thread (task, query_thread);
}

static void
apply_thread (GTask        *task,
              gpointer      source,
              gpointer      task_data,
              GCancellable *cancellable)
{
    SharingState *s = task_data;
    ApplyResult r;

    r = smb_apply (s);
    if (r == APPLY_OK)
    {
        r = nfs_apply (s);
    }

    s->result = r;
    g_task_return_boolean (task, TRUE);
}

static void
password_response (AdwAlertDialog *dialog,
                   const char     *response,
                   gpointer        user_data)
{
    AmmoniteSharingPage *self = AMMONITE_SHARING_PAGE (user_data);
    GtkWidget *entry = g_object_get_data (G_OBJECT (dialog), "entry");

    if (g_strcmp0 (response, "authorize") == 0)
    {
        g_autofree char *pw = g_strdup (gtk_editable_get_text (GTK_EDITABLE (entry)));

        gtk_editable_set_text (GTK_EDITABLE (entry), "");
        request_apply (self, pw);
        memset (pw, 0, strlen (pw));
    }
    else
    {
        self->loading = FALSE;
        update_sensitivity (self);
    }
}

static void
ask_password (AmmoniteSharingPage *self,
              const char          *error_text)
{
    AdwDialog *dialog = adw_alert_dialog_new (_("Authentication Required"),
                                              _("Enter your password to change file sharing settings."));
    GtkWidget *entry = gtk_password_entry_new ();

    gtk_password_entry_set_show_peek_icon (GTK_PASSWORD_ENTRY (entry), TRUE);
    g_object_set (entry, "activates-default", TRUE, NULL);
    adw_alert_dialog_set_extra_child (ADW_ALERT_DIALOG (dialog), entry);

    if (error_text != NULL)
    {
        g_autofree char *body = g_strdup_printf ("%s\n\n%s",
                                                 _("Enter your password to change file sharing settings."),
                                                 error_text);

        adw_alert_dialog_set_body (ADW_ALERT_DIALOG (dialog), body);
    }

    adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                    "cancel", _("_Cancel"),
                                    "authorize", _("_Authorize"),
                                    NULL);
    adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog), "authorize",
                                              ADW_RESPONSE_SUGGESTED);
    adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "authorize");
    adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (dialog), "cancel");
    g_object_set_data (G_OBJECT (dialog), "entry", entry);
    g_signal_connect_object (dialog, "response", G_CALLBACK (password_response), self, G_CONNECT_DEFAULT);

    adw_dialog_present (dialog, GTK_WIDGET (self));
    gtk_widget_grab_focus (entry);
}

static void
apply_done (GObject      *source,
            GAsyncResult *result,
            gpointer      user_data)
{
    AmmoniteSharingPage *self = AMMONITE_SHARING_PAGE (source);
    SharingState *s = g_task_get_task_data (G_TASK (result));

    switch (s->result)
    {
        case APPLY_OK:
        {
            AdwToast *toast = adw_toast_new (_("Sharing settings applied"));

            adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (self->toast_overlay), toast);
            set_banner (self, NULL);
            refresh (self);
            break;
        }

        case APPLY_NEED_PASSWORD:
        {
            ask_password (self, NULL);
            break;
        }

        case APPLY_BAD_PASSWORD:
        {
            ask_password (self, _("Incorrect password, please try again."));
            break;
        }

        case APPLY_ERROR:
        default:
        {
            self->loading = FALSE;
            update_sensitivity (self);
            set_banner (self, s->error != NULL && *s->error != '\0'
                              ? s->error : _("Failed to change sharing settings."));
            break;
        }
    }

    g_object_unref (self);
}

static void
request_apply (AmmoniteSharingPage *self,
               const char          *password)
{
    SharingState *s = g_new0 (SharingState, 1);
    g_autoptr (GTask) task = NULL;

    s->path = g_strdup (self->path);
    s->password = g_strdup (password);

    /* availability is encoded by whether the groups are sensitive */
    s->smb_available = gtk_widget_get_sensitive (self->smb_group);
    s->nfs_available = gtk_widget_get_sensitive (self->nfs_group);

    s->smb_enabled = adw_switch_row_get_active (ADW_SWITCH_ROW (self->smb_switch));
    s->smb_name = g_strstrip (g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->smb_name))));
    s->smb_comment = g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->smb_comment)));
    s->smb_write = adw_switch_row_get_active (ADW_SWITCH_ROW (self->smb_write));
    s->smb_guest = adw_switch_row_get_active (ADW_SWITCH_ROW (self->smb_guest));
    s->smb_old_name = g_strdup (g_object_get_data (G_OBJECT (self), "smb-old-name"));

    s->nfs_enabled = adw_switch_row_get_active (ADW_SWITCH_ROW (self->nfs_switch));
    s->nfs_clients = g_strstrip (g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->nfs_clients))));
    s->nfs_write = adw_switch_row_get_active (ADW_SWITCH_ROW (self->nfs_write));

    self->loading = TRUE;
    update_sensitivity (self);

    task = g_task_new (g_object_ref (self), NULL, apply_done, NULL);
    g_task_set_task_data (task, s, (GDestroyNotify) sharing_state_free);
    g_task_run_in_thread (task, apply_thread);
}

static void
on_apply_clicked (AmmoniteSharingPage *self)
{
    request_apply (self, NULL);
}

static void
on_switch_toggled (AmmoniteSharingPage *self)
{
    update_sensitivity (self);
}

static void
on_showing (AmmoniteSharingPage *self)
{
    refresh (self);
}

static GtkWidget *
add_switch_row (GtkWidget  *group,
                const char *title)
{
    GtkWidget *row = adw_switch_row_new ();

    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

    return row;
}

static GtkWidget *
add_entry_row (GtkWidget  *group,
               const char *title)
{
    GtkWidget *row = adw_entry_row_new ();

    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);

    return row;
}

static void
ammonite_sharing_page_finalize (GObject *object)
{
    AmmoniteSharingPage *self = AMMONITE_SHARING_PAGE (object);

    g_free (self->path);

    G_OBJECT_CLASS (ammonite_sharing_page_parent_class)->finalize (object);
}

static void
ammonite_sharing_page_class_init (AmmoniteSharingPageClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = ammonite_sharing_page_finalize;
}

static void
ammonite_sharing_page_init (AmmoniteSharingPage *self)
{
    GtkWidget *toolbar = adw_toolbar_view_new ();
    GtkWidget *page = adw_preferences_page_new ();
    GtkWidget *apply_group = adw_preferences_group_new ();

    adw_navigation_page_set_title (ADW_NAVIGATION_PAGE (self), _("Sharing"));
    adw_navigation_page_set_tag (ADW_NAVIGATION_PAGE (self), "sharing");

    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar), adw_header_bar_new ());

    self->banner = adw_banner_new ("");
    adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar), self->banner);

    /* SMB */
    self->smb_group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (self->smb_group),
                                     _("Windows Sharing (SMB/CIFS)"));
    self->smb_switch = add_switch_row (self->smb_group, _("Share via _SMB"));
    adw_preferences_row_set_use_underline (ADW_PREFERENCES_ROW (self->smb_switch), TRUE);
    self->smb_name = add_entry_row (self->smb_group, _("Share Name"));
    self->smb_comment = add_entry_row (self->smb_group, _("Comment"));
    self->smb_write = add_switch_row (self->smb_group, _("Allow others to change files"));
    self->smb_guest = add_switch_row (self->smb_group, _("Allow guest access (no password)"));
    g_signal_connect_swapped (self->smb_switch, "notify::active", G_CALLBACK (on_switch_toggled), self);

    /* NFS */
    self->nfs_group = adw_preferences_group_new ();
    adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (self->nfs_group), _("NFS"));
    self->nfs_switch = add_switch_row (self->nfs_group, _("Share via _NFS"));
    adw_preferences_row_set_use_underline (ADW_PREFERENCES_ROW (self->nfs_switch), TRUE);
    self->nfs_clients = add_entry_row (self->nfs_group, _("Allowed Clients (host, network or *)"));
    self->nfs_write = add_switch_row (self->nfs_group, _("Allow clients to change files"));
    g_signal_connect_swapped (self->nfs_switch, "notify::active", G_CALLBACK (on_switch_toggled), self);

    /* Apply */
    self->apply_button = gtk_button_new_with_mnemonic (_("_Apply"));
    gtk_widget_add_css_class (self->apply_button, "suggested-action");
    gtk_widget_add_css_class (self->apply_button, "pill");
    gtk_widget_set_halign (self->apply_button, GTK_ALIGN_CENTER);
    g_signal_connect_swapped (self->apply_button, "clicked", G_CALLBACK (on_apply_clicked), self);
    adw_preferences_group_add (ADW_PREFERENCES_GROUP (apply_group), self->apply_button);

    adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (self->smb_group));
    adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (self->nfs_group));
    adw_preferences_page_add (ADW_PREFERENCES_PAGE (page), ADW_PREFERENCES_GROUP (apply_group));
    self->content = page;

    self->toast_overlay = adw_toast_overlay_new ();
    adw_toast_overlay_set_child (ADW_TOAST_OVERLAY (self->toast_overlay), page);
    adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar), self->toast_overlay);
    adw_navigation_page_set_child (ADW_NAVIGATION_PAGE (self), toolbar);

    g_signal_connect (self, "showing", G_CALLBACK (on_showing), NULL);
}

AmmoniteSharingPage *
ammonite_sharing_page_new (const char *path)
{
    AmmoniteSharingPage *self = g_object_new (AMMONITE_TYPE_SHARING_PAGE, NULL);

    self->path = g_strdup (path);

    return self;
}
