system_monitor_sources = []

msm_resource = gnome.compile_resources(
  'msm',
  'msm.gresource.xml',
  source_dir: 'src',
)

system_monitor_sources += [
  'argv.cpp',
  'procman.cpp',
  'interface.cpp',
  'gpu-monitor.cpp',
  'nvidia-smi.cpp',
  'callbacks.cpp',
  'load-graph.cpp',
  'proctable.cpp',
  'prettytable.cpp',
  'util.cpp',
  'procactions.cpp',
  'procdialogs.cpp',
  'memmaps.cpp',
  'openfiles.cpp',
  'procproperties.cpp',
  'smooth_refresh.cpp',
  'disks.cpp',
  'selinux.cpp',
  'cgroups.cpp',
  'procman_gksu.cpp',
  'procman_pkexec.cpp',
  'sysinfo.cpp',
  'lsof.cpp',
  'selection.cpp',
  'settings-keys.cpp',
  'iconthemewrapper.cpp',
  'procman-app.cpp',
  'gsm_color_button.c',
   msm_resource
]

system_monitor_headers = [
  'argv.h',
  'callbacks.h',
  'cgroups.h',
  'defaulttable.h',
  'disks.h',
  'gsm_color_button.h',
  'iconthemewrapper.h',
  'interface.h',
  'gpu-monitor.h',
  'nvidia-smi.h',
  'load-graph.h',
  'lsof.h',
  'memmaps.h',
  'openfiles.h',
  'prettytable.h',
  'procactions.h',
  'procdialogs.h',
  'procman-app.h',
  'procman_gksu.h',
  'procman.h',
  'procman_pkexec.h',
  'procproperties.h',
  'proctable.h',
  'selection.h',
  'selinux.h',
  'settings-keys.h',
  'smooth_refresh.h',
  'sysinfo.h',
  'util.h',
]

msm_schemas = configure_file(
  input : 'org.mate.system-monitor.gschema.xml.in',
  output: 'org.mate.system-monitor.gschema.xml',
  configuration: dataconf,
  install: true,
  install_dir: join_paths(get_option('datadir'), 'glib-2.0', 'schemas'),
)

msm_gsettings = gnome.mkenums('org.mate.mate-system-monitor.enums.xml',
  sources: system_monitor_headers,
  comments: '<!-- @comment@ -->',
  fhead:    '<schemalist>',
  vhead:    '  <@type@ id=\'org.mate.mate-system-monitor.@EnumName@\'>',
  vprod:    '    <value nick=\'@valuenick@\' value=\'@valuenum@\'/>',
  vtail:    '  </@type@>',
  ftail:    '</schemalist>',
  install_header: true,
  install_dir: join_paths(get_option('datadir'), 'glib-2.0', 'schemas'),
)

executable(meson.project_name(),
  system_monitor_sources,
  include_directories: rootInclude,
  dependencies: [
    gmodule,
    gtkmm,
    libgtop,
    libsystemd,
    libwnck,
    libxml,
    librsvg,
  ],
  install: true,
  install_dir : get_option('bindir')
)

nvidia_parser_test = executable('nvidia-smi-test',
  '../tests/nvidia-smi-test.cpp', 'nvidia-smi.cpp',
  include_directories: include_directories('.'),
)
test('nvidia-smi-parser', nvidia_parser_test)

nvidia_ui_test = executable('gpu-monitor-test',
  '../tests/gpu-monitor-test.cpp', 'nvidia-smi.cpp',
  include_directories: [rootInclude, include_directories('.')],
  dependencies: [gtk3, dependency('gio-2.0')],
)
test('nvidia-gpu-ui', nvidia_ui_test, timeout: 30)

color_button_test = executable('color-button-test',
  '../tests/color-button-test.c', 'gsm_color_button.c',
  include_directories: [rootInclude, include_directories('.')],
  dependencies: [gtk3, librsvg],
  link_args: ['-lm'],
)
test('color-button-redraw', color_button_test)
