#include <config.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <glibmm.h>
#include <glib/gi18n.h>

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <glibtop/fsusage.h>
#include <glibtop/mountlist.h>
#include <glibtop/mem.h>
#include <glibtop/sysinfo.h>

#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <math.h>
#include <errno.h>

#include <exception>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <sys/utsname.h>

#include "sysinfo.h"
#include "procman.h"
#include "util.h"


using std::string;
using std::vector;


namespace {

    class SysInfo
    {
    public:
        string hostname;
        string distro_name;
        string distro_release;
        string kernel;
        string mate_version;
        guint64 memory_bytes;
        guint64 free_space_bytes;

        string graphics;
        string processors;

        SysInfo()
        {
            this->load_processors_info();
            this->load_graphics_info();
            this->load_memory_info();
            this->load_disk_info();
            this->load_uname_info();
            this->load_mate_version();
        }

        virtual ~SysInfo()
        { }

        virtual void set_distro_labels(GtkWidget* name, GtkWidget* release)
        {
            g_object_set(G_OBJECT(name),
                        "label",
                        ("<big><big><b>" + this->distro_name + "</b></big></big>").c_str(),
                        NULL);

            char* markup;
#ifdef __linux__
            if (this->distro_release != "")
            {
                /* Translators: The first string parameter is release version (codename),
                 * the second one is the architecture, 32 or 64-bit */
                markup = g_strdup_printf(_("Release %s %s"),
                                         this->distro_release.c_str(),
                                         this->get_os_type().c_str());
            }
            else
            {
                /* Translators: The string parameter is the architecture, 32 or 64-bit */
                markup = g_strdup_printf(_("%s Version"),
                                         this->get_os_type().c_str());
            }
#else
            /* Translators: The string parameter is release version (codename) */
            markup = g_strdup_printf(_("Release %s"),
                                     this->distro_release.c_str());
#endif

            g_object_set(G_OBJECT(release),
                         "label",
                         markup,
                         NULL);

            g_free(markup);
        }

        static string release()
        {
            return uname().release;
        }

        static string system()
        {
            return uname().sysname;
        }

    private:

        void load_memory_info()
        {
            glibtop_mem mem;

            glibtop_get_mem(&mem);
            this->memory_bytes = mem.total;
        }

        string get_os_type ()
        {
            int bits;

            if (GLIB_SIZEOF_VOID_P == 8)
                bits = 64;
            else
                bits = 32;

            /* translators: This is the type of architecture, for example:
             * "64-bit" or "32-bit" */
            char* bytes = g_strdup_printf (_("%d-bit"), bits);
            string retval(bytes);
            g_free(bytes);
            return retval;
        }

        typedef struct
        {
            const char* regex;
            const char* replacement;
        } ReplaceStrings;

        static char* remove_duplicate_whitespace (const char* old)
        {
            char* result;
            GRegex* re;
            GError* error = NULL;
            const GRegexMatchFlags flags = static_cast<GRegexMatchFlags>(0);

            re = g_regex_new ("[ \t\n\r]+", G_REGEX_MULTILINE, flags, &error);
            if (re == NULL) {
                g_warning ("Error building regex: %s", error->message);
                g_error_free (error);
                return g_strdup (old);
            }
            result = g_regex_replace (re, old, -1, 0, " ", flags, &error);
            g_regex_unref (re);
            if (result == NULL) {
                g_warning ("Error replacing string: %s", error->message);
                g_error_free (error);
                return g_strdup (old);
            }

            return result;
        }

        static char* prettify_info (const char *info)
        {
            char* pretty;
            const GRegexCompileFlags cflags = static_cast<GRegexCompileFlags>(0);
            const GRegexMatchFlags mflags = static_cast<GRegexMatchFlags>(0);

            static const ReplaceStrings rs[] = {
                { "Intel[(]R[)]", "Intel\302\256"},
                { "Core[(]TM[)]", "Core\342\204\242"},
                { "Atom[(]TM[)]", "Atom\342\204\242"},
            };

            pretty = g_markup_escape_text (info, -1);

            for (uint i = 0; i < G_N_ELEMENTS (rs); i++) {
                GError* error;
                GRegex* re;
                char* result;

                error = NULL;

                re = g_regex_new (rs[i].regex, cflags, mflags, &error);
                if (re == NULL) {
                    g_warning ("Error building regex: %s", error->message);
                    g_error_free (error);
                    continue;
                }

                result = g_regex_replace_literal (re, pretty, -1, 0,
                                             rs[i].replacement, mflags, &error);

                g_regex_unref (re);

                if (error != NULL) {
                    g_warning ("Error replacing %s: %s", rs[i].regex, error->message);
                    g_error_free (error);
                    continue;
                }

                g_free (pretty);
                pretty = result;
            }

            return pretty;
        }

        static char* get_renderer_from_helper (gboolean discrete_gpu)
        {
            int status;
            const char *argv[] = { LIBEXECDIR "/mate-session-check-accelerated", NULL };
            g_auto(GStrv) envp = NULL;
            g_autofree char *renderer = NULL;
            g_autoptr(GError) error = NULL;

            if (discrete_gpu)
            {
                envp = g_get_environ ();
                envp = g_environ_setenv (envp, "DRI_PRIME", "1", TRUE);
            }

            if (!g_spawn_sync (NULL, (char **) argv, envp, G_SPAWN_DEFAULT, NULL, NULL, &renderer, NULL, &status, &error))
            {
                g_debug ("Failed to get %s GPU: %s",
                         discrete_gpu ? "discrete" : "integrated",
                         error->message);
                return NULL;
            }

            if (!g_spawn_check_exit_status (status, NULL))
                return NULL;

            if (renderer == NULL || *renderer == '\0')
                return NULL;

            return prettify_info (renderer);
        }


        void load_graphics_info()
        {
            g_autofree char *renderer = NULL;

            try
            {
                g_autoptr(GDBusProxy) session_proxy = NULL;
                g_autoptr(GVariant) renderer_variant = NULL;
                g_autoptr(GError) error = NULL;

                session_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                               G_DBUS_PROXY_FLAGS_NONE,
                                                               NULL,
                                                               "org.gnome.SessionManager",
                                                               "/org/gnome/SessionManager",
                                                               "org.gnome.SessionManager",
                                                               NULL, &error);
                if (error != NULL)
                {
                    throw std::runtime_error ("Unable to connect to create a proxy for org.gnome.SessionManager");
                }

                renderer_variant = g_dbus_proxy_get_cached_property (session_proxy, "Renderer");
                if (!renderer_variant)
                {
                    throw std::runtime_error ("Unable to retrieve org.gnome.SessionManager.Renderer property.");
                }

                renderer = prettify_info (g_variant_get_string (renderer_variant, NULL));
            }
            catch (std::exception& e)
            {
                g_warning ("%s", e.what());
            }

            if (renderer == NULL)
                renderer = get_renderer_from_helper (FALSE);

            if (renderer == NULL)
                renderer = get_renderer_from_helper (TRUE);

            if (renderer)
            {
                this->graphics = g_strdup (renderer);
            }
            else
            {
                this->graphics = _("Unknown");
            }
        }

        void load_processors_info()
        {
            const glibtop_sysinfo *info = glibtop_get_sysinfo();

            GHashTable* counts;
            GString* cpu;
            GHashTableIter iter;
            gpointer key, value;

            counts = g_hash_table_new (g_str_hash, g_str_equal);

            /* count duplicates */
            for (uint i = 0; i != info->ncpu; ++i) {
                const char* const keys[] = { "model name", "cpu", "Processor" };
                char* model;
                int* count;

                model = NULL;

                for (int j = 0; model == NULL && j != G_N_ELEMENTS (keys); ++j) {
                    model = static_cast<char*>(g_hash_table_lookup (info->cpuinfo[i].values,
                                                 keys[j]));
                }

                if (model == NULL)
                    continue;

                count = static_cast<int*>(g_hash_table_lookup (counts, model));
                if (count == NULL)
                    g_hash_table_insert (counts, model, GINT_TO_POINTER (1));
                else
                    g_hash_table_replace (counts, model, GINT_TO_POINTER (GPOINTER_TO_INT (count) + 1));
            }

            cpu = g_string_new (NULL);
            g_hash_table_iter_init (&iter, counts);
            while (g_hash_table_iter_next (&iter, &key, &value)) {
                char* stripped;
                int   count;

                count = GPOINTER_TO_INT (value);
                stripped = remove_duplicate_whitespace ((const char *)key);
                if (count > 1)
                    g_string_append_printf (cpu, "%s \303\227 %d ", stripped, count);
                else
                    g_string_append_printf (cpu, "%s ", stripped);
                g_free (stripped);
            }

            g_hash_table_destroy (counts);
            this->processors = string(prettify_info (cpu->str));
            g_string_free (cpu, TRUE);
        }

        void load_disk_info()
        {
            glibtop_mountentry *entries;
            glibtop_mountlist mountlist;

            entries = glibtop_get_mountlist(&mountlist, 0);

            this->free_space_bytes = 0;

            for (guint i = 0; i != mountlist.number; ++i) {
                if ((string(entries[i].mountdir).compare("/") == 0) && \
                    (string(entries[i].type).compare("zfs") == 0)) {
                    // Root filesystem is ZFS based: Calculate usage based on "/".
                    glibtop_fsusage usage;
                    glibtop_get_fsusage(&usage, "/");
                    this->free_space_bytes = usage.bavail * usage.block_size;
                    break;
                }

                if (string(entries[i].devname).find("/dev/") != 0)
                    continue;

                if (string(entries[i].mountdir).find("/media/") == 0)
                    continue;

                glibtop_fsusage usage;
                glibtop_get_fsusage(&usage, entries[i].mountdir);
                this->free_space_bytes += usage.bavail * usage.block_size;
            }

            g_free(entries);
        }

        static const struct utsname & uname()
        {
            static struct utsname name;

            if (!name.sysname[0]) {
                ::uname(&name);
            }

            return name;
        }

        void load_uname_info()
        {
            this->hostname = uname().nodename;
#if defined(__linux__)
            this->kernel = string(uname().sysname) + ' ' + uname().release + ' ' + uname().machine;
#elif defined(__sun) && defined(__SVR4)
            this->kernel = string(uname().sysname) + ' ' + uname().release + ' ' + uname().version + ' ' + uname().machine;
#else
            this->kernel = string(uname().version) + ' ' + uname().machine;
#endif
        }


        void load_mate_version()
        {
            xmlDocPtr document;
            xmlXPathContextPtr context;
            const string nodes[3] = { "string(/mate-version/platform)",
                                      "string(/mate-version/minor)",
                                      "string(/mate-version/micro)" };
            string values[3];

            if (not (document = xmlParseFile(DATADIR "/mate-about/mate-version.xml")))
                return;

            if (not (context = xmlXPathNewContext(document)))
                return;

            for (size_t i = 0; i != 3; ++i)
            {
                xmlXPathObjectPtr xpath;
                xpath = xmlXPathEvalExpression(BAD_CAST nodes[i].c_str(), context);

                if (xpath and xpath->type == XPATH_STRING)
                    values[i] = reinterpret_cast<const char*>(xpath->stringval);

                xmlXPathFreeObject(xpath);
            }

            xmlXPathFreeContext(context);
            xmlFreeDoc(document);

            if (!values[0].empty() && !values[1].empty() && !values[2].empty())
                this->mate_version = values[0] + '.' + values[1] + '.' + values[2];
        }
    };

    class FreeBSDSysInfo
        : public SysInfo
    {
    public:
	FreeBSDSysInfo()
        {
            this->load_freebsd_info();
        }

    private:
        void load_freebsd_info()
        {
            this->distro_name = "FreeBSD";
            this->distro_release = release();
        }
    };

    class SolarisSysInfo
        : public SysInfo
    {
    public:
        SolarisSysInfo()
        {
            this->load_solaris_info();
        }

    private:
        void load_solaris_info()
        {
            std::ifstream input("/etc/release");

            if (input) {
                std::string s;
                std::getline(input, s);
                std::size_t found = s.find("OpenIndiana ");
                if (found!=std::string::npos) {
                    this->distro_name = "OpenIndiana";
                    this->distro_release = s.substr(found + strlen("OpenIndiana "));
                } else if (!s.empty()) {
                    this->distro_release = s;
                }
            }

            if (this->distro_release.empty()) {
                this->distro_name = "Solaris";
            }
        }
    };


    class LSBSysInfo
        : public SysInfo
    {
    public:
        LSBSysInfo()
            : re(Glib::Regex::create("^.+?:\\s*(.+)\\s*$"))
        {
            // start();
        }

        virtual void set_distro_labels(GtkWidget* name, GtkWidget* release)
        {
            this->name = name;
            this->release = release;

            this->start();
        }


    private:

        sigc::connection child_watch;
        int lsb_fd;
        GtkWidget* name;
        GtkWidget* release;

        void strip_description(string &s) const
        {
            const GRegexMatchFlags flags = static_cast<GRegexMatchFlags>(0);
            GMatchInfo* info = 0;

           if (g_regex_match(this->re->gobj(), s.c_str(), flags, &info)) {
               s = make_string(g_match_info_fetch(info, 1));
               g_match_info_free(info);
           }
        }

        std::istream& get_value(std::istream &is, string &s) const
        {
            if (std::getline(is, s))
                this->strip_description(s);
            return is;
        }


        void read_lsb(Glib::Pid pid, int status)
        {
            this->child_watch.disconnect();

            if (!WIFEXITED(status) or WEXITSTATUS(status) != 0) {
                g_error("Child %d failed with status %d", int(pid), status);
                return;
            }

            Glib::RefPtr<Glib::IOChannel> channel = Glib::IOChannel::create_from_fd(this->lsb_fd);
            Glib::ustring content;

            while (channel->read_to_end(content) == Glib::IO_STATUS_AGAIN)
                ;

            channel->close();
            Glib::spawn_close_pid(pid);

            procman_debug("lsb_release output = '%s'", content.c_str());

            string release, codename;
            std::istringstream input(content);

            this->get_value(input, this->distro_name)
                and this->get_value(input, release)
                and this->get_value(input, codename);

            this->distro_release = release;
            if (codename != "" && codename != "n/a")
                this->distro_release += " (" + codename + ')';

            this->SysInfo::set_distro_labels(this->name, this->release);
        }


        void start()
        {
            std::vector<string> argv(2);
            argv[0] = "lsb_release";
            argv[1] = "-irc";

            Glib::SpawnFlags flags = Glib::SPAWN_DO_NOT_REAP_CHILD
                | Glib::SPAWN_SEARCH_PATH
                | Glib::SPAWN_STDERR_TO_DEV_NULL;

            Glib::Pid child;

            try {
                Glib::spawn_async_with_pipes("/", // wd
                                             argv,
                                             flags,
                                             sigc::slot<void>(), // child setup
                                             &child,
                                             0, // stdin
                                             &this->lsb_fd); // stdout
            } catch (Glib::SpawnError &e) {
                g_error("g_spawn_async_with_pipes error: %s", e.what().c_str());
                return;
            }

            sigc::slot<void,GPid, int> slot = sigc::mem_fun(this, &LSBSysInfo::read_lsb);
            this->child_watch = Glib::signal_child_watch().connect(slot, child);
        }


        void sync_lsb_release()
        {
            char *out= 0;
            GError *error = 0;
            int status;

            if (g_spawn_command_line_sync("lsb_release -irc",
                                          &out,
                                          0,
                                          &status,
                                          &error)) {
                string release, codename;
                if (!error and WIFEXITED(status) and WEXITSTATUS(status) == 0) {
                    std::istringstream input(out);
                    this->get_value(input, this->distro_name)
                        and this->get_value(input, release)
                        and this->get_value(input, codename);
                    this->distro_release = release;
                if (codename != "" && codename != "n/a")
                    this->distro_release += " (" + codename + ')';
                }
            }

            if (error)
                g_error_free(error);

            g_free(out);
        }

    private:
        Glib::RefPtr<Glib::Regex> re;
    };


    class NetBSDSysInfo
        : public SysInfo
    {
    public:
        NetBSDSysInfo()
        {
            this->load_netbsd_info();
        }

    private:
        void load_netbsd_info()
        {
            this->distro_name = "NetBSD";

            std::ifstream input("/etc/release");

            if (input)
                std::getline(input, this->distro_release);
        }
    };


    class GenericSysInfo
        : public SysInfo
    {
    public:
        GenericSysInfo()
        {
            this->load_os_release();
        }

    private:
        void load_os_release()
        {
            std::ifstream input("/etc/os-release");

            if (input) {
                while (!input.eof()) {
                    string s;
                    int start;
                    std::getline(input, s);
                    if (s.find("NAME=") == 0) {
                        start = strlen("NAME=");
                        if ((s.at(start) == '\"') && (s.at(s.size() - 1) == '\"')) {
                            this->distro_name = s.substr(start + 1, s.size() - start - 2);
                        } else {
                            this->distro_name = s.substr(start);
                        }
                    } else if (s.find("VERSION=") == 0) {
                        start = strlen("VERSION=");
                        // also strip the surrounding quotes
                        this->distro_release = s.substr(start + 1, s.size() - start - 2);
                    } else if (s.find("# VERSION=") == 0) {
                        start = strlen("# VERSION=");
                        // also strip the surrounding quotes
                        this->distro_release = s.substr(start + 1, s.size() - start - 2);
                    }
                }
                input.close();
                input.clear();
            }
            if (this->distro_release.empty()) {
                input.open("/etc/debian_version");
                if (input) {
                    getline(input, this->distro_release);
                }
            }
        }
    };

    class MintSysInfo
        : public SysInfo
    {
    public:
        MintSysInfo()
        {
            this->load_os_release();
        }

    private:
        void load_os_release()
        {
            this->distro_name = "Linux Mint";
            std::ifstream input("/etc/linuxmint/info");
            if (input) {
                while (!input.eof()) {
                    string s;
                    int len;
                    std::getline(input, s);
                    if (s.find("DESCRIPTION=") == 0) {
                        len = strlen("DESCRIPTION=");
                        // also strip the surrounding quotes
                       this->distro_release = s.substr(len + 1, s.size() - len - 2);
                   }
                }
            }
        }
    };

    class OpenBSDSysInfo
        : public SysInfo
    {
    public:
        OpenBSDSysInfo()
        {
            this->load_openbsd_info();
        }

    private:
        void load_openbsd_info()
        {
            this->distro_name = "OpenBSD";
            this->distro_release = this->kernel;

            std::ifstream input("/etc/motd");

            if (input)
                std::getline(input, this->kernel);
        }
    };

    SysInfo* get_sysinfo()
    {
        if (g_file_test ("/etc/linuxmint/info", G_FILE_TEST_EXISTS)) {
            return new MintSysInfo;
        }
        else if (g_file_test ("/etc/os-release", G_FILE_TEST_EXISTS)) {
            return new GenericSysInfo;
        }
        else if (char *p = g_find_program_in_path("lsb_release")) {
            g_free(p);
            return new LSBSysInfo;
        }
        else if (SysInfo::system() == "FreeBSD") {
            return new FreeBSDSysInfo;
        }
        else if (SysInfo::system() == "SunOS") {
            return new SolarisSysInfo;
        }
        else if (SysInfo::system() == "NetBSD") {
            return new NetBSDSysInfo;
        }
        else if (SysInfo::system() == "OpenBSD") {
            return new OpenBSDSysInfo;
        }

        return new SysInfo;
    }
}


#define X_PAD  0
#define Y_PAD  0
#define LOGO_W 92
#define LOGO_H 351
#define RADIUS 5

static gboolean sysinfo_logo_draw (GtkWidget *widget, cairo_t *context, gpointer data_ptr)
{
    GtkAllocation allocation;
    GtkStyle *style;
    cairo_t *cr;
    cairo_pattern_t *cp;

    cr = gdk_cairo_create(gtk_widget_get_window(widget));

    gtk_widget_get_allocation (widget, &allocation);
    cairo_translate(cr, allocation.x, allocation.y);

    cairo_move_to(cr, X_PAD + RADIUS, Y_PAD);
    cairo_line_to(cr, X_PAD + LOGO_W - RADIUS, Y_PAD);
    cairo_arc(cr, X_PAD + LOGO_W - RADIUS, Y_PAD + RADIUS, RADIUS, -0.5 * M_PI, 0);
    cairo_line_to(cr, X_PAD + LOGO_W, Y_PAD + LOGO_H - RADIUS);
    cairo_arc(cr, X_PAD + LOGO_W - RADIUS, Y_PAD + LOGO_H - RADIUS, RADIUS, 0, 0.5 * M_PI);
    cairo_line_to(cr, X_PAD + RADIUS, Y_PAD + LOGO_H);
    cairo_arc(cr, X_PAD + RADIUS, Y_PAD + LOGO_H - RADIUS, RADIUS, 0.5 * M_PI, -1.0 * M_PI);
    cairo_line_to(cr, X_PAD, Y_PAD + RADIUS);
    cairo_arc(cr,  X_PAD + RADIUS, Y_PAD + RADIUS, RADIUS, -1.0 * M_PI, -0.5 * M_PI);

    cp = cairo_pattern_create_linear(0, Y_PAD, 0, Y_PAD + LOGO_H);
    style = gtk_widget_get_style (widget);
    cairo_pattern_add_color_stop_rgba(cp, 0.0,
                                      style->base[GTK_STATE_SELECTED].red / 65535.0,
                                      style->base[GTK_STATE_SELECTED].green / 65535.0,
                                      style->base[GTK_STATE_SELECTED].blue / 65535.0,
                                      1.0);
    cairo_pattern_add_color_stop_rgba(cp, 1.0,
                                      style->base[GTK_STATE_SELECTED].red / 65535.0,
                                      style->base[GTK_STATE_SELECTED].green / 65535.0,
                                      style->base[GTK_STATE_SELECTED].blue / 65535.0,
                                      0.0);
    cairo_set_source(cr, cp);
    cairo_fill(cr);

    cairo_pattern_destroy(cp);
    cairo_destroy(cr);

    return FALSE;
}

static GtkWidget*
add_section(GtkBox *vbox , const char * title, int num_row, int num_col, GtkWidget **out_frame)
{
    GtkWidget *table;

    GtkWidget *frame = gtk_frame_new(title);
    gtk_frame_set_label_align(GTK_FRAME(frame), 0.0, 0.5);
    gtk_label_set_use_markup(
        GTK_LABEL(gtk_frame_get_label_widget(GTK_FRAME(frame))),
        TRUE
        );
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

    table = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(table), 6);
    gtk_grid_set_column_spacing(GTK_GRID(table), 6);
    gtk_widget_set_margin_start (GTK_WIDGET(table), 12);
    gtk_container_set_border_width(GTK_CONTAINER(table), 6);
    gtk_container_add(GTK_CONTAINER(frame), table);

    if(out_frame)
        *out_frame = frame;

    return table;
}


static GtkWidget*
add_row(GtkGrid * table, const char * label, const char * value, int row)
{
    GtkWidget *header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(header), label);
    gtk_label_set_selectable(GTK_LABEL(header), TRUE);
    gtk_widget_set_can_focus(header, FALSE);
    gtk_label_set_xalign (GTK_LABEL (header), 0.0);
    gtk_grid_attach(
        table, header,
        0, row, 1, 1);

    GtkWidget *label_widget = gtk_label_new(value);
    gtk_label_set_selectable(GTK_LABEL(label_widget), TRUE);
    gtk_widget_set_can_focus(label_widget, FALSE);
    gtk_label_set_xalign (GTK_LABEL (label_widget), 0.0);
    gtk_grid_attach(
        table, label_widget,
        1, row, 1, 1);

    return label_widget;
}


static GtkWidget *
procman_create_sysinfo_view(void)
{
    GtkWidget *hbox;
    GtkWidget *vbox;

    SysInfo *data = get_sysinfo();;

    GtkWidget * logo;

    GtkWidget *distro_frame;
    GtkWidget *distro_release_label;
    GtkWidget *distro_table;

    GtkWidget *hardware_table;
    GtkWidget *disk_space_table;

    GtkWidget *header;

    gchar *markup;

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);

    /* left-side logo */
    if (g_file_test (DATADIR "/pixmaps/mate-system-monitor/distribution/side.png", G_FILE_TEST_EXISTS)) {
        logo = gtk_image_new_from_file(DATADIR "/pixmaps/mate-system-monitor/distribution/side.png");
    }
    else {
        logo = gtk_image_new_from_file(DATADIR "/pixmaps/mate-system-monitor/side.png");
    }
    gtk_widget_set_valign (logo, GTK_ALIGN_START);
    gtk_widget_set_margin_start (logo, 5);
    gtk_widget_set_margin_end (logo, 5);
    gtk_widget_set_margin_top (logo, 12);
    gtk_widget_set_margin_bottom (logo, 12);
    gtk_box_pack_start(GTK_BOX(hbox), logo, FALSE, FALSE, 0);

    g_signal_connect(G_OBJECT(logo), "draw", G_CALLBACK(sysinfo_logo_draw), NULL);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    // hostname

    markup = g_strdup_printf("<big><big><b><u>%s</u></b></big></big>",
                             data->hostname.c_str());
    GtkWidget *hostname_frame = gtk_frame_new(markup);
    g_free(markup);
    gtk_frame_set_label_align(GTK_FRAME(hostname_frame), 0.0, 0.5);
    gtk_label_set_use_markup(
        GTK_LABEL(gtk_frame_get_label_widget(GTK_FRAME(hostname_frame))),
        TRUE
        );
    gtk_frame_set_shadow_type(GTK_FRAME(hostname_frame), GTK_SHADOW_NONE);
    gtk_box_pack_start(GTK_BOX(vbox), hostname_frame, FALSE, FALSE, 0);


    /* distro section */

    unsigned table_size = 2;
    if (data->mate_version != "")
        table_size++;
    distro_table = add_section(GTK_BOX(vbox), "???", table_size, 1, &distro_frame);

    unsigned table_count = 0;

    distro_release_label = gtk_label_new("???");
    gtk_label_set_selectable(GTK_LABEL(distro_release_label), TRUE);
    gtk_widget_set_can_focus(distro_release_label, FALSE);
    gtk_label_set_xalign (GTK_LABEL (distro_release_label), 0.0);
    gtk_grid_attach(
        GTK_GRID(distro_table), distro_release_label,
        0, table_count, 1, 1);
    table_count++;
    data->set_distro_labels(gtk_frame_get_label_widget(GTK_FRAME(distro_frame)), distro_release_label);

    markup = g_strdup_printf(_("Kernel %s"), data->kernel.c_str());
    header = gtk_label_new(markup);
    gtk_label_set_selectable(GTK_LABEL(header), TRUE);
    gtk_widget_set_can_focus(header, FALSE);
    g_free(markup);
    gtk_label_set_xalign (GTK_LABEL (header), 0.0);
    gtk_grid_attach(
        GTK_GRID(distro_table), header,
        0, table_count, 1, 1);
    table_count++;

    if (data->mate_version != "")
    {
        markup = g_strdup_printf(_("MATE %s"), data->mate_version.c_str());
        header = gtk_label_new(markup);
        gtk_label_set_selectable(GTK_LABEL(header), TRUE);
        gtk_widget_set_can_focus(header, FALSE);
        g_free(markup);
        gtk_label_set_xalign (GTK_LABEL (header), 0.0);
        gtk_grid_attach(
            GTK_GRID(distro_table), header,
            0, table_count, 1, 1);
        table_count++;
    }

    /* hardware section */

    markup = g_strdup_printf("<b>%s</b>", _("Hardware"));
    hardware_table = add_section(GTK_BOX(vbox), markup, 1, 3, NULL);
    g_free(markup);

    markup = g_format_size_full(data->memory_bytes, G_FORMAT_SIZE_IEC_UNITS);
    add_row(GTK_GRID(hardware_table), _("Memory:"), markup, 0);
    g_free(markup);

    markup = NULL;
    add_row(GTK_GRID(hardware_table), _("Processor:"),
            data->processors.c_str(), 1);
    add_row(GTK_GRID(hardware_table), _("Graphics:"),
            data->graphics.c_str(), 2);

    if(markup)
        g_free(markup);

    /* disk space section */

    markup = g_strdup_printf("<b>%s</b>", _("System Status"));
    disk_space_table = add_section(GTK_BOX(vbox), markup, 1, 2, NULL);
    g_free(markup);

    markup = g_format_size(data->free_space_bytes);
    add_row(GTK_GRID(disk_space_table), _("Available disk space:"), markup, 0);
    g_free(markup);

    return hbox;
}



namespace procman
{
    void build_sysinfo_ui()
    {
        static GtkWidget* ui;

        if (!ui) {
            ProcData* procdata = ProcData::get_instance();
            ui = procman_create_sysinfo_view();
            GtkBox* box = GTK_BOX(gtk_notebook_get_nth_page(GTK_NOTEBOOK(procdata->notebook),
                                                            PROCMAN_TAB_SYSINFO));
            gtk_box_pack_start(box, ui, TRUE, TRUE, 0);
            gtk_widget_show_all(ui);
        }
    }
}
