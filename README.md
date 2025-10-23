# as-macro
<img src="https://github.com/user-attachments/assets/ab7c0fcd-b786-4672-9856-51ef4b415554" width="500">

as-macroは、中水準プログラミングと高級アセンブラを合わせたアセンブラを作るプロジェクトです

## 紹介動画
https://github.com/user-attachments/assets/3e8e74df-f1b1-4552-8f6b-757e1280aee9


## サンプルコード
```
import std;
import std.io;

pub fn main() {
    println("Hello, World!");
};
```

```
pub as memcpy(in dst: *void@reg+mem, in src: *void@reg+mem, in size: u64@reg+mem+imm) {
    let dst_auto: *u8@auto = dst;
    let src_auto: *u8@auto = src;
    let size_auto: u64@auto = size;

    {
        let dst_rdi: *u8@rdi = dst_auto;
        let src_rsi: *u8@rsi = src_auto;
        let size_rcx: u64@rcx = size_auto;
        
        cld();

        rep(size_rcx);
        movs(dst_rdi, src_rsi);
    };
};
```

## Wiki
[as-macro Wiki](https://github.com/kntt32/as-macro/wiki)

## 関連リポジトリ
[as-macro-rs](https://github.com/kntt32/as-macro-rs) : as-macroアセンブラをRustで書き直し  
[as-os](https://github.com/kntt32/as-os/) : as-macroとRustを使用する予定のOS
