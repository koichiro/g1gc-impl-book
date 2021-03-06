= 準備

HotspotVMとは何かという点から話を進めていきましょう。

== HotspotVMとは

HotspotVMはサン・マイクロシステムズ社（現：オラクル）主導で開発されているもっともポピュラーなJavaVMです。

HotspotVMの特徴は「プログラムの実行頻度の高い部分のみ機械語にコンパイルする」という点です。
これにはプログラムの実行時間を多く費やす部分（実行頻度の高い部分）を最適化し、プログラム全体の実行時間を短くしようという狙いがあります。
また、機械語へのコンパイルをある程度の範囲に絞るため、コンパイル時間が短くなるという効果もあります。
「実行頻度の高い部分」をHotspotと呼びます。この点がHotspotVMの名前の由来となっています。

HotspotVMのもう1つの特徴は複数のGCアルゴリズムが実装されているという点でしょう。
通常GCアルゴリズムは、レスポンス性能とスループット性能のどちらかを優先してチューンアップします。一般的にレスポンス性能を優先したGCアルゴリズムはスループット性能が低下します。
逆にスループット性能を優先したGCアルゴリズムはレスポンス性能が低下します。
このようなジレンマがあるため、現在のところ完璧なGCは存在しません。HotspotVMはこのジレンマに対する回答として「複数のGCアルゴリズムを実装する」という手法をとりました。
プログラマはアプリケーションの特性に合わせてHotspotVMのGCアルゴリズムを選択することができます。
つまり、レスポンス性能を求めるアプリケーションの場合は、それに適したGCアルゴリズムを「プログラマ」が選択できるということです。プログラマによるGCアルゴリズムの選択は優れた手法だと言えるでしょう。

== OpenJDKとは

Javaの開発用プログラミングツール群のことをまとめて「Java SE Development Kit（JDK）」と呼びます。

JDKにはHotspotVMの他に、JavaのソースコードをJavaバイトコードにコンパイルするJavaコンパイラ、Javaソースコードからドキュメントを生成するツール等が同梱されています。

2006年11月、当時のサンはJDKのソースコードをGPLv2@<fn>{gpl}の下で無償公開することを発表しました。
このオープンソース版のJDKを「OpenJDK」と呼びます。

OpenJDKの最新バージョンは「OpenJDK7」と呼ばれています。
一方、オラクルが正式に提供するJDKの最新バージョンは「JDK7」と呼ばれています。
OpenJDK7とJDK7は名前は異なるものですが、両者のコードはほぼ同じです。
ただし、JDKにはライセンス的にクローズドなコードが一部あるため、OpenJDKではそれをオープンソースとして書き直しているようです。

//footnote[gpl][GPLv2（GNU General Public License）：コピーレフトのソフトウェアライセンスの第2版]

== ソースコード入手

OpenJDKの公式サイトは次のURLです。

@<href>{http://openjdk.java.net/}

//image[openjdk_site][OpenJDK公式サイト]

現在の最新のリリース版はOpenJDK7です。開発マイルストーンは次のURLで現在でも見ることができます。

@<href>{http://openjdk.java.net/projects/jdk7/milestones/}

本章ではOpenJDK7の最新バージョンである「jdk7-b147」を解説対象とします。

では、ソースコードをダウンロードしましょう。

OpenJDK7の最新開発バージョンのソースコードは次のURLからダウンロードすることができます。

@<href>{http://download.java.net/openjdk/jdk7/}

特定の開発バージョンのソースコードを入手したい場合は「Mercurial」@<fn>{mercurial}を使います。
MercurialとはPythonで作られたフリーの分散バージョン管理システムです。

OpenJDK内には複数のプロジェクトがあり、それにともなって複数のリポジトリがあります。
本章ではHotspotVMのソースコードのみ必要となりますので、Mercurialを使ってHotspotVMプロジェクトのリポジトリからソースコードを入手しましょう。
次のコマンドを入力すると、リポジトリからコードツリーをチェックアウトできます。

//emlist{
hg clone -r jdk7-b147 http://hg.openjdk.java.net/jdk7/jdk7/hotspot hotspot
//}

//footnote[mercurial][Mercurial公式サイト：http://mercurial.selenic.com/wiki/]

== ソース構成

HotspotVMのソースコードを入手したら、その中に「src」という名前のディレクトリがあるとおもいます。
そこにHotspotVMのソースコードが置かれています。

//table[dir][ディレクトリ構成]{
ディレクトリ	概要
------------------------------
cpu			CPU依存コード群
os			OS依存コード群
os_cpu		OS、CPU依存コード群（Linuxでかつx86等）
share		共通コード群
//}

@<table>{dir}内最後の「share」ディレクトリ内には「vm」というディレクトリがあります。
この「vm」ディレクトリの中にHotspotVMの大半部分のコードが置かれています。

//comment[TODO: vm内ディレクトリ構成]

また、「src」ディレクトリ内のソースコード分布を@<table>{source_stat}に示します。

//table[source_stat][ソースコード分布]{
言語	ソースコード行数	割合
---------------------------
C++		420,791 		93%
Java	21,231			5%
C		7,432			2%
//}

HotspotVMは約45万行のソースコードから成り立っており、そのほとんどがC++で書かれています。

== G1GCのアルゴリズム

HotspotVMには次のGCアルゴリズムが実装されています。

 * 世代別GC
 * 並列世代別GC
 * 並行マークスイープGC
 * G1GC

本書ではOpenJDK7から導入されたG1GCについて説明します。

== 特殊なクラス

HotspotVM内のほとんどのクラスは次の2つのクラスのいずれかを継承しています。

 * @<code>{CHeapObj}クラス
 * @<code>{AllStatic}クラス

ソースコード上ではこれらのクラスが頻出しますので、ここで意味をおさえておきましょう。

=== CHeapObjクラス

@<code>{CHeapObj}クラスはCのヒープ領域上のメモリで管理されるクラスです。
@<code>{CHeapObj}クラスを継承するクラスのインスタンスは、Cヒープ上にメモリ確保されます。

@<code>{CHeapObj}クラスの特殊な点はオペレータの@<code>{new}と@<code>{delete}を書き換え、C++の通常のアロケーションにデバッグ処理を追加してるところです。
このデバッグ処理は開発時にのみ使用されます。

@<code>{CHeapObj}クラスにはいくつかのデバッグ処理が実装されていますが、その中で「メモリ破壊検知」について簡単に見てみましょう。

デバッグ時、@<code>{CHeapObj}クラス（または継承先クラス）のインスタンスを生成する際に、わざと余分にアロケーションします。
アロケーションされたメモリのイメージを@<img>{cheap_obj_debug_by_new}に示します。

//image[cheap_obj_debug_by_new][デバッグ時のCHeapObjインスタンス]

余分にアロケーションしたメモリ領域を@<img>{cheap_obj_debug_by_new}内の「メモリ破壊検知領域」として使います。
メモリ破壊検知領域には@<code>{0xAB}という値を書き込んでおきます。
@<code>{CHeapObj}クラスのインスタンスとして@<img>{cheap_obj_debug_by_new}内の真ん中のメモリ領域を使用します。

そして、@<code>{CHeapObj}クラスのインスタンスの@<code>{delete}時に「メモリ破壊検知領域」が@<code>{0xAB}のままであるかをチェックします。
もし、書き換わっていれば、@<code>{CHeapObj}クラスのインスタンスの範囲を超えたメモリ領域に書き込みが行われたということです。
これはメモリ破壊が行われた証拠となりますので、発見次第エラーを出力し、終了します。

=== AllStaticクラス

@<code>{AllStatic}クラスは「静的な情報のみをもつクラス」という意味を持つ特殊なクラスです。

@<code>{AllStatic}クラスを継承したクラスはインスタンスを生成できません。
継承するクラスにはグローバル変数やそのアクセサ、静的（static）なメンバ関数などがよく定義されます。
グローバル変数や関数を1つの名前空間にまとめたいときに、@<code>{AllStatic}クラスを継承します。

== 各OS用のインタフェース

HotspotVMは様々なOS上で動作する必要があります。
そのため、各OSのAPIを統一のインタフェースを使って扱う便利な機構が用意されています。

//source[share/vm/runtime/os.hpp]{
80: class os: AllStatic {
       ...
223:   static char*  reserve_memory(size_t bytes, char* addr = 0,
224:                                size_t alignment_hint = 0);
       ...
732: };
//}

@<code>{os}クラスは@<code>{AllStatic}クラスを継承するためインスタンスを作らずに利用します。

@<code>{os}クラスに定義されたメンバ関数の実体は各OSに対して用意されています。

 * os/posix/vm/os_posix.cpp
 * os/linux/vm/os_linux.cpp
 * os/windows/vm/os_windows.cpp
 * os/solaris/vm/os_solaris.cpp

上記ファイルはOpenJDKのビルド時に各OSに合う適切なものが選択され、コンパイル・リンクされます。
@<code>{os/posix/vm/os_posix.cpp}はPOSIX API準拠のOS（LinuxとSorarisの両方）に対してリンクされます。例えばLinux環境では@<code>{os/posix/vm/os_posix.cpp}と@<code>{os/linux/vm/os_linux.cpp}がリンクされます。

そのため、例えば上記の@<code>{share/vm/runtime/os.hpp}で定義されている@<code>{os::reserve_memory()}を呼び出し時には、各OSで別々の@<code>{os::reserve_memory}が実行されます。

@<code>{os:xxx()}というメンバ関数はソースコード上によく登場しますので、よく覚えておいてください。

