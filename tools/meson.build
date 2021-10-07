commands = [
  'renice',
  'kill',
]

foreach command : commands
  executable('gsm-' + command,
    'msm_execute_helper.c',
    dependencies: [
      glib,
    ],
    c_args: '-DCOMMAND="@0@"'.format(command),
    install: true,
    install_dir: join_paths(get_option('libexecdir'), meson.project_name())
  )
endforeach
