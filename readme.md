# HexChat

[![License](https://img.shields.io/badge/license-GPL%20v2-blue.svg)](COPYING)

HexChat is a feature-rich, cross-platform IRC (Internet Relay Chat) client based on XChat. It provides a modern, user-friendly interface with extensive customization options, plugin support, and scripting capabilities.

## Features

- **Cross-Platform**: Runs on Windows, Linux, macOS, and other UNIX-like systems
- **Modern GTK3 Interface**: Clean, customizable UI with theme support
- **Multi-Server Support**: Connect to multiple IRC networks simultaneously
- **Scripting**: Extensible via Python, Perl, and Lua plugins
- **Security**: Built-in SSL/TLS support for secure connections
- **Customization**: Configurable colors, fonts, layouts, and keyboard shortcuts
- **File Transfers**: DCC send/receive with resume support
- **Spell Checking**: Multi-language spell checking support
- **Notification System**: Desktop notifications and sound alerts
- **Plugin Architecture**: Extend functionality with loadable plugins

## Installation

### Windows
Download the latest installer from the [releases page](https://hexchat.github.io/downloads.html).

### Linux
Most distributions provide HexChat in their repositories:

```bash
# Debian/Ubuntu
sudo apt install hexchat

# Fedora
sudo dnf install hexchat

# Arch Linux
sudo pacman -S hexchat
```

### macOS
```bash
brew install hexchat
```

### Building from Source
See the [Building](#building) section below.

## Building

HexChat uses the [Meson](https://mesonbuild.com/) build system.

### Dependencies

#### Required
- **GLib** >= 2.36.0
- **GTK+** >= 3.22.0 (GTK3)
- **GModule**

#### Optional
- **OpenSSL** >= 0.9.8 (for TLS/SSL support)
- **DBus-GLib** (for single-instance and scripting interface, Unix only)
- **libcanberra** >= 0.22 (for sound alerts, Unix only)
- **iso-codes** (for spell checking)
- **Python** 3.x (for Python scripting plugin)
- **Perl** 5.x (for Perl scripting plugin)
- **Lua** / LuaJIT (for Lua scripting plugin)
- **libpci** (for sysinfo plugin on Unix)

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/hexchat/hexchat.git
cd hexchat

# Configure the build
meson setup builddir

# Build
meson compile -C builddir

# Install (optional)
meson install -C builddir
```

### Build Options

Configure build options using `-D` flags:

```bash
# Enable/disable features
meson setup builddir -Dtls=enabled -Dplugin=true -Ddbus=auto

# Enable/disable plugins
meson setup builddir -Dwith-python=python3 -Dwith-perl=perl -Dwith-lua=luajit

# Build with all plugins
meson setup builddir -Dwith-checksum=true -Dwith-fishlim=true -Dwith-sysinfo=true
```

See `meson_options.txt` for all available build options.

## Documentation

- **User Documentation**: [https://hexchat.readthedocs.org](https://hexchat.readthedocs.org/en/latest/index.html)
- **FAQ**: [https://hexchat.readthedocs.org/en/latest/faq.html](https://hexchat.readthedocs.org/en/latest/faq.html)
- **Changelog**: [https://hexchat.readthedocs.org/en/latest/changelog.html](https://hexchat.readthedocs.org/en/latest/changelog.html)

### Scripting APIs
- [Python API Documentation](https://hexchat.readthedocs.org/en/latest/script_python.html)
- [Perl API Documentation](https://hexchat.readthedocs.org/en/latest/script_perl.html)

### IRC Resources
- **IRCHelp.org**: [http://irchelp.org](http://irchelp.org) - General information about IRC

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

### Development

- Use the existing code style and conventions
- Test your changes thoroughly
- Update documentation as needed
- Follow the Git commit message guidelines

## Project Status

**Note**: HexChat is currently undergoing migration from GTK2 to GTK3. The build system has been updated to use GTK3 (>= 3.22.0), and work is in progress to update deprecated GTK2 APIs throughout the codebase.

## License

HexChat is released under the **GNU General Public License version 2** (GPL v2) with an additional exemption that allows compiling, linking, and/or using OpenSSL. You may provide binary packages linked to the OpenSSL libraries, provided that all other requirements of the GPL are met.

See the [COPYING](COPYING) file for the full license text.

## Credits

- **X-Chat** ("xchat") - Copyright (c) 1998-2010 By Peter Zelezny
- **HexChat** ("hexchat") - Copyright (c) 2009-2014 By Berke Viktor

HexChat is based on XChat and maintains compatibility while adding new features and improvements.

## Links

- **Homepage**: [https://hexchat.github.io](https://hexchat.github.io)
- **Downloads**: [https://hexchat.github.io/downloads.html](https://hexchat.github.io/downloads.html)
- **GitHub Repository**: [https://github.com/hexchat/hexchat](https://github.com/hexchat/hexchat)
- **Issue Tracker**: [https://github.com/hexchat/hexchat/issues](https://github.com/hexchat/hexchat/issues)
