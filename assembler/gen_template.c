#include <stdio.h>
#include "types.h"
#include "parser.h"
#include "vec.h"
#include "gen.h"

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
        snprintf(template->valid_path, 256, "%.255s", Parser_path(parser));
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
    template->expanded_flag = false;

    *parser = parser_copy;
    return ParserMsg_new(parser->offset, NULL);
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
    template.expanded_flag = self->expanded_flag;

    return template;
}

void Template_free(Template self) {
    Vec_free(self.vars);
}

void Template_free_for_vec(inout void* ptr) {
    Template* self = ptr;
    Template_free(*self);
}

