{
  lib,
  stdenv,
  libev,
  libx11,
  libxt,
  libxft,
  libxrender,
  libxext,
  ncurses,
  fontconfig,
  freetype,
  harfbuzz,
  pkg-config,
  gdk-pixbuf,
  openssl,
  perl,
  libptytty,
  makeDesktopItem,
  perlSupport ? true,
  gdkPixbufSupport ? true,
  emojiSequences ? true,
  shareTls ? true, # HTTPS for share:start

  src ? ../.,
  version ? "9.32",
}:

let
  desktopItem = makeDesktopItem {
    name = "wrxvt";
    exec = "wrxvt";
    icon = "utilities-terminal";
    comment = "A modernized rxvt-unicode";
    desktopName = "wrxvt";
    genericName = "Terminal";
    categories = [
      "System"
      "TerminalEmulator"
    ];
  };
in
stdenv.mkDerivation {
  pname = "wrxvt";
  inherit version src;

  # libev is a separate CVS module that rxvt-unicode compiles in from
  # libev/; nixpkgs' libev source is the matching release.
  postUnpack = ''
    tar -xzf ${libev.src}
    mv libev-${libev.version} "$sourceRoot/libev"
  '';

  nativeBuildInputs = [ pkg-config ];
  buildInputs = [
    libx11
    libxt
    libxft
    ncurses # tic, for the terminfo output
    fontconfig
    freetype
    libxrender
    libptytty
  ]
  ++ lib.optionals perlSupport [
    perl
    libxext
  ]
  ++ lib.optional gdkPixbufSupport gdk-pixbuf
  ++ lib.optional emojiSequences harfbuzz
  ++ lib.optional shareTls openssl;

  outputs = [
    "out"
    "terminfo"
  ];

  configureFlags = [
    "--with-terminfo=${placeholder "terminfo"}/share/terminfo"
    "--enable-everything"
    "--enable-256-color" # not part of --enable-everything
    (lib.enableFeature perlSupport "perl")
    (lib.enableFeature gdkPixbufSupport "pixbuf")
    (lib.enableFeature emojiSequences "emoji-sequences")
    (lib.enableFeature shareTls "share-tls")
    "CXXFLAGS=-std=c++17"
  ];

  env = {
    LDFLAGS = toString [
      "-lfontconfig"
      "-lXrender"
      "-lpthread"
    ];
    CFLAGS = "-I${freetype.dev}/include/freetype2";
  };

  preConfigure = ''
    # tic writes the terminfo entry here (see man tic)
    mkdir -p $terminfo/share/terminfo
    export TERMINFO=$terminfo/share/terminfo
  ''
  + lib.optionalString perlSupport ''
    # the perl library ends up on PERL5LIB through lib/perl5/site_perl
    mkdir -p $out/$(dirname ${perl.libPrefix})
    ln -s $out/lib/urxvt $out/${perl.libPrefix}
  '';

  enableParallelBuilding = true;

  postInstall = ''
    mkdir -p $out/nix-support
    echo "$terminfo" >> $out/nix-support/propagated-user-env-packages
    cp -r ${desktopItem}/share/applications/ $out/share/
  '';

  meta = {
    description = "A modernized rxvt-unicode: same speed, more of today's terminal features";
    homepage = "https://github.com/hydrastro/wrxvt";
    license = lib.licenses.gpl3Plus;
    platforms = lib.platforms.unix;
    mainProgram = "wrxvt";
  };
}
