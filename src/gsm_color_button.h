/*
 * Mate system monitor color pickers
 * Copyright (C) 2007 Karl Lattimer <karl@qdh.org.uk>
 * All rights reserved.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with the software; see the file COPYING. If not,
 * write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GSM_COLOR_BUTTON_H__
#define __GSM_COLOR_BUTTON_H__

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* The GtkColorSelectionButton widget is a simple color picker in a button.
 * The button displays a sample of the currently selected color. When
 * the user clicks on the button, a color selection dialog pops up.
 * The color picker emits the "color_set" signal when the color is set.
 */
#define GSM_TYPE_COLOR_BUTTON            (gsm_color_button_get_type ())
G_DECLARE_DERIVABLE_TYPE (GSMColorButton, gsm_color_button, GSM, COLOR_BUTTON, GtkDrawingArea)

/* Widget types */
enum
{
    GSMCP_TYPE_CPU,
    GSMCP_TYPE_PIE,
    GSMCP_TYPE_NETWORK_IN,
    GSMCP_TYPE_NETWORK_OUT,
    GSMCP_TYPES
};

struct _GSMColorButtonClass
{
    GtkWidgetClass parent_class;

    void (*color_set) (GSMColorButton * cp);

    /* Padding for future expansion */
    void (*_gtk_reserved1) (void);
    void (*_gtk_reserved2) (void);
    void (*_gtk_reserved3) (void);
    void (*_gtk_reserved4) (void);
};

GtkWidget *gsm_color_button_new (const GdkRGBA * color, guint type);
void gsm_color_button_set_color (GSMColorButton * color_button, const GdkRGBA * color);
void gsm_color_button_set_fraction (GSMColorButton * color_button, const gdouble fraction);
void gsm_color_button_set_cbtype (GSMColorButton * color_button, guint type);
void gsm_color_button_get_color (GSMColorButton * color_button, GdkRGBA * color);
gdouble gsm_color_button_get_fraction (GSMColorButton * color_button);
guint gsm_color_button_get_cbtype (GSMColorButton * color_button);
void gsm_color_button_set_title (GSMColorButton * color_button, const gchar * title);
const gchar* gsm_color_button_get_title(GSMColorButton* color_button);

G_END_DECLS
#endif /* __GSM_COLOR_BUTTON_H__ */
