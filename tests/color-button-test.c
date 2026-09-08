/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <gtk/gtk.h>
#include <stdint.h>
#include "gsm_color_button.h"

static void assert_color(GtkWidget *button)
{
    int width = gtk_widget_get_allocated_width(button);
    int height = gtk_widget_get_allocated_height(button);
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create(surface);
    gtk_widget_draw(button, cr);
    cairo_surface_flush(surface);
    unsigned char *data = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    uint32_t pixel = *(uint32_t *)(data + (height / 2) * stride + (width / 2) * 4);
    // The CPU swatch must paint into GTK's supplied surface, not its GdkWindow.
    g_assert_cmphex(pixel, ==, 0xffff0000);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

int main(int argc, char **argv)
{
    if (!gtk_init_check(&argc, &argv))
        return 77;
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GdkRGBA red = {1, 0, 0, 1};
    GtkWidget *button = gsm_color_button_new(&red, GSMCP_TYPE_CPU);
    GtkWidget *spacer = gtk_drawing_area_new();
    gtk_widget_set_size_request(spacer, 100, 1000);
    gtk_container_add(GTK_CONTAINER(window), scroll);
    gtk_container_add(GTK_CONTAINER(scroll), box);
    gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), spacer, FALSE, FALSE, 0);
    gtk_window_set_default_size(GTK_WINDOW(window), 200, 200);
    gtk_widget_show_all(window);
    while (g_main_context_iteration(NULL, FALSE)) {}
    assert_color(button);
    GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroll));
    gtk_adjustment_set_value(adjustment, 500);
    while (g_main_context_iteration(NULL, FALSE)) {}
    gtk_adjustment_set_value(adjustment, 0);
    gtk_widget_queue_draw(button);
    while (g_main_context_iteration(NULL, FALSE)) {}
    assert_color(button);
    gtk_widget_destroy(window);
    g_print("Color button redraw test passed\n");
    return 0;
}
