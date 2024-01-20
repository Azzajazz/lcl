#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>
#include <assert.h>

/****************************** IMPORTANT ******************************/
// 1: In the middle of refactoring and cleaning up error reporting. Next is parseArgList


//////////////
// File API //
//////////////

FILE* tryFOpen(char* filePath, char* mode) {
    FILE* file = fopen(filePath, mode);
    if (!file) {
        fprintf(stderr, "[ERROR]: Could not open file %s in mode \"rb\".\nReason: %s\n", filePath, strerror(errno));
        exit(1);
    }
    return file;
}

void tryFSeek(FILE* file, long offset, int origin) {
    int res = fseek(file, offset, origin);
    if (res) {
        fprintf(stderr, "[ERROR]: Seek failed!\nReason: %s\n", strerror(errno));
        exit(1);
    }
}

long tryFTell(FILE* file) {
    long pos = ftell(file);
    if (pos < 0) {
        fprintf(stderr, "[ERROR]: Tell failed!\nReason: %s\n", strerror(errno));
        exit(1);
    }
    return pos;
}

void tryFRead(void* buffer, size_t size, size_t count, FILE* stream) {
    fread(buffer, size, count, stream);
    if (ferror(stream)) {
        fprintf(stderr, "[ERROR]: Could not read from file!\nReason: %s\n", strerror(errno));
        exit(1);
    }
}

long getFileSize(FILE* file) {
    tryFSeek(file, 0, SEEK_END);
    long fileSize = tryFTell(file);
    tryFSeek(file, 0, SEEK_SET);
    return fileSize;
}

char* readEntireFile(char* filePath) {
    FILE* file = tryFOpen(filePath, "rb");
    long fileSize = getFileSize(file);
    char* contents = malloc(fileSize + 1);
    tryFRead(contents, 1, fileSize, file);
    contents[fileSize] = '\0';
    return contents;
}



///////////////
// Maybe API //
///////////////

#define MAYBE_DEF(type) \
    typedef struct {    \
        bool exists;    \
        type inner;     \
    } _Maybe_##type

#define MAYBE_PTR_DEF(type) \
    typedef struct {        \
        bool exists;        \
        type* inner;        \
    } _Maybe_Ptr_##type

#define MAYBE(type) _Maybe_##type

#define MAYBE_PTR(type) _Maybe_Ptr_##type

MAYBE_DEF(int);

/////////////////////
// String View API //
/////////////////////

typedef struct {
    char* start;
    size_t length;
} String_View;

#define SV_FMT "%.*s"
#define SV_ARG(sv) (int)(sv).length, (sv).start

bool svEqualsCStr(String_View sv, char* cstr) {
    for (size_t i = 0; i < sv.length; ++i) {
        if (cstr[i] == '\0' || cstr[i] != sv.start[i]) {
            return false;
        }
    }
    return cstr[sv.length] == '\0';
}

String_View svUntil(char delim, char* cstr) {
    size_t length = 0;
    for (char* p = cstr; *p && *p != delim; ++p) {
        length++;
    }
    return (String_View){cstr, length};
}

MAYBE(int) svParseInt(String_View sv) {
    MAYBE(int) result;
    result.exists = false;
    result.inner = 0;
    if (sv.length == 0) {
        return result;
    }

    bool negative = false;
    size_t svIndex = 0;
    if (*sv.start == '-') {
        negative = true;
        svIndex++;
    }
    else if (*sv.start == '+') {
        svIndex++;
    }

    for (; svIndex < sv.length; ++svIndex) {
        if (!isdigit(sv.start[svIndex])) {
            return result;
        }
        result.inner *= 10;
        result.inner += sv.start[svIndex] - '0';
    }
    result.exists = true;
    return result;
}

///////////////
// Lexer API //
///////////////

typedef enum {
    TOKEN_EOF,
    TOKEN_COMMA,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_DCOLON,
    TOKEN_IDENT_OR_KEYWORD,
} Token_Type;

typedef struct {
    Token_Type type;
    String_View text;
    size_t lineNum;
    size_t charNum;
    String_View line;
    //TODO: This is only used for getToken right now. Do we really need it?
    size_t consumed; // The number of characters consumed to retrieve this token.
} Token;
MAYBE_DEF(Token);

typedef struct {
    char* code;
    char* fileName;
    size_t lineNum;
    size_t charNum;
    String_View line;
} Lexer;

Lexer makeLexer(char* code, char* fileName) {
    return (Lexer){
        .code = code,
        .fileName = fileName,
        .lineNum = 1,
        .charNum = 1,
        .line = svUntil('\n', code)
    };
}

void lexerAdvance(Lexer* lexer, size_t steps) {
    for (size_t i = 0; i < steps; ++i) {
        lexer->charNum++;
        if (*lexer->code == '\n') {
            lexer->lineNum++;
            lexer->charNum = 1;
            lexer->line = svUntil('\n', lexer->code + 1);
        }
        lexer->code++;
    }
}

bool isLexemeTerminator(char c) {
    return isspace(c)
        || c == '\0'
        || c == ','
        || c == '('
        || c == ')'
        || c == '{'
        || c == '}'
        || c == ':';
}

//TODO: Would this be cleaner if it returned a MAYBE(Token) and we removed TOKEN_EOF?
Token getToken(Lexer* lexer) {
    while (isspace(*lexer->code)) {
        lexerAdvance(lexer, 1);
    }

    Token result;
    result.lineNum = lexer->lineNum;
    result.charNum = lexer->charNum;
    result.line = lexer->line;
    switch(*lexer->code) {
        case '\0':
            result.type = TOKEN_EOF;
            result.text = (String_View){lexer->code, 1};
            lexerAdvance(lexer, 1);
            break;
        case ',':
            result.type = TOKEN_COMMA;
            result.text = (String_View){lexer->code, 1};
            lexerAdvance(lexer, 1);
            break;
        case '(':
            result.type = TOKEN_LPAREN;
            result.text = (String_View){lexer->code, 1};
            lexerAdvance(lexer, 1);
            break;
        case ')':
            result.type = TOKEN_RPAREN;
            result.text = (String_View){lexer->code, 1};
            lexerAdvance(lexer, 1);
            break;
        case '{':
            result.type = TOKEN_LBRACE;
            result.text = (String_View){lexer->code, 1};
            lexerAdvance(lexer, 1);
            break;
        case '}':
            result.type = TOKEN_RBRACE;
            result.text = (String_View){lexer->code, 1};
            lexerAdvance(lexer, 1);
            break;
        case ':': {
            if (lexer->code[1] == ':') {
                result.type = TOKEN_DCOLON;
                result.text = (String_View){lexer->code, 2};
                lexerAdvance(lexer, 2);
            }
            break;
        }
        default: {
            size_t identLength = 0;
            while(!isLexemeTerminator(lexer->code[identLength])) {
                identLength++;
            }
            result.type = TOKEN_IDENT_OR_KEYWORD;
            result.text = (String_View){lexer->code, identLength};
            lexerAdvance(lexer, identLength);
            break;
        } 
    }
    return result;
}

Token peekToken(Lexer* lexer) {
    Lexer newLexer = *lexer;
    Token token = getToken(&newLexer);
    return token;
}

void printToken(Token token) {
    char* typeString;
    switch (token.type) {
        case TOKEN_EOF:
            typeString = "EOF";
            break;
        case TOKEN_COMMA:
            typeString = "COMMA";
            break;
        case TOKEN_LPAREN:
            typeString = "LPAREN";
            break;
        case TOKEN_RPAREN:
            typeString = "RPAREN";
            break;
        case TOKEN_LBRACE:
            typeString = "LBRACE";
            break;
        case TOKEN_RBRACE:
            typeString = "RBRACE";
            break;
        case TOKEN_DCOLON:
            typeString = "DCOLON";
            break;
        case TOKEN_IDENT_OR_KEYWORD:
            typeString = "IDENT";
            break;
    }
    printf("Type: %s, text: "SV_FMT"\n", typeString, SV_ARG(token.text));
}



////////////////
// Parser API //
////////////////

typedef struct {
    size_t lineNumStart;
    size_t charNumStart;
    String_View lineStart;
    size_t lineNumEnd;
    size_t charNumEnd;
    String_View lineEnd;
} Lex_Scope;

void printScope(Lex_Scope scope) {
    if (scope.lineNumStart == scope.lineNumEnd) {
        printf("  "SV_FMT"\n", SV_ARG(scope.lineStart));
        printf("  ");
        // This loop starts at one since the charNums in scopes are 1-indexed.
        for (size_t i = 1; i < scope.charNumStart; ++i) {
            printf(" ");
        }
        for (size_t i = 0; i < scope.charNumEnd - scope.charNumStart; ++i) {
            printf("^");
        }
    }
    else {
        printf("  Line %5zu: "SV_FMT"\n", scope.lineNumStart, SV_ARG(scope.lineStart));
        printf("   ...         ");
        // This loop starts at one since the charNums in scopes are 1-indexed.
        for (size_t i = 1; i < scope.charNumStart; ++i) {
            printf(" ");
        }
        for (size_t i = scope.charNumStart; i < scope.lineStart.length; ++i) {
            printf("^");
        }
        printf("  Line %5zu: "SV_FMT"\n", scope.lineNumStart, SV_ARG(scope.lineStart));
        printf("               ");
        // This loop starts at one since the charNums in scopes are 1-indexed.
        for (size_t i = 1; i < scope.charNumEnd; ++i) {
            printf("^");
        }
    }
    printf("\n");
}

Lex_Scope scopeToken(Token token) {
    return (Lex_Scope){
        .lineNumStart = token.lineNum,
        .charNumStart = token.charNum,
        .lineStart = token.line,
        .lineNumEnd = token.lineNum,
        .charNumEnd = token.charNum + token.text.length,
        .lineEnd = token.line,
    };
}

Lex_Scope scopeBetween(Token start, Token end) {
    return (Lex_Scope){
        .lineNumStart = start.lineNum,
        .charNumStart = start.charNum,
        .lineStart = start.line,
        .lineNumEnd = end.lineNum,
        .charNumEnd = end.charNum + end.text.length,
        .lineEnd = end.line,
    };
}

// Eats up to, but not including, the next token of type `wanted`.
// If a token of type `TOKEN_EOF` is found, returns a token of type `TOKEN_EOF`.
// NOTE: The parameter `start` is necessary since `eatUpTo` is usually called after a failed `getToken`.
//   If the next token is of type `wanted`, then `start` is returned.
Token eatUpTo(Lexer* lexer, Token start, Token_Type wanted) {
    Token peeked = peekToken(lexer);
    if (peeked.type == wanted) {
        return start;
    }
    Token result;
    while (peeked.type != wanted && peeked.type != TOKEN_EOF) {
        result = getToken(lexer);
        peeked = peekToken(lexer);
    }
    if (peeked.type == TOKEN_EOF) {
        return peeked;
    }
    return result;
}

Token eatUpToKeyword(Lexer* lexer, Token start, char* wanted) {
    Token peeked = peekToken(lexer);
    if (peeked.type == TOKEN_IDENT_OR_KEYWORD && svEqualsCStr(peeked.text, wanted)) {
        return start;
    }
    Token result;
    while ((peeked.type != TOKEN_IDENT_OR_KEYWORD || !svEqualsCStr(peeked.text, wanted)) && peeked.type != TOKEN_EOF) {
        result = getToken(lexer);
        peeked = peekToken(lexer);
    }
    if (peeked.type == TOKEN_EOF) {
        return peeked;
    }
    return result;
}

enum Node_Type;
union Node_Data;
struct AST_Node;

typedef enum Node_Type {
    NODE_FUNCTION,
    NODE_SCOPE,
    NODE_EXPR_LIST,
    NODE_ARG_LIST,
    //TODO: Should we have NODE_LIT with a type field?
    NODE_INT,
    NODE_RETURN,
} Node_Type;

typedef union Node_Data {
    //TODO: Rename these fields to avoid collisions
    struct {
        // NODE_FUNCTION
        String_View name;
        struct AST_Node* args;
        struct AST_Node* body;
    };
    struct {
        // NODE_SCOPE
        struct AST_Node* exprs;
    };
    struct {
        // NODE_EXPR_LIST
        struct AST_Node* expr;
        struct AST_Node* nextExpr;
    };
    struct {
        // NODE_ARG_LIST
        String_View arg;
        struct AST_Node* nextArg;
    };
} Node_Data;

typedef struct AST_Node {
    Node_Type type;
    Node_Data data;
} AST_Node;
MAYBE_PTR_DEF(AST_Node);
    
typedef struct {
    AST_Node* nodes;
    size_t length;
    size_t capacity;
} AST_Node_Arena;

AST_Node_Arena makeNodeArena(size_t capacity) {
    return (AST_Node_Arena){
        .nodes = malloc(capacity * sizeof(AST_Node)),
        .length = 0,
        .capacity = capacity,
    };
}

AST_Node* allocateNode(AST_Node_Arena* arena) {
    if (arena->length == arena->capacity) {
        arena->capacity *= 2;
        arena->nodes = realloc(arena->nodes, arena->capacity * sizeof(AST_Node));
    }
    arena->length++;
    return arena->nodes + arena->length - 1;
}

MAYBE_PTR(AST_Node) parseArgList(AST_Node_Arena* arena, Lexer* lexer) {
    MAYBE_PTR(AST_Node) result;
    result.exists = true;
    Token token = getToken(lexer);
    if (token.type != TOKEN_LPAREN) {
        result.exists = false;
    }

    token = getToken(lexer);
    if (token.type == TOKEN_RPAREN) {
        result.inner = NULL;
        return result;
    }
    if (token.type != TOKEN_IDENT_OR_KEYWORD) {
        result.exists = false;
    }

    //OPTIMIZATION: If result.exists is false, we could avoid some allocations here.
    result.inner = allocateNode(arena);
    result.inner->type = NODE_ARG_LIST;
    result.inner->data.arg = token.text;
    result.inner->data.nextArg = NULL;
    AST_Node* curr = result.inner;

    token = getToken(lexer);
    while (token.type != TOKEN_RPAREN) {
        if (token.type != TOKEN_COMMA) {
            result.exists = false;
        }
        token = getToken(lexer);
        if (token.type != TOKEN_IDENT_OR_KEYWORD) {
            result.exists = false;
        }
        curr->data.nextArg = allocateNode(arena);
        curr = curr->data.nextArg;
        curr->type = NODE_ARG_LIST;
        curr->data.arg = token.text;
        curr->data.nextArg = NULL;
        token = getToken(lexer);
    }
    return result;
}

MAYBE_PTR(AST_Node) parseScope(AST_Node_Arena* arena, Lexer* lexer) {
    MAYBE_PTR(AST_Node) result;
    result.exists = true;
    Token token = getToken(lexer);
    if (token.type != TOKEN_LBRACE) {
        result.exists = false;
    }
    if (result.exists) {
        result.inner = allocateNode(arena);
        result.inner->type = NODE_SCOPE;
        result.inner->data.exprs = NULL;
    }
    return result;
}

MAYBE_PTR(AST_Node) parseFunction(AST_Node_Arena* arena, Lexer* lexer) {
    MAYBE_PTR(AST_Node) result;
    result.exists = true;
    Token token = getToken(lexer);
    if (token.type != TOKEN_IDENT_OR_KEYWORD) {
        Token last = eatUpTo(lexer, token, TOKEN_IDENT_OR_KEYWORD);
        printf("%s:%zu:%zu: error! Expected an identifier, got \""SV_FMT"\":\n", lexer->fileName, token.lineNum, token.charNum, SV_ARG(token.text));
        printScope(scopeToken(token));
        if (last.type == TOKEN_EOF) {
            exit(1);
        }
        token = getToken(lexer); // Required, since `eatUpTo` does not consume the token of the desired type.
        result.exists = false;
    }
    String_View name = token.text;

    token = getToken(lexer);
    if (token.type != TOKEN_DCOLON) {
        Token last = eatUpTo(lexer, token, TOKEN_DCOLON);
        if (last.type == TOKEN_EOF) {
            printf("%s:%zu:%zu: error! Unexpected end of file while parsing function definition. Parsing got to here:\n", lexer->fileName, token.lineNum, token.charNum);
            printScope(scopeToken(token));
            exit(1);
        }
        else {
            printf("%s:%zu:%zu: error! Unexpected text between function name and \"::\":\n", lexer->fileName, token.lineNum, token.charNum);
            printScope(scopeBetween(token, last));
        }
        token = getToken(lexer); // Required, since `eatUpTo` does not consume the token of the desired type.
        result.exists = false;
    }

    token = getToken(lexer);
    if (token.type != TOKEN_IDENT_OR_KEYWORD || !svEqualsCStr(token.text, "func")) {
        Token last = eatUpToKeyword(lexer, token, "func");
        if (last.type == TOKEN_EOF) {
            printf("%s:%zu:%zu: error! Unexpected end of file while parsing function definition. Parsing got to here:\n", lexer->fileName, token.lineNum, token.charNum);
            printScope(scopeToken(token));
            exit(1);
        }
        else {
            printf("%s:%zu:%zu: error! Unexpected text between \"::\" and \"func\" keyword:\n", lexer->fileName, token.lineNum, token.charNum);
            printScope(scopeBetween(token, last));
        }
        token = getToken(lexer); // Required, since `eatUpTo` does not consume the token of the desired type.
        result.exists = false;
    }

    MAYBE_PTR(AST_Node) args = parseArgList(arena, lexer);
    if (!args.exists) {
        result.exists = false;
    }

    MAYBE_PTR(AST_Node) body = parseScope(arena, lexer);
    if (!body.exists) {
        result.exists = false;
    }

    if (result.exists) {
        result.inner = allocateNode(arena);
        result.inner->type = NODE_FUNCTION;
        result.inner->data.name = name;
        result.inner->data.args = args.inner;
        result.inner->data.body = body.inner;
    }
    return result;
}

printIndented(unsigned indent, char* fmt, ...) {
    va_list args;
    for (unsigned i = 0; i < indent; ++i) {
        printf("  ");
    }
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

static void printNodeIndented(AST_Node node, unsigned indent) {
    switch (node.type) {
        case NODE_FUNCTION: {
            printIndented(indent, "type=FUNCTION, name="SV_FMT", args=", SV_ARG(node.data.name));
            if (node.data.args == NULL) {
                printf("NONE, body=");
            }
            else {
                printf("(\n");
                printNodeIndented(*node.data.args, indent + 1);
                printIndented(indent, "), body=");
            }
            if (node.data.body == NULL) {
                printf("NONE\n");
            }
            else {
                printf("(\n");
                printNodeIndented(*node.data.body, indent + 1);
                printIndented(indent, ")\n");
            }
            break;
        }
        case NODE_SCOPE: {
            printIndented(indent, "type=SCOPE, exprs=");
            if (node.data.exprs == NULL) {
                printf("NONE\n");
            }
            else {
                printf("(\n");
                printNodeIndented(*node.data.exprs, indent + 1);
                printIndented(indent, ")\n");
            }
            break; 
        }
        case NODE_EXPR_LIST: {
            //TODO
            break;
        }
        case NODE_ARG_LIST: {
            printIndented(indent, "type=ARG_LIST, args=(\n");
            printIndented(indent + 1, SV_FMT, SV_ARG(node.data.arg));
            while (node.data.nextArg != NULL) {
                node = *node.data.nextArg;
                printf(", "SV_FMT, SV_ARG(node.data.arg));
            }
            printf("\n");
            printIndented(indent, ")\n");
            break;
        }
    }
}

void printNode(AST_Node node) {
    printNodeIndented(node, 0);
}

// Read in file simple.lcl - DONE
// Have an iterator lexer - DONE
// Write a simple grammar - TEMP/DONE
//   function := ident "::" "func" arg-list scope
//   scope := "{" expr-list "}"
//   arg-list := "(" ident ")" | "(" ident "," arg-list ")"
//   ident := STRING
//   expr-list := EMPTY
// Write a simple parser for that grammar
// ???--v
// Convert to C code
// Compile C code to executable

int main() {
    char* fileName = "simple.lcl";
    char* code = readEntireFile(fileName);
    Lexer lexer = makeLexer(code, fileName);
    AST_Node_Arena arena = makeNodeArena(10);
    MAYBE_PTR(AST_Node) func = parseFunction(&arena, &lexer);
    if (func.exists) {
        printNode(*func.inner);
    }
    return 0;
}
