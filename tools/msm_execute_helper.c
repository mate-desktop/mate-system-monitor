#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <glib.h>

int main(int argc, char* argv[])
{
    gchar **argv_modified = g_new0 (gchar *, argc + 1);
    memcpy (argv_modified, argv, argc * sizeof (char*));
    argv_modified[0] = COMMAND;
    int errsv = 0;

    if (execvp (COMMAND, argv_modified) == -1) {
        errsv = errno;
    }

    g_free (argv_modified);
    return errsv;
}
