# ================================
# 1. HOST BUILD (native) - for mrbc tool generation
# Uses separate directory to avoid conflicts with desktop builds
# ================================
MRuby::Build.new("host-emscripten") do |conf|
  toolchain :clang

  conf.enable_debug

  # Default host gems + stdlib
  conf.gembox "default"
end


# ================================
# 2. EMSCRIPTEN BUILD (cross)
# Output directory: build/lupine-emscripten/
# ================================
MRuby::CrossBuild.new("lupine-emscripten") do |conf|
  toolchain :clang

  # Emscripten tools
  conf.cc.command       = ENV["EMCC"] || "emcc"
  conf.linker.command   = ENV["EMCC"] || "emcc"
  conf.archiver.command = ENV["EMAR"] || "emar"

  # --- compiler flags ---
  conf.cc.flags << %w[
    -O3
    -sSTRICT=1
    -sENVIRONMENT=web
  ]

  # --- linker flags ---
  conf.linker.flags << %w[
    -O3
    -sSTRICT=1
    -sALLOW_MEMORY_GROWTH=1
    -sMODULARIZE=1
    -sEXPORT_ES6=1
  ]

  # Include mRuby compiler for runtime script evaluation (minimal set to avoid command line length issues)
  conf.gem core: 'mruby-compiler'
  conf.gem core: 'mruby-eval'
  conf.gem core: 'mruby-metaprog'
  conf.gem core: 'mruby-error'
  conf.gem core: 'mruby-sprintf'
  conf.gem core: 'mruby-math'
end
