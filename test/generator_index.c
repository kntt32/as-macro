#include <assert.h>
#include <elf.h>
#include "gen.h"
#include "gen_elf.h"

#define OK(expr) assert(SResult_is_success(expr))

int main(void) {
    Generator a = Generator_new(Vec_new(sizeof(char*)));
    Generator b = Generator_new(Vec_new(sizeof(char*)));
    OK(Generator_new_section(&a, ".text"));
    OK(Generator_append_binary(&a, ".text", 0x90));
    /* A per-process section cache used to leak indices between generators. */
    OK(Generator_new_section(&b, ".data"));
    OK(Generator_new_section(&b, ".text"));
    u32 len;
    OK(Generator_binary_len(&b, ".text", &len));
    assert(len == 0);
    assert(!SResult_is_success(Generator_new_section(&b, ".text")));
    Type type;
    OK(Generator_get_type(&a, "u64", "", &type));
    assert(type.size == 8);
    Type_free(type);
    assert(!SResult_is_success(Generator_get_type(&a, "missing", "", &type)));

    /* Force the indexed vectors and hash tables to grow independently. */
    for(u32 i = 0; i < 1000; i++) {
        char name[32];
        snprintf(name, sizeof(name), "function%u", i);
        Asmacro macro = Asmacro_new_fn_extern(name, Vec_new(sizeof(Variable)), "");
        OK(Generator_add_asmacro(&a, macro));
    }
    Asmacro wrapper = Asmacro_new_fn_wrapper("function0", Vec_new(sizeof(Variable)), "");
    OK(Generator_add_asmacro(&a, wrapper));
    Asmacro private_macro = Asmacro_new_fn_extern("function0", Vec_new(sizeof(Variable)), "module");
    OK(Generator_add_asmacro(&a, private_macro));
    Vec macros;
    OK(Generator_get_asmacro(&a, "function0", &macros));
    assert(macros.len == 2);
    assert(((Asmacro*)Vec_index(&macros, 0))->type == Asmacro_FnWrapper);
    assert(strcmp(((Asmacro*)Vec_index(&macros, 1))->valid_path, "module") == 0);
    Vec_free_all(macros, Asmacro_free_for_vec);

    OK(Generator_append_label(&a, NULL, "forward", true, Label_Func));
    OK(Generator_append_label(&a, ".text", "local", false, Label_Notype));
    OK(Generator_append_label(&a, ".text", "forward", true, Label_Func));
    assert(a.labels.len == 2);
    OK(Generator_append_binary(&a, ".text", 0x90));
    OK(Generator_end_label(&a, "forward"));
    assert(((Label*)Vec_index(&a.labels, 0))->size == 1);

    OK(Generator_append_rela(&a, ".text", "forward", 0, false));
    OK(Generator_append_binary(&a, ".text", 0x90));
    OK(Generator_append_binary(&a, ".text", 0x90));
    OK(Generator_addend_rela(&a, ".text"));
    Rela* rela = Vec_index(&a.relas, 0);
    assert(rela->flag && rela->addend == -2);
    OK(Generator_addend_rela(&a, ".text"));
    assert(rela->addend == -2);
    assert(ELF64_R_SYM(Elf64_Rela_from(rela, &a).r_info) == 2);
    /* Adding a local label changes all global ELF symbol indices. */
    OK(Generator_append_label(&a, ".text", "another_local", false, Label_Notype));
    assert(ELF64_R_SYM(Elf64_Rela_from(rela, &a).r_info) == 3);

    OK(Generator_new_section(&a, ".data"));
    OK(Generator_append_rela(&a, ".data", "forward", 7, false));
    OK(Generator_append_rela(&a, ".text", "local", 5, true));
    OK(Generator_append_rela(&a, ".text", "forward", 0, false));
    OK(Generator_append_binary(&a, ".text", 0x90));
    OK(Generator_addend_rela(&a, ".text"));
    assert(!((Rela*)Vec_index(&a.relas, 1))->flag);
    assert(((Rela*)Vec_index(&a.relas, 2))->addend == 5);
    assert(((Rela*)Vec_index(&a.relas, 3))->addend == -1);
    OK(Generator_append_binary(&a, ".data", 0x00));
    OK(Generator_addend_rela(&a, ".data"));
    assert(((Rela*)Vec_index(&a.relas, 1))->addend == 6);

    Generator_free(a);
    Generator_free(b);
    puts("generator index tests passed");
}
