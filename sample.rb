# これは mruby-bin-suzuna でビルドして出来た bin/suzuna に与えることの出来るサンプルファイルです。
#
# このファイルはクリエイティブ・コモンズ ゼロ ライセンス (CC0; パブリックドメイン) の下で利用できます。
# https://creativecommons.org/publicdomain/zero/1.0/deed.ja

mystorage = Object.new

mystorage.instance_eval do
  # 200 MiB のバッファを用意
  @buf = StringIO.new("\0" * (200 * 1024 * 1024))

  # mruby-2.1.0 の mruby-io は File#truncate と File#size がなく、File.open の mode は整数フラグを受け付けない
  #@buf = File.open("rawdisk.bin", File::RDWR | File::CREAT | File::BINARY);
  #@buf.truncate(2000 * 1024 * 1024)
end

class << mystorage
  # ボリュームサイズを返して下さい。
  #
  # #sectorsize の整数倍でなければなりません。
  def mediasize
    @buf.size
  end

  # ボリュームの読み書き属性を返して下さい。
  #
  # Suzuna::FLAG_READWRITE、Suzuna::FLAG_READONLY、Suzuna::FLAG_WRITEONLY のいずれか一つです。
  def flags
    Suzuna::FLAG_READWRITE
    #Suzuna::FLAG_READONLY
    #Suzuna::FLAG_WRITEONLY
  end

  # ボリュームのセクタサイズ (ブロックサイズ) を返して下さい。
  def sectorsize
    512
  end

  # (任意) ggatel info の時に表示される文字情報を返して下さい。
  def info
    "Powered by #{MRUBY_DESCRIPTION}"
  end

  # (任意) ggatel destroy された時に呼ばれます。
  #
  # 返り値は無視されます。
  def cleanup
    nil
  end

  # ボリュームから読み込む時に呼ばれます。
  #
  # errno の値を返して下さい。
  #
  # nil を返すことで、エラーがなかったという意味になります。
  def readat(offset, size, buf)
    @buf.seek offset
    @buf.read(size, buf)
    nil
  end

  # ボリュームへ書き込む時に呼ばれます。
  #
  # errno の値を返して下さい。
  #
  # nil を返すことで、エラーがなかったという意味になります。
  def writeat(offset, buf)
    @buf.seek offset
    @buf.write(buf)
    nil
  end

  # ボリュームの指定ブロックが不要となった時に呼ばれます。
  #
  # errno の値を返して下さい。
  #
  # nil を返すことで、エラーがなかったという意味になります。
  #
  # trim コマンドを持つハードウェアに対する処理を記述しておくといいかもしれません。
  def deleteat(offset, size)
    #@buf.seek offset
    #@buf.write("\0" * size)
    nil
  end
end

# GEOM Gate ローカルプロバイダとして処理されるオブジェクトを登録します。
Suzuna.unit(mystorage)

# ファイルから抜けると、実際の I/O 処理が行われるようになります。
