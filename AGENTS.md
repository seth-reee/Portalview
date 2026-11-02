# Packaging Portalview

Follow ../AGENTS.md. Native and ARM packages use the same pinned source archive.
Keep CMakeLists.txt and packaging/PKGBUILD versions aligned; the app reads the CMake version.

Artifacts belong in ../release-artifacts/Portalview. The source archive contains
CMakeLists.txt, LICENSE, README.md, Portalview.png, src/, qml/, tests/, and
packaging/portalview.desktop, flake.nix, flake.lock, and packaging/nix/ under Portalview-VERSION/. Exclude build output,
saved connections, and PKGBUILD. Pin its SHA256 in packaging/PKGBUILD.

The recipe downloads the pinned source archive from the matching GitHub release.
Before publishing, put it in SRCDEST. Keep checksum and dependency checks enabled.

Native build, from the artifact directory with PKGBUILD copied there:

```sh
SRCDEST="$PWD" PKGDEST="$PWD" makepkg --cleanbuild --force
```

ARM build, from the project directory:

```sh
bash packaging/build-arm.sh /home/darrikm/Projects/release-artifacts/Portalview
```

The ARM script derives portalview-qt-arm64:latest from the repaired cached
omarchy-qt-arm64:latest image, adding Qt Quick and FreeRDP dependencies. It runs
makepkg as the host UID with separate ARM build output and .pkg.tar.zst format.
Docker access requires sandbox escalation. Both builds run the connection and
offscreen UI tests. Inspect metadata, package contents, ELF architecture and run
an offscreen smoke test of the packaged ARM executable. QEMU checks do not verify
real ARM desktop or RDP behavior. Building does not authorize publishing.

# Nix packages

flake.nix pins nixpkgs and FreeRDP 3.31.1 source for x86_64-linux/aarch64-linux.
Build with `nix build .#portalview`; its check phase runs the application tests.
Keep the source archive's Nix files in packaging/prepare-ci.sh in sync with the flake.
CI builds both systems on native runners before release, exports the complete runtime
closures, and publishes them with SHA256SUMS-nix-* manifests. Do not reuse Arch
FreeRDP binaries in Nix packages. The X11 client requires compositor XWayland support.
