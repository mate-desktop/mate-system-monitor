/* SPDX-License-Identifier: GPL-2.0-or-later */
// Include the controller to exercise its private lifecycle with a fake executable.
#include "gpu-monitor.cpp"
#include <glib/gstdio.h>

static void drain(const State& state)
{
    const gint64 limit = g_get_monotonic_time() + 8 * G_USEC_PER_SEC;
    while (state->process && g_get_monotonic_time() < limit) {
        while (g_main_context_iteration(nullptr, FALSE)) {}
        g_usleep(1000);
    }
    g_assert_null(state->process);
}

int main(int argc, char **argv)
{
    g_setenv("LC_ALL", "C", TRUE);
    g_setenv("LANGUAGE", "C", TRUE);
    if (!gtk_init_check(&argc, &argv))
        return 77; // Use xvfb-run on headless builders.
    History history;
    history.append(0, 10);
    history.append(30 * G_USEC_PER_SEC, -1);
    history.append(61 * G_USEC_PER_SEC, 90);
    g_assert_cmpuint(history.points.size(), ==, 2);
    g_assert_cmpfloat(history.points.front().second, ==, -1);
    for (int i = 0; i < 200; ++i)
        history.append(61 * G_USEC_PER_SEC + i, 50);
    g_assert_cmpuint(history.points.size(), ==, 128);
    g_unsetenv("MATE_SYSTEM_MONITOR_DISABLE_NVIDIA");
    gchar *dir = g_dir_make_tmp("msm-gpu-test-XXXXXX", nullptr);
    g_assert_nonnull(dir);
    gchar *program = g_build_filename(dir, "nvidia-smi", nullptr);
    g_setenv("PATH", dir, TRUE);
    auto script = [&](const char *body) {
        g_assert_true(g_file_set_contents(program, body, -1, nullptr));
        g_assert_cmpint(g_chmod(program, 0700), ==, 0);
    };
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget *resources = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), resources);
    GtkWidget *cpu = gtk_label_new("CPU History");
    GtkWidget *memory = gtk_label_new("Memory and Swap History");
    gtk_box_pack_start(GTK_BOX(resources), cpu, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(resources), memory, FALSE, FALSE, 0);
    gpu_monitor_attach(resources);
    gtk_widget_show_all(window);
    State state = *static_cast<State *>(g_object_get_data(G_OBJECT(resources), "nvidia-monitor"));
    GList *children = gtk_container_get_children(GTK_CONTAINER(resources));
    g_assert_true(g_list_nth_data(children, 0) == cpu);
    g_assert_true(g_list_nth_data(children, 1) == state->section);
    g_assert_true(g_list_nth_data(children, 2) == memory);
    g_list_free(children);
    g_source_remove(state->timer);
    state->timer = 0; // Tests drive polls deterministically.
    auto query = [&]() { state->next_query = 0; poll(&state); drain(state); };
    g_assert_false(gtk_widget_get_visible(state->section));
    query(); // No executable.
    g_assert_true(state->rows.empty());
    script("#!/bin/sh\nprintf 'GPU-a, 37, 4096, 8192, 61, First\\nGPU-b, N/A, 0, 4096, N/A, Second\\n'\n");
    query();
    g_assert_cmpuint(state->rows.size(), ==, 2);
    g_assert_true(gtk_widget_get_visible(state->section));
    GtkWidget *first = state->rows.at("GPU-a").box;
    g_assert_cmpuint(state->rows.at("GPU-a").history->points.size(), ==, 1);
    g_assert_cmpfloat(state->rows.at("GPU-a").history->points.back().second, ==, 37);
    g_assert_cmpfloat(state->rows.at("GPU-b").history->points.back().second, ==, -1);
    g_assert_cmpfloat(gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(state->rows.at("GPU-a").memory)), ==, 0.5);
    g_assert_cmpstr(gtk_label_get_text(GTK_LABEL(state->rows.at("GPU-b").usage)), ==, "GPU utilization: unavailable");
    script("#!/bin/sh\nprintf 'GPU-b, 1, 0, 4096, 40, Second\\nGPU-a, 2, 1, 8192, 50, First\\n'\n");
    query();
    g_assert_cmpuint(state->rows.at("GPU-a").history->points.size(), ==, 2);
    g_assert_true(first == state->rows.at("GPU-a").box); // Stable identity after reorder.
    gtk_widget_hide(resources);
    state->next_query = 0;
    poll(&state);
    g_assert_null(state->process);
    gtk_widget_show(resources);
    script("#!/bin/sh\nprintf 'GPU-a, 2, 1, 8192, 50, First\\n'\n");
    query();
    g_assert_cmpuint(state->rows.size(), ==, 1); // Device removal.
    script("#!/bin/sh\necho driver-failure >&2\nexit 9\n");
    query();
    g_assert_true(state->rows.empty());
    g_assert_false(gtk_widget_get_visible(state->section));
    g_assert_cmpint(state->next_query - g_get_monotonic_time(), >, 25 * G_USEC_PER_SEC);
    script("#!/bin/sh\nprintf 'GPU-a, 0, 0, 8192, 45, Recovered\\n'\n");
    query();
    g_assert_cmpuint(state->rows.size(), ==, 1);
    script("#!/bin/sh\nexec /bin/sleep 30\n");
    state->next_query = 0;
    poll(&state);
    GSubprocess *pending = state->process;
    poll(&state);
    g_assert_true(pending == state->process); // No overlapping queries.
    drain(state); // Actual five-second timeout and cancellation.
    g_assert_true(state->rows.empty());
    state->next_query = 0;
    poll(&state);
    gtk_widget_destroy(window); // Destroy during an asynchronous query.
    drain(state);
    g_assert_true(state->stopped);
    g_assert_cmpuint(state->deadline, ==, 0);
    g_setenv("MATE_SYSTEM_MONITOR_DISABLE_NVIDIA", "1", TRUE);
    resources = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    g_object_ref_sink(resources);
    gpu_monitor_attach(resources);
    g_assert_null(g_object_get_data(G_OBJECT(resources), "nvidia-monitor"));
    gtk_widget_destroy(resources);
    g_object_unref(resources);
    g_remove(program);
    g_rmdir(dir);
    g_free(program);
    g_free(dir);
    g_print("NVIDIA UI/lifecycle tests passed\n");
}
