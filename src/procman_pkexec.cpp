#include <config.h>

#include "procman_pkexec.h"

gboolean
procman_pkexec_create_root_password_dialog (const char *command)
{
    gchar *command_line;
    gboolean success;
    GError *error = NULL;

    command_line = g_strdup_printf ("pkexec --disable-internal-agent %s/msm-%s",
                                    PKGLIBEXECDIR, command);
    success = g_spawn_command_line_sync (command_line, NULL, NULL, NULL, &error);
    g_free (command_line);

    if (!success) {
        g_critical ("Could not run pkexec (\"%s\") : %s\n",
                    command, error->message);
        g_error_free (error);
        return FALSE;
    }

    g_debug ("pkexec did fine\n");
    return TRUE;
}

gboolean
procman_has_pkexec (void)
{
    return g_file_test("/usr/bin/pkexec", G_FILE_TEST_EXISTS);
}

