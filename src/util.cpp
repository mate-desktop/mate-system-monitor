#include <config.h>

#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <glib.h>
#include <gtk/gtk.h>

#include <glibtop/proctime.h>
#include <glibtop/procstate.h>
#include <unistd.h>

#include <stddef.h>
#include <cstring>

#include "util.h"
#include "procman.h"

gchar *
procman_format_date_for_display(time_t time_raw)
{
    gchar *result = NULL;
    const char *format;
    GDateTime *date_time, *today;
    GTimeSpan date_age;

    date_time = g_date_time_new_from_unix_local (time_raw);
    today = g_date_time_new_now_local ();

    date_age = g_date_time_difference (today, date_time);
    if (date_age < G_TIME_SPAN_DAY) {
        format = _("Today %l:%M %p");
    } else if (date_age < 2 * G_TIME_SPAN_DAY) {
        format = _("Yesterday %l:%M %p");
    } else if (date_age < 7 * G_TIME_SPAN_DAY) {
        format = _("%a %l:%M %p");
    } else if (g_date_time_get_year (date_time) == g_date_time_get_year (today)) {
        format = _("%b %d %l:%M %p");
    } else {
	format = _("%b %d %Y");
    }

    g_date_time_unref (today);
    result = g_date_time_format (date_time, format);
    g_date_time_unref (date_time);

    return result;
}

const char*
format_process_state(guint state)
{
    const char *status;

    switch (state)
        {
        case GLIBTOP_PROCESS_RUNNING:
            status = _("Running");
            break;

        case GLIBTOP_PROCESS_STOPPED:
            status = _("Stopped");
            break;

        case GLIBTOP_PROCESS_ZOMBIE:
            status = _("Zombie");
            break;

        case GLIBTOP_PROCESS_UNINTERRUPTIBLE:
            status = _("Uninterruptible");
            break;

        default:
            status = _("Sleeping");
            break;
        }

    return status;
}



static char *
mnemonic_safe_process_name(const char *process_name)
{
    const char *p;
    GString *name;

    name = g_string_new ("");

    for(p = process_name; *p; ++p)
    {
        g_string_append_c (name, *p);

        if(*p == '_')
            g_string_append_c (name, '_');
    }

    return g_string_free (name, FALSE);
}



static inline unsigned divide(unsigned *q, unsigned *r, unsigned d)
{
    *q = *r / d;
    *r = *r % d;
    return *q != 0;
}


/*
 * @param d: duration in centiseconds
 * @type d: unsigned
 */
gchar *
procman::format_duration_for_display(unsigned centiseconds)
{
    unsigned weeks = 0, days = 0, hours = 0, minutes = 0, seconds = 0;

    (void)(divide(&seconds, &centiseconds, 100)
           && divide(&minutes, &seconds, 60)
           && divide(&hours, &minutes, 60)
           && divide(&days, &hours, 24)
           && divide(&weeks, &days, 7));

    if (weeks)
        /* xgettext: weeks, days */
        return g_strdup_printf(_("%uw%ud"), weeks, days);

    if (days)
        /* xgettext: days, hours (0 -> 23) */
        return g_strdup_printf(_("%ud%02uh"), days, hours);

    if (hours)
        /* xgettext: hours (0 -> 23), minutes, seconds */
        return g_strdup_printf(_("%u:%02u:%02u"), hours, minutes, seconds);

    /* xgettext: minutes, seconds, centiseconds */
    return g_strdup_printf(_("%u:%02u.%02u"), minutes, seconds, centiseconds);
}



GtkWidget*
procman_make_label_for_mmaps_or_ofiles(const char *format,
                                       const char *process_name,
                                       unsigned pid)
{
    GtkWidget *label;
    char *name, *title;

    name = mnemonic_safe_process_name (process_name);
    title = g_strdup_printf(format, name, pid);
    label = gtk_label_new_with_mnemonic (title);
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);

    g_free (title);
    g_free (name);

    return label;
}

gchar *
procman::get_nice_level (gint nice)
{
    if (nice < -7)
        return _("Very High");
    else if (nice < -2)
        return _("High");
    else if (nice < 3)
        return _("Normal");
    else if (nice < 7)
        return _("Low");
    else
        return _("Very Low");
}


gboolean
load_symbols(const char *module, ...)
{
    GModule *mod;
    gboolean found_all = TRUE;
    va_list args;

    mod = g_module_open(module, static_cast<GModuleFlags>(G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL));

    if (!mod)
        return FALSE;

    procman_debug("Found %s", module);

    va_start(args, module);

    while (1) {
        const char *name;
        void **symbol;

        name = va_arg(args, char*);

        if (!name)
            break;

        symbol = va_arg(args, void**);

        if (g_module_symbol(mod, name, symbol)) {
            procman_debug("Loaded %s from %s", name, module);
        }
        else {
            procman_debug("Could not load %s from %s", name, module);
            found_all = FALSE;
            break;
        }
    }

    va_end(args);


    if (found_all)
        g_module_make_resident(mod);
    else
        g_module_close(mod);

    return found_all;
}


static gboolean
is_debug_enabled(void)
{
    static gboolean init;
    static gboolean enabled;

    if (!init) {
        enabled = g_getenv("MATE_SYSTEM_MONITOR_DEBUG") != NULL;
        init = TRUE;
    }

    return enabled;
}


static double
get_relative_time(void)
{
    static unsigned long start_time;
    GTimeVal tv;

    if (G_UNLIKELY(!start_time)) {
        glibtop_proc_time buf;
        glibtop_get_proc_time(&buf, getpid());
        start_time = buf.start_time;
    }

    g_get_current_time(&tv);
    return (tv.tv_sec - start_time) + 1e-6 * tv.tv_usec;
}

static guint64
get_size_from_column(GtkTreeModel* model, GtkTreeIter* first,
                             const guint index)
{
    GValue value = { 0 };
    gtk_tree_model_get_value(model, first, index, &value);

    guint64 size;
    switch (G_VALUE_TYPE(&value)) {
        case G_TYPE_UINT:
            size = g_value_get_uint(&value);
            break;
        case G_TYPE_ULONG:
            size = g_value_get_ulong(&value);
            break;
        case G_TYPE_UINT64:
            size = g_value_get_uint64(&value);
            break;
        default:
            g_assert_not_reached();
    }

    g_value_unset(&value);
    return size;
}

void
procman_debug_real(const char *file, int line, const char *func,
                   const char *format, ...)
{
    va_list args;
    char *msg;

    if (G_LIKELY(!is_debug_enabled()))
        return;

    va_start(args, format);
    msg = g_strdup_vprintf(format, args);
    va_end(args);

    g_debug("[%.3f %s:%d %s] %s", get_relative_time(), file, line, func, msg);

    g_free(msg);
}



namespace procman
{
    void memory_size_cell_data_func(GtkTreeViewColumn *col, GtkCellRenderer *renderer,
                                    GtkTreeModel *model, GtkTreeIter *iter,
                                    gpointer user_data)
    {
        const guint index = GPOINTER_TO_UINT(user_data);

        guint64 size;
        GValue value = { 0 };

        gtk_tree_model_get_value(model, iter, index, &value);

        switch (G_VALUE_TYPE(&value)) {
            case G_TYPE_ULONG:
                size = g_value_get_ulong(&value);
                break;

            case G_TYPE_UINT64:
                size = g_value_get_uint64(&value);
                break;

            default:
                g_assert_not_reached();
        }

        g_value_unset(&value);

        char *str = g_format_size_full(size, G_FORMAT_SIZE_IEC_UNITS);
        g_object_set(renderer, "text", str, NULL);
        g_free(str);
    }

    /*
      Same as above but handles size == 0 as not available
     */
    void memory_size_na_cell_data_func(GtkTreeViewColumn *col, GtkCellRenderer *renderer,
                                       GtkTreeModel *model, GtkTreeIter *iter,
                                       gpointer user_data)
    {
     	const guint index = GPOINTER_TO_UINT(user_data);

        guint64 size;
        GValue value = { 0 };

        gtk_tree_model_get_value(model, iter, index, &value);

        switch (G_VALUE_TYPE(&value)) {
            case G_TYPE_ULONG:
                size = g_value_get_ulong(&value);
                break;

          case G_TYPE_UINT64:
              size = g_value_get_uint64(&value);
              break;

          default:
              g_assert_not_reached();
        }

	g_value_unset(&value);

        if (size == 0) {
            char *str = g_strdup_printf ("<i>%s</i>", _("N/A"));
            g_object_set(renderer, "markup", str, NULL);
            g_free(str);
        }
	else {
            char *str = g_format_size_full(size, G_FORMAT_SIZE_IEC_UNITS);
            g_object_set(renderer, "text", str, NULL);
            g_free(str);
        }
    }

    void storage_size_cell_data_func(GtkTreeViewColumn *col, GtkCellRenderer *renderer,
                                     GtkTreeModel *model, GtkTreeIter *iter,
                                     gpointer user_data)
    {
        const guint index = GPOINTER_TO_UINT(user_data);

        guint64 size;
        GValue value = { 0 };

        gtk_tree_model_get_value(model, iter, index, &value);

        switch (G_VALUE_TYPE(&value)) {
            case G_TYPE_ULONG:
                size = g_value_get_ulong(&value);
                break;

            case G_TYPE_UINT64:
                size = g_value_get_uint64(&value);
                break;

            default:
                g_assert_not_reached();
        }

        g_value_unset(&value);

        char *str = g_format_size(size);
        g_object_set(renderer, "text", str, NULL);
        g_free(str);
    }

    /*
      Same as above but handles size == 0 as not available
     */
    void storage_size_na_cell_data_func(GtkTreeViewColumn *col, GtkCellRenderer *renderer,
                                        GtkTreeModel *model, GtkTreeIter *iter,
                                        gpointer user_data)
    {
     	const guint index = GPOINTER_TO_UINT(user_data);

        guint64 size;
        GValue value = { 0 };

        gtk_tree_model_get_value(model, iter, index, &value);

        switch (G_VALUE_TYPE(&value)) {
            case G_TYPE_ULONG:
                size = g_value_get_ulong(&value);
                break;

          case G_TYPE_UINT64:
              size = g_value_get_uint64(&value);
              break;

          default:
              g_assert_not_reached();
        }

	g_value_unset(&value);

        if (size == 0) {
            char *str = g_strdup_printf ("<i>%s</i>", _("N/A"));
            g_object_set(renderer, "markup", str, NULL);
            g_free(str);
        }
	else {
            char *str = g_format_size(size);
            g_object_set(renderer, "text", str, NULL);
            g_free(str);
        }
    }

    void io_rate_cell_data_func(GtkTreeViewColumn *, GtkCellRenderer *renderer,
                                GtkTreeModel *model, GtkTreeIter *iter,
                                gpointer user_data)
    {
        const guint index = GPOINTER_TO_UINT(user_data);

        guint64 size;
        GValue value = { 0 };

        gtk_tree_model_get_value(model, iter, index, &value);

        switch (G_VALUE_TYPE(&value)) {
            case G_TYPE_ULONG:
                size = g_value_get_ulong(&value);
                break;

            case G_TYPE_UINT64:
                size = g_value_get_uint64(&value);
                break;

            default:
                g_assert_not_reached();
        }

        g_value_unset(&value);

        if (size == 0) {
            char *str = g_strdup_printf ("<i>%s</i>", _("N/A"));
            g_object_set(renderer, "markup", str, NULL);
            g_free(str);
        }
        else {
            char *str = g_format_size(size);
            char *formatted_str = g_strdup_printf(_("%s/s"), str);
            g_object_set(renderer, "text", formatted_str, NULL);
            g_free(formatted_str);
            g_free(str);
        }

    }

    void duration_cell_data_func(GtkTreeViewColumn *, GtkCellRenderer *renderer,
                                 GtkTreeModel *model, GtkTreeIter *iter,
                                 gpointer user_data)
    {
        const guint index = GPOINTER_TO_UINT(user_data);

        unsigned time;
        GValue value = { 0 };

        gtk_tree_model_get_value(model, iter, index, &value);

        switch (G_VALUE_TYPE(&value)) {
            case G_TYPE_ULONG:
            time = g_value_get_ulong(&value);
            break;

            case G_TYPE_UINT64:
                time = g_value_get_uint64(&value);
                break;

            default:
                g_assert_not_reached();
        }

        g_value_unset(&value);

        time = 100 * time / ProcData::get_instance()->frequency;
        char *str = format_duration_for_display(time);
        g_object_set(renderer, "text", str, NULL);
        g_free(str);
    }


    void time_cell_data_func(GtkTreeViewColumn *, GtkCellRenderer *renderer,
                             GtkTreeModel *model, GtkTreeIter *iter,
                             gpointer user_data)
    {
        const guint index = GPOINTER_TO_UINT(user_data);

        time_t time;
        GValue value = { 0 };

        gtk_tree_model_get_value(model, iter, index, &value);

        switch (G_VALUE_TYPE(&value)) {
            case G_TYPE_ULONG:
                time = g_value_get_ulong(&value);
                break;

            default:
                g_assert_not_reached();
        }

        g_value_unset(&value);

        gchar *str = procman_format_date_for_display(time);
        g_object_set(renderer, "text", str, NULL);
        g_free(str);
    }

    void status_cell_data_func(GtkTreeViewColumn *, GtkCellRenderer *renderer,
                               GtkTreeModel *model, GtkTreeIter *iter,
                               gpointer user_data)
    {
        const guint index = GPOINTER_TO_UINT(user_data);

        guint state;
        GValue value = { 0 };

        gtk_tree_model_get_value(model, iter, index, &value);

        switch (G_VALUE_TYPE(&value)) {
            case G_TYPE_UINT:
                state = g_value_get_uint(&value);
                break;

            default:
                g_assert_not_reached();
        }

        g_value_unset(&value);

        const char *str = format_process_state(state);
        g_object_set(renderer, "text", str, NULL);
    }

    void priority_cell_data_func(GtkTreeViewColumn *, GtkCellRenderer *renderer,
                             GtkTreeModel *model, GtkTreeIter *iter,
                             gpointer user_data)
    {
        const guint index = GPOINTER_TO_UINT(user_data);

        GValue value = { 0 };

        gtk_tree_model_get_value(model, iter, index, &value);

        gint priority = g_value_get_int(&value);

        g_value_unset(&value);

        g_object_set(renderer, "text", procman::get_nice_level(priority), NULL);

    }

    gint priority_compare_func(GtkTreeModel* model, GtkTreeIter* first,
                            GtkTreeIter* second, gpointer user_data)
    {
        const guint index = GPOINTER_TO_UINT(user_data);
        GValue value1 = { 0 };
        GValue value2 = { 0 };
        gtk_tree_model_get_value(model, first, index, &value1);
        gtk_tree_model_get_value(model, second, index, &value2);
        gint result = g_value_get_int(&value1) - g_value_get_int(&value2);
        g_value_unset(&value1);
        g_value_unset(&value2);
        return result;
    }

    gint number_compare_func(GtkTreeModel* model, GtkTreeIter* first,
                            GtkTreeIter* second, gpointer user_data)
    {
        const guint index = GPOINTER_TO_UINT(user_data);

        guint64 size1, size2;
        size1 = get_size_from_column(model, first, index);
        size2 = get_size_from_column(model, second, index);

        if ( size2 > size1 )
            return 1;
        else if ( size2 < size1 )
            return -1;
        return 0;
    }

    template<>
    void tree_store_update<const char>(GtkTreeModel* model, GtkTreeIter* iter, int column, const char* new_value)
    {
        char* current_value;

        gtk_tree_model_get(model, iter, column, &current_value, -1);

        if (g_strcmp0(current_value, new_value) != 0)
            gtk_tree_store_set(GTK_TREE_STORE(model), iter, column, new_value, -1);

        g_free(current_value);
    }
}
