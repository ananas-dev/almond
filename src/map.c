#include <SDL3/SDL_iostream.h>
#include <cglm/cglm.h>
#include <stdio.h>
#include <assert.h>

typedef struct {
    const char* data;
    size_t current;
    size_t start;
} Lexer;

typedef enum {
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_IDENTIFIER,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_EOF,
    TOKEN_ERROR,
} TokenKind;

typedef struct {
    TokenKind kind;
    const char* value;
    size_t size;
} Token;

static bool is_eof(Lexer* lexer)
{
    return lexer->data[lexer->current] == '\0';
}

static bool is_decimal(char c)
{
    return c >= '0' && c <= '9';
}

static bool is_alpha(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static bool is_alphanum(char c)
{
    return is_alpha(c) || is_decimal(c);
}

static char peek_char(Lexer* lexer)
{
    if (is_eof(lexer))
        return '\0';
    return lexer->data[lexer->current];
}

static char peek_next_char(Lexer* lexer)
{
    if (is_eof(lexer))
        return '\0';
    return lexer->data[lexer->current + 1];
}

static char consume_char(Lexer* lexer)
{
    char c = peek_char(lexer);
    lexer->current++;
    return c;
}

static void skip_whitespaces(Lexer* lexer)
{
    for (;;) {
        char c = peek_char(lexer);
        switch (c) {
        case ' ':
        case '\n':
        case '\r':
        case '\t':
            consume_char(lexer);
            break;
        case '/':
            if (peek_next_char(lexer) == '/') {
                while (peek_char(lexer) != '\n' && !is_eof(lexer))
                    consume_char(lexer);
            }
            break;
        default:
            return;
        }
    }
}

Token make_token(Lexer* lexer, TokenKind kind)
{
    return (Token){
        .kind = kind,
        .value = &lexer->data[lexer->start],
        .size = lexer->current - lexer->start,
    };
}

Token lexer_next(Lexer* lexer)
{
    skip_whitespaces(lexer);
    if (is_eof(lexer))
        return make_token(lexer, TOKEN_EOF);

    lexer->start = lexer->current;

    char c = consume_char(lexer);

    switch (c) {
    case '(':
        return make_token(lexer, TOKEN_LEFT_PAREN);
    case ')':
        return make_token(lexer, TOKEN_RIGHT_PAREN);
    case '{':
        return make_token(lexer, TOKEN_LEFT_BRACE);
    case '}':
        return make_token(lexer, TOKEN_RIGHT_BRACE);
    case '"': {
        while (consume_char(lexer) != '"') {
            if (is_eof(lexer))
                return make_token(lexer, TOKEN_EOF);
        }

        return make_token(lexer, TOKEN_STRING);
    }
    default: {
        if (is_decimal(c) || c == '-') {
            while (is_decimal(peek_char(lexer)))
                consume_char(lexer);

            if (peek_char(lexer) == '.' && is_decimal(peek_next_char(lexer))) {
                consume_char(lexer);

                while (is_decimal(peek_char(lexer)))
                    consume_char(lexer);
            }

            return make_token(lexer, TOKEN_NUMBER);
        }

        if (is_alpha(c)) {
            while (is_alphanum(peek_char(lexer)))
                consume_char(lexer);
            return make_token(lexer, TOKEN_IDENTIFIER);
        }
    }
    break;
    }

    return make_token(lexer, TOKEN_ERROR);
}

typedef struct {
    Lexer lexer;
} Parser;

typedef struct {
    const char *key, value;
} Metadata;

typedef struct {
    const char* classname;
    const char* model;
} Entity;

static float parse_number(Parser* parser)
{
    Token token = lexer_next(&parser->lexer);

    if (token.kind != TOKEN_NUMBER) {
        assert(false && "Expected number");
    }

    // Avoid bloating the stack
    if (token.size > 64) {
        assert(false && "String too long");
    }

    char number_str[token.size + 1];
    SDL_memcpy(number_str, token.value, token.size);
    number_str[token.size] = '\0';

    float number = (float)SDL_strtod(number_str, NULL);

    printf("%f ", number);

    return number;
}

static void parse_vector3(Parser* parser)
{
    if (lexer_next(&parser->lexer).kind != TOKEN_LEFT_PAREN) {
        assert(false && "Missing opening paren");
    }

    parse_number(parser);
    parse_number(parser);
    parse_number(parser);

    puts("\n");

    if (lexer_next(&parser->lexer).kind != TOKEN_RIGHT_PAREN) {
        assert(false && "Missing closing paren");
    }
}

static void parse_entity(Parser* parser)
{
    for (;;) {
        Token token = lexer_next(&parser->lexer);

        if (token.kind == TOKEN_STRING) {
            Token value_token = lexer_next(&parser->lexer);

            if (value_token.kind != TOKEN_STRING) {
                assert(false && "Malformed metadata");
            }

            printf("%.*s: %.*s\n", (int)token.size, token.value, (int)value_token.size, value_token.value);
        } else if (token.kind == TOKEN_LEFT_BRACE) {
            for (;;) {
                parse_vector3(parser);
                parse_vector3(parser);
                parse_vector3(parser);

                if (lexer_next(&parser->lexer).kind != TOKEN_IDENTIFIER) {
                    assert(false);
                }

                parse_number(parser);
                parse_number(parser);
                parse_number(parser);
                parse_number(parser);
                parse_number(parser);

                Lexer checkpoint = parser->lexer;

                if (lexer_next(&parser->lexer).kind == TOKEN_RIGHT_BRACE) {
                    break;
                };

                parser->lexer = checkpoint;
            }
        } else if (token.kind == TOKEN_RIGHT_BRACE) {
            break;
        }
    }

}

static void parse(Parser* parser)
{
    Token token = lexer_next(&parser->lexer);

    while (token.kind == TOKEN_LEFT_BRACE) {
        parse_entity(parser);
        token = lexer_next(&parser->lexer);
    }
}

void load_map(const char* filename)
{
    size_t map_data_size;
    char* map_data = SDL_LoadFile(filename, &map_data_size);

    assert(map_data);

    // TODO: Load into a map arena idk

    Lexer lexer = { 0 };
    lexer.data = map_data;

    Parser parser = { 0 };
    parser.lexer = lexer;

    parse(&parser);

    // Token token = lexer_next(&lexer);
    //
    // while (token.kind != TOKEN_EOF && token.kind != TOKEN_ERROR) {
    //     printf("TOKEN [%d]: %.*s\n", token.kind, (int)token.size, token.value);
    //     token = lexer_next(&lexer);
    // }

    SDL_free(map_data);
}