// -*- c++ -*-

#ifndef _PROCMAN_PRETTYTABLE_H_
#define _PROCMAN_PRETTYTABLE_H_

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glibmm/refptr.h>
#include <gdkmm/pixbuf.h>

#include <map>
#include <string>

extern "C" {
#include <libmatewnck/libmatewnck.h>
}

#include "iconthemewrapper.h"

class ProcInfo;

using std::string;



class PrettyTable
{
 public:
  PrettyTable();
  ~PrettyTable();

  void set_icon(ProcInfo &);

private:

  static void on_application_opened(MatewnckScreen* screen, MatewnckApplication* app, gpointer data);
  static void on_application_closed(MatewnckScreen* screen, MatewnckApplication* app, gpointer data);

  void register_application(pid_t pid, Glib::RefPtr<Gdk::Pixbuf> icon);
  void unregister_application(pid_t pid);


  Glib::RefPtr<Gdk::Pixbuf> get_icon_from_theme(const ProcInfo &);
  Glib::RefPtr<Gdk::Pixbuf> get_icon_from_default(const ProcInfo &);
  Glib::RefPtr<Gdk::Pixbuf> get_icon_from_matewnck(const ProcInfo &);
  Glib::RefPtr<Gdk::Pixbuf> get_icon_from_name(const ProcInfo &);
  Glib::RefPtr<Gdk::Pixbuf> get_icon_for_kernel(const ProcInfo &);
  Glib::RefPtr<Gdk::Pixbuf> get_icon_dummy(const ProcInfo &);

  bool get_default_icon_name(const string &cmd, string &name);

  typedef std::map<string, Glib::RefPtr<Gdk::Pixbuf> > IconCache;
  typedef std::map<pid_t, Glib::RefPtr<Gdk::Pixbuf> > IconsForPID;

  IconsForPID apps;
  IconCache defaults;
  procman::IconThemeWrapper theme;
};


#endif /* _PROCMAN_PRETTYTABLE_H_ */
