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
            shareTls = false;
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
        system: pkgs:
        let
          wrxvt = self.packages.${system}.wrxvt;

          # the same, compiled with AddressSanitizer and UBSan
          wrxvt-asan = wrxvt.overrideAttrs (old: {
            pname = "wrxvt-asan";
            configureFlags = map (
              f:
              if pkgs.lib.hasPrefix "CXXFLAGS=" f then
                f + " -g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined"
              else
                f
            ) old.configureFlags;
            env = old.env // {
              LDFLAGS = old.env.LDFLAGS + " -fsanitize=address,undefined";
            };
            dontStrip = true;
          });

          # the test suite (tests/), against a package, under Xvfb
          testsWith =
            name: package: environment:
            pkgs.runCommand name
              (
                {
                  nativeBuildInputs = [
                    package
                    (pkgs.python3.withPackages (p: [ p.python-xlib ])) # xlib: a drag source
                    pkgs.perl
                    pkgs.xvfb
                    pkgs.imagemagick
                    pkgs.xdotool
                    pkgs.xclip # OSC 52
                    pkgs.xprop # OSC 7
                    pkgs.xrdb # colours that follow the resources
                    pkgs.ncurses # tic and infocmp, for the terminfo entry
                  ];
                  FONTCONFIG_FILE = testFonts pkgs;
                  WRXVT = pkgs.lib.getExe package;
                }
                // environment
              )
              ''
                set -o pipefail
                export HOME=$TMPDIR
                cp -r ${./tests} tests && chmod -R u+w tests
                mkdir doc && cp -r ${./doc/etc} doc/etc
                # the Unicode tables' unit tests (test_units.py) include these
                mkdir src && cp ${./src/grapheme.h} ${./src/grapheme-properties.h} \
                  ${./src/emoji-sequences.h} ${./src/emoji-properties.h} src/
                cd tests
                if [ -n "$ASAN_OPTIONS" ]; then
                  export ASAN_OPTIONS="$ASAN_OPTIONS:log_path=$TMPDIR/sanitizer"
                  export UBSAN_OPTIONS="$UBSAN_OPTIONS:log_path=$TMPDIR/sanitizer"
                fi
                python3 -m unittest discover -v 2>&1 | tee $out
                # sanitizer reports fail the check even when the tests passed
                if ls $TMPDIR/sanitizer.* >/dev/null 2>&1; then
                  cat $TMPDIR/sanitizer.* >&2
                  exit 1
                fi
              '';
        in
        {
          build = wrxvt;
          minimal = self.packages.${system}.wrxvt-minimal;
          tests = testsWith "wrxvt-tests" wrxvt { };
          # every test again under the sanitizers: memory errors and
          # undefined behaviour anywhere, the random-input test included
          tests-asan = testsWith "wrxvt-tests-asan" wrxvt-asan {
            ASAN_OPTIONS = "detect_leaks=0";
            UBSAN_OPTIONS = "print_stacktrace=1";
          };
        }
      );

      devShells = forAllSystems (
        system: pkgs: {
          default = pkgs.mkShell {
            inputsFrom = [ self.packages.${system}.wrxvt ];
            packages = [
              pkgs.autoconf
              pkgs.automake
              (pkgs.python3.withPackages (p: [ p.python-xlib ]))
              pkgs.xvfb
              pkgs.imagemagick
              pkgs.xdotool
              pkgs.xclip
              pkgs.xprop
              pkgs.xrdb
              pkgs.gdb
              pkgs.libev
            ];
            FONTCONFIG_FILE = testFonts pkgs;
            shellHook = ''
              #if [ ! -e libev/ev++.h ]; then
              #  echo "unpacking libev into ./libev (a separate CVS module)"
              #  tar -xzf ${pkgs.libev.src} && mv libev-${pkgs.libev.version} libev
              #fi
              echo "build: ./configure --enable-everything --enable-256-color --enable-emoji-sequences --enable-share-tls && make"
              echo "test:  make check"
            '';
          };
        }
      );

      formatter = forAllSystems (system: pkgs: pkgs.nixfmt);
    };
}
