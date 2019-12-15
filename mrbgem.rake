MRuby::Gem::Specification.new("mruby-bin-suzuna") do |s|
  s.summary = "Framework of GEOM Gate local class provider for mruby"
  version = File.read(File.join(__dir__, "README.md")).scan(/^\s*[\-\*] version:\s*(\d+(?:\.\w+)+)/i).flatten[-1]
  s.version = version if version
  s.license = "BSD-2-Clause"
  s.author  = "dearblue"
  s.homepage = "https://github.com/dearblue/mruby-bin-suzuna"

  add_dependency "mruby-error", core: "mruby-error"
  #add_dependency "mruby-string-ext", core: "mruby-string-ext"
  add_dependency "mruby-aux", github: "dearblue/mruby-aux"

  if cc.command =~ /\b(?:g?cc|clang)\d*\b/
    cc.flags << %w(-Wno-declaration-after-statement)
  end

  s.bins = %W(suzuna)
end
