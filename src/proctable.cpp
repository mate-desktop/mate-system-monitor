/* Procman tree view and process updating
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


#include <config.h>


#include <string.h>
#include <math.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <glibtop.h>
#include <glibtop/loadavg.h>
#include <glibtop/proclist.h>
#include <glibtop/procio.h>
#include <glibtop/procstate.h>
#include <glibtop/procmem.h>
#include <glibtop/procmap.h>
#include <glibtop/proctime.h>
#include <glibtop/procuid.h>
#include <glibtop/procargs.h>
#include <glibtop/prockernel.h>
#include <glibtop/mem.h>
#include <glibtop/swap.h>
#include <sys/stat.h>
#include <pwd.h>
#include <time.h>

#include <set>
#include <list>

#ifdef HAVE_SYSTEMD
#include <systemd/sd-login.h>
#endif

#include "procman.h"
#include "selection.h"
#include "proctable.h"
#include "callbacks.h"
#include "prettytable.h"
#include "util.h"
#include "interface.h"
#include "selinux.h"
#include "cgroups.h"


ProcInfo::UserMap ProcInfo::users;
ProcInfo::List ProcInfo::all;
std::map<pid_t, guint64> ProcInfo::cpu_times;


ProcInfo* ProcInfo::find(pid_t pid)
{
    Iterator it(ProcInfo::all.find(pid));
    return (it == ProcInfo::all.end() ? NULL : it->second);
}


static void
cb_columns_changed(GtkTreeView *treeview, gpointer user_data)
{
    ProcData * const procdata = static_cast<ProcData*>(user_data);

    procman_save_tree_state(procdata->settings,
                            GTK_WIDGET(treeview),
                            "proctree");
}

static void
cb_sort_changed (GtkTreeSortable *model, gpointer user_data)
{
    ProcData * const procdata = static_cast<ProcData*>(user_data);

    procman_save_tree_state (procdata->settings,
                             GTK_WIDGET (procdata->tree),
                             "proctree");
}


static GtkTreeViewColumn*
my_gtk_tree_view_get_column_with_sort_column_id(GtkTreeView *treeview, int id)
{
    GList *columns, *it;
    GtkTreeViewColumn *col = NULL;

    columns = gtk_tree_view_get_columns(treeview);

    for(it = columns; it; it = it->next)
    {
        if(gtk_tree_view_column_get_sort_column_id(static_cast<GtkTreeViewColumn*>(it->data)) == id)
        {
            col = static_cast<GtkTreeViewColumn*>(it->data);
            break;
        }
    }

    g_list_free(columns);

    return col;
}


void
proctable_set_columns_order(GtkTreeView *treeview, GSList *order)
{
    GtkTreeViewColumn* last = NULL;
    GSList *it;

    for(it = order; it; it = it->next)
    {
        int id;
        GtkTreeViewColumn *cur;

        id = GPOINTER_TO_INT(it->data);

        cur = my_gtk_tree_view_get_column_with_sort_column_id(treeview, id);

        if(cur && cur != last)
        {
            gtk_tree_view_move_column_after(treeview, cur, last);
            last = cur;
        }
    }
}


GSList*
proctable_get_columns_order(GtkTreeView *treeview)
{
    GList *columns, *col;
    GSList *order = NULL;

    columns = gtk_tree_view_get_columns(treeview);

    for(col = columns; col; col = col->next)
    {
        int id;

        id = gtk_tree_view_column_get_sort_column_id(static_cast<GtkTreeViewColumn*>(col->data));
        order = g_slist_prepend(order, GINT_TO_POINTER(id));
    }

    g_list_free(columns);

    order = g_slist_reverse(order);

    return order;
}

static guint timeout_id = 0;
static GtkTreeViewColumn *current_column;

static gboolean
save_column_width (gpointer data)
{
    gint width;
    gchar *key;
    int id;
    GSettings *settings;

    settings = g_settings_get_child (G_SETTINGS (data), "proctree");
    id = gtk_tree_view_column_get_sort_column_id (current_column);
    width = gtk_tree_view_column_get_width (current_column);

    key = g_strdup_printf ("col-%d-width", id);
    g_settings_set_int(settings, key, width);
    g_free (key);

    if (timeout_id) {
        g_source_remove (timeout_id);
        timeout_id = 0;
    }

    return FALSE;
}

static void
cb_proctable_column_resized(GtkWidget *widget, GParamSpec *pspec, gpointer data)
{
    current_column = GTK_TREE_VIEW_COLUMN(widget);

    if (timeout_id)
        g_source_remove (timeout_id);

    timeout_id = g_timeout_add (250, save_column_width, data);
}

static gboolean
search_equal_func(GtkTreeModel *model,
                  gint column,
                  const gchar *key,
                  GtkTreeIter *iter,
                  gpointer search_data)
{
    char* name;
    char* user;
    gboolean found;

    gtk_tree_model_get(model, iter,
                       COL_NAME, &name,
                       COL_USER, &user,
                       -1);

    found = !((name && strcasestr(name, key))
              || (user && strcasestr(user, key)));

    g_free(name);
    g_free(user);

    return found;
}



GtkWidget *
proctable_new (ProcData * const procdata)
{
    GtkWidget *proctree;
    GtkWidget *scrolled;
    GtkTreeStore *model;
    GtkTreeSelection *selection;
    GtkTreeViewColumn *column;
    GtkCellRenderer *cell_renderer;

    const gchar *titles[] = {
        N_("Process Name"),
        N_("User"),
        N_("Status"),
        N_("Virtual Memory"),
        N_("Resident Memory"),
        N_("Writable Memory"),
        N_("Shared Memory"),
        N_("X Server Memory"),
        /* xgettext:no-c-format */ N_("% CPU"),
        N_("CPU Time"),
        N_("Started"),
        N_("Nice"),
        N_("ID"),
        N_("Security Context"),
        N_("Command Line"),
        N_("Memory"),
        /* xgettext: combined noun, the function the process is waiting in, see wchan ps(1) */
        N_("Waiting Channel"),
        N_("Control Group"),
        N_("Unit"),
        N_("Session"),
        /* TRANSLATORS: Seat = i.e. the physical seat the session of the process belongs to, only
	for multi-seat environments. See http://en.wikipedia.org/wiki/Multiseat_configuration */
        N_("Seat"),
        N_("Owner"),
        N_("Disk Read Total"),
        N_("Disk Write Total"),
        N_("Disk Read"),
        N_("Disk Write"),
        N_("Priority"),
        NULL,
        "POINTER"
    };

    gint i;

    scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);

    model = gtk_tree_store_new (NUM_COLUMNS,
                                G_TYPE_STRING,      /* Process Name */
                                G_TYPE_STRING,      /* User         */
                                G_TYPE_UINT,        /* Status       */
                                G_TYPE_ULONG,       /* VM Size      */
                                G_TYPE_ULONG,       /* Resident Memory */
                                G_TYPE_ULONG,       /* Writable Memory */
                                G_TYPE_ULONG,       /* Shared Memory */
                                G_TYPE_ULONG,       /* X Server Memory */
                                G_TYPE_UINT,        /* % CPU        */
                                G_TYPE_UINT64,      /* CPU time     */
                                G_TYPE_ULONG,       /* Started      */
                                G_TYPE_INT,         /* Nice         */
                                G_TYPE_UINT,        /* ID           */
                                G_TYPE_STRING,      /* Security Context */
                                G_TYPE_STRING,      /* Arguments    */
                                G_TYPE_ULONG,       /* Memory       */
                                G_TYPE_STRING,      /* wchan        */
                                G_TYPE_STRING,      /* Cgroup       */
                                G_TYPE_STRING,      /* Unit         */
                                G_TYPE_STRING,      /* Session      */
                                G_TYPE_STRING,      /* Seat         */
                                G_TYPE_STRING,      /* Owner        */
                                G_TYPE_UINT64,      /* Disk read total */
                                G_TYPE_UINT64,      /* Disk write total*/
                                G_TYPE_UINT64,      /* Disk read    */
                                G_TYPE_UINT64,      /* Disk write   */
                                G_TYPE_STRING,      /* Priority     */
                                GDK_TYPE_PIXBUF,    /* Icon         */
                                G_TYPE_POINTER,     /* ProcInfo     */
                                G_TYPE_STRING       /* Sexy tooltip */
        );

    proctree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
    gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (proctree), COL_TOOLTIP);
    g_object_set(G_OBJECT(proctree),
                 "show-expanders", procdata->config.show_tree,
                 NULL);
    gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (proctree),
                                         search_equal_func,
                                         NULL,
                                         NULL);
    g_object_unref (G_OBJECT (model));

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (proctree));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

    column = gtk_tree_view_column_new ();

    cell_renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, cell_renderer, FALSE);
    gtk_tree_view_column_set_attributes (column, cell_renderer,
                                         "pixbuf", COL_PIXBUF,
                                         NULL);

    cell_renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, cell_renderer, FALSE);
    gtk_tree_view_column_set_attributes (column, cell_renderer,
                                         "text", COL_NAME,
                                         NULL);
    gtk_tree_view_column_set_title (column, _(titles[0]));
    gtk_tree_view_column_set_sort_column_id (column, COL_NAME);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_min_width (column, 20);
    gtk_tree_view_column_set_reorderable (column, TRUE);
    g_signal_connect(G_OBJECT(column), "notify::fixed-width", G_CALLBACK(cb_proctable_column_resized), procdata->settings);
    gtk_tree_view_append_column (GTK_TREE_VIEW (proctree), column);
    gtk_tree_view_set_expander_column (GTK_TREE_VIEW (proctree), column);


    for (i = COL_USER; i <= COL_PRIORITY; i++) {

        GtkCellRenderer *cell;
        GtkTreeViewColumn *col;

        cell = gtk_cell_renderer_text_new();
        col = gtk_tree_view_column_new();
        gtk_tree_view_column_pack_start(col, cell, TRUE);
        gtk_tree_view_column_set_title(col, _(titles[i]));
        gtk_tree_view_column_set_sort_column_id(col, i);
        gtk_tree_view_column_set_resizable(col, TRUE);
        gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_FIXED);
        g_signal_connect(G_OBJECT(col), "notify::fixed-width", G_CALLBACK(cb_proctable_column_resized), procdata->settings);
        gtk_tree_view_column_set_reorderable(col, TRUE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(proctree), col);

        // type
        switch (i) {
            case COL_MEMXSERVER:
                gtk_tree_view_column_set_cell_data_func(col, cell,
                                                        &procman::memory_size_cell_data_func,
                                                        GUINT_TO_POINTER(i),
                                                        NULL);
                break;
            case COL_VMSIZE:
            case COL_MEMRES:
            case COL_MEMSHARED:
            case COL_MEM:
            case COL_MEMWRITABLE:
                gtk_tree_view_column_set_cell_data_func(col, cell,
                                                        &procman::memory_size_na_cell_data_func,
                                                        GUINT_TO_POINTER(i),
                                                        NULL);
                break;

            case COL_CPU_TIME:
                gtk_tree_view_column_set_cell_data_func(col, cell,
                                                        &procman::duration_cell_data_func,
                                                        GUINT_TO_POINTER(i),
                                                        NULL);
                break;

            case COL_START_TIME:
                gtk_tree_view_column_set_cell_data_func(col, cell,
                                                        &procman::time_cell_data_func,
                                                        GUINT_TO_POINTER(i),
                                                        NULL);
                break;

            case COL_STATUS:
                gtk_tree_view_column_set_cell_data_func(col, cell,
                                                        &procman::status_cell_data_func,
                                                        GUINT_TO_POINTER(i),
                                                        NULL);
                break;

            case COL_DISK_READ_TOTAL:
            case COL_DISK_WRITE_TOTAL:
                gtk_tree_view_column_set_cell_data_func(col, cell,
                                                        &procman::storage_size_na_cell_data_func,
                                                        GUINT_TO_POINTER(i),
                                                        NULL);
                break;

            case COL_DISK_READ_CURRENT:
            case COL_DISK_WRITE_CURRENT:
                gtk_tree_view_column_set_cell_data_func(col, cell,
                                                        &procman::io_rate_cell_data_func,
                                                        GUINT_TO_POINTER(i),
                                                        NULL);
                break;

            case COL_PRIORITY:
                gtk_tree_view_column_set_cell_data_func(col, cell,
                                                        &procman::priority_cell_data_func,
                                                        GUINT_TO_POINTER(COL_NICE),
                                                        NULL);
                break;

            default:
                gtk_tree_view_column_set_attributes(col, cell, "text", i, NULL);
        }

        // sorting
        switch (i) {
            case COL_MEMXSERVER:
            case COL_VMSIZE:
            case COL_MEMRES:
            case COL_MEMSHARED:
            case COL_MEM:
            case COL_MEMWRITABLE:
            case COL_CPU:
            case COL_CPU_TIME:
            case COL_DISK_READ_TOTAL:
            case COL_DISK_WRITE_TOTAL:
            case COL_DISK_READ_CURRENT:
            case COL_DISK_WRITE_CURRENT:
            case COL_START_TIME:
                gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(model), i,
                                                procman::number_compare_func, GUINT_TO_POINTER(i),
                                                NULL);
                break;
            case COL_PRIORITY:
                gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(model), i,
                                                procman::priority_compare_func,
                                                GUINT_TO_POINTER(COL_NICE), NULL);
                break;
            default:
                break;
        }

        // xalign
        switch(i)
        {
            case COL_VMSIZE:
            case COL_MEMRES:
            case COL_MEMWRITABLE:
            case COL_MEMSHARED:
            case COL_MEMXSERVER:
            case COL_CPU:
            case COL_NICE:
            case COL_PID:
            case COL_DISK_READ_TOTAL:
            case COL_DISK_WRITE_TOTAL:
            case COL_DISK_READ_CURRENT:
            case COL_DISK_WRITE_CURRENT:
            case COL_CPU_TIME:
            case COL_MEM:
                g_object_set(G_OBJECT(cell), "xalign", 1.0f, NULL);
                break;
        }

        // sizing
        switch (i) {
            case COL_ARGS:
                gtk_tree_view_column_set_min_width(col, 150);
                break;
            default:
                gtk_tree_view_column_set_min_width(col, 20);
                break;
        }
    }

    gtk_container_add (GTK_CONTAINER (scrolled), proctree);

    procdata->tree = proctree;

    procman_get_tree_state (procdata->settings, proctree, "proctree");

    /* Override column settings by hiding this column if it's meaningless: */
    if (!can_show_security_context_column ()) {
        column = my_gtk_tree_view_get_column_with_sort_column_id (GTK_TREE_VIEW (proctree), COL_SECURITYCONTEXT);
        gtk_tree_view_column_set_visible (column, FALSE);
    }

    if (!cgroups_enabled()) {
        column = my_gtk_tree_view_get_column_with_sort_column_id(GTK_TREE_VIEW(proctree), COL_CGROUP);
        gtk_tree_view_column_set_visible(column, FALSE);
    }

#ifdef HAVE_SYSTEMD
    if (!LOGIND_RUNNING())
#endif
    {
        for (i = COL_UNIT; i <= COL_OWNER; i++) {
            column = my_gtk_tree_view_get_column_with_sort_column_id(GTK_TREE_VIEW(proctree), i);
            gtk_tree_view_column_set_visible(column, FALSE);
        }
    }

    g_signal_connect (G_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (proctree))),
                      "changed",
                      G_CALLBACK (cb_row_selected), procdata);
    g_signal_connect (G_OBJECT (proctree), "popup_menu",
                      G_CALLBACK (cb_tree_popup_menu), procdata);
    g_signal_connect (G_OBJECT (proctree), "button_press_event",
                      G_CALLBACK (cb_tree_button_pressed), procdata);

    g_signal_connect (G_OBJECT(proctree), "columns-changed",
                      G_CALLBACK(cb_columns_changed), procdata);

    g_signal_connect (G_OBJECT (model), "sort-column-changed",
                      G_CALLBACK (cb_sort_changed), procdata);

    return scrolled;
}


ProcInfo::~ProcInfo()
{
    g_free(this->name);
    g_free(this->tooltip);
    g_free(this->arguments);
    g_free(this->security_context);
    g_free(this->cgroup_name);
    g_free(this->unit);
    g_free(this->session);
    g_free(this->seat);
}


static void
get_process_name (ProcInfo *info,
                  const gchar *cmd, const GStrv args)
{
    if (args) {
        // look for /usr/bin/very_long_name
        // and also /usr/bin/interpreter /usr/.../very_long_name
        // which may have use prctl to alter 'cmd' name
        for (int i = 0; i != 2 && args[i]; ++i) {
            char* basename;
            basename = g_path_get_basename(args[i]);

            if (g_str_has_prefix(basename, cmd)) {
                info->name = basename;
                return;
            }

            g_free(basename);
        }
    }

    info->name = g_strdup (cmd);
}

std::string
ProcInfo::lookup_user(guint uid)
{
    typedef std::pair<ProcInfo::UserMap::iterator, bool> Pair;
    ProcInfo::UserMap::value_type hint(uid, "");
    Pair p(ProcInfo::users.insert(hint));

    // procman_debug("User lookup for uid %u: %s", uid, (p.second ? "MISS" : "HIT"));

    if (p.second) {
        struct passwd* pwd;
        pwd = getpwuid(uid);

        if (pwd && pwd->pw_name)
            p.first->second = pwd->pw_name;
        else {
            char username[16];
            g_sprintf(username, "%u", uid);
            p.first->second = username;
        }
    }

    return p.first->second;
}

void
ProcInfo::set_user(guint uid)
{
    if (G_LIKELY(this->uid == uid))
        return;

    this->uid = uid;
    this->user = lookup_user(uid);
}

static void get_process_memory_writable(ProcInfo *info)
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
get_process_memory_info(ProcInfo *info)
{
    glibtop_proc_mem procmem;
    WnckResourceUsage xresources;

    wnck_pid_read_resource_usage (gdk_screen_get_display (gdk_screen_get_default ()),
                                  info->pid,
                                  &xresources);

    glibtop_get_proc_mem(&procmem, info->pid);

    info->vmsize    = procmem.vsize;
    info->memres    = procmem.resident;
    info->memshared = procmem.share;

    info->memxserver = xresources.total_bytes_estimate;

    get_process_memory_writable(info);

    // fake the smart memory column if writable is not available
    info->mem = info->memxserver + (info->memwritable ? info->memwritable : info->memres);
}



static void
update_info_mutable_cols(ProcInfo *info)
{
    ProcData * const procdata = ProcData::get_instance();
    GtkTreeModel *model;
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(procdata->tree));

    using procman::tree_store_update;

    tree_store_update(model, &info->node, COL_STATUS, info->status);
    tree_store_update(model, &info->node, COL_USER, info->user.c_str());
    tree_store_update(model, &info->node, COL_VMSIZE, info->vmsize);
    tree_store_update(model, &info->node, COL_MEMRES, info->memres);
    tree_store_update(model, &info->node, COL_MEMWRITABLE, info->memwritable);
    tree_store_update(model, &info->node, COL_MEMSHARED, info->memshared);
    tree_store_update(model, &info->node, COL_MEMXSERVER, info->memxserver);
    tree_store_update(model, &info->node, COL_CPU, info->pcpu);
    tree_store_update(model, &info->node, COL_CPU_TIME, info->cpu_time);
    tree_store_update(model, &info->node, COL_DISK_READ_TOTAL, info->disk_read_bytes_total);
    tree_store_update(model, &info->node, COL_DISK_WRITE_TOTAL, info->disk_write_bytes_total);
    tree_store_update(model, &info->node, COL_DISK_READ_CURRENT, info->disk_read_bytes_current);
    tree_store_update(model, &info->node, COL_DISK_WRITE_CURRENT, info->disk_write_bytes_current);
    tree_store_update(model, &info->node, COL_START_TIME, info->start_time);
    tree_store_update(model, &info->node, COL_NICE, info->nice);
    tree_store_update(model, &info->node, COL_MEM, info->mem);
    tree_store_update(model, &info->node, COL_WCHAN, info->wchan);
    tree_store_update(model, &info->node, COL_CGROUP, info->cgroup_name);
    tree_store_update(model, &info->node, COL_UNIT, info->unit);
    tree_store_update(model, &info->node, COL_SESSION, info->session);
    tree_store_update(model, &info->node, COL_SEAT, info->seat);
    tree_store_update(model, &info->node, COL_OWNER, info->owner.c_str());
}



static void
insert_info_to_tree (ProcInfo *info, ProcData *procdata, bool forced = false)
{
    GtkTreeModel *model;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (procdata->tree));

    if (procdata->config.show_tree) {

        ProcInfo *parent = 0;

        if (not forced)
            parent = ProcInfo::find(info->ppid);

        if (parent) {
            GtkTreePath *parent_node = gtk_tree_model_get_path(model, &parent->node);
            gtk_tree_store_insert(GTK_TREE_STORE(model), &info->node, &parent->node, 0);

            if (!gtk_tree_view_row_expanded(GTK_TREE_VIEW(procdata->tree), parent_node)
#ifdef __linux__
                // on linuxes we don't want to expand kthreadd by default (always has pid 2)
                && (parent->pid != 2)
#endif
            )
                gtk_tree_view_expand_row(GTK_TREE_VIEW(procdata->tree), parent_node, FALSE);
            gtk_tree_path_free(parent_node);
        } else
            gtk_tree_store_insert(GTK_TREE_STORE(model), &info->node, NULL, 0);
    }
    else
        gtk_tree_store_insert (GTK_TREE_STORE (model), &info->node, NULL, 0);

    gtk_tree_store_set (GTK_TREE_STORE (model), &info->node,
                        COL_POINTER, info,
                        COL_NAME, info->name,
                        COL_ARGS, info->arguments,
                        COL_TOOLTIP, info->tooltip,
                        COL_PID, info->pid,
                        COL_SECURITYCONTEXT, info->security_context,
                        -1);

    procdata->pretty_table.set_icon(*info);

    procman_debug("inserted %d%s", info->pid, (forced ? " (forced)" : ""));
}


/* Removing a node with children - make sure the children are queued
** to be readded.
*/
template<typename List>
static void
remove_info_from_tree (ProcData *procdata, GtkTreeModel *model,
                       ProcInfo *current, List &orphans, unsigned lvl = 0)
{
    GtkTreeIter child_node;

    if (std::find(orphans.begin(), orphans.end(), current) != orphans.end()) {
        procman_debug("[%u] %d already removed from tree", lvl, int(current->pid));
        return;
    }

    procman_debug("[%u] pid %d, %d children", lvl, int(current->pid),
                  gtk_tree_model_iter_n_children(model, &current->node));

    // it is not possible to iterate&erase over a treeview so instead we
    // just pop one child after another and recursively remove it and
    // its children

    while (gtk_tree_model_iter_children(model, &child_node, &current->node)) {
        ProcInfo *child = 0;
        gtk_tree_model_get(model, &child_node, COL_POINTER, &child, -1);
        remove_info_from_tree(procdata, model, child, orphans, lvl + 1);
    }

    g_assert(not gtk_tree_model_iter_has_child(model, &current->node));

    if (procdata->selected_process == current)
        procdata->selected_process = NULL;

    orphans.push_back(current);
    gtk_tree_store_remove(GTK_TREE_STORE(model), &current->node);
    procman::poison(current->node, 0x69);
}

static void
get_process_systemd_info(ProcInfo *info)
{
#ifdef HAVE_SYSTEMD
    uid_t uid;

    if (!LOGIND_RUNNING())
        return;

    free(info->unit);
    info->unit = NULL;
    sd_pid_get_unit(info->pid, &info->unit);

    free(info->session);
    info->session = NULL;
    sd_pid_get_session(info->pid, &info->session);

    free(info->seat);
    info->seat = NULL;

    if (info->session != NULL)
        sd_session_get_seat(info->session, &info->seat);

    if (sd_pid_get_owner_uid(info->pid, &uid) >= 0)
        info->owner = info->lookup_user(uid);
    else
        info->owner = "";
#endif
}

static void
update_info (ProcData *procdata, ProcInfo *info)
{
    glibtop_proc_state procstate;
    glibtop_proc_uid procuid;
    glibtop_proc_time proctime;
    glibtop_proc_kernel prockernel;
    glibtop_proc_io procio;

    glibtop_get_proc_kernel(&prockernel, info->pid);
    g_strlcpy(info->wchan, prockernel.wchan, sizeof info->wchan);

    glibtop_get_proc_state (&procstate, info->pid);
    info->status = procstate.state;

    glibtop_get_proc_uid (&procuid, info->pid);
    glibtop_get_proc_time (&proctime, info->pid);
    glibtop_get_proc_io (&procio, info->pid);

    get_process_memory_info(info);

    info->set_user(procstate.uid);

    // if the cpu time has increased reset the status to running
    // regardless of kernel state (https://bugzilla.gnome.org/606579)
    guint64 difference = proctime.rtime - info->cpu_time;
    if (difference > 0)
        info->status = GLIBTOP_PROCESS_RUNNING;
    info->pcpu = difference * 100 / procdata->cpu_total_time;
    info->pcpu = MIN(info->pcpu, 100);

    if (not procdata->config.solaris_mode)
      info->pcpu *= procdata->config.num_cpus;

    ProcInfo::cpu_times[info->pid] = info->cpu_time = proctime.rtime;
    info->nice = procuid.nice;

    gdouble update_interval_seconds = procdata->config.update_interval / 1000.0;
    difference = procio.disk_wbytes - info->disk_write_bytes_total;
    info->disk_write_bytes_current = difference > update_interval_seconds ? difference/update_interval_seconds : 0ULL;
    difference = procio.disk_rbytes - info->disk_read_bytes_total;
    info->disk_read_bytes_current = difference > update_interval_seconds ? difference/update_interval_seconds : 0ULL;

    info->disk_write_bytes_total = procio.disk_wbytes;
    info->disk_read_bytes_total = procio.disk_rbytes;

    // set the ppid only if one can exist
    // i.e. pid=0 can never have a parent
    if (info->pid > 0) {
        info->ppid = procuid.ppid;
    }

    g_assert(info->pid != info->ppid);
    g_assert(info->ppid != -1 || info->pid == 0);

    /* get cgroup data */
    get_process_cgroup_info(info);

    get_process_systemd_info(info);
}


ProcInfo::ProcInfo(pid_t pid)
    : node(),
      pixbuf(),
      tooltip(NULL),
      name(NULL),
      arguments(NULL),
      security_context(NULL),
      pid(pid),
      ppid(-1),
      uid(-1)
{
    ProcInfo * const info = this;
    glibtop_proc_state procstate;
    glibtop_proc_time proctime;
    glibtop_proc_args procargs;
    gchar** arguments;

    glibtop_get_proc_state (&procstate, pid);
    glibtop_get_proc_time (&proctime, pid);
    arguments = glibtop_get_proc_argv (&procargs, pid, 0);

    /* FIXME : wrong. name and arguments may change with exec* */
    get_process_name (info, procstate.cmd, static_cast<const GStrv>(arguments));

    std::string tooltip = make_string(g_strjoinv(" ", arguments));
    if (tooltip.empty())
        tooltip = procstate.cmd;

    info->tooltip = g_markup_escape_text(tooltip.c_str(), -1);

    info->arguments = g_strescape(tooltip.c_str(), "\\\"");
    g_strfreev(arguments);

    guint64 cpu_time = proctime.rtime;
    std::map<pid_t, guint64>::iterator it(ProcInfo::cpu_times.find(pid));
    if (it != ProcInfo::cpu_times.end())
    {
        if (proctime.rtime >= it->second)
          cpu_time = it->second;
    }
    info->cpu_time = cpu_time;
    info->start_time = proctime.start_time;

    get_process_selinux_context (info);
    info->cgroup_name = NULL;
    get_process_cgroup_info(info);

    info->unit = info->session = info->seat = NULL;
    get_process_systemd_info(info);
}

static void
refresh_list (ProcData *procdata, const pid_t* pid_list, const guint n)
{
    typedef std::list<ProcInfo*> ProcList;
    ProcList addition;

    GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (procdata->tree));
    guint i;

    // Add or update processes in the process list
    for(i = 0; i < n; ++i) {
        ProcInfo *info = ProcInfo::find(pid_list[i]);

        if (!info) {
            info = new ProcInfo(pid_list[i]);
            ProcInfo::all[info->pid] = info;
            addition.push_back(info);
        }

        update_info (procdata, info);
    }


    // Remove dead processes from the process list and from the
    // tree. children are queued to be readded at the right place
    // in the tree.

    const std::set<pid_t> pids(pid_list, pid_list + n);

    ProcInfo::Iterator it(ProcInfo::begin());

    while (it != ProcInfo::end()) {
        ProcInfo * const info = it->second;
        ProcInfo::Iterator next(it);
        ++next;

        if (pids.find(info->pid) == pids.end()) {
            procman_debug("ripping %d", info->pid);
            remove_info_from_tree(procdata, model, info, addition);
            addition.remove(info);
            ProcInfo::all.erase(it);
            delete info;
        }

        it = next;
    }

    // INVARIANT
    // pid_list == ProcInfo::all + addition


    if (procdata->config.show_tree) {

        // insert process in the tree. walk through the addition list
        // (new process + process that have a new parent). This loop
        // handles the dependencies because we cannot insert a process
        // until its parent is in the tree.

        std::set<pid_t> in_tree(pids);

        for (ProcList::iterator it(addition.begin()); it != addition.end(); ++it)
            in_tree.erase((*it)->pid);


        while (not addition.empty()) {
            procman_debug("looking for %d parents", int(addition.size()));
            ProcList::iterator it(addition.begin());

            while (it != addition.end()) {
                procman_debug("looking for %d's parent with ppid %d",
                              int((*it)->pid), int((*it)->ppid));


                // inserts the process in the treeview if :
                // - it has no parent (ppid = -1),
                //   ie it is for example the [kernel] on FreeBSD
                // - it is init
                // - its parent is already in tree
                // - its parent is unreachable
                //
                // rounds == 2 means that addition contains processes with
                // unreachable parents
                //
                // FIXME: this is broken if the unreachable parent becomes active
                // i.e. it gets active or changes ower
                // so we just clear the tree on __each__ update
                // see proctable_update_list (ProcData * const procdata)


                if ((*it)->ppid <= 0 or in_tree.find((*it)->ppid) != in_tree.end()) {
                    insert_info_to_tree(*it, procdata);
                    in_tree.insert((*it)->pid);
                    it = addition.erase(it);
                    continue;
                }

                ProcInfo *parent = ProcInfo::find((*it)->ppid);
                // if the parent is unreachable
                if (not parent) {
                    // or std::find(addition.begin(), addition.end(), parent) == addition.end()) {
                    insert_info_to_tree(*it, procdata, true);
                    in_tree.insert((*it)->pid);
                    it = addition.erase(it);
                    continue;
                }

                ++it;
            }
        }
    }
    else {
        // don't care of the tree
        for (ProcList::iterator it(addition.begin()); it != addition.end(); ++it)
            insert_info_to_tree(*it, procdata);
    }


    for (ProcInfo::Iterator it(ProcInfo::begin()); it != ProcInfo::end(); ++it)
        update_info_mutable_cols(it->second);
}


static void
proctable_update_list (ProcData * const procdata)
{
    pid_t* pid_list;
    glibtop_proclist proclist;
    glibtop_cpu cpu;
    gint which, arg;
    procman::SelectionMemento selection;

    switch (procdata->config.whose_process) {
        case ALL_PROCESSES:
            which = GLIBTOP_KERN_PROC_ALL;
            arg = 0;
            break;

        case ACTIVE_PROCESSES:
            which = GLIBTOP_KERN_PROC_ALL | GLIBTOP_EXCLUDE_IDLE;
            arg = 0;
            if (procdata->config.show_tree)
            {
                selection.save(procdata->tree);
            }
            break;

        default:
            which = GLIBTOP_KERN_PROC_UID;
            arg = getuid ();
            if (procdata->config.show_tree)
            {
                selection.save(procdata->tree);
            }
            break;
    }

    pid_list = glibtop_get_proclist (&proclist, which, arg);

    /* FIXME: total cpu time elapsed should be calculated on an individual basis here
    ** should probably have a total_time_last gint in the ProcInfo structure */
    glibtop_get_cpu (&cpu);
    procdata->cpu_total_time = MAX(cpu.total - procdata->cpu_total_time_last, 1);
    procdata->cpu_total_time_last = cpu.total;

    // FIXME: not sure if glibtop always returns a sorted list of pid
    // but it is important otherwise refresh_list won't find the parent
    std::sort(pid_list, pid_list + proclist.number);

    refresh_list (procdata, pid_list, proclist.number);

    selection.restore(procdata->tree);

    g_free (pid_list);

    /* proclist.number == g_list_length(procdata->info) == g_hash_table_size(procdata->pids) */
}


void
proctable_update (ProcData * const procdata)
{
    char* string;

    string = make_loadavg_string();
    gtk_label_set_text (GTK_LABEL(procdata->loadavg), string);
    g_free (string);

    proctable_update_list (procdata);
}


void
proctable_clear_tree (ProcData * const procdata)
{
    GtkTreeModel *model;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (procdata->tree));

    gtk_tree_store_clear (GTK_TREE_STORE (model));

    proctable_free_table (procdata);

    update_sensitivity(procdata);
}


void
proctable_free_table (ProcData * const procdata)
{
    for (ProcInfo::Iterator it(ProcInfo::begin()); it != ProcInfo::end(); ++it)
        delete it->second;

    ProcInfo::all.clear();
}



char*
make_loadavg_string(void)
{
    glibtop_loadavg buf;

    glibtop_get_loadavg(&buf);

    return g_strdup_printf(
        _("Load averages for the last 1, 5, 15 minutes: "
          "%0.2f, %0.2f, %0.2f"),
        buf.loadavg[0],
        buf.loadavg[1],
        buf.loadavg[2]);
}



void
ProcInfo::set_icon(Glib::RefPtr<Gdk::Pixbuf> icon)
{
  this->pixbuf = icon;

  GtkTreeModel *model;
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(ProcData::get_instance()->tree));
  gtk_tree_store_set(GTK_TREE_STORE(model), &this->node,
                     COL_PIXBUF, (this->pixbuf ? this->pixbuf->gobj() : NULL),
                    -1);
}
