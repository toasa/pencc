typedef enum enm_tk {
    TK_INT, // 46, 101, ...
    TK_ADD, // +
    TK_SUB, // -
    TK_MUL, // *
    TK_EOF,
} TokenType;

typedef struct Token {
    TokenType type;
    int val;
    struct Token *next;
} Token;

Token *tokenize(char *);

void printTokens(Token *);