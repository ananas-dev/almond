#include "map.h"

#include "string_view.h"

#include "geometry.h"
#include <almond.h>
#include <assert.h>

struct Lexer {
    const char* data;
    size_t current;
    size_t start;
};

enum class TokenKind {
    String,
    Number,
    Identifier,
    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    Eof,
    Error,
};

struct Token {
    TokenKind kind = TokenKind::Error;
    StringView value;
};

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
        return make_token(lexer, TokenKind::Eof);

    lexer->start = lexer->current;

    char c = consume_char(lexer);

    switch (c) {
    case '(':
        return make_token(lexer, TokenKind::LeftParen);
    case ')':
        return make_token(lexer, TokenKind::RightParen);
    case '{':
        return make_token(lexer, TokenKind::LeftBrace);
    case '}':
        return make_token(lexer, TokenKind::RightBrace);
    case '"': {
        while (consume_char(lexer) != '"') {
            if (is_eof(lexer))
                return make_token(lexer, TokenKind::Eof);
        }

        return make_token(lexer, TokenKind::String);
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

            return make_token(lexer, TokenKind::Number);
        }

        if (is_alpha(c)) {
            while (is_alphanum(peek_char(lexer)))
                consume_char(lexer);
            return make_token(lexer, TokenKind::Identifier);
        }
    } break;
    }

    return make_token(lexer, TokenKind::Error);
}

typedef struct {
    Lexer lexer;
    Arena& arena;
    MapEntityCallback* entity_callback;
    void* userdata;
} Parser;

typedef struct {
    const char *key, value;
} Metadata;

static float parse_number(Parser* parser)
{
    Token token = lexer_next(&parser->lexer);

    if (token.kind != TokenKind::Number) {
        assert(false && "Expected number");
    }

    return token.value.to_float();
}

static glm::vec3 parse_vector3(Parser* parser)
{
    if (lexer_next(&parser->lexer).kind != TokenKind::LeftParen) {
        assert(false && "Missing opening paren");
    }

    glm::vec3 vec;
    vec.x = parse_number(parser);
    vec.y = parse_number(parser);
    vec.z = parse_number(parser);

    if (lexer_next(&parser->lexer).kind != TokenKind::RightParen) {
        assert(false && "Missing closing paren");
    }

    return vec;
}

static void parse_entity(Parser* parser)
{
    TempMemory temp = parser->arena.begin_temp_memory();

    auto* entity = parser->arena.Push<MapEntity>();
    entity->brushes = parser->arena.PushArray<Brush>(100);
    entity->brushes_count = 0;

    for (;;) {
        Token token = lexer_next(&parser->lexer);

        if (token.kind == TokenKind::Eof) {
            break;
        }

        if (token.kind == TokenKind::String) {
            if (token.value.length <= 2) {
                continue;
            }

            Token value_token = lexer_next(&parser->lexer);

            if (value_token.kind != TokenKind::String) {
                assert(false && "Malformed metadata");
            }

            if (token.value == R"("classname")"_sv) {
                entity->classname = value_token.value.substring(0, value_token.value.length - 1);
            }
        } else if (token.kind == TokenKind::LeftBrace) {
            if (entity->brushes_count == 100) {
                assert(false);
            }

            Brush* brush = &entity->brushes[entity->brushes_count++];
            brush->points = parser->arena.PushArray<Plane>(100);
            brush->count = 0;

            for (;;) {
                glm::vec3 a = parse_vector3(parser);
                glm::vec3 b = parse_vector3(parser);
                glm::vec3 c = parse_vector3(parser);

                if (brush->count >= 100) {
                    assert(false);
                }

                Plane* plane = &brush->points[brush->count++];

                *plane = plane_from_points(a, b, c);

                Token material = lexer_next(&parser->lexer);

                if (material.kind != TokenKind::Identifier) {
                    assert(false);
                }

                // plane->material = token.value;

                float x_offset = parse_number(parser);
                float y_offset = parse_number(parser);
                plane->offset = glm::vec2(x_offset, y_offset);

                plane->rotation = parse_number(parser);

                float scale_x = parse_number(parser);
                float scale_y = parse_number(parser);
                plane->scale = glm::vec2(scale_x, scale_y);

                Lexer checkpoint = parser->lexer;

                if (lexer_next(&parser->lexer).kind == TokenKind::RightBrace) {
                    break;
                };

                parser->lexer = checkpoint;
            }
        } else if (token.kind == TokenKind::RightBrace) {
            break;
        }
    }

    parser->entity_callback(entity, parser->userdata, parser->arena);

    parser->arena.end_temp_memory(temp);
}

static void parse(Parser* parser)
{
    Token token = lexer_next(&parser->lexer);

    while (token.kind == TokenKind::LeftBrace) {
        parse_entity(parser);
        token = lexer_next(&parser->lexer);
    }
}

void parse_map(const char* data, MapEntityCallback* entity_callback, void* userdata, Arena& arena)
{
    Lexer lexer = { data };
    Parser parser = { lexer, arena, entity_callback, userdata };
    parse(&parser);
}