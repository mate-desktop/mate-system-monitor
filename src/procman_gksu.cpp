#include <config.h>

#include "procman.h"
#include "procman_gksu.h"

gboolean
procman_gksu_create_root_password_dialog (const char *command)
{
    gchar *command_line;
    gboolean success;
    GError *error = NULL;

    command_line = g_strdup_printf ("gksu '%s'", command);
    success = g_spawn_command_line_sync (command_line, NULL, NULL, NULL, &error);
    g_free (command_line);

    if (!success) {
        g_critical ("Could not run gksu '%s' : %s\n",
                    command, error->message);
        g_error_free (error);
        return FALSE;
    }

    g_debug ("gksu did fine\n");
    return TRUE;
}

gboolean
procman_has_gksu (void)
{
    return g_file_test ("/usr/bin/gksu", G_FILE_TEST_EXISTS);
}

