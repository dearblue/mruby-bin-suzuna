#!ruby

require "pathname"

MRuby::Build.new do |conf|
  toolchain :clang

  conf.build_dir = "build/suzuna"
  cc.defines << "MRB_INT64"
  cxx.defines << "MRB_INT64"

  cc.include_paths << "/usr/local/include"
  cxx.include_paths << "/usr/local/include"

  linker.libraries << "pthread"

  #enable_debug

  exclude_gems = %w(mruby-exit)

  mrbroot = Pathname(MRUBY_ROOT)
  mrbgemsdir = mrbroot + "mrbgems"
  mrbgems = mrbgemsdir.glob("*/mrbgem.rake")
  mrbgems.map! { |e| e.dirname.relative_path_from(mrbgemsdir).to_s }
  mrbgems.reject! { |e| e =~ /^mruby-(?:bin|test)/ || exclude_gems.include?(e) }
  mrbgems.sort!
  mrbgems.each { |g| gem core: g }

  gem core: "mruby-bin-mruby"

  gem mgem: "mruby-errno"
  gem mgem: "mruby-stringio"
  gem mgem: "mruby-onig-regexp"
  #gem mgem: "mruby-optparse"   mruby-exit に依存するため使わない
  gem mgem: "mruby-tiny-opt-parser"
  gem mgem: "mruby-sqlite"
  gem mgem: "mruby-digest"
  gem mgem: "mruby-crc"
  gem mgem: "mruby-lz4"
  gem mgem: "mruby-lzma"
  gem mgem: "mruby-zstd"
  gem __dir__
end
