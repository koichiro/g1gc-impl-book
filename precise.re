= 正確なGCへの道（HotspotVM編）

この章ではHotspotVMが正確なGCのためにどのような工夫をしているかを見ていきます。

== プリミティブ型と参照型の変数

//listnum[primitive_and_reference_type][プリミティブ型と参照型]{
int primitiveType = 1;                // プリミティブ型
Object referenceType = new Object();  // 参照型
//}

Javaの変数に格納される型として@<code>{int}、@<code>{float}といった「プリミティブ型」があります(@<list>{primitive_and_reference_type}の1行目)。
プリミティブ型はJava上では数値として扱われます。
それと同様にC++（HotpostVM）上でも@<code>{int}や@<code>{float}といった数値として扱われます。

一方、Objectクラス（またはその子クラス）のインスタンスを指す「参照型」があります(@<list>{primitive_and_reference_type}の2行目)。
参照型はC++（HotspotVM）上ではオブジェクトへのポインタとして扱われます。

ここで問題となるのが「プリミティブ型はVM上で数値として扱われる」という点です。
つまり、プリミティブ型の値は「ポインタのような非ポインタ」である可能性があります。
したがって、GCを正確なGCとするためには、プリミティブ型と参照型の変数を識別しなければなりません。

== HotspotVMの構造

参照型の識別の前に、HotspotVMの構造を簡単に説明しておきましょう。

まず、HotspotVMは基本的にバイトコード（.classファイル）内の命令セットを1つずつ読み込んで、命令に従った処理をこなします。
命令セットは実行する操作を定義した1バイトの「オペコード」と、操作が用いるデータとなる「オペランド」で構成されています。
オペコードは実際には@<code>{0x32}のようなただのバイト列です。
しかし、これでは人間が読むにはあまりに辛いため、通常、オペコードを人間が読めるような@<code>{aaload}という形式で表現します。これを「ニーモニック」と呼びます。

そして、HotspotVMにはJVMスタックとフレームというものがあります。
Cのコールスタック、コールフレームと役割は同じです。
Java上のメソッドが呼ばれると対応するフレームがJVMスタックに積まれ、メソッドの実行が終了するとフレームがJVMスタックから降ろされます。

//image[method_stack_frame][JVMスタック、フレーム]

また、フレームの中にはローカル変数とオペランドスタックというものがあります。
ローカル変数はメソッド内で使用するローカル変数を格納する部分です。
また、メソッドの引数もローカル変数として扱われます。

HotspotVMは「スタックマシン」です。
そのため、VM上の計算はスタックを使って処理します。
HotspotVMはメソッドフレーム内のオペランドスタックを使って計算を行います。

== HotspotVMの実行フロー

では、実際のサンプルコードを使って、ローカル変数とオペランドスタックがどのように使われるか見ていきましょう。

//listnum[TwoDifferentLocalVars][TwoDifferentLocalVars.java]{
class TwoDifferentLocalVars {
    public static void main(String args[]){
        int primitiveType = 1;                // プリミティブ型
        Object referenceType = new Object();  // 参照型
    }
}
//}

@<list>{TwoDifferentLocalVars}の@<code>{main()}メソッドはプリミティブ型と参照型をローカル変数に格納するシンプルなメソッドです。
このメソッドをJavaバイトコードに変換したものが@<list>{TwoDifferentLocalVars_bytecode}です。

//list[TwoDifferentLocalVars_bytecode][TwoDifferentLocalVars.java:バイトコード]{
pc( 0): iconst_1
pc( 1): istore_1
pc( 2): new           #2 // class java/lang/Object
pc( 5): dup
pc( 6): invokespecial #1 // Method java/lang/Object."<init>"
pc( 9): astore_2
pc(10): return
//}

@<list>{TwoDifferentLocalVars_bytecode}内のバイトコードに振られている番号は行番号ではなく、メソッド内のバイトコードに一意に振られているプログラムカウンタ(以下、pc)です。
HotspotVMは@<list>{TwoDifferentLocalVars_bytecode}を上から順番に実行し、ローカル変数にプリミティブ型と参照型の値を格納します。
バイトコードは一見難しそうに見えますが、VMの実行フローのイメージと命令セットの意味が多少理解できれば、読み解くのはそれほど難しくありません。

//table[mnemonic_mean_1][ニーモニックと命令内容]{
ニーモニック	命令内容
---------------------------
iconst_'i'	'i'の部分にあたるintの定数をオペランドスタックに積む
istore_'n'	ローカル変数配列の'n'番目にオペランドスタックの先頭のint型の値を格納する
new		新たなオブジェクトを生成し、オペランドスタックに積む
dup		オペランドスタックの先頭にある値を複製し、オペランドスタックに複製した値を積む
invokespecial	インスタンス初期化メソッド等の特殊なメソッドを呼び出す
astore_'n'	ローカル変数配列の'n'番目にオペランドスタックの先頭の参照型の値を格納する
return		メソッドからvoidをリターンする
//}

@<table>{mnemonic_mean_1}には@<list>{TwoDifferentLocalVars_bytecode}に登場するニーモニックとその命令内容を示しています。

//image[bytecode_flow][バイトコード実行フロー]

@<img>{bytecode_flow}はバイトコードの実行フローを示しています。
最終的にローカル変数1に@<code>{1}が格納され、ローカル変数2には@<code>{Object}クラスのインスタンスのアドレスが格納されています。
ローカル変数1は@<list>{TwoDifferentLocalVars}内の@<code>{primitiveType}変数であり、ローカル変数2は@<code>{referenceType}変数です。

また、上記の様にバイトコードを読み込みながら命令セットを1つずつ実行するインタプリタを「バイトコードインタプリタ」といいます。

さて、GCの話に戻りましょう。
もし、プログラムカウンタ10（以下、pc10）の状態でGCが実行された場合は、GCはローカル変数2が参照するオブジェクトのみを「確実に生きている」と判断しなければなりません。
そして、プリミティブ型の値が格納されているローカル変数1はGC対象ではないということを識別する必要があります。
HotspotVMはどのようにしてローカル変数（またはオペランドスタック）内の値を識別しているのでしょうか？

== 参照マップ

ここで注目するのはJavaの型情報です。
@<list>{TwoDifferentLocalVars_bytecode}を見るとローカル変数やオペランドスタックに格納される際のニーモニックが、プリミティブ型と参照型で異なっていることがわかります。
プリミティブ型の場合、@<code>{istore_1}となっており、参照型の場合は@<code>{astore_2}となっていますね。

HotspotVMはバイトコードの型情報を利用して、GC発生時のフレーム内の「参照マップ」というものを作成します。
参照マップとはその名前の通り、参照型を格納しているローカル変数やオペランドスタックの位置を示した地図です。
実際の参照マップは@<code>{00100}というようなビット列です。
この参照マップは、ビット列のビットが立っている部分に対応するローカル変数（またはオペランドスタック）に参照型の値が格納されていることを示しています。

== 抽象的インタプリタ

参照マップは「抽象的インタプリタ」によって作成されます。
「抽象的インタプリタ」とは簡単にいえば「型情報のみを記録するインタプリタ」のことです。
抽象的インタプリタはバイトコードを実行した結果、ローカル変数とオペランドスタック内に格納された値の型のみを記録します。
実際に格納された値について、抽象的インタプリタは無関心です。

では、前の「HotspotVMの実行フロー」で説明したバイトコード（@<list>{TwoDifferentLocalVars_bytecode}）を使って、抽象的インタプリタと通常のインタプリタとでは具体的にどのように動作が異なるのか見比べていきましょう。

//list[TwoDifferentLocalVars_abs][TwoDifferentLocalVars.java:バイトコード実行フロー（抽象的インタプリタ）]{
BasicBlock#0
pc( 0): locals = 'r  ', stack = ''   // iconst_1
pc( 1): locals = 'r  ', stack = 'v'  // istore_1
pc( 2): locals = 'rv ', stack = ''   // new           #2
pc( 5): locals = 'rv ', stack = 'r'  // dup
pc( 6): locals = 'rv ', stack = 'rr' // invokespecial #1
pc( 9): locals = 'rv ', stack = 'r'  // astore_2
pc(10): locals = 'rvr', stack = ''   // return
//}

抽象的インタプリタの実行フローは簡単に書き表すことができます。
@<list>{TwoDifferentLocalVars_abs}がその実行フローです。
@<list>{TwoDifferentLocalVars_abs}中の@<code>{locals}はローカル変数、@<code>{stack}はオペランドスタックです。
ローカル変数やオペランドスタックにある@<code>{r}（reference）は参照型、@<code>{v}（value）はプリミティブ型と考えてください。
ローカル変数内の半角スペースはまだ初期化されていないという意味です。
また、@<code>{BasicBlock#0}の役割については後の@<hd>{条件分岐時の参照マップ}の項で説明しますので、今は無視してください。

抽象的インタプリタは、ある命令セット実行前のローカル変数とオペランドスタックの型情報を記録します。
例えばpc0では@<code>{iconst_1}の実行前の型情報が記録されています。
そのため、ローカル変数（@<code>{locals}）には引数の@<code>{args}の型を表す@<code>{r}のみが記録されています。
続いて、pc1では@<code>{iconst_1}の実行した結果、オペランドスタック（@<code>{stack}）には1の型
を表す@<code>{v}のみが記録されています。

このように、抽象的インタプリタは実際の値は気にせず、型情報のみを淡々と記録します。
そして、参照マップは抽象的インタプリタが記録したバイトコードに対応する型情報から作成されます。

== 参照マップの作成

GCは基本的に命令セット実行中の様々なタイミングで発生します。
オブジェクトを生成する命令セット実行中にGCが発生するかもしれませんし、足し算の行う命令セット実行中にGCが発生するかもしれません。

もしある命令セット実行中にGCが発生したとき、フレーム内のローカル変数とオペランドスタック内にある参照型が指すオブジェクトはGCに確実に生きていると判断されなければなりません。
そのためには、GCが発生時の命令セット実行時の参照マップを作成する必要があります。

@<list>{TwoDifferentLocalVars_abs}のpc5の命令セット（@<code>{dup}オペコード）を実行中にGCが発生したと想定し、どのように参照マップが作成されるかを見ていきましょう。

//image[pointers_map_at_gc][参照マップ作成]

@<img>{pointers_map_at_gc}は生成した参照マップを示しています。
ローカル変数0とオペランドスタックの先頭に対応するビットが立っていることがわかります。
GCは、この参照マップを見て、ローカル変数0とオペランドスタックの先頭に参照型の値が格納されていると判断し、それらが参照するオブジェクトはGCに確実に「生きている」と判断されます。

== 条件分岐時の参照マップ

今までは条件分岐なしのサンプルプログラムを例にとって参照マップの作成を見てきましたが、実は条件分岐が1つ入るだけで、参照マップの作成は格段と難しくなります。
次のサンプルコードを見てください。

//listnum[TwoControlPath][TwoControlPath.java]{
class TwoControlPath {
    static public void main(String args[]){
        if (args.length == 0) {
            Object referenceType = new Object();
            return;
        } else {
            int primitiveType = 1;
            return;
        }
    }
}
//}

@<list>{TwoControlPath}は引数である@<code>{args}のサイズを判断して、ローカル変数の@<code>{referenceType}か、@<code>{primitiveType}にそれぞれ値を格納します。

@<list>{TwoControlPath}の@<code>{main()}メソッドのバイトコードは次の通りです。

//list[TwoControlPath_bytecoe][TwoControlPath.java:バイトコード]{
pc( 0): aload_0
pc( 1): arraylength
pc( 2): ifne          14
pc( 5): new           #2 // class java/lang/Object
pc( 8): dup
pc( 9): invokespecial #1 // Method java/lang/Object."<init>"
pc(12): astore_1
pc(13): return
pc(14): iconst_1
pc(15): istore_1
pc(16): return
//}

@<list>{TwoControlPath_bytecoe}には新しく@<code>{ifne}というニーモニックが登場しています。
@<code>{ifne}の命令内容は「オペランドスタックの先頭のint型の値を取り出し、その値は@<code>{0}でなければ指定したpcにジャンプする」です。
pc2では@<code>{ifne 14}となっていますので、オペランドスタックの先頭の値（@<code>{args.length}）が@<code>{0}ではない場合、pc14にジャンプします。

さて、ここで注目して欲しいのはpc12とpc15です。
どちらともローカル変数1に対して、プリミティブ型、参照型の値をそれぞれ格納しています。
つまり、条件分岐によってはローカル変数に格納される値が変わるということです。
pc13の時点でGCが発生すれば、ローカル変数1の型は参照型ですが、pc16の時点でGCが発生すれば、ローカル変数1の型はプリミティブ型なのです。

そのため、抽象的インタプリタはすべての状況における型情報を記録しなければなりません。
@<list>{TwoControlPath}では@<code>{args.length}が@<code>{0}であった場合と、それ以外の場合の型情報を記録しなければなりません。

そこで抽象的インタプリタはバイトコードを「ベーシックブロック」という単位に切り分けます。
@<list>{TwoControlPath_bytecoe}の場合は次のようになります。

//list[TwoControlPath_abs_bb][TwoControlPath.java:バイトコード実行フロー（抽象的インタプリタ）]{
BasicBlock#0
pc( 0): locals = 'r ' stack = ''    // aload_0
pc( 1): locals = 'r ' stack = 'r'   // arraylength
pc( 2): locals = 'r ' stack = 'v'   // ifne 14

BasicBlock#2
pc( 5): locals = 'r ' stack = ''    // new
pc( 8): locals = 'r ' stack = 'r'   // dup
pc( 9): locals = 'r ' stack = 'rr'  // invokespecial
pc(12): locals = 'r ' stack = 'r'   // astore_1
pc(13): locals = 'rr' stack = ''    // return

BasicBlock#1
pc(14): locals = 'r ' stack = ''    // iconst_1
pc(15): locals = 'r ' stack = 'v'   // istore_1
pc(16): locals = 'rv' stack = ''    // return
//}

@<list>{TwoControlPath_abs_bb}を見ると、if文の真文、偽文のそれぞれがベーシックブロックに分けられていることがわかります（@<code>{BasicBlock#1}、@<code>{BasicBlock#2}）。
@<code>{BasicBlock#1}のpc14と、@<code>{BasicBlock#2}のpc5のローカル変数、オペランドスタックの型情報は、@<code>{BasicBlock#0}のpc2の実行後のものです。@<code>{BasicBlock#2}では真文のバイトコードを実行しその型情報を記録しています。
一方、@<code>{BasicBlock#1}は偽文の型情報を記録しています。

また、@<list>{TwoControlPath_abs_bb}のpc13とpc16の時点のローカル変数1の型情報が異なる点に注目してください。
つまり、pc13とpc16の時点でGCが発生してもローカル変数1の型を参照マップによってきちんと識別することができます。

このベーシックブロックという仕組みによって、メソッド内の様々な状況の型情報を記録することができます。
ベーシックブロックは条件分岐だけではなく、ループやswitch文、try-catch文等にも同様に使用されます。
@<list>{TwoDifferentLocalVars}のように条件分岐等がメソッド内に登場に登場しない場合は、メソッド内のすべてのバイトコードが@<code>{BasicBlock#0}として扱われます。

== メソッド呼び出し時の参照マップ

今までは実行中フレームの参照マップがどのように作られるかを見てきました。
しかし、JVMスタックにフレームが1個以上積まれていた場合、実行中フレームの下にあるフレームはメソッド呼び出し時の参照マップを作ることになります。

//image[pointer_map_jvm_stack][各フレームの参照マップ作成]

次はメソッド呼び出し時の参照マップがどのように作られるかを見てみましょう。
次のサンプルコードを見てください。

//listnum[MethodCall][MethodCall.java]{
class MethodCall {
    static public void main(String args[]){
        Object referenceType = new Object();
        int primitiveType = 1;
        something(referenceType, primitiveType);
    }

    static void gcCall(Object a, int b){
        System.gc();  // GCの実行
    }
}
//}

@<list>{MethodCall}が@<list>{TwoDifferentLocalVars}と違う点は5行目の@<code>{gcStart()}メソッドを呼び出している点です。
@<code>{gcStart()}メソッドはGCの実行を行うメソッドです。
参照型とプリミティブ型の引数を受け取りますが、これは説明のためのもので、実際には使用しません。

@<list>{MethodCall}の@<code>{main()}メソッドのバイトコードは次のようになります。

//list[MethodCall_bytecode][MethodCall.java:バイトコード]{
pc( 0): iconst_1
pc( 1): istore_1
pc( 2): new           #2 // class java/lang/Object
pc( 5): dup
pc( 6): invokespecial #1 // Method java/lang/Object."<init>"
pc( 9): astore_2
pc(10): iload_1
pc(11): aload_2
pc(12): invokestatic  #3 // Method gcStart
pc(15): return
//}

@<list>{MethodCall_bytecode}が@<list>{TwoDifferentLocalVars_bytecode}と異なるのはpc10～pc15です。
ここでは@<code>{gcStart()}メソッドの呼び出しを行っています。
pc10、pc11でオペランドスタックに@<code>{gcStart()}メソッド用の引数を積み、pc12で@<code>{gcStart()}メソッドを呼び出します。

次に、@<list>{MethodCall_bytecode}を抽象的インタプリタにかけてみましょう。

//list[MethodCall_abs][TwoControlPath.java:バイトコード実行フロー（抽象的インタプリタ）]{
BasicBlock#0
pc( 0): locals = 'r  ' stack = ''   // iconst_1
pc( 1): locals = 'r  ' stack = 'v'  // istore_1
pc( 2): locals = 'rv ' stack = ''   // new
pc( 5): locals = 'rv ' stack = 'r'  // dup
pc( 6): locals = 'rv ' stack = 'rr' // invokespecial
pc( 9): locals = 'rv ' stack = 'r'  // astore_2
pc(10): locals = 'rvr' stack = ''   // iload_1
pc(11): locals = 'rvr' stack = 'v'  // aload_2
pc(12): locals = 'rvr' stack = 'vr' // invokestatic
pc(15): locals = 'rvr' stack = ''   // return
//}

@<code>{gcCall()}メソッドでGCが発生しますので、参照マップは@<list>{MethodCall_abs}のpc12時点のものが作成されます。

//image[pointers_map_at_gc_by_method_call][参照マップ作成（メソッド呼び出し時）]

@<img>{pointers_map_at_gc_by_method_call}を見てください。
実はメソッド呼び出し時の参照マップを作成する際、メソッドの引数に渡すオペランドスタックの値を無視します。
これは引数として渡すオペランドスタックの値が呼び出したメソッドのローカル変数として扱われるためです。
無視したオペランドスタックの値は、呼び出したメソッドのローカル変数として参照マップを使って正しく識別されます。

== ハンドルエリアとハンドルマーク

これまではJVMスタックに対する正確なGCのための工夫を見てきました。
ここからはネイティブな(C++の)コールスタックに対する工夫を見ていきましょう。

HotspotVMは「ハンドルエリア」と「ハンドルマーク」を使ってコールスタック内のオブジェクトへのポインタを管理しています。
V8はHotspotVMを参考にして作られていますので、正確にはV8がHotspotVMのやり方を真似したということになります。

@<list>{handle_area_sample}はハンドラのみを生成するサンプルコードです。

//listnum[handle_area_sample][ハンドラの生成]{
void make_handles(oop obj1, oop obj2) {
     Handle h1(obj1);  // ハンドラ1生成
     Handle h2(obj2);  // ハンドラ2生成
}
//}

HotspotVMのハンドラはスレッド毎にある「ハンドルエリア」に確保されます。
そのため、@<list>{handle_area_sample}で生成されたハンドラは@<img>{handle_area}の様に確保されます。

//image[handle_area][make_handles()実行イメージ]

このままだと、ハンドラが確保されたままになってしまうので、HotspotVMにはもう1つ「ハンドルマーク」というハンドルスコープとほぼ同じ機能があります。

@<list>{handle_area_sample_with_handle_mark}は@<list>{handle_area_sample}にハンドルマークを追加したサンプルコードです。

//listnum[handle_area_sample_with_handle_mark][ハンドラの生成：ハンドルマーク有り]{
void make_handles(oop obj1, oop obj2) {
     HandleMark hm;
     Handle h1(obj1);
     Handle h2(obj2);
}
//}

2行目に登場する@<code>{HandleMark}クラスは、コンストラクタでハンドルアリーナの先頭をマーク（記録）します。
そして、@<code>{HandleMark}クラスのデストラクタでは、マークしておいた位置にハンドルアリーナの先頭を移動させます。

@<img>{handle_area_with_handle_mark}は@<list>{handle_area_sample_with_handle_mark}の実行イメージです。

//image[handle_area_with_handle_mark][make_handles()実行イメージ：ハンドルマーク有り]
