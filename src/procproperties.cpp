/* Process properties dialog
 * Copyright (C) 2010 Krishnan Parthasarathi <krishnan.parthasarathi@gmail.com>
 *                    Robert Ancell <robert.ancell@canonical.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <config.h>

#include <glib/gi18n.h>
#include <glibtop/procmem.h>
#include <glibtop/procmap.h>
#include <glibtop/procstate.h>

#include "procman.h"
#include "procproperties.h"
#include "proctable.h"
#include "util.h"
#include "e_date.h"

enum
{
    COL_PROP = 0,
    COL_VAL,
    NUM_COLS,
};

typedef struct _proc_arg {
    const gchar *prop;
    gchar *val;
} proc_arg;

static void
get_process_memory_writable (ProcInfo *info)
{
    glibtop_proc_map buf;
    glibtop_map_entry *maps;

    maps = glibtop_get_proc_map(&buf, info->pid);

    gulong memwritable = 0;
    const unsigned number = buf.number;

    for (unsigned i = 0; i < number; ++i) {
#ifdef __linux__
        memwritable += maps[i].private_dirty;
#else
        if (maps[i].perm & GLIBTOP_MAP_PERM_WRITE)
            memwritable += maps[i].size;
#endif
    }

    info->memwritable = memwritable;

    g_free(maps);
}

static void
get_process_memory_info (ProcInfo *info)
{
    glibtop_proc_mem procmem;
    WnckResourceUsage xresources;

    wnck_pid_read_resource_usage (gdk_screen_get_display (gdk_screen_get_default ()),
                                  info->pid,
                                  &xresources);

    glibtop_get_proc_mem(&procmem, info->pid);

    info->vmsize	= procmem.vsize;
    info->memres	= procmem.resident;
    info->memshared	= procmem.share;

    info->memxserver = xresources.total_bytes_estimate;

    get_process_memory_writable(info);

    // fake the smart memory column if writable is not available
    info->mem = info->memxserver + (info->memwritable ? info->memwritable : info->memres);
}

static gchar*
format_memsize(guint64 size)
{
    if (size == 0)
        return g_strdup(_("N/A"));
    else
        return g_format_size_full(size, G_FORMAT_SIZE_IEC_UNITS);
}

static void
fill_proc_properties (GtkWidget *tree, ProcInfo *info)
{
    guint i;
    GtkListStore *store;

    if (!info)
        return;

    get_process_memory_info(info);

    proc_arg proc_props[] = {
        { N_("Process Name"), g_strdup_printf("%s", info->name)},
        { N_("User"), g_strdup_printf("%s (%d)", info->user.c_str(), info->uid)},
        { N_("Status"), g_strdup(format_process_state(info->status))},
        { N_("Memory"), format_memsize(info->mem)},
        { N_("Virtual Memory"), format_memsize(info->vmsize)},
        { N_("Resident Memory"), format_memsize(info->memres)},
        { N_("Writable Memory"), format_memsize(info->memwritable)},
        { N_("Shared Memory"), format_memsize(info->memshared)},
        { N_("X Server Memory"), format_memsize(info->memxserver)},
        { N_("CPU"), g_strdup_printf("%d%%", info->pcpu)},
        { N_("CPU Time"), procman::format_duration_for_display(100 * info->cpu_time / ProcData::get_instance()->frequency) },
        { N_("Started"), procman_format_date_for_display(info->start_time) },
        { N_("Nice"), g_strdup_printf("%d", info->nice)},
        { N_("Priority"), g_strdup_printf("%s", procman::get_nice_level(info->nice)) },
        { N_("ID"), g_strdup_printf("%d", info->pid)},
        { N_("Security Context"), info->security_context ? g_strdup_printf("%s", info->security_context) : g_strdup(_("N/A"))},
        { N_("Command Line"), g_strdup_printf("%s", info->arguments)},
        { N_("Waiting Channel"), g_strdup_printf("%s", info->wchan)},
        { N_("Control Group"), info->cgroup_name ? g_strdup_printf("%s", info->cgroup_name) : g_strdup(_("N/A"))},
        { NULL, NULL}
    };

    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(tree)));
    for (i = 0; proc_props[i].prop; i++) {
        GtkTreeIter iter;

        if (!gtk_tree_model_iter_nth_child (GTK_TREE_MODEL(store), &iter, NULL, i)) {
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter, COL_PROP, gettext(proc_props[i].prop), -1);
        }

        gtk_list_store_set(store, &iter, COL_VAL, g_strstrip(proc_props[i].val), -1);
        g_free(proc_props[i].val);
    }
}

static void
update_procproperties_dialog (GtkWidget *tree)
{
    ProcInfo *info;

    pid_t pid = GPOINTER_TO_UINT(static_cast<pid_t*>(g_object_get_data (G_OBJECT (tree), "selected_info")));
    info = ProcInfo::find(pid);

    fill_proc_properties(tree, info);
}

static void
close_procprop_dialog (GtkDialog *dialog, gint id, gpointer data)
{
    GtkWidget *tree = static_cast<GtkWidget*>(data);
    guint timer;

    timer = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (tree), "timer"));
    g_source_remove (timer);

    gtk_widget_destroy (GTK_WIDGET (dialog));
}

static GtkWidget *
create_procproperties_tree (ProcData *procdata)
{
    GtkWidget *tree;
    GtkListStore *model;
    GtkTreeViewColumn *column;
    GtkCellRenderer *cell;
    gint i;

    model = gtk_list_store_new (NUM_COLS,
                                G_TYPE_STRING,	/* Property */
                                G_TYPE_STRING	/* Value */
        );

    tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
    g_object_unref (G_OBJECT (model));

    for (i = 0; i < NUM_COLS; i++) {
        cell = gtk_cell_renderer_text_new ();

        column = gtk_tree_view_column_new_with_attributes (NULL,
                                                           cell,
                                                           "text", i,
                                                           NULL);
        gtk_tree_view_column_set_resizable (column, TRUE);
        gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
    }

    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(tree), FALSE);
    fill_proc_properties(tree, procdata->selected_process);

    return tree;
}

static gboolean
procprop_timer (gpointer data)
{
    GtkWidget *tree = static_cast<GtkWidget*>(data);
    GtkTreeModel *model;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree));
    g_assert(model);

    update_procproperties_dialog (tree);

    return TRUE;
}

static void
create_single_procproperties_dialog (GtkTreeModel *model, GtkTreePath *path,
                                     GtkTreeIter *iter, gpointer data)
{
    ProcData *procdata = static_cast<ProcData*>(data);
    GtkWidget *procpropdialog;
    GtkWidget *dialog_vbox, *vbox;
    GtkWidget *cmd_hbox;
    GtkWidget *label;
    GtkWidget *scrolled;
    GtkWidget *tree;
    ProcInfo *info;
    guint timer;

    gtk_tree_model_get (model, iter, COL_POINTER, &info, -1);

    if (!info)
        return;

    procpropdialog = gtk_dialog_new_with_buttons (_("Process Properties"), NULL,
                                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  "gtk-close", GTK_RESPONSE_CLOSE,
                                                  NULL);
    gtk_window_set_resizable (GTK_WINDOW (procpropdialog), TRUE);
    gtk_window_set_default_size (GTK_WINDOW (procpropdialog), 575, 400);
    gtk_container_set_border_width (GTK_CONTAINER (procpropdialog), 5);

    vbox = gtk_dialog_get_content_area (GTK_DIALOG (procpropdialog));
    gtk_box_set_spacing (GTK_BOX (vbox), 2);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

    dialog_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width (GTK_CONTAINER (dialog_vbox), 5);
    gtk_box_pack_start (GTK_BOX (vbox), dialog_vbox, TRUE, TRUE, 0);

    cmd_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), cmd_hbox, FALSE, FALSE, 0);

    label = procman_make_label_for_mmaps_or_ofiles (
        _("Properties of process \"%s\" (PID %u):"),
        info->name,
        info->pid);

    gtk_box_pack_start (GTK_BOX (cmd_hbox),label, FALSE, FALSE, 0);

    scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
                                         GTK_SHADOW_IN);

    tree = create_procproperties_tree (procdata);
    gtk_container_add (GTK_CONTAINER (scrolled), tree);
    g_object_set_data (G_OBJECT (tree), "selected_info", GUINT_TO_POINTER (info->pid));

    gtk_box_pack_start (GTK_BOX (dialog_vbox), scrolled, TRUE, TRUE, 0);
    gtk_widget_show_all (scrolled);

    g_signal_connect (G_OBJECT (procpropdialog), "response",
                      G_CALLBACK (close_procprop_dialog), tree);

    gtk_widget_show_all (procpropdialog);

    timer = g_timeout_add_seconds (5, procprop_timer, tree);
    g_object_set_data (G_OBJECT (tree), "timer", GUINT_TO_POINTER (timer));

    update_procproperties_dialog (tree);
}

void
create_procproperties_dialog (ProcData *procdata)
{
    gtk_tree_selection_selected_foreach (procdata->selection, create_single_procproperties_dialog,
                                         procdata);
}
