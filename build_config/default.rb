MRuby::Build.new do |conf|
  # Default toolchain (GCC or Clang)
  toolchain :gcc

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
    # Use C99 standard
    cc.flags << '-std=c99'

    # Warning flags
    cc.flags << '-Wall' << '-Werror-implicit-function-declaration'

    # Optimization
    cc.flags << '-O2'

    # Position independent code (for static linking)
    cc.flags << '-fPIC' unless RUBY_PLATFORM.include?('darwin')

    # Define macros
    cc.defines << 'MRB_USE_FLOAT32' if ENV['MRB_USE_FLOAT']
  end

  # C++ compiler settings
  conf.cxx do |cxx|
    cxx.flags = conf.cc.flags.dup
    cxx.flags.delete('-std=c99')
    cxx.flags << '-std=c++11'
  end

  # Linker settings
  conf.linker do |linker|
    # Default linker settings
  end

  # Archiver settings
  conf.archiver do |archiver|
    # Default archiver (ar)
  end
end
