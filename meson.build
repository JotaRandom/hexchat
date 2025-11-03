project('hexchat',
  'c',
  version: '2.16.1',
  license: 'GPL-2.0-or-later',
  default_options: [
    'buildtype=debugoptimized',
    'warning_level=1',
    'b_lundef=true',
    'b_pie=true',
    'with-python=python3-embed',
    'with-perl=perl',
    'plugin=true',
    'c_std=gnu99'
  ],
  meson_version: '>= 0.50.0'
)

# Set C standard based on compiler
cc = meson.get_compiler('c')
if cc.get_id() in ['gcc', 'clang']
  if not cc.has_argument('-std=gnu99')
    error('C compiler does not support -std=gnu99 flag')
  endif
  add_project_arguments('-std=gnu99', language: 'c')
endif

# Get options from meson_options.txt
plugin_enabled = get_option('plugin')
lua_impl = get_option('with-lua').to_lower()
python_impl = get_option('with-python').to_lower()
perl_opt = get_option('with-perl')
perl_path = get_option('perl-path')
libcanberra_enabled = get_option('with-libcanberra')
dbus_enabled = get_option('with-dbus')
tls_impl = get_option('with-tls').to_lower()

# Handle Perl configuration
perl_enabled = perl_opt != 'false' and perl_opt != ''
if perl_enabled and perl_path == ''
  # If perl-path is not set, use the value from with-perl if it's a path
  if perl_opt != 'perl' and perl_opt != 'true' and perl_opt != ''
    perl_path = perl_opt
  else
    # Default system path
    perl_path = '/usr/bin/perl'
  endif
  message('Using Perl at: ' + perl_path)
endif

# Initialize configuration data
conf = configuration_data()
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('PACKAGE_STRING', '@0@ @1@'.format(meson.project_name(), meson.project_version()))
conf.set_quoted('PACKAGE_BUGREPORT', 'https://github.com/hexchat/hexchat/issues')
conf.set_quoted('LOCALEDIR', get_option('prefix') / get_option('localedir'))
conf.set_quoted('LIBDIR', get_option('prefix') / get_option('libdir'))
conf.set_quoted('DATADIR', get_option('prefix') / get_option('datadir'))
conf.set_quoted('SYSCONFDIR', get_option('prefix') / get_option('sysconfdir'))

# Detect operating system
host_system = host_machine.system()
is_windows = host_system == 'windows'
is_linux = host_system == 'linux'
is_darwin = host_system == 'darwin'

# Common dependencies
pkg = import('pkgconfig')
cmake = import('cmake')

# Required dependencies
required_deps = [
  dependency('glib-2.0', version: '>= 2.56.0'),
  dependency('gthread-2.0'),
  dependency('gmodule-2.0'),
  dependency('gmodule-export-2.0'),
  dependency('gio-2.0'),
  dependency('gdk-pixbuf-2.0'),
  dependency('libxml-2.0'),
  cc.find_library('m', required: false),
  cc.find_library('dl', required: false)
]

# Optional dependencies
optional_deps = []

# GTK+ frontend
gtk_enabled = get_option('gtk-frontend')
if gtk_enabled
  gtk_dep = dependency('gtk+-3.0', version: '>= 3.22.0', required: false)
  if not gtk_dep.found()
    warning('GTK+ 3.0 not found, disabling GTK+ frontend')
    gtk_enabled = false
  else
    optional_deps += gtk_dep
    optional_deps += dependency('gdk-3.0')
    optional_deps += dependency('gtk+-unix-print-3.0', required: false)
  endif
endif
conf.set10('ENABLE_GTK', gtk_enabled)

# Text frontend
text_enabled = get_option('text-frontend')
conf.set10('ENABLE_TEXT', text_enabled)

# Print configuration summary
message('\nConfiguration summary:')
message('-------------------')
message('Build type:         @0@'.format(get_option('buildtype')))
message('Prefix:             @0@'.format(get_option('prefix')))
message('C compiler:         @0@'.format(cc.get_id()))
message('C flags:            @0@'.format(cc.get_supported_arguments(['-Wall', '-Wextra', '-Wno-unused-parameter', '-Wno-missing-field-initializers'])))
message('Plugin support:     @0@'.format(plugin_enabled))
message('Lua implementation: @0@'.format(lua_impl))
message('Python support:     @0@'.format(python_impl))
message('Perl support:       @0@'.format(perl_enabled))
if perl_enabled
  message('Perl path:          @0@'.format(perl_path))
endif
message('GTK+ frontend:      @0@'.format(gtk_enabled))
message('Text frontend:      @0@'.format(text_enabled))
message('libcanberra:        @0@'.format(libcanberra_enabled))
message('D-Bus support:      @0@'.format(dbus_enabled))
message('TLS implementation: @0@'.format(tls_impl))
message('-------------------\n')

# Import modules
i18n = import('i18n')
gnome = import('gnome')

# Set up compiler flags
if cc.get_id() == 'msvc'
  add_project_arguments(
    '/W3',
    '/DWIN32_LEAN_AND_MEAN',
    '/D_CRT_SECURE_NO_WARNINGS',
    '/D_SCL_SECURE_NO_WARNINGS',
    language: 'c'
  )
else
  common_cflags = [
    '-Wall',
    '-Wextra',
    '-Wno-unused-parameter',
    '-Wno-missing-field-initializers',
    '-Wno-deprecated-declarations',
    '-fno-strict-aliasing',
    '-D_FORTIFY_SOURCE=2',
    '-fstack-protector-strong'
  ]
  
  add_project_arguments(
    cc.get_supported_arguments(common_cflags),
    language: 'c'
  )
  
  # Add link flags
  add_project_link_arguments(
    cc.get_supported_link_arguments(['-Wl,-z,relro', '-Wl,-z,now']),
    language: 'c'
  )
endif

# Handle platform-specific configurations
if is_windows
  windows = import('windows')
  conf.set('_WIN32_WINNT', '0x0601')  # Windows 7
  conf.set('WINVER', '0x0601')        # Windows 7
  conf.set('_WINDOWS', '1')
  conf.set('WIN32_LEAN_AND_MEAN', '1')
  conf.set('NOMINMAX', '1')
  
  # Windows-specific dependencies
  required_deps += [
    cc.find_library('ws2_32'),
    cc.find_library('iphlpapi'),
    cc.find_library('shell32'),
    cc.find_library('ole32'),
    cc.find_library('user32'),
    cc.find_library('gdi32'),
    cc.find_library('comctl32'),
    cc.find_library('comdlg32'),
    cc.find_library('winmm'),
    cc.find_library('shlwapi'),
    cc.find_library('dwmapi'),
    cc.find_library('uxtheme'),
    cc.find_library('version')
  ]
  
  # Check for Windows SDK
  if cc.has_header('sdkddkver.h')
    conf.set('HAVE_SDKDDKVER_H', '1')
  endif
  
  # Check for Windows headers
  foreach h : ['winsock2.h', 'ws2tcpip.h', 'windows.h', 'shellapi.h', 'shlobj.h']
    if cc.has_header(h)
      conf.set('HAVE_' + h.underscorify().to_upper(), '1')
    endif
  endforeach
else
  # Unix-specific dependencies and checks
  required_deps += [
    cc.find_library('pthread', required: true),
    cc.find_library('rt', required: false),
    cc.find_library('resolv', required: false)
  ]
  
  # Check for common Unix headers
  foreach h : ['unistd.h', 'sys/select.h', 'sys/time.h', 'sys/utsname.h', 'sys/wait.h']
    if cc.has_header(h)
      conf.set('HAVE_' + h.underscorify().to_upper(), '1')
    endif
  endforeach
  
  # Check for functions
  foreach f : ['fork', 'vfork', 'pipe', 'pipe2', 'socketpair', 'setenv', 'unsetenv', 'mkstemp', 'mkdtemp']
    if cc.has_function(f)
      conf.set('HAVE_' + f.to_upper(), '1')
    endif
  endforeach
  
  # Check for large file support
  if cc.has_header_symbol('features.h', '_FILE_OFFSET_BITS')
    conf.set('_FILE_OFFSET_BITS', '64')
  endif
  
  if cc.has_header_symbol('features.h', '_LARGEFILE64_SOURCE')
    conf.set('_LARGEFILE64_SOURCE', '1')
  endif
  
  # Check for gettext
  intl_dep = cc.find_library('intl', required: false)
  if intl_dep.found()
    required_deps += intl_dep
    conf.set('HAVE_LIBINTL', '1')
  endif
endif

# Check for required headers
foreach h : ['stdint.h', 'inttypes.h', 'stdlib.h', 'string.h', 'strings.h', 'memory.h', 'locale.h', 'wchar.h']
  if cc.has_header(h)
    conf.set('HAVE_' + h.underscorify().to_upper(), '1')
  endif
endforeach

# Check for required functions
foreach f : ['memset', 'memcpy', 'memmove', 'strcasecmp', 'strncasecmp', 'strdup', 'strndup', 'strerror', 'strerror_r']
  if cc.has_function(f)
    conf.set('HAVE_' + f.to_upper(), '1')
  endif
endforeach

# Check for OpenSSL or GnuTLS for TLS support
if tls_impl == 'openssl'
  openssl_dep = dependency('openssl', version: '>= 1.0.0', required: false)
  if openssl_dep.found()
    conf.set('HAVE_OPENSSL', '1')
    optional_deps += openssl_dep
  else
    warning('OpenSSL not found, TLS support will be disabled')
    tls_impl = 'none'
  endif
elif tls_impl == 'gnutls'
  gnutls_dep = dependency('gnutls', version: '>= 3.0.0', required: false)
  if gnutls_dep.found()
    conf.set('HAVE_GNUTLS', '1')
    optional_deps += gnutls_dep
  else
    warning('GnuTLS not found, TLS support will be disabled')
    tls_impl = 'none'
  endif
else
  message('TLS support disabled by user request')
  conf.set('HAVE_OPENSSL', '0')
  conf.set('HAVE_GNUTLS', '0')
endif

# Check for D-Bus support
if dbus_enabled and not is_windows
  dbus_dep = dependency('dbus-1', version: '>= 1.0.0', required: false)
  if dbus_dep.found()
    conf.set('HAVE_DBUS', '1')
    optional_deps += dbus_dep
  else
    warning('D-Bus not found, D-Bus support will be disabled')
    dbus_enabled = false
  endif
else
  dbus_enabled = false
  conf.set('HAVE_DBUS', '0')
endif

# Check for libcanberra
if libcanberra_enabled and not is_windows
  libcanberra_dep = dependency('libcanberra', required: false)
  if libcanberra_dep.found()
    conf.set('HAVE_LIBCANBERRA', '1')
    optional_deps += libcanberra_dep
  else
    warning('libcanberra not found, sound notifications will be disabled')
    libcanberra_enabled = false
  endif
else
  libcanberra_enabled = false
  conf.set('HAVE_LIBCANBERRA', '0')
endif
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_STRING', '@0@ @1@'.format(meson.project_name(), meson.project_version()))
conf.set_quoted('GETTEXT_PACKAGE', 'hexchat')
conf.set_quoted('LOCALEDIR', join_paths(get_option('prefix'), get_option('datadir'), 'locale'))
conf.set_quoted('G_LOG_DOMAIN', 'hexchat')
conf.set10('ENABLE_NLS', true)

# GTK3 configuration
conf.set10('G_DISABLE_SINGLE_INCLUDES', true)
conf.set10('GDK_PIXBUF_DISABLE_SINGLE_INCLUDES', true)
conf.set('GLIB_VERSION_MAX_ALLOWED', 'GLIB_VERSION_2_56')
conf.set('GLIB_VERSION_MIN_REQUIRED', 'GLIB_VERSION_2_56')
conf.set('GDK_VERSION_MIN_REQUIRED', 'GDK_VERSION_3_22')
conf.set('GDK_VERSION_MAX_ALLOWED', 'GDK_VERSION_3_22')

# Check for Perl support
perl_dep = dependency('', required: false)
if perl_enabled and not is_windows  # Perl on Windows is handled differently
  perl = find_program('perl', required: false)
  if perl.found()
    perl_version = run_command(perl, '-e', 'print $^V', check: false)
    if perl_version.returncode() == 0 and perl_version.stdout().version_compare('>=5.20.0')
      perl_libdir = run_command(perl, '-MConfig', '-e', 'print $Config{archlib}', check: false)
      if perl_libdir.returncode() == 0
        perl_inc = run_command(perl, '-MConfig', '-e', 'print "$Config{archlib}/CORE"', check: false)
        if perl_inc.returncode() == 0
          conf.set_quoted('PERL_INC_DIR', perl_inc.stdout().strip())
          conf.set('HAVE_PERL', '1')
          
          # Create a custom dependency for Perl
          perl_dep = declare_dependency(
            compile_args: ['-I' + perl_inc.stdout().strip()],
            link_args: ['-L' + perl_libdir.stdout().strip(), '-lperl'],
            variables: {
              'perl': perl.full_path(),
              'perl_archlib': perl_libdir.stdout().strip(),
              'perl_inc': perl_inc.stdout().strip()
            }
          )
          
          message('Perl support enabled: ' + perl_version.stdout().strip())
        endif
      endif
    else
      warning('Perl 5.20.0 or newer is required, found: ' + (perl_version.returncode() == 0 ? perl_version.stdout().strip() : 'unknown'))
      perl_enabled = false
    endif
  else
    warning('Perl executable not found, Perl support will be disabled')
    perl_enabled = false
  endif
endif

if not perl_enabled
  conf.set('HAVE_PERL', '0')
  message('Perl support disabled')
endif

# Check for Python support
python_dep = dependency('', required: false)
if python_impl != 'false'
  python = import('python').find_installation(python_impl == 'python3-embed' ? 'python3' : python_impl)
  if python.found()
    python_version = python.language_version()
    if python_version.version_compare('>=3.5')
      python_inc = run_command(python, '-c', 'import sysconfig; print(sysconfig.get_path("include"))', check: false)
      if python_inc.returncode() == 0
        python_inc = python_inc.stdout().strip()
        python_lib = run_command(python, '-c', 'import sysconfig; print(sysconfig.get_config_var("LIBDIR") or "")', check: false)
        python_lib = python_lib.returncode() == 0 ? python_lib.stdout().strip() : ''
        
        # Create a custom dependency for Python
        python_dep = declare_dependency(
          compile_args: ['-I' + python_inc],
          link_args: python_lib != '' ? ['-L' + python_lib, '-lpython' + python_version] : [],
          variables: {
            'python': python.full_path(),
            'python_inc': python_inc,
            'python_lib': python_lib,
            'python_version': python_version
          }
        )
        
        conf.set('HAVE_PYTHON', '1')
        conf.set_quoted('PYTHON_VERSION', python_version)
        message('Python support enabled: ' + python_version)
      endif
    else
      warning('Python 3.5 or newer is required, found: ' + python_version)
      python_impl = 'false'
    endif
  else
    warning('Python not found, Python support will be disabled')
    python_impl = 'false'
  endif
endif

if python_impl == 'false'
  conf.set('HAVE_PYTHON', '0')
  message('Python support disabled')
endif

# Check for Lua support
lua_dep = dependency('', required: false)
if lua_impl != 'false'
  lua = dependency(lua_impl, version: '>=5.1', required: false)
  if lua.found()
    lua_dep = lua
    conf.set('HAVE_LUA', '1')
    message('Lua support enabled: ' + lua_impl)
  else
    warning('Lua not found, Lua support will be disabled')
    lua_impl = 'false'
  endif
endif

if lua_impl == 'false'
  conf.set('HAVE_LUA', '0')
  message('Lua support disabled')
endif

# Generate config.h
config_h = configure_file(
  output: 'config.h',
  configuration: conf
)

# Add config.h to the include directories
config_inc = include_directories('.')

# Add all dependencies to a single list
all_deps = required_deps + optional_deps
if perl_dep.found()
  all_deps += perl_dep
endif
if python_dep.found()
  all_deps += python_dep
endif
if lua_dep.found()
  all_deps += lua_dep
endif

# Define common include directories
inc = include_directories(
  'src/common',
  'src/fe-common',
  'src/fe-gtk',
  'src/fe-text',
  'src/plugins',
  'src/plugins/checksum',
  'src/plugins/exec',
  'src/plugins/fishlim',
  'src/plugins/lua',
  'src/plugins/perl',
  'src/plugins/python',
  'src/plugins/sysinfo',
  'src/plugins/upd',
  'src/plugins/winamp',
  'src/plugins/win32',
  'src/plugins/xdcc',
  'src/plugins/xdcc/upd',
  'src/plugins/xdcc/win32',
  'src/plugins/xdcc/common',
  'src/plugins/xdcc/checksum',
  'src/plugins/xdcc/exec',
  'src/plugins/xdcc/fishlim',
  'src/plugins/xdcc/lua',
  'src/plugins/xdcc/perl',
  'src/plugins/xdcc/python',
  'src/plugins/xdcc/sysinfo',
  'src/plugins/xdcc/upd',
  'src/plugins/xdcc/winamp',
  'src/plugins/xdcc/win32',
  'src/plugins/xdcc/common',
  'src/plugins/xdcc/checksum',
  'src/plugins/xdcc/exec',
  'src/plugins/xdcc/fishlim',
  'src/plugins/xdcc/lua',
  'src/plugins/xdcc/perl',
  'src/plugins/xdcc/python',
  'src/plugins/xdcc/sysinfo',
  'src/plugins/xdcc/upd',
  'src/plugins/xdcc/winamp',
  'src/plugins/xdcc/win32'
)

# Add subdirectories
subdir('src/common')
subdir('src/fe-common')

if gtk_enabled
  subdir('src/fe-gtk')
endif

if text_enabled
  subdir('src/fe-text')
endif

if plugin_enabled
  subdir('src/plugins')
  
  if perl_enabled
    subdir('src/plugins/perl')
  endif
  
  if python_impl != 'false'
    subdir('src/plugins/python')
  endif
  
  if lua_impl != 'false'
    subdir('src/plugins/lua')
  endif
  
  subdir('src/plugins/checksum')
  subdir('src/plugins/exec')
  subdir('src/plugins/fishlim')
  subdir('src/plugins/sysinfo')
  
  if is_windows
    subdir('src/plugins/upd')
    subdir('src/plugins/winamp')
    subdir('src/plugins/win32')
  endif
endif

# Install data files
install_data(
  'data/hexchat.desktop',
  'data/hexchat.appdata.xml',
  'data/hexchat.png',
  'data/hexchat.svg',
  install_dir: join_paths(get_option('datadir'), 'applications')
)

# Install icons
foreach size: ['16x16', '32x32', '48x48', '64x64', '128x128', '256x256', '512x512', 'scalable']
  install_data(
    'data/icons/hicolor/' + size + '/apps/hexchat.png',
    install_dir: join_paths(get_option('datadir'), 'icons/hicolor', size, 'apps')
  )
  
  if size == 'scalable'
    install_data(
      'data/icons/hicolor/scalable/apps/hexchat.svg',
      install_dir: join_paths(get_option('datadir'), 'icons/hicolor/scalable/apps')
    )
  endif
endforeach

# Install man pages
install_man('data/man/hexchat.1')

# Install translations
subdir('po')

# Generate and install pkg-config file
pkg = import('pkgconfig')
pkg.generate(
  name: 'hexchat',
  description: 'HexChat IRC client',
  version: meson.project_version(),
  requires: ['glib-2.0', 'gthread-2.0', 'gmodule-2.0', 'gio-2.0', 'gdk-pixbuf-2.0'],
  requires_private: ['libxml-2.0'],
  libraries: [],
  subdirs: ['hexchat']
)

# Summary
summary({
  'Build type': get_option('buildtype'),
  'Prefix': get_option('prefix'),
  'C compiler': cc.get_id(),
  'C flags': cc.get_supported_arguments(common_cflags),
  'Linker flags': cc.get_supported_link_arguments(['-Wl,-z,relro', '-Wl,-z,now']),
  'Plugin support': plugin_enabled,
  'Lua implementation': lua_impl != 'false' ? lua_impl : 'disabled',
  'Python support': python_impl != 'false' ? python_impl : 'disabled',
  'Perl support': perl_enabled ? 'enabled' : 'disabled',
  'GTK+ frontend': gtk_enabled ? 'enabled' : 'disabled',
  'Text frontend': text_enabled ? 'enabled' : 'disabled',
  'libcanberra': libcanberra_enabled ? 'enabled' : 'disabled',
  'D-Bus support': dbus_enabled ? 'enabled' : 'disabled',
  'TLS implementation': tls_impl,
  'Install prefix': get_option('prefix'),
  'Bin directory': get_option('bindir'),
  'Lib directory': get_option('libdir'),
  'Data directory': get_option('datadir'),
  'Include directory': get_option('includedir'),
  'Man page directory': get_option('mandir'),
  'Locale directory': get_option('localedir'),
}, section: 'Configuration')

message('\nConfiguration complete. Run \'ninja -C ' + meson.build_root() + '\' to build.')

conf.set10('GDK_DISABLE_DEPRECATED', true)
conf.set10('GTK_DISABLE_DEPRECATED', true)

# Compiler and linker settings
cc = meson.get_compiler('c')

# Initialize feature flags to false, will be updated as we find dependencies
conf.set10('HAVE_PERL', false)
conf.set10('HAVE_PYTHON', false)
conf.set10('HAVE_LUA', false)
conf.set10('USE_OPENSSL', false)
conf.set10('USE_LIBCANBERRA', false)
conf.set10('USE_DBUS', false)
conf.set10('USE_PLUGIN', false)

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

# Handle optional dependencies
# libcanberra
if libcanberra_enabled
  libcanberra_dep = dependency('libcanberra-gtk3', version: '>= 0.30', required: false)
  if not libcanberra_dep.found()
    libcanberra_dep = dependency('libcanberra', version: '>= 0.30', required: false)
  endif
  if libcanberra_dep.found()
    conf.set10('USE_LIBCANBERRA', true)
  else
    warning('libcanberra not found, sound support will be limited')
    libcanberra_dep = dependency('', required: false)
  endif
else
  libcanberra_dep = dependency('', required: false)
  message('libcanberra support disabled by configuration')
endif

# D-Bus
if dbus_enabled
  dbus_glib_dep = dependency('dbus-glib-1', required: false)
  if dbus_glib_dep.found()
    conf.set10('USE_DBUS', true)
  else
    warning('D-Bus support requested but not found')
    dbus_glib_dep = dependency('', required: false)
  endif
else
  dbus_glib_dep = dependency('', required: false)
  message('D-Bus support disabled by configuration')
endif

# Set up all configuration before any configure_file calls

# Initialize plugin dependencies
luajit_dep = dependency('', required: false)
python3_dep = dependency('', required: false)
perl_dep = dependency('', required: false)

# Set initial plugin state
if plugin_enabled
  conf.set10('USE_PLUGIN', true)
  message('Plugin support enabled')
else
  conf.set10('USE_PLUGIN', false)
  message('Plugin support disabled by configuration')
  subdir_done()
endif

# Only require the dependencies if both plugin support and the specific feature are enabled
# Handle plugin dependencies
if get_option('plugin')
  # Lua plugin support
  if lua_impl != 'false'
    luajit_dep = dependency(lua_impl, version: '>= 2.0.0', required: true)
    conf.set10('HAVE_LUA', luajit_dep.found())
  endif
  
  # Python plugin support
  if python_impl != 'false'
    python3_dep = dependency(python_impl, required: true)
    conf.set10('HAVE_PYTHON', python3_dep.found())
  endif
  
  # Perl plugin support
  if perl_enabled
    message('Looking for Perl...')
    perl_dep = disabler()
    
    # Try system Perl first
    perl_dep = dependency('perl', required: false)
    
    # If custom path specified, try that too
    if perl_path != ''
      message('Trying custom Perl path: ' + perl_path)
      perl_dep = dependency('perl', 
                          method: 'pkg-config',
                          pkgconfig_define: ['prefix=' + perl_path],
                          required: false)
    endif
    
    if perl_dep.found()
      # Set Perl-specific flags
      perl_inc = run_command(perl_dep.get_pkgconfig_variable('perlpath'),
                           '-MConfig',
                           '-e', 'print $Config{archlibexp} . "/CORE"').stdout().strip()
      conf.set_quoted('PERL_INC_DIR', perl_inc)
      conf.set10('HAVE_PERL', true)
    else
      warning('Perl support was requested but Perl was not found')
      conf.set10('HAVE_PERL', false)
      perl_dep = dependency('', required: false)
    endif
  else
    # Perl is explicitly disabled
    message('Perl support is disabled')
    conf.set10('HAVE_PERL', false)
    perl_dep = dependency('', required: false)
  endif
endif


# Initialize any unset dependencies
foreach dep : ['luajit_dep', 'python3_dep', 'perl_dep', 'libcanberra_dep', 'dbus_glib_dep', 'tls_dep', 'ssl_dep', 'gnutls_dep']
  if not is_variable(dep)
    set_variable(dep, dependency('', required: false))
  endif
endforeach

# Generate config.h after all configuration is set
config_h = configure_file(
  output: 'config.h',
  configuration: conf
)
config_h_include = include_directories('.')
# Optional dependencies are now handled in the plugin section above

# D-Bus support
if dbus_enabled
    dbus_glib_dep = dependency('dbus-glib-1', required: true)
    if not dbus_glib_dep.found()
        error('D-Bus support requested but dbus-glib-1 not found')
    endif
    conf.set10('USE_DBUS', true)
else
    dbus_glib_dep = dependency('', required: false)
    conf.set10('USE_DBUS', false)
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

# Handle SSL/TLS dependencies
if tls_impl == 'openssl'
  message('Configuring OpenSSL for TLS support...')
  
  if cc.get_id() == 'msvc'
    # Windows/MSVC specific handling
    libssl_dep = cc.find_library('libssl', required: false)
    libcrypto_dep = cc.find_library('libcrypto', required: false)
    
    if not (libssl_dep.found() and libcrypto_dep.found())
      # Try alternative names
      libssl_dep = cc.find_library('ssleay32', required: false)
      libcrypto_dep = cc.find_library('libeay32', required: false)
    endif
    
    if not (libssl_dep.found() and libcrypto_dep.found())
      error('OpenSSL not found. On Windows, make sure OpenSSL is installed and in your PATH')
    endif
  else
    # Unix-like systems
    libssl_dep = dependency('openssl', version: '>= 1.0.0', required: false)
    if not libssl_dep.found()
      # Try alternative package names
      libssl_dep = cc.find_library('ssl', required: false)
      libcrypto_dep = cc.find_library('crypto', required: false)
    else
      # Modern OpenSSL provides both libraries through pkg-config
      libcrypto_dep = dependency('libcrypto', version: '>= 1.0.0', required: false)
    endif
  endif
endif

  
# OpenSSL configuration is handled in the earlier block

if tls_impl == 'gnutls'
  gnutls_dep = dependency('gnutls', required: true)
  if gnutls_dep.found()
    conf.set10('USE_GNUTLS', true)
    global_deps += gnutls_dep
    message('Using GnuTLS for TLS support')
  else
    error('GnuTLS not found but required for TLS support')
  endif
else
  libssl_dep = dependency('', required: false)
  libcrypto_dep = dependency('', required: false)
  gnutls_dep = dependency('', required: false)
  message('TLS support disabled by configuration')
  conf.set10('USE_OPENSSL', false)
  conf.set10('USE_GNUTLS', false)
endif

conf.set('GDK_PIXBUF_DISABLE_SINGLE_INCLUDES', true)
conf.set('GLIB_VERSION_MAX_ALLOWED', 'GLIB_VERSION_2_56')
conf.set('GLIB_VERSION_MIN_REQUIRED', 'GLIB_VERSION_2_56')
conf.set('GDK_VERSION_MIN_REQUIRED', 'GDK_VERSION_3_22')
conf.set('GDK_VERSION_MAX_ALLOWED', 'GDK_VERSION_3_22')

# Detected features
conf.set('HAVE_MEMRCHR', cc.has_function('memrchr'))
conf.set('HAVE_STRINGS_H', cc.has_header('strings.h'))

conf.set_quoted('HEXCHATLIBDIR',
  join_paths(get_option('prefix'), get_option('libdir'), 'hexchat/plugins')
)

if libssl_dep.found()
  conf.set('HAVE_X509_GET_SIGNATURE_NID',
    cc.has_function('X509_get_signature_nid', dependencies: libssl_dep)
  )
  conf.set('HAVE_SSL_CTX_GET_SSL_METHOD',
    cc.has_function('SSL_CTX_get_ssl_method', dependencies: libssl_dep)
  )
  conf.set('HAVE_DH_SET0_PQG',
    cc.has_function('DH_set0_pqg', dependencies: libssl_dep)
  )
  conf.set('HAVE_DH_GET0_KEY',
    cc.has_function('DH_get0_key', dependencies: libssl_dep)
  )
  conf.set('HAVE_DH_SET0_KEY',
    cc.has_function('DH_set0_key', dependencies: libssl_dep)
  )
  conf.set('HAVE_ERR_REMOVE_THREAD_STATE',
    cc.has_function('ERR_remove_thread_state', dependencies: libssl_dep)
  )
  conf.set('HAVE_ASN1_STRING_GET0_DATA',
    cc.has_function('ASN1_STRING_get0_data', dependencies: libssl_dep)
  )
endif

# Configuration will be generated at the end of the file
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

# Generate config.h before any subdirectories are processed
config_h = configure_file(
  output: 'config.h',
  configuration: conf
)
config_h_include = include_directories('.')

# Now that config.h is generated, we can process subdirectories

# Now that all configuration is done, process subdirectories
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
