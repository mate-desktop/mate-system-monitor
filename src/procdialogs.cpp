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

    icon = gtk_image_new_from_stock ("gtk-ok", GTK_ICON_SIZE_BUTTON);
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


static void
show_kill_dialog_toggled (GtkToggleButton *button, gpointer data)
{
    ProcData *procdata = static_cast<ProcData*>(data);
    GSettings *settings = procdata->settings;

    gboolean toggled;

    toggled = gtk_toggle_button_get_active (button);

    g_settings_set_boolean (settings, "kill-dialog", toggled);

}



static void
solaris_mode_toggled(GtkToggleButton *button, gpointer data)
{
    ProcData *procdata = static_cast<ProcData*>(data);
    GSettings *settings = procdata->settings;
    gboolean toggled;
    toggled = gtk_toggle_button_get_active(button);
    g_settings_set_boolean(settings, procman::settings::solaris_mode.c_str(), toggled);
}


static void
network_in_bits_toggled(GtkToggleButton *button, gpointer data)
{
    ProcData *procdata = static_cast<ProcData*>(data);
    GSettings *settings = procdata->settings;
    gboolean toggled;
    toggled = gtk_toggle_button_get_active(button);
    g_settings_set_boolean(settings, procman::settings::network_in_bits.c_str(), toggled);
}



static void
smooth_refresh_toggled(GtkToggleButton *button, gpointer data)
{
    ProcData *procdata = static_cast<ProcData*>(data);
    GSettings *settings = procdata->settings;

    gboolean toggled;

    toggled = gtk_toggle_button_get_active(button);

    g_settings_set_boolean(settings, SmoothRefresh::KEY.c_str(), toggled);
}



static void
show_all_fs_toggled (GtkToggleButton *button, gpointer data)
{
    ProcData *procdata = static_cast<ProcData*>(data);
    GSettings *settings = procdata->settings;

    gboolean toggled;

    toggled = gtk_toggle_button_get_active (button);

    g_settings_set_boolean (settings, "show-all-fs", toggled);
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

static GtkWidget *
create_field_page(GtkWidget *tree, const gchar *child_schema, const gchar *text)
{
    GtkWidget *vbox;
    GtkWidget *scrolled;
    GtkWidget *label;
    GtkWidget *treeview;
    GList *it, *columns;
    GtkListStore *model;
    GtkTreeViewColumn *column;
    GtkCellRenderer *cell;

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

    label = gtk_label_new_with_mnemonic (text);
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);

    scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled), GTK_SHADOW_IN);
    gtk_box_pack_start (GTK_BOX (vbox), scrolled, TRUE, TRUE, 0);

    model = gtk_list_store_new (3, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER);

    treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
    gtk_container_add (GTK_CONTAINER (scrolled), treeview);
    g_object_unref (G_OBJECT (model));
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), treeview);

    column = gtk_tree_view_column_new ();

    cell = gtk_cell_renderer_toggle_new ();
    gtk_tree_view_column_pack_start (column, cell, FALSE);
    gtk_tree_view_column_set_attributes (column, cell,
                                         "active", 0,
                                         NULL);
    if (g_strcmp0 (child_schema, "proctree") == 0)
        g_signal_connect (G_OBJECT (cell), "toggled", G_CALLBACK (proc_field_toggled), model);
    else if (g_strcmp0 (child_schema, "disktreenew") == 0)
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
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);

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

    return vbox;
}

void
procdialog_create_preferences_dialog (ProcData *procdata)
{
    static GtkWidget *dialog = NULL;

    typedef SpinButtonUpdater SBU;

    static SBU interval_updater("update-interval");
    static SBU graph_interval_updater("graph-update-interval");
    static SBU disks_interval_updater("disks-interval");

    GtkWidget *notebook;
    GtkWidget *proc_box;
    GtkWidget *sys_box;
    GtkWidget *main_vbox;
    GtkWidget *vbox, *vbox2, *vbox3;
    GtkWidget *hbox, *hbox2, *hbox3;
    GtkWidget *label;
    GtkAdjustment *adjustment;
    GtkWidget *spin_button;
    GtkWidget *check_button;
    GtkWidget *tab_label;
    GtkWidget *smooth_button;
    GtkSizeGroup *size;
    gfloat update;
    gchar *tmp;

    if (prefs_dialog)
        return;

    size = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

    dialog = gtk_dialog_new_with_buttons (_("System Monitor Preferences"),
                                          GTK_WINDOW (procdata->app),
                                          GTK_DIALOG_DESTROY_WITH_PARENT,
                                          "gtk-help", GTK_RESPONSE_HELP,
                                          "gtk-close", GTK_RESPONSE_CLOSE,
                                          NULL);
    /* FIXME: we should not declare the window size, but let it's   */
    /* driven by window childs. The problem is that the fields list */
    /* have to show at least 4 items to respect HIG. I don't know   */
    /* any function to set list height by contents/items inside it. */
    gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 500);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    prefs_dialog = dialog;

    main_vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    gtk_box_set_spacing (GTK_BOX (main_vbox), 2);

    notebook = gtk_notebook_new ();
    gtk_container_set_border_width (GTK_CONTAINER (notebook), 5);
    gtk_box_pack_start (GTK_BOX (main_vbox), notebook, TRUE, TRUE, 0);

    proc_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 18);
    gtk_container_set_border_width (GTK_CONTAINER (proc_box), 12);
    tab_label = gtk_label_new (_("Processes"));
    gtk_widget_show (tab_label);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), proc_box, tab_label);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start (GTK_BOX (proc_box), vbox, FALSE, FALSE, 0);

    tmp = g_strdup_printf ("<b>%s</b>", _("Behavior"));
    label = gtk_label_new (NULL);
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_label_set_markup (GTK_LABEL (label), tmp);
    g_free (tmp);
    gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new ("    ");
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

    vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 0);

    hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start (GTK_BOX (vbox2), hbox2, FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic (_("_Update interval in seconds:"));
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, FALSE, 0);

    hbox3 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start (GTK_BOX (hbox2), hbox3, TRUE, TRUE, 0);

    update = (gfloat) procdata->config.update_interval;
    adjustment = (GtkAdjustment *) gtk_adjustment_new(update / 1000.0,
                                   MIN_UPDATE_INTERVAL / 1000,
                                   MAX_UPDATE_INTERVAL / 1000,
                                   0.25,
                                   1.0,
                                   0);

    spin_button = gtk_spin_button_new (adjustment, 1.0, 2);
    gtk_box_pack_start (GTK_BOX (hbox3), spin_button, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (spin_button), "focus_out_event",
                      G_CALLBACK (SBU::callback), &interval_updater);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), spin_button);


    hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox2, FALSE, FALSE, 0);

    smooth_button = gtk_check_button_new_with_mnemonic(_("Enable _smooth refresh"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(smooth_button),
                                 g_settings_get_boolean(procdata->settings,
                                                        SmoothRefresh::KEY.c_str()));
    g_signal_connect(G_OBJECT(smooth_button), "toggled",
                     G_CALLBACK(smooth_refresh_toggled), procdata);
    gtk_box_pack_start(GTK_BOX(hbox2), smooth_button, TRUE, TRUE, 0);



    hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start (GTK_BOX (vbox2), hbox2, FALSE, FALSE, 0);

    check_button = gtk_check_button_new_with_mnemonic (_("Alert before ending or _killing processes"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                  procdata->config.show_kill_warning);
    g_signal_connect (G_OBJECT (check_button), "toggled",
                                G_CALLBACK (show_kill_dialog_toggled), procdata);
    gtk_box_pack_start (GTK_BOX (hbox2), check_button, FALSE, FALSE, 0);




    hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox2, FALSE, FALSE, 0);

    GtkWidget *solaris_button = gtk_check_button_new_with_mnemonic(_("Divide CPU usage by CPU count"));
    gtk_widget_set_tooltip_text(solaris_button, _("Solaris mode"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(solaris_button),
                                 g_settings_get_boolean(procdata->settings,
                                                        procman::settings::solaris_mode.c_str()));
    g_signal_connect(G_OBJECT(solaris_button), "toggled",
                     G_CALLBACK(solaris_mode_toggled), procdata);
    gtk_box_pack_start(GTK_BOX(hbox2), solaris_button, TRUE, TRUE, 0);




    hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start (GTK_BOX (vbox2), hbox2, FALSE, FALSE, 0);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start (GTK_BOX (proc_box), vbox, TRUE, TRUE, 0);

    tmp = g_strdup_printf ("<b>%s</b>", _("Information Fields"));
    label = gtk_label_new (NULL);
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_label_set_markup (GTK_LABEL (label), tmp);
    g_free (tmp);
    gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

    label = gtk_label_new ("    ");
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

    vbox2 = create_field_page (procdata->tree, "proctree", _("Process i_nformation shown in list:"));
    gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 0);

    sys_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width (GTK_CONTAINER (sys_box), 12);
    tab_label = gtk_label_new (_("Resources"));
    gtk_widget_show (tab_label);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), sys_box, tab_label);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start (GTK_BOX (sys_box), vbox, FALSE, FALSE, 0);

    tmp = g_strdup_printf ("<b>%s</b>", _("Graphs"));
    label = gtk_label_new (NULL);
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_label_set_markup (GTK_LABEL (label), tmp);
    g_free (tmp);
    gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, FALSE, 0);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new ("    ");
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

    vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 0);

    hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start (GTK_BOX (vbox2), hbox2, FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic (_("_Update interval in 1/10 sec:"));
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, FALSE, 0);
    gtk_size_group_add_widget (size, label);

    hbox3 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start (GTK_BOX (hbox2), hbox3, TRUE, TRUE, 0);

    update = (gfloat) procdata->config.graph_update_interval;
    adjustment = (GtkAdjustment *) gtk_adjustment_new(update / 1000.0, 0.25,
                                                      100.0, 0.25, 1.0, 0);
    spin_button = gtk_spin_button_new (adjustment, 1.0, 2);
    g_signal_connect (G_OBJECT (spin_button), "focus_out_event",
                      G_CALLBACK(SBU::callback),
                      &graph_interval_updater);
    gtk_box_pack_start (GTK_BOX (hbox3), spin_button, FALSE, FALSE, 0);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), spin_button);


    GtkWidget *bits_button;
    bits_button = gtk_check_button_new_with_mnemonic(_("Show network speed in bits"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bits_button),
                                 g_settings_get_boolean(procdata->settings,
                                                        procman::settings::network_in_bits.c_str()));

    g_signal_connect(G_OBJECT(bits_button), "toggled",
                     G_CALLBACK(network_in_bits_toggled), procdata);
    gtk_box_pack_start(GTK_BOX(vbox2), bits_button, TRUE, TRUE, 0);



    hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start (GTK_BOX (vbox2), hbox2, TRUE, TRUE, 0);

    /*
     * Devices
     */
    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
    tab_label = gtk_label_new (_("File Systems"));
    gtk_widget_show (tab_label);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), vbox, tab_label);

    tmp = g_strdup_printf ("<b>%s</b>", _("File Systems"));
    label = gtk_label_new (NULL);
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_label_set_markup (GTK_LABEL (label), tmp);
    g_free (tmp);
    gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new ("    ");
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

    vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 0);

    hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start (GTK_BOX (vbox2), hbox2, FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic (_("_Update interval in seconds:"));
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, FALSE, 0);

    hbox3 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start (GTK_BOX (hbox2), hbox3, TRUE, TRUE, 0);

    update = (gfloat) procdata->config.disks_update_interval;
    adjustment = (GtkAdjustment *) gtk_adjustment_new (update / 1000.0, 1.0,
                                                       100.0, 1.0, 1.0, 0);
    spin_button = gtk_spin_button_new (adjustment, 1.0, 0);
    gtk_box_pack_start (GTK_BOX (hbox3), spin_button, FALSE, FALSE, 0);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), spin_button);
    g_signal_connect (G_OBJECT (spin_button), "focus_out_event",
                      G_CALLBACK(SBU::callback),
                      &disks_interval_updater);


    hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start (GTK_BOX (vbox2), hbox2, FALSE, FALSE, 0);
    check_button = gtk_check_button_new_with_mnemonic (_("Show _all file systems"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
                                  procdata->config.show_all_fs);
    g_signal_connect (G_OBJECT (check_button), "toggled",
                      G_CALLBACK (show_all_fs_toggled), procdata);
    gtk_box_pack_start (GTK_BOX (hbox2), check_button, FALSE, FALSE, 0);


    vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start (GTK_BOX (vbox), vbox2, FALSE, FALSE, 0);

    label = gtk_label_new ("    ");
    gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, FALSE, 0);

    tmp = g_strdup_printf ("<b>%s</b>", _("Information Fields"));
    label = gtk_label_new (NULL);
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_label_set_markup (GTK_LABEL (label), tmp);
    g_free (tmp);
    gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

    label = gtk_label_new ("    ");
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

    vbox3 = create_field_page (procdata->disk_list, "disktreenew", _("File system i_nformation shown in list:"));
    gtk_box_pack_start (GTK_BOX (hbox), vbox3, TRUE, TRUE, 0);

    gtk_widget_show_all (dialog);
    g_signal_connect (G_OBJECT (dialog), "response",
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
