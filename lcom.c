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
// 1: The *eatUntil* functions will hang if a TOKEN_EOF is found.


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
    char* contents = malloc(fileSize);
    tryFRead(contents, 1, fileSize, file);
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
} Token;

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

void lexerAdvanceLine(Lexer* lexer) {
    while (*lexer->code && *lexer->code != '\n') {
        lexer->code++;
    }
    lexer->lineNum++;
    lexer->charNum = 1;
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
MAYBE_DEF(Lex_Scope);

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

void printArrows(unsigned indent, size_t start, size_t length) {
    for (unsigned i = 0; i < indent; ++i) {
        printf(" ");
    }
    for (size_t i = 0; i < start; ++i) {
        printf(" ");
    }
    for (size_t i = 0; i < length; ++i) {
        printf("^");
    }
    printf("\n");
}

void printArrowSpan(Lex_Scope scope) {
        if (scope.lineNumStart == scope.lineNumEnd) {
            printf("  "SV_FMT"\n", SV_ARG(scope.lineStart));
            printArrows(2, scope.charNumStart - 1, scope.charNumEnd - scope.charNumStart);
        }
        else {
            printf("  Line %5zu: "SV_FMT"\n", scope.lineNumStart, SV_ARG(scope.lineStart));
            printf("      ...");
            printArrows(5, scope.charNumStart - 1, scope.lineStart.length - scope.charNumStart);
            printf("  Line %5zu: "SV_FMT"\n", scope.lineNumEnd, SV_ARG(scope.lineEnd));
            printArrows(14, 0, scope.charNumEnd - 1);
        }
}

// IMPORTANT: 1
MAYBE(Lex_Scope) scopeAndEatUntil(Lexer* lexer, Token token, Token_Type wanted) {
    MAYBE(Lex_Scope) result;
    result.exists = true;
    result.inner.lineNumStart = token.lineNum;
    result.inner.charNumStart = token.charNum;
    result.inner.lineStart = token.line;
    while (token.type != wanted) {
        if (token.type == TOKEN_EOF) {
            result.exists = false;
            break;
        }
        result.inner.lineNumEnd = token.lineNum;
        result.inner.charNumEnd = token.charNum + token.text.length;
        result.inner.lineEnd = token.line;
        token = getToken(lexer);
    }
    return result;
}

// IMPORTANT: 1
MAYBE(Lex_Scope) scopeAndEatUntilKeyword(Lexer* lexer, Token token, char* keyword) {
    MAYBE(Lex_Scope) result;
    result.exists = true;
    result.inner.lineNumStart = token.lineNum;
    result.inner.charNumStart = token.charNum;
    result.inner.lineStart = token.line;
    while (token.type != TOKEN_IDENT_OR_KEYWORD || !svEqualsCStr(token.text, keyword)) {
        if (token.type == TOKEN_EOF) {
            result.exists = false;
            break;
        }
        result.inner.lineNumEnd = token.lineNum;
        result.inner.charNumEnd = token.charNum + token.text.length;
        result.inner.lineEnd = token.line;
        token = getToken(lexer);
    }
    return result;
}

// Returns true if token of type wanted is found, false if EOF is found.
bool eatUntil(Lexer* lexer, Token_Type wanted) {
    Token token = getToken(lexer);
    size_t lineNumStart = token.lineNum;
    while (token.type != wanted) {
        if (token.type == TOKEN_EOF) {
            return false;
        }
        token = getToken(lexer);
    }
    return true;
}

void vPrintError(char* fileName, Lex_Scope scope, char* msg, va_list args) {
    printf("%s:%zu:%zu: error! ", fileName, scope.lineNumStart, scope.charNumStart);
    vprintf(msg, args);
    printf(":\n");
    printArrowSpan(scope);
    printf("\n");
}

void printError(char* fileName, Lex_Scope scope, char* msg, ...) {
    va_list args;
    va_start(args, msg);
    vPrintError(fileName, scope, msg, args);
    va_end(args);
}

void reportAndEatUntil(Lexer* lexer, Lex_Scope scope, Token_Type wanted, char* msg, ...) {
    va_list args;
    va_start(args, msg);
    vPrintError(lexer->fileName, scope, msg, args);
    va_end(args);
    bool found = eatUntil(lexer, wanted);
    if (!found) {
        exit(1);
    }
}

//TODO: Can we support string formatting here?
void reportScopeAndEatUntil(Lexer* lexer, Lex_Scope scope, Token token, Token_Type wanted, char* msgFound, char* msgNotFound) {
    MAYBE(Lex_Scope) eatScope = scopeAndEatUntil(lexer, token, wanted);
    if (!eatScope.exists) {
       printError(lexer->fileName, scope, msgNotFound); 
       exit(1);
    }
    printError(lexer->fileName, eatScope.inner, msgFound);
}

void reportScopeAndEatUntilKeyword(Lexer* lexer, Lex_Scope scope, Token token, char* wanted, char* msgFound, char* msgNotFound) {
    MAYBE(Lex_Scope) eatScope = scopeAndEatUntilKeyword(lexer, token, wanted);
    if (!eatScope.exists) {
       printError(lexer->fileName, scope, msgNotFound); 
       exit(1);
    }
    printError(lexer->fileName, eatScope.inner, msgFound);
}

enum Node_Type;
union Node_Data;
struct AST_Node;

typedef enum Node_Type {
    NODE_FUNCTION,
    NODE_SCOPE,
    NODE_EXPR_LIST,
    NODE_ARG_LIST,
} Node_Type;

typedef union Node_Data {
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
        arena->nodes = realloc(arena->nodes, arena->capacity);
    }
    arena->length++;
    return arena->nodes + arena->length - 1;
}

AST_Node* parseArgList(AST_Node_Arena* arena, Lexer* lexer, bool* success) {
    Token token = getToken(lexer);
    if (token.type != TOKEN_LPAREN) {
        reportScopeAndEatUntil(lexer, scopeToken(token), token, TOKEN_LPAREN,
                "Junk between \"func\" keyword and argument list",
                "Expected \"(\" after \"func\" keyword");
        *success = false;
    }

    token = getToken(lexer);
    if (token.type == TOKEN_RPAREN) {
        return NULL;
    }
    if (token.type != TOKEN_IDENT_OR_KEYWORD) {
        reportAndEatUntil(lexer, scopeToken(token), TOKEN_IDENT_OR_KEYWORD, "Expected identifier, got \""SV_FMT"\"", SV_ARG(token.text));
        *success = false;
    }
    AST_Node* head = allocateNode(arena);
    head->type = NODE_ARG_LIST;
    head->data.arg = token.text;
    head->data.nextArg = NULL;
    AST_Node* curr = head;

    token = getToken(lexer);
    while (token.type != TOKEN_RPAREN) {
        if (token.type != TOKEN_COMMA) {
            reportAndEatUntil(lexer, scopeToken(token), TOKEN_COMMA, "Expected \",\", got \""SV_FMT"\"", SV_ARG(token.text));
            *success = false;
        }
        token = getToken(lexer);
        if (token.type != TOKEN_IDENT_OR_KEYWORD) {
            reportAndEatUntil(lexer, scopeToken(token), TOKEN_IDENT_OR_KEYWORD, "Expected identifier, got \""SV_FMT"\"", SV_ARG(token.text));
            *success = false;
        }
        curr->data.nextArg = allocateNode(arena);
        curr = curr->data.nextArg;
        curr->type = NODE_ARG_LIST;
        curr->data.arg = token.text;
        curr->data.nextArg = NULL;
        token = getToken(lexer);
    }
    return head;
}

AST_Node* parseScope(AST_Node_Arena* arena, Lexer* lexer, bool* success) {
    Token token = getToken(lexer);
    if (token.type != TOKEN_LBRACE) {
        reportAndEatUntil(lexer, scopeToken(token), TOKEN_LBRACE, "Expected \"{\", got \""SV_FMT"\"", SV_ARG(token.text));
        *success = false;
    }

    //TEMPORARY
    //TODO: Parse expression list
    token = getToken(lexer);
    assert(token.type == TOKEN_RBRACE);

    AST_Node* node = allocateNode(arena);
    node->type = NODE_SCOPE;
    node->data.exprs = NULL;
    return node;
}

AST_Node* parseFunction(AST_Node_Arena* arena, Lexer* lexer, bool* success) {
    Token token = getToken(lexer);
    if (token.type != TOKEN_IDENT_OR_KEYWORD) {
        reportAndEatUntil(lexer, scopeToken(token), TOKEN_IDENT_OR_KEYWORD, "Expected identifier, but got \""SV_FMT"\"", SV_ARG(token.text));
        *success = false;
    }
    String_View name = token.text;

    token = getToken(lexer);
    if (token.type != TOKEN_DCOLON) {
        //TODO: This could be made better if we supported string formatting
        reportScopeAndEatUntil(lexer, scopeToken(token), token, TOKEN_DCOLON,
                "Expected \"::\" after function identifier",
                "Junk between function name and \"::\"");
        *success = false;
    }

    token = getToken(lexer);
    if (token.type != TOKEN_IDENT_OR_KEYWORD || !svEqualsCStr(token.text, "func")) {
        reportScopeAndEatUntilKeyword(lexer, scopeToken(token), token, "func",
                "Expected \"func\" keyword after \"::\"",
                "Junk between \"::\" and \"func\" keyword");
        *success = false;
    }

    AST_Node* args = parseArgList(arena, lexer, success);

    AST_Node* body = parseScope(arena, lexer, success);

    AST_Node* node = allocateNode(arena);
    node->type = NODE_FUNCTION;
    node->data.name = name;
    node->data.args = args;
    node->data.body = body;
    return node;
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
//    Token token = getToken(&lexer);
//    while (token.type != TOKEN_EOF) {
//        printToken(token);
//        token = getToken(&lexer);
//    }
    AST_Node_Arena arena = makeNodeArena(10);
    bool success = true;
    AST_Node* func = parseFunction(&arena, &lexer, &success);
//    printNode(*func);
}
