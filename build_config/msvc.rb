MRuby::Build.new do |conf|
  # Gets set by the VS command prompts.
  if ENV['VisualStudioVersion'] || ENV['VSINSTALLDIR']
    toolchain :visualcpp
  else
    toolchain :gcc
  end

  # Use standard I/O
  conf.gembox 'default'

  # Generate mruby debugger command (require mruby-eval)
  conf.gem :core => "mruby-bin-debugger"

  # Bindings
  conf.gem :core => "mruby-bin-mirb"
  conf.gem :core => "mruby-bin-mruby"

  # Generate mruby command
  conf.gem :core => "mruby-bin-strip"

  # C compiler settings
  conf.cc do |cc|
    # Enable C99 standard
    cc.flags << '/std:c11' if cc.command == 'cl'

    # Warning level
    cc.flags << '/W3' if cc.command == 'cl'

    # Optimization flags
    cc.flags << '/O2' if cc.command == 'cl'

    # Multi-threaded DLL runtime (to match CMake project)
    cc.flags << '/MD' if cc.command == 'cl'

    # Define MSVC macro
    cc.defines << 'MRB_USE_FLOAT32' if ENV['MRB_USE_FLOAT']
  end

  # C++ compiler settings (for gems that use C++)
  conf.cxx do |cxx|
    cxx.flags = conf.cc.flags.dup
  end

  # Linker settings
  conf.linker do |linker|
    # Use default linker flags for MSVC
  end

  # Archiver settings for static library
  conf.archiver do |archiver|
    # Use lib.exe for MSVC
  end
end
