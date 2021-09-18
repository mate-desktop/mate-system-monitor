#ifndef H_PROCMAN_ICONTHEMEWRAPPER_H_1185707711
#define H_PROCMAN_ICONTHEMEWRAPPER_H_1185707711

#include <gtk/gtk.h>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <gdkmm/pixbuf.h>

namespace procman
{
    class IconThemeWrapper
    {
      public:
        // returns 0 instead of raising an exception
        Glib::RefPtr<Gdk::Pixbuf>
            load_icon(const Glib::ustring& icon_name, int size) const;
        Glib::RefPtr<Gdk::Pixbuf>
            load_gicon(const Glib::RefPtr<Gio::Icon>& gicon, int size, GtkIconLookupFlags flags) const;

        const IconThemeWrapper* operator->() const
        { return this; }
    };
}

#endif // H_PROCMAN_ICONTHEMEWRAPPER_H_1185707711
