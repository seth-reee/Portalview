# Portalview

A C++ and Qt Quick RDP connection manager for Omarchy, using FreeRDP 3's SDL client for remote session windows.

Enable **System Tray Icon** in the main window to keep Portalview running in the tray when you close its window. The preference persists across launches. Right click the tray icon for **Open Main Menu**, saved connections (with submenus for groups), and **Close** to quit and disconnect sessions. Choosing a connection opens its password prompt. Already open connections are marked and disabled. If no system tray is available, closing the window exits normally.

Save, search, edit and delete connections. Double click a connection or select Connect to open a session. Passwords are prompted each time and passed through a pipe, never saved or placed in process arguments. Closing Portalview ends its sessions.

Organize connections using the optional Group field. Choose an existing group or type a new name while adding or editing a connection. Use the group filter to show one group, Ungrouped, or All groups. Search also matches group names. Groups appear while at least one connection belongs to them; clear the Group field to ungroup an entry. Existing connections start ungrouped.

Each connection has a Performance mode toggle, disabled by default. It requests modem network settings and 16-bit color, and disables wallpaper, themes, font smoothing, desktop composition, full window dragging and menu animations. Save and reconnect to apply changes. The remote server may override visual settings or negotiated color depth.

The UI follows Vigilant's compact table layout, spacing, rounded controls and typography. Colors load from `$XDG_STATE_HOME/omarchy/current/theme/colors.toml` (default `~/.local/state`) and update within two seconds of an Omarchy theme change. A fallback palette supports other desktops.

## Build and run

Dependencies: `qt6-base`, `qt6-declarative`, `freerdp`, `cmake`, `ninja`, C++20 compiler.

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/portalview
```

Connections are stored under Qt's standard application data directory, normally `~/.local/share/Portalview/Portalview/connections.json`. Saves are atomic. Certificates must be trusted by default; the per-connection trust-on-first-use option remembers the initial certificate using FreeRDP's certificate store. Clipboard sharing is configurable.

The current version opens remote desktops in FreeRDP windows; it does not embed a FreeRDP framebuffer in QML. Gateway, credential vault, file sharing and connection import are not implemented yet.

## Install

```sh
cmake --install build --prefix "$HOME/.local"
```

This installs the executable and application launcher entry. For a staged install entirely in the project directory, use `--prefix "$PWD/stage"` instead.
