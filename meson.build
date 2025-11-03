project('hexchat', 'c',
  version: '2.16.2',
  meson_version: '>= 0.47.0',
  default_options: [
    'c_std=gnu89',
    'buildtype=debugoptimized',
    'warning_level=1',
  ]
)

i18n = import('i18n')
gnome = import('gnome')
cc = meson.get_compiler('c')


# Required GTK3 dependencies with version checks
libgtk_dep = dependency('gtk+-3.0', version: '>= 3.22.0',
  required: true,
  not_found_message: 'GTK3 development libraries not found. On Debian/Ubuntu, install libgtk-3-dev. On Fedora, install gtk3-devel. On Arch, install gtk3.'
)

# Core GTK components
libgdk_dep = dependency('gdk-3.0', version: '>= 3.22.0', required: true)
libgdk_pixbuf_dep = dependency('gdk-pixbuf-2.0', version: '>= 2.32.0', required: true)
libgio_dep = dependency('gio-2.0', version: '>= 2.56.0', required: true)
libgmodule_dep = dependency('gmodule-2.0', required: true)
libpango_dep = dependency('pango', version: '>= 1.38.0', required: true)
libatk_dep = dependency('atk', version: '>= 2.20.0', required: true)
libcairo_dep = dependency('cairo', version: '>= 1.14.0', required: true)
libglib_dep = dependency('glib-2.0', version: '>= 2.56.0', required: true)
libgobject_dep = dependency('gobject-2.0', version: '>= 2.56.0', required: true)

# Optional dependencies with proper fallbacks
libcanberra_dep = dependency('libcanberra-gtk3', version: '>= 0.30',
  required: get_option('libcanberra'),
  disabler: true
)

dbus_glib_dep = dependency('dbus-glib-1',
  required: get_option('dbus'),
  disabler: true
)

# Plugin dependencies
luajit_dep = dependency('', required: false)
python3_dep = dependency('', required: false)
perl_dep = dependency('', required: false)

# Only require the dependencies if both plugin support and the specific feature are enabled
if get_option('plugin')
  lua_opt = get_option('with-lua')
  if lua_opt != 'false'
    luajit_dep = dependency(lua_opt, version: '>= 2.0.0', required: true)
  endif
  
  python_opt = get_option('with-python')
  if python_opt != 'false'
    python3_dep = dependency(python_opt, required: true)
  endif
  
  perl_opt = get_option('with-perl')
  if perl_opt != 'false'
    perl_dep = dependency(perl_opt, required: false)
    if perl_dep.found()
      config_h.set('HAVE_PERL', 1)
    endif
  endif
endif

# Global dependencies
global_deps = [
  libgtk_dep,
  libgdk_dep,
  libgdk_pixbuf_dep,
  libgio_dep,
  libgmodule_dep,
  libpango_dep,
  libatk_dep,
  libcairo_dep,
  libglib_dep,
  libgobject_dep,
  dependency('gthread-2.0'),
  dependency('x11'),
  dependency('xext')
]
if cc.get_id() == 'msvc'
  libssl_dep = cc.find_library('libssl')
else
  libssl_dep = dependency('openssl', version: '>= 0.9.8',
                          required: get_option('tls'))
endif

config_h = configuration_data()
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
config_h.set_quoted('PACKAGE_NAME', meson.project_name())
config_h.set_quoted('GETTEXT_PACKAGE', 'hexchat')
config_h.set_quoted('LOCALEDIR', join_paths(get_option('prefix'),
                                 get_option('datadir'), 'locale'))
config_h.set_quoted('G_LOG_DOMAIN', 'hexchat')
config_h.set10('ENABLE_NLS', true)

# Optional features
config_h.set('USE_OPENSSL', libssl_dep.found())
config_h.set('USE_LIBCANBERRA', libcanberra_dep.found())
config_h.set('USE_DBUS', dbus_glib_dep.found())
config_h.set('USE_PLUGIN', get_option('plugin'))
config_h.set('HAVE_LUA', luajit_dep.found())
config_h.set('HAVE_PYTHON', python3_dep.found())
config_h.set('HAVE_PERL', perl_dep.found())

# GTK3 configuration
config_h.set('G_DISABLE_SINGLE_INCLUDES', true)
config_h.set('GDK_DISABLE_DEPRECATED', true)
config_h.set('GTK_DISABLE_DEPRECATED', true)
config_h.set('GTK_DISABLE_SINGLE_INCLUDES', true)
config_h.set('GDK_PIXBUF_DISABLE_DEPRECATED', true)
config_h.set('GDK_PIXBUF_DISABLE_SINGLE_INCLUDES', true)
config_h.set('GLIB_VERSION_MAX_ALLOWED', 'GLIB_VERSION_2_56')
config_h.set('GLIB_VERSION_MIN_REQUIRED', 'GLIB_VERSION_2_56')
config_h.set('GDK_VERSION_MIN_REQUIRED', 'GDK_VERSION_3_22')
config_h.set('GDK_VERSION_MAX_ALLOWED', 'GDK_VERSION_3_22')

# Detected features
config_h.set('HAVE_MEMRCHR', cc.has_function('memrchr'))
config_h.set('HAVE_STRINGS_H', cc.has_header('strings.h'))

config_h.set_quoted('HEXCHATLIBDIR',
  join_paths(get_option('prefix'), get_option('libdir'), 'hexchat/plugins')
)

if libssl_dep.found()
  config_h.set('HAVE_X509_GET_SIGNATURE_NID',
    cc.has_function('X509_get_signature_nid', dependencies: libssl_dep)
  )
  config_h.set('HAVE_SSL_CTX_GET_SSL_METHOD',
    cc.has_function('SSL_CTX_get_ssl_method', dependencies: libssl_dep)
  )
  config_h.set('HAVE_DH_SET0_PQG',
    cc.has_function('DH_set0_pqg', dependencies: libssl_dep)
  )
  config_h.set('HAVE_DH_GET0_KEY',
    cc.has_function('DH_get0_key', dependencies: libssl_dep)
  )
  config_h.set('HAVE_DH_SET0_KEY',
    cc.has_function('DH_set0_key', dependencies: libssl_dep)
  )
  config_h.set('HAVE_ERR_REMOVE_THREAD_STATE',
    cc.has_function('ERR_remove_thread_state', dependencies: libssl_dep)
  )
  config_h.set('HAVE_ASN1_STRING_GET0_DATA',
    cc.has_function('ASN1_STRING_get0_data', dependencies: libssl_dep)
  )
endif

configure_file(output: 'config.h', configuration: config_h)
config_h_include = include_directories('.')

if host_machine.system() == 'windows'
  add_project_arguments(
    '-DWIN32',
    '-DNTDDI_VERSION=NTDDI_WIN7',
    '-D_WIN32_WINNT=_WIN32_WINNT_WIN7',
     language: 'c')
  
  # Windows-specific dependencies
  winmm_dep = cc.find_library('winmm', required: true)
  ws2_32_dep = cc.find_library('ws2_32', required: true)
  shell32_dep = cc.find_library('shell32', required: true)
  ole32_dep = cc.find_library('ole32', required: true)
  
  global_deps += [winmm_dep, ws2_32_dep, shell32_dep, ole32_dep]
else
  # Unix-specific dependencies
  m_dep = cc.find_library('m', required: true)
  dl_dep = cc.find_library('dl', required: true)
  rt_dep = cc.find_library('rt', required: false)
  
  global_deps += [m_dep, dl_dep]
  if rt_dep.found()
    global_deps += rt_dep
  endif
  
  # Check for X11 and related libraries
  x11_dep = dependency('x11', required: true)
  xext_dep = dependency('xext', required: true)
  xi_dep = dependency('xi', required: false)
  
  global_deps += [x11_dep, xext_dep]
  if xi_dep.found()
    global_deps += xi_dep
  endif
endif


global_cflags = []
test_cflags = [
  '-funsigned-char',
  '-Wno-conversion',
  '-Wno-pointer-sign',
  '-Wno-padded',
  '-Wno-unused-parameter',
  '-Wno-missing-prototypes',
  '-Winline',
  '-Wstrict-prototypes',
  '-Werror=implicit-function-declaration',
  '-Werror=pointer-arith',
  '-Werror=init-self',
  ['-Werror=format-security', '-Werror=format=1'],
  '-Werror=missing-include-dirs',
  '-Werror=date-time',
]
foreach cflag : test_cflags
  if cc.has_multi_arguments(cflag)
    global_cflags += cflag
  endif
endforeach
if get_option('buildtype') != 'plain'
  if cc.has_argument('-fstack-protector-strong') and cc.links('''
     int main (void) {
       char buffer[16];
       strcpy(buffer, "foo");
       return 0;
     }
     ''', args: '-fstack-protector-all')
    global_cflags += '-fstack-protector-strong'

    if host_machine.system() == 'windows'
      global_deps += cc.find_library('ssp')
    endif
  endif
endif
add_project_arguments(global_cflags, language: 'c')


global_ldflags = []
test_ldflags = [
  '-Wl,-z,relro',
  '-Wl,-z,now',
  # mingw
  '-Wl,--nxcompat',
]
if not (host_machine.system() == 'windows' and get_option('debug'))
 test_ldflags += '-Wl,--dynamicbase'
endif
foreach ldflag : test_ldflags
  if meson.version().version_compare('>= 0.46.0')
    has_arg = cc.has_link_argument(ldflag)
  else
    has_arg = cc.has_argument(ldflag)
  endif

  if has_arg and cc.links('int main (void) { return 0; }', args: ldflag)
    global_ldflags += ldflag
  endif
endforeach
add_project_link_arguments(global_ldflags, language: 'c')

subdir('src')
if get_option('plugin')
  subdir('plugins')
endif
if cc.get_id() != 'msvc'
  subdir('data')
  subdir('po') # FIXME: build xgettext

  meson.add_install_script('meson_post_install.py',
    '@0@'.format(get_option('theme-manager'))
  )
endif

if meson.version().version_compare('>= 0.53.0')
  summary({
    'prefix': get_option('prefix'),
    'bindir': get_option('bindir'),
    'libdir': get_option('libdir'),
    'datadir': get_option('datadir'),
  }, section: 'Directories')

  summary({
    'TLS (openssl)': libssl_dep.found(),
    'Plugin Support': get_option('plugin'),
    'DBus Support': dbus_glib_dep.found(),
    'libcanberra': libcanberra_dep.found(),
  }, section: 'Features')

  summary({
    'Lua': get_option('with-lua'),
    'Python': get_option('with-python'),
    'Perl': get_option('with-perl'),
    'Perl Legacy API': get_option('with-perl-legacy-api'),
    'FiSH': get_option('with-fishlim'),
    'Sysinfo': get_option('with-sysinfo'),
    'DCC Checksum': get_option('with-checksum'),
  }, section: 'Plugins')
endif
