#include "map.h"

#include "geometry.h"
#include <almond.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    StringView value;
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
    StringView value = (StringView) {
        &lexer->data[lexer->start],
        lexer->current - lexer->start
    };

    return (Token) {
        .kind = kind,
        .value = value,
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
    } break;
    }

    return make_token(lexer, TOKEN_ERROR);
}

typedef struct {
    Lexer lexer;
    Arena* arena;
    MapEntityCallback* entity_callback;
    void* userdata;
} Parser;

typedef struct {
    const char *key, value;
} Metadata;

static float parse_number(Parser* parser)
{
    Token token = lexer_next(&parser->lexer);

    if (token.kind != TOKEN_NUMBER) {
        assert(false && "Expected number");
    }

    // Avoid bloating the stack
    if (token.value.count > 64) {
        assert(false && "String too long");
    }

    char number_str[token.value.count + 1];
    memcpy(number_str, token.value.data, token.value.count);
    number_str[token.value.count] = '\0';

    float number = (float)strtod(number_str, NULL);

    return number;
}

static void parse_vector3(Parser* parser, Vector3 vec)
{
    if (lexer_next(&parser->lexer).kind != TOKEN_LEFT_PAREN) {
        assert(false && "Missing opening paren");
    }

    vec[0] = parse_number(parser);
    vec[1] = parse_number(parser);
    vec[2] = parse_number(parser);

    if (lexer_next(&parser->lexer).kind != TOKEN_RIGHT_PAREN) {
        assert(false && "Missing closing paren");
    }
}

static void parse_entity(Parser* parser)
{
    TempMemory temp = begin_temp_memory(parser->arena);

    MapEntity* entity = PushStruct(parser->arena, MapEntity);
    entity->brushes = PushArray(parser->arena, Brush, 100);
    entity->brushes_count = 0;

    for (;;) {
        Token token = lexer_next(&parser->lexer);

        if (token.kind == TOKEN_EOF) {
            break;
        }

        if (token.kind == TOKEN_STRING) {
            if (token.value.count <= 2) {
                continue;
            }

            Token value_token = lexer_next(&parser->lexer);

            if (value_token.kind != TOKEN_STRING) {
                assert(false && "Malformed metadata");
            }

            if (token.value.count - 2 == sizeof("classname") - 1 && memcmp("classname", token.value.data + 1, token.value.count - 2) == 0) {
                entity->classname = value_token.value.data + 1;
                entity->classname_size = value_token.value.count - 2;
            }

            // printf("%.*s: %.*s\n", (int)token.size, token.value, (int)value_token.size, value_token.value);
        } else if (token.kind == TOKEN_LEFT_BRACE) {
            if (entity->brushes_count == 100) {
                assert(false);
            }

            Brush* brush = &entity->brushes[entity->brushes_count++];
            brush->points = PushArray(parser->arena, Plane, 100);
            brush->count = 0;

            for (;;) {
                Vector3 a, b, c;

                parse_vector3(parser, a);
                parse_vector3(parser, b);
                parse_vector3(parser, c);

                if (brush->count >= 100) {
                    assert(false);
                }

                Plane* plane = &brush->points[brush->count++];

                *plane = plane_from_points(a, b, c);

                Token material = lexer_next(&parser->lexer);

                if (material.kind != TOKEN_IDENTIFIER) {
                    assert(false);
                }

                plane->material = token.value;

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

    parser->entity_callback(entity, parser->userdata, parser->arena);

    end_temp_memory(temp);
}

static void parse(Parser* parser)
{
    Token token = lexer_next(&parser->lexer);

    while (token.kind == TOKEN_LEFT_BRACE) {
        parse_entity(parser);
        token = lexer_next(&parser->lexer);
    }
}

void parse_map(const char* data, MapEntityCallback* entity_callback, void* userdata, Arena* arena)
{
    Lexer lexer = { 0 };
    lexer.data = data;

    Parser parser = { 0 };
    parser.lexer = lexer;
    parser.entity_callback = entity_callback;
    parser.userdata = userdata;
    parser.arena = arena;

    parse(&parser);
}