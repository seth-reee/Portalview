{ lib, stdenv, cmake, ninja, qt6, dbus, freerdp, source, writeShellScript }:

stdenv.mkDerivation {
  pname = "portalview";
  version = builtins.elemAt (builtins.match ".*project\\(Portalview VERSION ([^ ]+) LANGUAGES.*"
    (builtins.readFile ../../CMakeLists.txt)) 0;
  src = lib.cleanSourceWith {
    src = source;
    filter = path: type:
      let name = baseNameOf path; in
      !(lib.hasInfix ".sync-conflict-" name)
      && !(builtins.elem name [ ".git" "build" "stage" "result" ]);
  };

  nativeBuildInputs = [ cmake ninja qt6.wrapQtAppsHook ];
  buildInputs = [ qt6.qtbase qt6.qtdeclarative qt6.qtwayland dbus ];
  cmakeFlags = [
    "-DBUILD_TESTING=ON"
    "-DDBUS_RUN_SESSION=${writeShellScript "portalview-test-dbus" ''
      exec ${dbus}/bin/dbus-run-session --config-file=${dbus}/share/dbus-1/session.conf "$@"
    ''}"
  ];
  doCheck = true;
  preCheck = ''
    export HOME="$TMPDIR/home"
    mkdir -p "$HOME"
    # Qt's local socket names exceed Linux's length limit under long build paths.
    export TMPDIR=/tmp
    export QT_QPA_PLATFORM=offscreen
    export QT_QPA_PLATFORMTHEME=
    export QT_QUICK_CONTROLS_STYLE=Basic
    export QML2_IMPORT_PATH="${qt6.qtdeclarative}/lib/qt-6/qml"
  '';

  postInstall = ''
    # Keep the X11 client in the immutable closure, with Nix-patched library paths.
    mkdir -p "$out/lib/portalview/freerdp-3.31.1/bin"
    ln -s "${freerdp}/bin/xfreerdp" "$out/lib/portalview/freerdp-3.31.1/bin/xfreerdp3"
    mkdir -p "$out/share/licenses/freerdp"
    tar -xOf ${freerdp.src} freerdp-3.31.1/LICENSE > "$out/share/licenses/freerdp/LICENSE"
  '';

  meta = {
    description = "Qt Quick remote desktop connection manager";
    homepage = "https://github.com/seth-reee/Portalview";
    license = lib.licenses.mit;
    mainProgram = "portalview";
    platforms = [ "x86_64-linux" "aarch64-linux" ];
  };
}
