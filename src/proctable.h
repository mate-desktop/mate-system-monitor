/* Procman - tree view
 * Copyright (C) 2001 Kevin Vandersloot
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef _PROCMAN_PROCTABLE_H_
#define _PROCMAN_PROCTABLE_H_

#include <glib.h>
#include <gtk/gtk.h>
#include "procman.h"

enum
{
    COL_NAME = 0,
    COL_USER,
    COL_STATUS,
    COL_VMSIZE,
    COL_MEMRES,
    COL_MEMWRITABLE,
    COL_MEMSHARED,
    COL_MEMXSERVER,
    COL_CPU,
    COL_CPU_TIME,
    COL_START_TIME,
    COL_NICE,
    COL_PID,
    COL_SECURITYCONTEXT,
    COL_ARGS,
    COL_MEM,
    COL_WCHAN,
    COL_CGROUP,
    COL_UNIT,
    COL_SESSION,
    COL_SEAT,
    COL_OWNER,
    COL_DISK_WRITE_TOTAL,
    COL_DISK_READ_TOTAL,
    COL_DISK_WRITE_CURRENT,
    COL_DISK_READ_CURRENT,
    COL_PRIORITY,
    COL_SURFACE,
    COL_POINTER,
    COL_TOOLTIP,
    NUM_COLUMNS
};


GtkWidget*      proctable_new (ProcData *data);
void            proctable_update_table (ProcData *data);
void            proctable_update (ProcData *data);
void            proctable_clear_tree (ProcData *data);
void            proctable_free_table (ProcData *data);

GSList*         proctable_get_columns_order(GtkTreeView *treeview);
void            proctable_set_columns_order(GtkTreeView *treeview, GSList *order);

char*           make_loadavg_string(void);

void            get_last_selected (GtkTreeModel *model, GtkTreePath *path,
                                   GtkTreeIter *iter, gpointer data);

#endif /* _PROCMAN_PROCTABLE_H_ */
