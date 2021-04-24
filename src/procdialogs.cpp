/* Procman - dialogs
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

#include <glib/gi18n.h>

#include <signal.h>
#include <string.h>

#include "procdialogs.h"
#include "proctable.h"
#include "callbacks.h"
#include "prettytable.h"
#include "procactions.h"
#include "util.h"
#include "load-graph.h"
#include "settings-keys.h"
#include "procman_gksu.h"
#include "procman_pkexec.h"
#include "cgroups.h"

#define GET_WIDGET(x) GTK_WIDGET(gtk_builder_get_object(builder, x))

static GtkWidget *renice_dialog = NULL;
static GtkWidget *prefs_dialog = NULL;
static gint new_nice_value = 0;


static void
kill_dialog_button_pressed (GtkDialog *dialog, gint id, gpointer data)
{
    struct KillArgs *kargs = static_cast<KillArgs*>(data);

    gtk_widget_destroy (GTK_WIDGET (dialog));

    if (id == GTK_RESPONSE_OK)
        kill_process (kargs->procdata, kargs->signal);

    g_free (kargs);
}

void
procdialog_create_kill_dialog (ProcData *procdata, int signal)
{
    GtkWidget *kill_alert_dialog;
    gchar *primary, *secondary, *button_text;
    struct KillArgs *kargs;

    kargs = g_new(KillArgs, 1);
    kargs->procdata = procdata;
    kargs->signal = signal;
    gint selected_count = gtk_tree_selection_count_selected_rows (procdata->selection);

    if ( selected_count == 1 ) {
        ProcInfo *selected_process = NULL;
        // get the last selected row
        gtk_tree_selection_selected_foreach (procdata->selection, get_last_selected,
                                         &selected_process);
        if (signal == SIGKILL) {
            /*xgettext: primary alert message for killing single process*/
            primary = g_strdup_printf (_("Are you sure you want to kill the selected process “%s” (PID: %u)?"),
                                       selected_process->name,
                                       selected_process->pid);
        } else {
            /*xgettext: primary alert message for ending single process*/
            primary = g_strdup_printf (_("Are you sure you want to end the selected process “%s” (PID: %u)?"),
                                       selected_process->name,
                                       selected_process->pid);
        }
    } else {
        if (signal == SIGKILL) {
            /*xgettext: primary alert message for killing multiple processes*/
            primary = g_strdup_printf (_("Are you sure you want to kill the %d selected processes?"),
                                       selected_count);
        } else {
            /*xgettext: primary alert message for ending multiple processes*/
            primary = g_strdup_printf (_("Are you sure you want to end the %d selected processes?"),
                                       selected_count);

        }
    }

    if ( signal == SIGKILL ) {
        /*xgettext: secondary alert message*/
        secondary = _("Killing a process may destroy data, break the "
                    "session or introduce a security risk. "
                    "Only unresponsive processes should be killed.");
        button_text = ngettext("_Kill Process", "_Kill Processes", selected_count);
    } else {
        /*xgettext: secondary alert message*/
        secondary = _("Ending a process may destroy data, break the "
                    "session or introduce a security risk. "
                    "Only unresponsive processes should be ended.");
        button_text = ngettext("_End Process", "_End Processes", selected_count);
    }

    kill_alert_dialog = gtk_message_dialog_new (GTK_WINDOW (procdata->app),
                                                static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                                GTK_MESSAGE_WARNING,
                                                GTK_BUTTONS_NONE,
                                                "%s",
                                                primary);
    g_free (primary);

    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (kill_alert_dialog),
                                              "%s",
                                              secondary);

    gtk_dialog_add_buttons (GTK_DIALOG (kill_alert_dialog),
                            "gtk-cancel", GTK_RESPONSE_CANCEL,
                            button_text, GTK_RESPONSE_OK,
                            NULL);

    gtk_dialog_set_default_response (GTK_DIALOG (kill_alert_dialog),
                                     GTK_RESPONSE_CANCEL);

    g_signal_connect (G_OBJECT (kill_alert_dialog), "response",
                      G_CALLBACK (kill_dialog_button_pressed), kargs);

    gtk_widget_show_all (kill_alert_dialog);
}

static void
renice_scale_changed (GtkAdjustment *adj, gpointer data)
{
    GtkWidget *label = GTK_WIDGET (data);

    new_nice_value = int(gtk_adjustment_get_value (adj));
    gchar* text = g_strdup_printf(_("(%s Priority)"), procman::get_nice_level (new_nice_value));
    gtk_label_set_text (GTK_LABEL (label), text);
    g_free(text);

}

static void
renice_dialog_button_pressed (GtkDialog *dialog, gint id, gpointer data)
{
    ProcData *procdata = static_cast<ProcData*>(data);

    if (id == 100) {
        if (new_nice_value == -100)
            return;
        renice(procdata, new_nice_value);
    }

    gtk_widget_destroy (GTK_WIDGET (dialog));
    renice_dialog = NULL;
}

void
procdialog_create_renice_dialog (ProcData *procdata)
{
    ProcInfo  *info;
    GtkWidget *dialog = NULL;
    GtkWidget *dialog_vbox;
    GtkWidget *vbox;
    GtkWidget *label;
    GtkWidget *priority_label;
    GtkWidget *grid;
    GtkAdjustment *renice_adj;
    GtkWidget *hscale;
    GtkWidget *button;
    GtkWidget *icon;
    gchar     *text;
    gchar     *dialog_title;

    if (renice_dialog)
        return;

    gtk_tree_selection_selected_foreach (procdata->selection, get_last_selected,
                                         &info);
    gint selected_count = gtk_tree_selection_count_selected_rows (procdata->selection);

    if (!info)
        return;

    if ( selected_count == 1 ) {
        dialog_title = g_strdup_printf (_("Change Priority of Process “%s” (PID: %u)"),
                                        info->name, info->pid);
    } else {
        dialog_title = g_strdup_printf (_("Change Priority of %d Selected Processes"),
                                        selected_count);
    }

    dialog = gtk_dialog_new_with_buttons (dialog_title, NULL,
                                          GTK_DIALOG_DESTROY_WITH_PARENT,
                                          "gtk-cancel", GTK_RESPONSE_CANCEL,
                                          NULL);
    g_free (dialog_title);

    renice_dialog = dialog;
    gtk_window_set_resizable (GTK_WINDOW (renice_dialog), FALSE);
    gtk_container_set_border_width (GTK_CONTAINER (renice_dialog), 5);

    button = gtk_button_new_with_mnemonic (_("Change _Priority"));
    gtk_widget_set_can_default (button, TRUE);

    icon = gtk_image_new_from_icon_name ("gtk-apply", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image (GTK_BUTTON (button), icon);

    gtk_dialog_add_action_widget (GTK_DIALOG (renice_dialog), button, 100);
    gtk_dialog_set_default_response (GTK_DIALOG (renice_dialog), 100);
    new_nice_value = -100;

    dialog_vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    gtk_box_set_spacing (GTK_BOX (dialog_vbox), 2);
    gtk_container_set_border_width (GTK_CONTAINER (dialog_vbox), 5);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), vbox, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);

    grid = gtk_grid_new ();
    gtk_grid_set_column_spacing (GTK_GRID(grid), 12);
    gtk_grid_set_row_spacing (GTK_GRID(grid), 6);
    gtk_box_pack_start (GTK_BOX (vbox), grid, TRUE, TRUE, 0);

    label = gtk_label_new_with_mnemonic (_("_Nice value:"));
    gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 2);

    renice_adj = gtk_adjustment_new (info->nice, RENICE_VAL_MIN, RENICE_VAL_MAX, 1, 1, 0);
    new_nice_value = 0;
    hscale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, renice_adj);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), hscale);
    gtk_scale_set_digits (GTK_SCALE (hscale), 0);
    gtk_widget_set_hexpand (hscale, TRUE);
    gtk_grid_attach (GTK_GRID (grid), hscale, 1, 0, 1, 1);
    text = g_strdup_printf(_("(%s Priority)"), procman::get_nice_level (info->nice));
    priority_label = gtk_label_new (text);
    gtk_grid_attach (GTK_GRID (grid), priority_label, 1, 1, 1, 1);
    g_free(text);

    text = g_strconcat("<small><i><b>", _("Note:"), "</b> ",
        _("The priority of a process is given by its nice value. A lower nice value corresponds to a higher priority."),
        "</i></small>", NULL);
    label = gtk_label_new (_(text));
    gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
    gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
    gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
    g_free (text);

    g_signal_connect (G_OBJECT (dialog), "response",
                      G_CALLBACK (renice_dialog_button_pressed), procdata);
    g_signal_connect (G_OBJECT (renice_adj), "value_changed",
                      G_CALLBACK (renice_scale_changed), priority_label);

    gtk_widget_show_all (dialog);


}

static void
prefs_dialog_button_pressed (GtkDialog *dialog, gint id, gpointer data)
{
    if (id == GTK_RESPONSE_HELP)
    {
        GError* error = 0;
        if (!g_app_info_launch_default_for_uri("help:mate-system-monitor/mate-system-monitor-prefs", NULL, &error))
        {
            g_warning("Could not display preferences help : %s", error->message);
            g_error_free(error);
        }
    }
    else
    {
        gtk_widget_destroy (GTK_WIDGET (dialog));
        prefs_dialog = NULL;
    }
}


class SpinButtonUpdater
{
public:
    SpinButtonUpdater(const string& key)
        : key(key)
    { }

    static gboolean callback(GtkWidget *widget, GdkEventFocus *event, gpointer data)
    {
        SpinButtonUpdater* updater = static_cast<SpinButtonUpdater*>(data);
        gtk_spin_button_update(GTK_SPIN_BUTTON(widget));
        updater->update(GTK_SPIN_BUTTON(widget));
        return FALSE;
    }

private:

    void update(GtkSpinButton* spin)
    {
        int new_value = int(1000 * gtk_spin_button_get_value(spin));
        g_settings_set_int(ProcData::get_instance()->settings,
                           this->key.c_str(), new_value);

        procman_debug("set %s to %d", this->key.c_str(), new_value);
    }

    const string key;
};


static void
field_toggled (const gchar *child_schema, GtkCellRendererToggle *cell, gchar *path_str, gpointer data)
{
    GtkTreeModel *model = static_cast<GtkTreeModel*>(data);
    GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
    GtkTreeIter iter;
    GtkTreeViewColumn *column;
    gboolean toggled;
    GSettings *settings = g_settings_get_child (ProcData::get_instance()->settings, child_schema);
    gchar *key;
    int id;

    if (!path)
        return;

    gtk_tree_model_get_iter (model, &iter, path);

    gtk_tree_model_get (model, &iter, 2, &column, -1);
    toggled = gtk_cell_renderer_toggle_get_active (cell);

    gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, !toggled, -1);
    gtk_tree_view_column_set_visible (column, !toggled);

    id = gtk_tree_view_column_get_sort_column_id (column);

    key = g_strdup_printf ("col-%d-visible", id);
    g_settings_set_boolean (settings, key, !toggled);
    g_free (key);

    gtk_tree_path_free (path);
}

static void
proc_field_toggled (GtkCellRendererToggle *cell, gchar *path_str, gpointer data)
{
    field_toggled ("proctree", cell, path_str, data);
}

static void
disk_field_toggled (GtkCellRendererToggle *cell, gchar *path_str, gpointer data)
{
    field_toggled ("disktreenew", cell, path_str, data);
}

static void
create_field_page(GtkBuilder* builder, GtkWidget *tree, const gchar *widgetname)
{
    GtkTreeView *treeview;
    GList *it, *columns;
    GtkListStore *model;
    GtkTreeViewColumn *column;
    GtkCellRenderer *cell;
    gchar *full_widgetname;

    full_widgetname = g_strdup_printf ("%s_columns", widgetname);
    treeview = GTK_TREE_VIEW (gtk_builder_get_object (builder, full_widgetname));
    g_free (full_widgetname);

    model = gtk_list_store_new (3, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER);

    gtk_tree_view_set_model (GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(model));
    g_object_unref (G_OBJECT (model));

    column = gtk_tree_view_column_new ();

    cell = gtk_cell_renderer_toggle_new ();
    gtk_tree_view_column_pack_start (column, cell, FALSE);
    gtk_tree_view_column_set_attributes (column, cell,
                                         "active", 0,
                                         NULL);

    if (g_strcmp0 (widgetname, "proctree") == 0)
        g_signal_connect (G_OBJECT (cell), "toggled", G_CALLBACK (proc_field_toggled), model);
    else if (g_strcmp0 (widgetname, "disktreenew") == 0)
        g_signal_connect (G_OBJECT (cell), "toggled", G_CALLBACK (disk_field_toggled), model);

    gtk_tree_view_column_set_clickable (column, TRUE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    column = gtk_tree_view_column_new ();

    cell = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, cell, FALSE);
    gtk_tree_view_column_set_attributes (column, cell,
                                         "text", 1,
                                         NULL);

    gtk_tree_view_column_set_title (column, "Not Shown");
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    columns = gtk_tree_view_get_columns (GTK_TREE_VIEW (tree));

    for(it = columns; it; it = it->next)
    {
        GtkTreeViewColumn *column = static_cast<GtkTreeViewColumn*>(it->data);
        GtkTreeIter iter;
        const gchar *title;
        gboolean visible;
        gint column_id;

        title = gtk_tree_view_column_get_title (column);
        if (!title)
            title = _("Icon");

        column_id = gtk_tree_view_column_get_sort_column_id(column);
        if ((column_id == COL_CGROUP) && (!cgroups_enabled()))
            continue;

        if ((column_id == COL_UNIT ||
             column_id == COL_SESSION ||
             column_id == COL_SEAT ||
             column_id == COL_OWNER)
#ifdef HAVE_SYSTEMD
            && !LOGIND_RUNNING()
#endif
                )
            continue;

        visible = gtk_tree_view_column_get_visible (column);

        gtk_list_store_append (model, &iter);
        gtk_list_store_set (model, &iter, 0, visible, 1, title, 2, column,-1);
    }

    g_list_free(columns);
}

void
procdialog_create_preferences_dialog (ProcData *procdata)
{
    typedef SpinButtonUpdater SBU;

    static SBU interval_updater("update-interval");
    static SBU graph_interval_updater("graph-update-interval");
    static SBU disks_interval_updater("disks-interval");

    GtkWidget *notebook;
    GtkAdjustment *adjustment;
    GtkWidget *spin_button;
    GtkWidget *check_button;
    GtkWidget *smooth_button;
    GtkBuilder *builder;
    gfloat update;

    if (prefs_dialog)
        return;

    builder = gtk_builder_new_from_resource("/org/mate/system-monitor/preferences.ui");

    prefs_dialog = GET_WIDGET("preferences_dialog");
    notebook = GET_WIDGET("notebook");
    spin_button = GET_WIDGET("processes_interval_spinner");

    update = (gfloat) procdata->config.update_interval;
    adjustment = (GtkAdjustment *) gtk_adjustment_new(update / 1000.0,
                                   MIN_UPDATE_INTERVAL / 1000,
                                   MAX_UPDATE_INTERVAL / 1000,
                                   0.25,
                                   1.0,
                                   0);

    gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON(spin_button), adjustment);
    g_signal_connect (G_OBJECT (spin_button), "focus_out_event",
                      G_CALLBACK (SBU::callback), &interval_updater);

    smooth_button = GET_WIDGET("smooth_button");
    g_settings_bind(procdata->settings, SmoothRefresh::KEY.c_str(), smooth_button, "active", G_SETTINGS_BIND_DEFAULT);

    check_button = GET_WIDGET("check_button");
    g_settings_bind(procdata->settings, "kill-dialog", check_button, "active", G_SETTINGS_BIND_DEFAULT);

    GtkWidget *solaris_button = GET_WIDGET("solaris_button");
    g_settings_bind(procdata->settings, procman::settings::solaris_mode.c_str(), solaris_button, "active", G_SETTINGS_BIND_DEFAULT);

    create_field_page (builder, procdata->tree, "proctree");

    update = (gfloat) procdata->config.graph_update_interval;
    adjustment = (GtkAdjustment *) gtk_adjustment_new(update / 1000.0, 0.25,
                                                      100.0, 0.25, 1.0, 0);

    spin_button = GET_WIDGET("resources_interval_spinner");
    gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON(spin_button), adjustment);

    g_signal_connect (G_OBJECT (spin_button), "focus_out_event",
                      G_CALLBACK(SBU::callback),
                      &graph_interval_updater);

    GtkWidget *bits_button = GET_WIDGET("bits_button");
    g_settings_bind(procdata->settings, procman::settings::network_in_bits.c_str(), bits_button, "active", G_SETTINGS_BIND_DEFAULT);


    update = (gfloat) procdata->config.disks_update_interval;
    adjustment = (GtkAdjustment *) gtk_adjustment_new (update / 1000.0, 1.0,
                                                       100.0, 1.0, 1.0, 0);

    spin_button = GET_WIDGET("devices_interval_spinner");
    gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON(spin_button), adjustment);
    g_signal_connect (G_OBJECT (spin_button), "focus_out_event",
                      G_CALLBACK(SBU::callback),
                      &disks_interval_updater);

    check_button = GET_WIDGET("all_devices_check");
    g_settings_bind(procdata->settings, "show-all-fs", check_button, "active", G_SETTINGS_BIND_DEFAULT);

    create_field_page (builder, procdata->disk_list, "disktreenew");

    gtk_widget_show_all (prefs_dialog);
    g_signal_connect (G_OBJECT (prefs_dialog), "response",
                       G_CALLBACK (prefs_dialog_button_pressed), procdata);

    switch (procdata->config.current_tab) {
    case PROCMAN_TAB_SYSINFO:
    case PROCMAN_TAB_PROCESSES:
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);
        break;
    case PROCMAN_TAB_RESOURCES:
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 1);
        break;
    case PROCMAN_TAB_DISKS:
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 2);
        break;
    }

    gtk_builder_connect_signals (builder, NULL);
    g_object_unref (G_OBJECT (builder));
}


static char *
procman_action_to_command(ProcmanActionType type,
                          gint pid,
                          gint extra_value)
{
    switch (type) {
    case PROCMAN_ACTION_KILL:
           return g_strdup_printf("kill -s %d %d", extra_value, pid);
    case PROCMAN_ACTION_RENICE:
        return g_strdup_printf("renice %d %d", extra_value, pid);
    default:
        g_assert_not_reached();
    }
}


/*
 * type determines whether if dialog is for killing process or renice.
 * type == PROCMAN_ACTION_KILL,   extra_value -> signal to send
 * type == PROCMAN_ACTION_RENICE, extra_value -> new priority.
 */
gboolean
procdialog_create_root_password_dialog(ProcmanActionType type,
                                       ProcData *procdata,
                                       gint pid,
                                       gint extra_value)
{
    char * command;
    gboolean ret = FALSE;

    command = procman_action_to_command(type, pid, extra_value);

    procman_debug("Trying to run '%s' as root", command);

    if (procman_has_pkexec())
        ret = procman_pkexec_create_root_password_dialog(command);
    else if (procman_has_gksu())
        ret = procman_gksu_create_root_password_dialog(command);

    g_free(command);
    return ret;
}
