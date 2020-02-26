/* Procman - callbacks
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

#include <giomm.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <signal.h>

#include "callbacks.h"
#include "interface.h"
#include "proctable.h"
#include "util.h"
#include "procactions.h"
#include "procman.h"
#include "procdialogs.h"
#include "memmaps.h"
#include "openfiles.h"
#include "procproperties.h"
#include "load-graph.h"
#include "disks.h"
#include "lsof.h"
#include "sysinfo.h"

void
cb_kill_sigstop(GtkAction *action, gpointer data)
{
    ProcData * const procdata = static_cast<ProcData*>(data);

    /* no confirmation */
    kill_process (procdata, SIGSTOP);
}




void
cb_kill_sigcont(GtkAction *action, gpointer data)
{
    ProcData * const procdata = static_cast<ProcData*>(data);

    /* no confirmation */
    kill_process (procdata, SIGCONT);
}




static void
kill_process_helper(ProcData *procdata, int sig)
{
    if (procdata->config.show_kill_warning)
        procdialog_create_kill_dialog (procdata, sig);
    else
        kill_process (procdata, sig);
}


void
cb_edit_preferences (GtkAction *action, gpointer data)
{
    ProcData * const procdata = static_cast<ProcData*>(data);

    procdialog_create_preferences_dialog (procdata);
}


void
cb_renice (GtkAction *action, GtkRadioAction *current, gpointer data)
{
    ProcData * const procdata = static_cast<ProcData*>(data);

    gint selected = gtk_radio_action_get_current_value(current);

    if (selected == CUSTOM_PRIORITY)
    {
       procdialog_create_renice_dialog (procdata);
    } else {
       gint new_nice_value = 0;
       switch (selected) {
           case VERY_HIGH_PRIORITY: new_nice_value = -20; break;
           case HIGH_PRIORITY: new_nice_value = -5; break;
           case NORMAL_PRIORITY: new_nice_value = 0; break;
           case LOW_PRIORITY: new_nice_value = 5; break;
           case VERY_LOW_PRIORITY: new_nice_value = 19; break;
       }
       renice(procdata, new_nice_value);
    }
}


void
cb_end_process (GtkAction *action, gpointer data)
{
    kill_process_helper(static_cast<ProcData*>(data), SIGTERM);
}


void
cb_kill_process (GtkAction *action, gpointer data)
{
    kill_process_helper(static_cast<ProcData*>(data), SIGKILL);
}


void
cb_show_memory_maps (GtkAction *action, gpointer data)
{
    ProcData * const procdata = static_cast<ProcData*>(data);

    create_memmaps_dialog (procdata);
}

void
cb_show_open_files (GtkAction *action, gpointer data)
{
    ProcData *procdata = static_cast<ProcData*>(data);

    create_openfiles_dialog (procdata);
}

void
cb_show_process_properties (GtkAction *action, gpointer data)
{
    ProcData *procdata = static_cast<ProcData*>(data);
    create_procproperties_dialog (procdata);
}

void
cb_show_lsof(GtkAction *action, gpointer data)
{
    ProcData *procdata = static_cast<ProcData*>(data);
    procman_lsof(procdata);
}


void
cb_about (GtkAction *action, gpointer data)
{
    ProcData *procdata = static_cast<ProcData*>(data);

    const gchar * const authors[] = {
        "Adam Erdman <hekel@archlinux.info>",
        "Alexander Pyhalov <apyhalov@gmail.com>",
        "Andreas Nilsson <nisses.mail@home.se>",
        "Benoît Dejean <bdejean@gmail.com>",
        "Chris Kühl <chrisk@openismus.com>",
        "Clement Lefebvre <clement.lefebvre@linuxmint.com>",
        "Elias Aebi <user142@hotmail.com>",
        "Erik Johnsson",
        "Jorgen Scheibengruber",
        "Karl Lattimer",
        "Kevin Vandersloot",
        "Laurent Napias <tamplan@free.fr>",
        "Marcel Dijkstra <marcel.dykstra@gmail.com>",
        "Martin Wimpress <martin@mate-desktop.org>",
        "Matias De lellis <mati86dl@gmail.com>",
        "Mike Gabriel <mike.gabriel@das-netzwerkteam.de>",
        "Nelson Marques <nmo.marques@gmail.com>",
        "Obata Akio https://github.com/obache",
        "Pablo Barciela <scow@riseup.net>",
        "Paolo Borelli",
        "Perberos <perberos@gmail.com>",
        "Piotr Drąg <piotrdrag@gmail.com>",
        "Robert Buj <robert.buj@gmail.com>",
        "Sander Sweers <infirit@gmail.com>",
        "Scott Balneaves <sbalneav@alburg.net>",
        "Soong Noonien https://github.com/SoongNoonien",
        "Stefano Karapetsas <stefano@karapetsas.com>",
        "Steve Zesch <stevezesch2@gmail.com>",
        "Victor Kareh <vkareh@redhat.com>",
        "Vlad Orlov <monsta@inbox.ru>",
        "Wolfgang Ulbrich <mate@raveit.de>",
        "Wu Xiaotian <yetist@gmail.com>",
        "Yaakov Selkowitz <yselkowitz@users.sourceforge.net>",
        "Youri Mouton <youri@NetBSD.org>",
        NULL
    };

    const gchar * const documenters[] = {
        "Bill Day",
        "Sun Microsystems",
        NULL
    };

    const gchar * const artists[] = {
        "Baptiste Mille-Mathias",
        NULL
    };

    const gchar * license[] = {
        N_("System Monitor is free software; you can redistribute it and/or modify "
        "it under the terms of the GNU General Public License as published by "
        "the Free Software Foundation; either version 2 of the License, or "
        "(at your option) any later version."),
        N_("System Monitor is distributed in the hope that it will be useful, "
        "but WITHOUT ANY WARRANTY; without even the implied warranty of "
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
        "GNU General Public License for more details."),
        N_("You should have received a copy of the GNU General Public License "
        "along with System Monitor; if not, write to the Free Software Foundation, Inc., "
        "51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA")
    };

    gchar *license_trans;
    license_trans = g_strjoin ("\n\n", _(license[0]), _(license[1]), _(license[2]), NULL);

    gtk_show_about_dialog (
        GTK_WINDOW (procdata->app),
        "program-name",       _("System Monitor"),
        "version",            VERSION,
        "title",              _("About System Monitor"),
        "comments",           _("View current processes and monitor system state"),
        "copyright",          _("Copyright \xc2\xa9 2001-2004 Kevin Vandersloot\n"
                                "Copyright \xc2\xa9 2005-2007 Benoît Dejean\n"
                                "Copyright \xc2\xa9 2011-2020 MATE developers"),
        "logo-icon-name",     "utilities-system-monitor",
        "authors",            authors,
        "artists",            artists,
        "documenters",        documenters,
        "translator-credits", _("translator-credits"),
        "license",            license_trans,
        "wrap-license",       TRUE,
        "website",            "https://mate-desktop.org",
        NULL
        );

    g_free (license_trans);
}


void
cb_help_contents (GtkAction *action, gpointer data)
{
    GError* error = 0;
    if (!g_app_info_launch_default_for_uri("help:mate-system-monitor", NULL, &error)) {
        g_warning("Could not display help : %s", error->message);
        g_error_free(error);
    }
}


void
cb_app_exit (GtkAction *action, gpointer data)
{
    ProcData * const procdata = static_cast<ProcData*>(data);

    cb_app_delete (NULL, NULL, procdata);
}


gboolean
cb_app_delete (GtkWidget *window, GdkEventAny *event, gpointer data)
{
    ProcData * const procdata = static_cast<ProcData*>(data);

    procman_save_config (procdata);
    if (procdata->timeout)
        g_source_remove (procdata->timeout);
    if (procdata->disk_timeout)
        g_source_remove (procdata->disk_timeout);

    gtk_main_quit ();

    return TRUE;
}



void
cb_end_process_button_pressed (GtkButton *button, gpointer data)
{
    kill_process_helper(static_cast<ProcData*>(data), SIGTERM);
}


static void change_settings_color(GSettings *settings, const char *key,
                   GSMColorButton *cp)
{
    GdkRGBA c;
    char *color;

    gsm_color_button_get_color(cp, &c);
    color = gdk_rgba_to_string (&c);
    g_settings_set_string (settings, key, color);
    g_free (color);
}


void
cb_cpu_color_changed (GSMColorButton *cp, gpointer data)
{
    char key[80];
    gint i = GPOINTER_TO_INT (data);
    GSettings *settings = g_settings_new (GSM_GSETTINGS_SCHEMA);

    g_snprintf(key, sizeof key, "cpu-color%d", i);

    change_settings_color(settings, key, cp);
}

void
cb_mem_color_changed (GSMColorButton *cp, gpointer data)
{
    ProcData * const procdata = static_cast<ProcData*>(data);
    change_settings_color(procdata->settings, "mem-color", cp);
}


void
cb_swap_color_changed (GSMColorButton *cp, gpointer data)
{
    ProcData * const procdata = static_cast<ProcData*>(data);
    change_settings_color(procdata->settings, "swap-color", cp);
}

void
cb_net_in_color_changed (GSMColorButton *cp, gpointer data)
{
    ProcData * const procdata = static_cast<ProcData*>(data);
    change_settings_color(procdata->settings, "net-in-color", cp);
}

void
cb_net_out_color_changed (GSMColorButton *cp, gpointer data)
{
    ProcData * const procdata = static_cast<ProcData*>(data);
    change_settings_color(procdata->settings, "net-out-color", cp);
}

static void
get_last_selected (GtkTreeModel *model, GtkTreePath *path,
           GtkTreeIter *iter, gpointer data)
{
    ProcInfo **info = static_cast<ProcInfo**>(data);

    gtk_tree_model_get (model, iter, COL_POINTER, info, -1);
}


void
cb_row_selected (GtkTreeSelection *selection, gpointer data)
{
    ProcData * const procdata = static_cast<ProcData*>(data);

    procdata->selection = selection;

    procdata->selected_process = NULL;

    /* get the most recent selected process and determine if there are
    ** no selected processes
    */
    gtk_tree_selection_selected_foreach (procdata->selection, get_last_selected,
                         &procdata->selected_process);
    if (procdata->selected_process) {
        gint value;
        gint nice = procdata->selected_process->nice;
        if (nice < -7)
            value = VERY_HIGH_PRIORITY;
        else if (nice < -2)
            value = HIGH_PRIORITY;
        else if (nice < 3)
            value = NORMAL_PRIORITY;
        else if (nice < 7)
            value = LOW_PRIORITY;
        else
            value = VERY_LOW_PRIORITY;

        GtkRadioAction* normal = GTK_RADIO_ACTION(gtk_action_group_get_action(procdata->action_group, "Normal"));
        block_priority_changed_handlers(procdata, TRUE);
        gtk_radio_action_set_current_value(normal, value);
        block_priority_changed_handlers(procdata, FALSE);

    }
    update_sensitivity(procdata);
}


gboolean
cb_tree_button_pressed (GtkWidget *widget,
            GdkEventButton *event,
            gpointer data)
{
    ProcData * const procdata = static_cast<ProcData*>(data);

    if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
        do_popup_menu (procdata, event);

    return FALSE;
}


gboolean
cb_tree_popup_menu (GtkWidget *widget, gpointer data)
{
    ProcData * const procdata = static_cast<ProcData*>(data);

    do_popup_menu (procdata, NULL);

    return TRUE;
}


void
cb_switch_page (GtkNotebook *nb, GtkWidget *page,
        gint num, gpointer data)
{
    cb_change_current_page (nb, num, data);
}

void
cb_change_current_page (GtkNotebook *nb, gint num, gpointer data)
{
    ProcData * const procdata = static_cast<ProcData*>(data);

    procdata->config.current_tab = num;


    if (num == PROCMAN_TAB_PROCESSES) {

        cb_timeout (procdata);

        if (!procdata->timeout)
            procdata->timeout = g_timeout_add (
                procdata->config.update_interval,
                cb_timeout, procdata);

        update_sensitivity(procdata);
    }
    else {
        if (procdata->timeout) {
            g_source_remove (procdata->timeout);
            procdata->timeout = 0;
        }

        update_sensitivity(procdata);
    }


    if (num == PROCMAN_TAB_RESOURCES) {
        load_graph_start (procdata->cpu_graph);
        load_graph_start (procdata->mem_graph);
        load_graph_start (procdata->net_graph);
    }
    else {
        load_graph_stop (procdata->cpu_graph);
        load_graph_stop (procdata->mem_graph);
        load_graph_stop (procdata->net_graph);
    }


    if (num == PROCMAN_TAB_DISKS) {

        cb_update_disks (procdata);

        if(!procdata->disk_timeout) {
            procdata->disk_timeout =
                g_timeout_add (procdata->config.disks_update_interval,
                           cb_update_disks,
                           procdata);
        }
    }
    else {
        if(procdata->disk_timeout) {
            g_source_remove (procdata->disk_timeout);
            procdata->disk_timeout = 0;
        }
    }

    if (num == PROCMAN_TAB_SYSINFO) {
        procman::build_sysinfo_ui();
    }
}



gint
cb_user_refresh (GtkAction*, gpointer data)
{
    ProcData * const procdata = static_cast<ProcData*>(data);
    proctable_update (procdata);
    return FALSE;
}


gint
cb_timeout (gpointer data)
{
    ProcData * const procdata = static_cast<ProcData*>(data);
    guint new_interval;

    proctable_update (procdata);

    if (procdata->smooth_refresh->get(new_interval))
    {
        procdata->timeout = g_timeout_add(new_interval,
                          cb_timeout,
                          procdata);
        return FALSE;
    }

    return TRUE;
}


void
cb_radio_processes(GtkAction *action, GtkRadioAction *current, gpointer data)
{
    ProcData * const procdata = static_cast<ProcData*>(data);

    procdata->config.whose_process = gtk_radio_action_get_current_value(current);

    g_settings_set_int (procdata->settings, "view-as",
                  procdata->config.whose_process);
}
