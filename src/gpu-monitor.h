/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MSM_GPU_MONITOR_H
#define MSM_GPU_MONITOR_H
#include <gtk/gtk.h>

// Lifetime and polling are tied to the Resources container.
void gpu_monitor_attach(GtkWidget *resources);
#endif
