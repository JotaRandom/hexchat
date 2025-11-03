common_sources = [
  'cfgfiles.c',
  'chanopt.c',
  'ctcp.c',
  'dcc.c',
  'hexchat.c',
  'history.c',
  'ignore.c',
  'inbound.c',
  'modes.c',
  'network.c',
  'notify.c',
  'outbound.c',
  'plugin.c',
  'plugin-identd.c',
  'plugin-timer.c',
  'proto-irc.c',
  'scram.c',
  'server.c',
  'servlist.c',
	'text.c',
  'tree.c',
  'url.c',
  'userlist.c',
  'util.c'
]

common_sysinfo_deps = []

# Define dependencies
common_deps = [
  dependency('gio-2.0'),
  dependency('gmodule-2.0'),
  dependency('glib-2.0'),
]

# SSL/TLS support
libssl_dep = dependency('openssl', required: false)
if libssl_dep.found()
  common_deps += libssl_dep
endif

# DBus GLib support
dbus_glib_dep = dependency('dbus-glib-1', required: false)

# Add libcanberra if available
if libcanberra_enabled
  common_deps += dependency('libcanberra')
endif

# Add global deps if defined
if is_variable('global_deps')
  common_deps += global_deps
endif

# GModule dependency
libgmodule_dep = dependency('gmodule-2.0')

common_includes = [
  config_h_include,
  include_directories('.')
]

common_cflags = [
  '-DHAVE_CONFIG_H',
]

if host_machine.system() == 'windows'
  common_deps += [
    cc.find_library('ws2_32'), # winsock
    cc.find_library('winmm'), # playsound
  ]
  common_sysinfo_deps += [
    cc.find_library('wbemuuid'), # sysinfo
  ]

  common_sources += 'sysinfo/win32/backend.c'
  common_includes += include_directories('sysinfo')
endif

marshal = gnome.genmarshal('marshal',
  sources: 'marshalers.list',
  prefix: '_hexchat_marshal',
  internal: true
)

make_te = find_program('make-te.py')

textevents = custom_target('textevents',
  input: 'textevents.in',
  output: ['textevents.h', 'textenums.h'],
  command: [make_te, '@INPUT@', '@OUTPUT0@', '@OUTPUT1@']
)

# TODO:
#   LOOKUPD
#   SIGACTION
#   HAVE_GTK_MAC

if libssl_dep.found()
  common_sources += 'ssl.c'
  message('Building with SSL support')
endif

if dbus_glib_dep.found()
  if run_command('test', '-d', 'dbus', check: false).returncode() == 0
    subdir('dbus')
    if is_variable('hexchat_dbus_dep')
      common_deps += hexchat_dbus_dep
      common_includes += include_directories('dbus')
      message('Building with DBus support')
    endif
  else
    message('DBus directory not found, DBus support disabled')
  endif
endif

if get_option('plugin')
  common_deps += libgmodule_dep
  install_headers('hexchat-plugin.h')
  message('Building with plugin support')
endif

hexchat_common = static_library('hexchatcommon',
  sources: [textevents] + marshal + common_sources,
  include_directories: config_h_include,
  dependencies: common_deps + common_sysinfo_deps,
  c_args: common_cflags,
  pic: true
)

# Define GIO dependency
libgio_dep = dependency('gio-2.0')

hexchat_common_dep = declare_dependency(
  sources: [textevents] + marshal,
  link_with: hexchat_common,
  include_directories: common_includes,
  compile_args: common_cflags,
  dependencies: [libgio_dep] + common_deps,
)

hexchat_plugin_dep = declare_dependency(
  include_directories: common_includes,
  compile_args: common_cflags,
  dependencies: common_deps,
)
