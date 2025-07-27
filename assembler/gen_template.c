#include <stdio.h>
#include <assert.h>
#include "types.h"
#include "parser.h"
#include "vec.h"
#include "gen.h"
#include "util.h"

static ParserMsg Template_parse_arguments(Parser parser, inout Template* template) {
    Vec args_vec = Vec_new(sizeof(char)*256);
    while(!Parser_is_empty(&parser)) {
        char token[256];
        PARSERMSG_UNWRAP(
            Parser_parse_ident(&parser, token), Vec_free(args_vec)
        );
        Vec_push(&args_vec, token);

        if(!Parser_is_empty(&parser)) {
            PARSERMSG_UNWRAP(
                Parser_parse_symbol(&parser, ","), Vec_free(args_vec)
            );
        }
    }

    template->vars = args_vec;
    return ParserMsg_new(parser.offset, NULL);
}

ParserMsg Template_parse(inout Parser* parser, out Template* template) {
    // (pub) template $ident ( $ident, .. ) { .. };
    Parser parser_copy = *parser;
    bool public_flag = ParserMsg_is_success(Parser_parse_keyword(&parser_copy, "pub"));
    PARSERMSG_UNWRAP(
        Parser_parse_keyword(&parser_copy, "template"), (void)NULL
    );

    PARSERMSG_UNWRAP(
        Parser_parse_ident(&parser_copy, template->name), (void)NULL
    );
    if(public_flag) {
        template->valid_path[0] = '\0';
    }else {
        wrapped_strcpy(template->valid_path, Parser_path(parser), 256);
    }

    Parser paren_parser;
    PARSERMSG_UNWRAP(
        Parser_parse_paren(&parser_copy, &paren_parser), (void)NULL
    );
    Parser block_parser;
    PARSERMSG_UNWRAP(
        Parser_parse_block(&parser_copy, &block_parser), (void)NULL
    );

    PARSERMSG_UNWRAP(
        Template_parse_arguments(paren_parser, template), (void)NULL
    );
    template->parser = block_parser;
    template->parser_vars = Vec_new(sizeof(Vec*));

    *parser = parser_copy;
    return ParserMsg_new(parser->offset, NULL);
}

SResult Template_expand(inout Template* self, Vec parser_vars, out bool* expanded_flag, out optional Parser* template_parser) {
    assert(self != NULL && expanded_flag != NULL && template_parser != NULL);
    assert(Vec_size(&parser_vars) == sizeof(ParserVar));

    if(Vec_len(&self->vars) != Vec_len(&parser_vars)) {
        Vec_free(parser_vars);
        return SResult_new("mismatching template arguments");
    }

    assert(Vec_size(&self->parser_vars) == sizeof(Vec*));
    for(u32 i=0; i<Vec_len(&self->parser_vars); i++) {
        Vec** expanded_parser_vars = Vec_index(&self->parser_vars, i);
        assert(Vec_size(*expanded_parser_vars) == sizeof(ParserVar));
        if(Vec_cmp(*expanded_parser_vars, &parser_vars, ParserVar_cmp_value_for_vec)) {
            Vec_free(parser_vars);
            *expanded_flag = true;
            return SResult_new(NULL);
        }
    }

    for(u32 i=0; i<Vec_len(&self->vars); i++) {
        ParserVar* parser_var = Vec_index(&parser_vars, i);
        char* name = Vec_index(&self->vars, i);

        wrapped_strcpy(parser_var->name, name, sizeof(parser_var->name));
    }

    Vec* parser_vars_buff = malloc(sizeof(Vec));
    UNWRAP_NULL(parser_vars_buff);
    *parser_vars_buff = parser_vars;
    Vec_push(&self->parser_vars, &parser_vars_buff);

    *template_parser = self->parser;
    Parser_set_parser_vars(template_parser, parser_vars_buff);

    *expanded_flag = false;

    return SResult_new(NULL);
}

void Template_print(in Template* self) {
    printf("Template { name: %s, valid_path: %s, vars_len: %u, parser: ",
        self->name,
        self->valid_path,
        Vec_len(&self->vars));
    Parser_print(&self->parser);
    printf(" }");
}

void Template_print_for_vec(in void* self) {
    Template_print(self);
}

Template Template_clone(in Template* self) {
    Template template;

    strcpy(template.name, self->name);
    strcpy(template.valid_path, self->valid_path);
    template.vars = Vec_clone(&self->vars, NULL);
    template.parser = self->parser;
    template.parser_vars = Vec_new(sizeof(ParserVar));

    return template;
}

static void Template_free_helper(in void* ptr) {
    Vec** vec_ptr = ptr;
    Vec* vec = *vec_ptr;

    Vec_free(*vec);
    free(vec);
}

void Template_free(Template self) {
    Vec_free(self.vars);
    Vec_free_all(self.parser_vars, Template_free_helper);
}

void Template_free_for_vec(inout void* ptr) {
    Template* self = ptr;
    Template_free(*self);
}

