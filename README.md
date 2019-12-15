# mruby-bin-suzuna - The framework of FreeBSD GEOM Gate local class provider for mruby

FreeBSD の GEOM Gate local class provider を組み立てるためのフレームワークです。

カーネルドライバを書くことなく、任意のブロックデバイスを mruby 上で実現できます。


## できること

\# そのうち書きます。


## くみこみかた

`build_config.rb` に gem として追加し、mruby をビルドして下さい。

```ruby
MRuby::Build.new do |conf|
  conf.gem "mruby-bin-suzuna", github: "dearblue/mruby-bin-suzuna"
end
```


## つかいかた

`mruby-bin-suzuna` を `build_config.rb` に記述子してビルドします。
手っ取り早く利用したい人のために、`mruby-bin-suzuna/quick_config.rb` を用意してあります。

```terminal
% git clone -b 2.1.0 https://github.com/mruby/mruby.git workspace/mruby
% git clone https://github.com/dearblue/mruby-bin-suzuna.git workspace/mruby-bin-suzuna
% cd workspace/mruby
% ./minirake -j8 MRUBY_CONFIG=../mruby-bin-suzuna/quick_config.rb
...
```

ビルドが成功したら、GEOM Gate ローカルプロバイダとして動作させる Ruby スクリプトを書きます。
名前は何でもいいです。

[mruby-bin-suzuna/sample.rb](sample.rb) として用意してあるので、例としてこのファイルを使うことにします。

あとは root で実行するだけです (内部で `kldload geom_gate` されます)。
ビルドした実行ファイルは `bin/suzuna` (あるいは `build/suzuna/bin/suzuna`) にあります。

```
% su -m root -c 'bin/suzuna sample.rb'
...wait to finish...
```

root で実行したくない場合は、`/dev/ggctl` のパーミッションを変更してしまいましょう。
***その場合はセキュリティー上の問題が出たり、想定外のデータの破損につながるかも知れないので、あなたが責任を取れる環境下で行って下さい。***

`suzuna` を実行すると `/dev/ggateXXX` が生えるので、`newfs` するなり何なりしていじれます。
***swap デバイスとして利用するとカーネルパニックしたりデッドロックが発生してシステムが動作しなくなるかも知れません。絶対にやめましょう。***

```
% ls /dev/ggate*
/dev/ggate0
```

停止させたい場合は `ggatel destroy` して下さい。
`ctrl+C` で止めると、最悪の場合、書き込んだデータの損失に繋がります (そのうち直します)。


## Specification

- Package name: mruby-bin-suzuna
- Version: 0.0.1.CONCEPT
- Product quality: CONCEPT
- Author: [dearblue](https://github.com/dearblue)
- Project page: <https://github.com/dearblue/mruby-bin-suzuna>
- Licensing: [2 clause BSD License](LICENSE)
- Support platform: ***FreeBSD only***
- Dependency external mrbgems: (NONE)
- Bundled C libraries (git-submodules): (NONE)
