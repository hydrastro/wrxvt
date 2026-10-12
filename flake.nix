{
  description = "wrxvt: a modernized rxvt-unicode";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "i686-linux"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f system nixpkgs.legacyPackages.${system});

      # only the files the build uses, so editing tests or docs doesn't rebuild
      source = nixpkgs.lib.fileset.toSource {
        root = ./.;
        fileset = nixpkgs.lib.fileset.difference ./. (
          nixpkgs.lib.fileset.unions [
            (nixpkgs.lib.fileset.maybeMissing ./result)
            ./flake.nix
            (nixpkgs.lib.fileset.maybeMissing ./flake.lock)
            ./nix
            ./tests
          ]
        );
      };

      testFonts =
        pkgs:
        pkgs.makeFontsConf {
          fontDirectories = [
            pkgs.dejavu_fonts
            pkgs.noto-fonts-color-emoji
          ];
        };
    in
    {
      overlays.default = final: prev: {
        wrxvt = final.callPackage ./nix/package.nix { src = source; };
      };

      packages = forAllSystems (
        system: pkgs: {
          wrxvt = pkgs.callPackage ./nix/package.nix { src = source; };
          # without perl and image support: the smallest build
          wrxvt-minimal = self.packages.${system}.wrxvt.override {
            perlSupport = false;
            gdkPixbufSupport = false;
            emojiSequences = false;
          };
          default = self.packages.${system}.wrxvt;
        }
      );

      apps = forAllSystems (
        system: pkgs: {
          default = {
            type = "app";
            program = pkgs.lib.getExe self.packages.${system}.wrxvt;
            meta.description = "Run wrxvt";
          };
        }
      );

      checks = forAllSystems (
        system: pkgs: {
          build = self.packages.${system}.wrxvt;
          minimal = self.packages.${system}.wrxvt-minimal;
          # the test suite (tests/), against the installed package, under Xvfb
          tests =
            pkgs.runCommand "wrxvt-tests"
              {
                nativeBuildInputs = [
                  self.packages.${system}.wrxvt
                  pkgs.python3
                  pkgs.perl
                  pkgs.xvfb
                  pkgs.imagemagick
                  pkgs.xdotool
                ];
                FONTCONFIG_FILE = testFonts pkgs;
                WRXVT = pkgs.lib.getExe self.packages.${system}.wrxvt;
              }
              ''
                export HOME=$TMPDIR
                cp -r ${./tests} tests && chmod -R u+w tests && cd tests
                python3 -m unittest discover -v 2>&1 | tee $out
              '';
        }
      );

      devShells = forAllSystems (
        system: pkgs: {
          default = pkgs.mkShell {
            inputsFrom = [ self.packages.${system}.wrxvt ];
            packages = [
              pkgs.autoconf
              pkgs.automake
              pkgs.python3
              pkgs.xvfb
              pkgs.imagemagick
              pkgs.xdotool
              pkgs.gdb
            ];
            FONTCONFIG_FILE = testFonts pkgs;
            shellHook = ''
              if [ ! -e libev/ev++.h ]; then
                echo "unpacking libev into ./libev (a separate CVS module)"
                tar -xzf ${pkgs.libev.src} && mv libev-${pkgs.libev.version} libev
              fi
              echo "build: ./configure --enable-everything --enable-emoji-sequences && make"
              echo "test:  make check"
            '';
          };
        }
      );

      formatter = forAllSystems (system: pkgs: pkgs.nixfmt);
    };
}
