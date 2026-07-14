MRuby::Build.new do |conf|
  # Force the Visual C++ toolchain so the host build emits an MSVC-ABI
  # static library (libmruby.lib) that links against the Lupine engine
  # when built with MSVC or Clang-cl on Windows.
  conf.toolchain :visualcpp

  # include the GEM box
  conf.gembox 'default'
end
