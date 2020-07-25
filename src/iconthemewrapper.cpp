#include <config.h>

#include <gtkmm/icontheme.h>
#include <giomm/error.h>

#include "iconthemewrapper.h"


Glib::RefPtr<Gdk::Pixbuf>
procman::IconThemeWrapper::load_icon(const Glib::ustring& icon_name, int size) const
{
    gint scale = gdk_window_get_scale_factor (gdk_get_default_root_window ());
    try
    {
      return Gtk::IconTheme::get_default()->load_icon(icon_name, size, scale, Gtk::ICON_LOOKUP_USE_BUILTIN | Gtk::ICON_LOOKUP_FORCE_SIZE);
    }
    catch (Gtk::IconThemeError &error)
    {
        if (error.code() != Gtk::IconThemeError::ICON_THEME_NOT_FOUND)
            g_error("Cannot load icon '%s' from theme: %s", icon_name.c_str(), error.what().c_str());
        return Glib::RefPtr<Gdk::Pixbuf>();
    }
    catch (Gio::Error &error)
    {
        g_debug("Could not load icon '%s' : %s", icon_name.c_str(), error.what().c_str());
        return Glib::RefPtr<Gdk::Pixbuf>();
    }
}

Glib::RefPtr<Gdk::Pixbuf>
procman::IconThemeWrapper::load_gicon(const Glib::RefPtr<Gio::Icon>& gicon,
                                      int size, Gtk::IconLookupFlags flags) const
{
    Gtk::IconInfo icon_info;
    gint scale = gdk_window_get_scale_factor (gdk_get_default_root_window ());
    icon_info = Gtk::IconTheme::get_default()->lookup_icon(gicon, size, scale, flags);

    if (!icon_info) {
        return Glib::RefPtr<Gdk::Pixbuf>();
    }

    try
    {
        return icon_info.load_icon();
    }
    catch (Gtk::IconThemeError &error)
    {
        if (error.code() != Gtk::IconThemeError::ICON_THEME_NOT_FOUND)
            g_error("Cannot load gicon from theme: %s", error.what().c_str());
        return Glib::RefPtr<Gdk::Pixbuf>();
    }
    catch (Gio::Error &error)
    {
        g_debug("Could not load gicon: %s", error.what().c_str());
        return Glib::RefPtr<Gdk::Pixbuf>();
    }
}
