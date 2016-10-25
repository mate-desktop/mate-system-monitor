#ifndef _PROCMAN_PKEXEC_H_
#define _PROCMAN_PKEXEC_H_

#include <glib.h>

gboolean
procman_pkexec_create_root_password_dialog(const char *command);

gboolean
procman_has_pkexec(void) G_GNUC_CONST;

#endif /* _PROCMAN_PKEXEC_H_ */
