{
  description = "Portalview remote desktop manager with pinned FreeRDP";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/1c89083e451e891152365b799595cf7790a43325";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in {
      packages = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };
          freerdp = (pkgs.freerdp.override {
            buildServer = false;
            withWaylandSupport = false;
            withManPages = false;
          }).overrideAttrs (old: {
            version = "3.31.1";
            meta = old.meta // {
              changelog = "https://github.com/FreeRDP/FreeRDP/releases/tag/3.31.1";
            };
            src = pkgs.fetchurl {
              url = "https://pub.freerdp.com/releases/freerdp-3.31.1.tar.gz";
              sha256 = "4a2629026896cb4e26fb8ed2d6ca6aa4ab89ca95528dfbae2550c2f6bc866991";
            };
          });
          portalview = pkgs.callPackage ./packaging/nix/package.nix {
            inherit freerdp;
            source = self;
          };
        in {
          inherit portalview;
          default = portalview;
        });
      checks = forAllSystems (system: {
        inherit (self.packages.${system}) portalview;
      });
    };
}
