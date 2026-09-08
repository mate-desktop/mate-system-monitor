/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <config.h>
#include "gpu-monitor.h"
#include "nvidia-smi.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <map>
#include <memory>
#include <set>
#include <deque>
#include <algorithm>

namespace {
struct History {
    std::deque<std::pair<gint64, double>> points;

    void trim(gint64 now)
    {
        while (!points.empty() && points.front().first < now - 60 * G_USEC_PER_SEC)
            points.pop_front();
    }

    void append(gint64 now, double utilization)
    {
        trim(now);
        points.emplace_back(now, utilization);
        // Also bound memory if queries complete unusually rapidly.
        if (points.size() > 128)
            points.pop_front();
    }
};

// Draw into GTK's supplied context so scrolling and partial redraws stay correct.
gboolean draw_history(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    auto& history = *static_cast<History *>(data);
    const gint64 now = g_get_monotonic_time();
    history.trim(now);
    const double left = 56, top = 8;
    const double width = std::max(1.0, gtk_widget_get_allocated_width(widget) - left - 16);
    const double height = std::max(1.0, gtk_widget_get_allocated_height(widget) - top - 24);
    GtkStyleContext *style = gtk_widget_get_style_context(widget);
    GdkRGBA foreground;
    gtk_style_context_get_color(style, gtk_style_context_get_state(style), &foreground);
    gtk_render_background(style, cr, 0, 0,
                          gtk_widget_get_allocated_width(widget), gtk_widget_get_allocated_height(widget));
    PangoLayout *layout = gtk_widget_create_pango_layout(widget, nullptr);
    PangoAttrList *attributes = pango_attr_list_new();
    pango_attr_list_insert(attributes, pango_attr_scale_new(0.8));
    pango_layout_set_attributes(layout, attributes);
    pango_attr_list_unref(attributes);
    auto text = [&](const char *value, double x, double y, bool right) {
        pango_layout_set_text(layout, value, -1);
        int w, h;
        pango_layout_get_pixel_size(layout, &w, &h);
        gdk_cairo_set_source_rgba(cr, &foreground);
        cairo_move_to(cr, right ? x - w : x, y);
        pango_cairo_show_layout(cr, layout);
    };
    cairo_save(cr);
    cairo_set_line_width(cr, 1);
    for (int i = 0; i <= 4; ++i) {
        const double y = top + i * height / 4;
        cairo_set_source_rgba(cr, foreground.red, foreground.green, foreground.blue, 0.25);
        cairo_move_to(cr, left, y);
        cairo_line_to(cr, left + width, y);
        cairo_stroke(cr);
        gchar *value = g_strdup_printf("%d%%", 100 - 25 * i);
        text(value, left - 7, y - 6, true);
        g_free(value);
    }
    for (int i = 0; i <= 6; ++i) {
        const double x = left + i * width / 6;
        cairo_set_source_rgba(cr, foreground.red, foreground.green, foreground.blue, 0.25);
        cairo_move_to(cr, x, top);
        cairo_line_to(cr, x, top + height);
        cairo_stroke(cr);
        gchar *value = i == 0 ? g_strdup_printf(_("%u seconds"), 60u) :
                               g_strdup_printf("%d", 60 - i * 10);
        text(value, x, top + height + 3, i == 6);
        g_free(value);
    }
    cairo_rectangle(cr, left, top, width, height);
    cairo_clip(cr);
    cairo_set_source_rgb(cr, 0.30, 0.65, 0.12);
    cairo_set_line_width(cr, 1.5);
    bool connected = false;
    gint64 previous = 0;
    for (const auto& point : history.points) {
        if (point.second < 0) {
            connected = false;
            continue;
        }
        const double x = left + width * (1.0 - double(now - point.first) / (60 * G_USEC_PER_SEC));
        const double y = top + height * (1.0 - point.second / 100);
        if (connected && point.first - previous <= 5 * G_USEC_PER_SEC)
            cairo_line_to(cr, x, y);
        else
            cairo_move_to(cr, x, y);
        connected = true;
        previous = point.first;
    }
    cairo_stroke(cr);
    cairo_restore(cr);
    g_object_unref(layout);
    return FALSE;
}

struct Row {
    GtkWidget *box;
    GtkWidget *name;
    GtkWidget *usage;
    GtkWidget *graph;
    History *history;
    GtkWidget *memory;
    GtkWidget *temperature;
};

struct Monitor {
    GtkWidget *resources = nullptr;
    GtkWidget *section = nullptr;
    GtkWidget *rows_box = nullptr;
    std::map<std::string, Row> rows;
    GSubprocess *process = nullptr;
    GCancellable *cancel = nullptr;
    guint timer = 0;
    guint deadline = 0;
    gint64 next_query = 0;
    bool stopped = false;

    ~Monitor()
    {
        g_clear_object(&process);
        g_clear_object(&cancel);
    }
};
using State = std::shared_ptr<Monitor>;

GtkWidget *label(const char *text)
{
    GtkWidget *widget = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(widget), 0);
    gtk_label_set_ellipsize(GTK_LABEL(widget), PANGO_ELLIPSIZE_END);
    return widget;
}

Row make_row(Monitor& state)
{
    Row row;
    row.box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    row.name = label("");
    row.usage = label("");
    row.graph = gtk_drawing_area_new();
    gtk_widget_set_size_request(row.graph, -1, 110);
    row.history = new History;
    g_object_set_data_full(G_OBJECT(row.graph), "gpu-history", row.history,
                          [](gpointer data) { delete static_cast<History *>(data); });
    g_signal_connect(row.graph, "draw", G_CALLBACK(draw_history), row.history);
    row.memory = gtk_progress_bar_new();
    row.temperature = label("");
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(row.memory), TRUE);
    for (auto widget : {row.name, row.graph, row.usage, row.memory, row.temperature})
        gtk_box_pack_start(GTK_BOX(row.box), widget, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(state.rows_box), row.box, FALSE, FALSE, 0);
    gtk_widget_show_all(row.box);
    return row;
}

void update(Monitor& state, const std::vector<Nvidia::Sample>& samples)
{
    std::set<std::string> present;
    for (const auto& sample : samples) {
        present.insert(sample.uuid);
        auto found = state.rows.find(sample.uuid);
        if (found == state.rows.end())
            found = state.rows.emplace(sample.uuid, make_row(state)).first;
        Row& row = found->second;
        gtk_label_set_text(GTK_LABEL(row.name), sample.name.c_str());
        gtk_widget_set_tooltip_text(row.box, sample.uuid.c_str());
        gchar *text = sample.utilization < 0 ? g_strdup(_("GPU utilization: unavailable")) :
            g_strdup_printf(_("GPU utilization: %.0f%%"), sample.utilization);
        gtk_label_set_text(GTK_LABEL(row.usage), text);
        row.history->append(g_get_monotonic_time(), sample.utilization);
        gtk_widget_queue_draw(row.graph);
        g_free(text);
        const bool memory_valid = sample.memory_used >= 0 && sample.memory_total > 0;
        text = memory_valid ? g_strdup_printf(_("VRAM: %.0f / %.0f MiB"),
                                              sample.memory_used, sample.memory_total) :
                              g_strdup(_("VRAM: unavailable"));
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(row.memory), text);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(row.memory),
                                      memory_valid ? sample.memory_used / sample.memory_total : 0);
        g_free(text);
        text = sample.temperature < 0 ? g_strdup(_("Temperature: unavailable")) :
            g_strdup_printf(_("Temperature: %.0f °C"), sample.temperature);
        gtk_label_set_text(GTK_LABEL(row.temperature), text);
        g_free(text);
    }
    for (auto it = state.rows.begin(); it != state.rows.end();) {
        if (!present.count(it->first)) {
            gtk_widget_destroy(it->second.box);
            it = state.rows.erase(it);
        } else {
            ++it;
        }
    }
    gtk_widget_set_visible(state.section, !samples.empty());
}

gboolean timed_out(gpointer data)
{
    auto& state = *static_cast<Monitor *>(data);
    state.deadline = 0;
    // Cancellation releases the asynchronous operation even if driver I/O hangs.
    g_subprocess_force_exit(state.process);
    g_cancellable_cancel(state.cancel);
    return G_SOURCE_REMOVE;
}

void completed(GObject *source, GAsyncResult *result, gpointer data)
{
    std::unique_ptr<State> holder(static_cast<State *>(data));
    Monitor& state = **holder;
    gchar *output = nullptr;
    GError *error = nullptr;
    const bool ok = g_subprocess_communicate_utf8_finish(G_SUBPROCESS(source), result,
                                                        &output, nullptr, &error);
    if (state.deadline) {
        g_source_remove(state.deadline);
        state.deadline = 0;
    }
    if (!state.stopped) {
        const auto samples = ok && g_subprocess_get_successful(G_SUBPROCESS(source)) ?
            Nvidia::parse_samples(output ? output : "") : std::vector<Nvidia::Sample>();
        update(state, samples); // Clear stale metrics on any failure.
        state.next_query = g_get_monotonic_time() +
                           (samples.empty() ? 30 : 2) * G_USEC_PER_SEC;
    }
    g_free(output);
    g_clear_error(&error);
    g_clear_object(&state.process);
    g_clear_object(&state.cancel);
}

gboolean poll(gpointer data)
{
    State state = *static_cast<State *>(data);
    if (state->stopped || !gtk_widget_get_mapped(state->resources))
        return G_SOURCE_CONTINUE;
    for (const auto& row : state->rows)
        gtk_widget_queue_draw(row.second.graph);
    if (state->process || g_get_monotonic_time() < state->next_query)
        return G_SOURCE_CONTINUE;

    gchar *program = g_find_program_in_path("nvidia-smi");
    if (!program) {
        update(*state, {});
        state->next_query = g_get_monotonic_time() + 30 * G_USEC_PER_SEC;
        return G_SOURCE_CONTINUE;
    }
    GSubprocessLauncher *launcher = g_subprocess_launcher_new(
        static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                      G_SUBPROCESS_FLAGS_STDERR_SILENCE));
    g_subprocess_launcher_setenv(launcher, "LC_ALL", "C", TRUE);
    GError *error = nullptr;
    state->process = g_subprocess_launcher_spawn(launcher, &error, program,
        "--query-gpu=uuid,utilization.gpu,memory.used,memory.total,temperature.gpu,name",
        "--format=csv,noheader,nounits", nullptr);
    g_free(program);
    g_object_unref(launcher);
    if (!state->process) {
        g_clear_error(&error);
        update(*state, {});
        state->next_query = g_get_monotonic_time() + 30 * G_USEC_PER_SEC;
        return G_SOURCE_CONTINUE;
    }
    state->cancel = g_cancellable_new();
    state->deadline = g_timeout_add_seconds(5, timed_out, state.get());
    g_subprocess_communicate_utf8_async(state->process, nullptr, state->cancel,
                                       completed, new State(state));
    return G_SOURCE_CONTINUE;
}

void destroyed(GtkWidget *, gpointer data)
{
    auto& state = **static_cast<State *>(data);
    state.stopped = true;
    if (state.timer) {
        g_source_remove(state.timer);
        state.timer = 0;
    }
    if (state.deadline) {
        g_source_remove(state.deadline);
        state.deadline = 0;
    }
    if (state.process) {
        g_subprocess_force_exit(state.process);
        g_cancellable_cancel(state.cancel);
    }
}
}

void gpu_monitor_attach(GtkWidget *resources)
{
    if (g_strcmp0(g_getenv("MATE_SYSTEM_MONITOR_DISABLE_NVIDIA"), "1") == 0)
        return;
    State state = std::make_shared<Monitor>();
    state->resources = resources;
    state->section = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *heading = label(_("NVIDIA GPU History"));
    PangoAttrList *attributes = pango_attr_list_new();
    pango_attr_list_insert(attributes, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes(GTK_LABEL(heading), attributes);
    pango_attr_list_unref(attributes);
    gtk_box_pack_start(GTK_BOX(state->section), heading, FALSE, FALSE, 0);
    gtk_widget_show(heading);
    // Parent show_all() must not reveal the section before successful detection.
    gtk_widget_set_no_show_all(state->section, TRUE);
    state->rows_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_box_pack_start(GTK_BOX(state->section), state->rows_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(resources), state->section, FALSE, FALSE, 0);
    gtk_box_reorder_child(GTK_BOX(resources), state->section, 1);
    gtk_widget_show(state->rows_box);
    auto holder = new State(state);
    g_object_set_data_full(G_OBJECT(resources), "nvidia-monitor", holder,
                          [](gpointer data) { delete static_cast<State *>(data); });
    g_signal_connect(resources, "destroy", G_CALLBACK(destroyed), holder);
    state->timer = g_timeout_add_seconds(1, poll, holder);
}
