# as-macro
as-macroは、中水準プログラミングと高級アセンブラを合わせたようなas-macroと、リンカーas-linkをセルフホストするというプロジェクトです
# コード例
```
import core_x64;

as quat(x: i64@reg) {
    add(x, x);
    add(x, x);
};

fn double(x: i64@rax) {
    add(x, x);
};

pub fn main() {
    let a: i64@rax;
    a = 5;
    double(quat(a));
    let b: i64@rdi;
    b = 8;
    quat(b);
};
```
準備中
