#ifndef H_MATE_SYSTEM_MONITOR_DISKS_1123719137
#define H_MATE_SYSTEM_MONITOR_DISKS_1123719137

#include "procman.h"

void
create_disk_view(ProcData *procdata, GtkBuilder *builder);

int
cb_update_disks(gpointer procdata);

#endif /* H_MATE_SYSTEM_MONITOR_DISKLIST_1123719137 */
