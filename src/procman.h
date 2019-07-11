/* Procman
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
#ifndef _PROCMAN_PROCMAN_H_
#define _PROCMAN_PROCMAN_H_


#include <glibmm/refptr.h>
#include <cairo-gobject.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <glibtop/cpu.h>

#include <time.h>
#include <sys/types.h>

#include <map>
#include <string>

struct ProcInfo;
struct ProcData;
struct LoadGraph;

#include "smooth_refresh.h"
#include "prettytable.h"

#define GSM_GSETTINGS_SCHEMA "org.mate.system-monitor"

enum
{
    ALL_PROCESSES,
    MY_PROCESSES,
    ACTIVE_PROCESSES
};

enum
{
    VERY_HIGH_PRIORITY,
    HIGH_PRIORITY,
    NORMAL_PRIORITY,
    LOW_PRIORITY,
    VERY_LOW_PRIORITY,
    CUSTOM_PRIORITY
};

static const unsigned MIN_UPDATE_INTERVAL =   1 * 1000;
static const unsigned MAX_UPDATE_INTERVAL = 100 * 1000;


namespace procman
{
    extern const std::string SHOW_SYSTEM_TAB_CMD;
    extern const std::string SHOW_PROCESSES_TAB_CMD;
    extern const std::string SHOW_RESOURCES_TAB_CMD;
    extern const std::string SHOW_FILE_SYSTEMS_TAB_CMD;
}



enum ProcmanTab
{
    PROCMAN_TAB_SYSINFO,
    PROCMAN_TAB_PROCESSES,
    PROCMAN_TAB_RESOURCES,
    PROCMAN_TAB_DISKS
};


struct ProcConfig
{
    gint        width;
    gint        height;
    gint        xpos;
    gint        ypos;
    gboolean    maximized;
    gboolean    show_kill_warning;
    gboolean    show_tree;
    gboolean    show_all_fs;
    int         update_interval;
    int         graph_update_interval;
    int         disks_update_interval;
    gint        whose_process;
    gint        current_tab;
    GdkRGBA     cpu_color[GLIBTOP_NCPU];
    GdkRGBA     mem_color;
    GdkRGBA     swap_color;
    GdkRGBA     net_in_color;
    GdkRGBA     net_out_color;
    GdkRGBA     bg_color;
    GdkRGBA     frame_color;
    gint        num_cpus;
    bool solaris_mode;
    bool network_in_bits;
};



struct MutableProcInfo
{
MutableProcInfo()
  : disk_write_bytes_current(0ULL),
    disk_read_bytes_current(0ULL),
    disk_write_bytes_total(0ULL),
    disk_read_bytes_total(0ULL),
    status(0U)
    { }

    std::string user;

    gchar wchan[40];

    // all these members are filled with libgtop which uses
    // guint64 (to have fixed size data) but we don't need more
    // than an unsigned long (even for 32bit apps on a 64bit
    // kernel) as these data are amounts, not offsets.
    gulong vmsize;
    gulong memres;
    gulong memshared;
    gulong memwritable;
    gulong mem;

    // wnck gives an unsigned long
    gulong memxserver;

    gulong start_time;
    guint64 cpu_time;
    guint64 disk_write_bytes_current;
    guint64 disk_read_bytes_current;
    guint64 disk_write_bytes_total;
    guint64 disk_read_bytes_total;
    guint status;
    guint pcpu;
    gint nice;
    gchar *cgroup_name;

    gchar *unit;
    gchar *session;
    gchar *seat;

    std::string owner;
};


class ProcInfo
: public MutableProcInfo
{
    /* undefined */ ProcInfo& operator=(const ProcInfo&);
    /* undefined */ ProcInfo(const ProcInfo&);

    typedef std::map<guint, std::string> UserMap;
    /* cached username */
    static UserMap users;

  public:

    // TODO: use a set instead
    // sorted by pid. The map has a nice property : it is sorted
    // by pid so this helps a lot when looking for the parent node
    // as ppid is nearly always < pid.
    typedef std::map<pid_t, ProcInfo*> List;
    typedef List::iterator Iterator;

    static List all;

    static ProcInfo* find(pid_t pid);
    static Iterator begin() { return ProcInfo::all.begin(); }
    static Iterator end() { return ProcInfo::all.end(); }


    ProcInfo(pid_t pid);
    ~ProcInfo();
    // adds one more ref to icon
    void set_icon(Glib::RefPtr<Gdk::Pixbuf> icon);
    void set_user(guint uid);
    std::string lookup_user(guint uid);

    GtkTreeIter      node;
    cairo_surface_t *surface;
    gchar           *tooltip;
    gchar           *name;
    gchar           *arguments;
    gchar           *security_context;

    const pid_t      pid;
    pid_t            ppid;
    guint            uid;

// private:
    // tracks cpu time per process keeps growing because if a
    // ProcInfo is deleted this does not mean that the process is
    // not going to be recreated on the next update.  For example,
    // if dependencies + (My or Active), the proclist is cleared
    // on each update.  This is a workaround
    static std::map<pid_t, guint64> cpu_times;
};

struct ProcData
{
    // lazy initialization
    static ProcData* get_instance();

    GtkUIManager    *uimanager;
    GtkActionGroup    *action_group;
    GtkWidget    *statusbar;
    gint        tip_message_cid;
    GtkWidget    *tree;
    GtkWidget    *loadavg;
    GtkWidget    *endprocessbutton;
    GtkWidget    *popup_menu;
    GtkWidget    *disk_list;
    GtkWidget    *notebook;
    ProcConfig    config;
    LoadGraph    *cpu_graph;
    LoadGraph    *mem_graph;
    LoadGraph    *net_graph;
    gint        cpu_label_fixed_width;
    gint        net_label_fixed_width;
    ProcInfo    *selected_process;
    GtkTreeSelection *selection;
    guint        timeout;
    guint        disk_timeout;

    PrettyTable    pretty_table;

    GSettings       *settings;
    GtkWidget        *app;
    GtkUIManager    *menu;

    unsigned    frequency;

    SmoothRefresh  *smooth_refresh;

    guint64 cpu_total_time;
    guint64 cpu_total_time_last;

private:
    ProcData();
    /* undefined */ ProcData(const ProcData &);
    /* undefined */ ProcData& operator=(const ProcData &);
};

void        procman_save_config (ProcData *data);
void        procman_save_tree_state (GSettings *settings, GtkWidget *tree, const gchar *prefix);
gboolean    procman_get_tree_state (GSettings *settings, GtkWidget *tree, const gchar *prefix);





struct ReniceArgs
{
    ProcData *procdata;
    int nice_value;
};


struct KillArgs
{
    ProcData *procdata;
    int signal;
};

#endif /* _PROCMAN_PROCMAN_H_ */
