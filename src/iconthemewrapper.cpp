#include <config.h>

#include <gtk/gtk.h>

#include "iconthemewrapper.h"


Glib::RefPtr<Gdk::Pixbuf>
procman::IconThemeWrapper::load_icon(const Glib::ustring& icon_name, int size) const
{
    gint scale = gdk_window_get_scale_factor (gdk_get_default_root_window ());
    GError * error = NULL;
    GdkPixbuf * icon = gtk_icon_theme_load_icon_for_scale (gtk_icon_theme_get_default (), icon_name.c_str(), size, scale, static_cast<GtkIconLookupFlags>(GTK_ICON_LOOKUP_USE_BUILTIN | GTK_ICON_LOOKUP_FORCE_SIZE), &error);
    if (error != NULL) {
        if (error->domain == GTK_ICON_THEME_ERROR) {
            if (error->code != GTK_ICON_THEME_NOT_FOUND)
                g_error("Cannot load icon '%s' from theme: %s", icon_name.c_str(), error->message);
        }
        else {
            g_debug("Could not load icon '%s' : %s", icon_name.c_str(), error->message);
        }
        g_error_free (error);
        return Glib::RefPtr<Gdk::Pixbuf>();
    }
    return Glib::wrap(icon);
}

Glib::RefPtr<Gdk::Pixbuf>
procman::IconThemeWrapper::load_gicon(const Glib::RefPtr<Gio::Icon>& gicon,
                                      int size, GtkIconLookupFlags flags) const
{
    gint scale = gdk_window_get_scale_factor (gdk_get_default_root_window ());
    GtkIconInfo * icon_info = gtk_icon_theme_lookup_by_gicon_for_scale (gtk_icon_theme_get_default (), gicon->gobj(), size, scale, flags);

    if (icon_info == NULL) {
        return Glib::RefPtr<Gdk::Pixbuf>();
    }

    GError * error = NULL;
    GdkPixbuf * icon = gtk_icon_info_load_icon (icon_info, &error);
    if (error != NULL) {
        if (error->domain == GTK_ICON_THEME_ERROR) {
            if (error->code != GTK_ICON_THEME_NOT_FOUND)
                g_error("Cannot load gicon from theme: %s", error->message);
        }
        else {
            g_debug("Could not load gicon: %s", error->message);
        }
        g_object_unref (icon_info);
        g_error_free (error);
        return Glib::RefPtr<Gdk::Pixbuf>();
    }
    g_object_unref (icon_info);
    return Glib::wrap(icon);
}
