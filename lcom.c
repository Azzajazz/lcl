#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>
#include <assert.h>

/****************************** URGENT ******************************/
// The *eatUntil* functions will hang if a TOKEN_EOF is found.


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
} Arrow_Span;

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

void printArrowSpan(Arrow_Span span) {
        if (span.lineNumStart == span.lineNumEnd) {
            printf("  "SV_FMT"\n", SV_ARG(span.lineStart));
            printArrows(2, span.charNumStart - 1, span.charNumEnd - span.charNumStart);
        }
        else {
            printf("  Line %5zu: "SV_FMT"\n", span.lineNumStart, SV_ARG(span.lineStart));
            printf("      ...");
            printArrows(5, span.charNumStart - 1, span.lineStart.length - span.charNumStart);
            printf("  Line %5zu: "SV_FMT"\n", span.lineNumEnd, SV_ARG(span.lineEnd));
            printArrows(14, 0, span.charNumEnd - 1);
        }
}

void printError(char* fileName, Arrow_Span span, char* msg, ...) {
    printf("%s:%zu:%zu: error! ", fileName, span.lineNumStart, span.charNumStart);
    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
    printf(":\n");
    printArrowSpan(span);
    printf("\n");
}

// URGENT: 1
Arrow_Span spanAndEatUntil(Lexer* lexer, Token token, Token_Type wanted) {
    Arrow_Span span;
    span.lineNumStart = token.lineNum;
    span.charNumStart = token.charNum;
    span.lineStart = token.line;
    while (token.type != wanted) {
        span.lineNumEnd = token.lineNum;
        span.charNumEnd = token.charNum + token.text.length;
        span.lineEnd = token.line;
        token = getToken(lexer);
    }
    return span;
}

// URGENT: 1
Arrow_Span spanAndEatUntilKeyword(Lexer* lexer, Token token, char* keyword) {
    Arrow_Span span;
    span.lineNumStart = token.lineNum;
    span.charNumStart = token.charNum;
    span.lineStart = token.line;
    while (token.type != TOKEN_IDENT_OR_KEYWORD || !svEqualsCStr(token.text, keyword)) {
        span.lineNumEnd = token.lineNum;
        span.charNumEnd = token.charNum + token.text.length;
        span.lineEnd = token.line;
        token = getToken(lexer);
    }
    return span;
}

void eatUntil(Lexer* lexer, Token_Type wanted) {
    Token token = getToken(lexer);
    while (token.type != wanted) {
        token = getToken(lexer);
    }
}

Arrow_Span spanToken(Token token) {
    return (Arrow_Span){
        .lineNumStart = token.lineNum,
        .charNumStart = token.charNum,
        .lineStart = token.line,
        .lineNumEnd = token.lineNum,
        .charNumEnd = token.charNum + token.text.length,
        .lineEnd = token.line,
    };
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
        Arrow_Span span = spanAndEatUntil(lexer, token, TOKEN_LPAREN);
        printError(lexer->fileName, span, "Junk between \"func\" keyword and argument list");
        *success = false;
    }

    token = getToken(lexer);
    if (token.type == TOKEN_RPAREN) {
        return NULL;
    }
    assert(token.type == TOKEN_IDENT_OR_KEYWORD);
    if (token.type != TOKEN_IDENT_OR_KEYWORD) {
        printError(lexer->fileName, spanToken(token), "Expected identifier, got \""SV_FMT"\"", SV_ARG(token.text));
        eatUntil(lexer, TOKEN_IDENT_OR_KEYWORD);
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
            printError(lexer->fileName, spanToken(token), "Expected \",\", got \""SV_FMT"\"", SV_ARG(token.text));
            eatUntil(lexer, TOKEN_COMMA);
            *success = false;
        }
        token = getToken(lexer);
        if (token.type != TOKEN_IDENT_OR_KEYWORD) {
            printError(lexer->fileName, spanToken(token), "Expected identifier, got \""SV_FMT"\"", SV_ARG(token.text));
            eatUntil(lexer, TOKEN_IDENT_OR_KEYWORD);
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
        printError(lexer->fileName, spanToken(token), "Expected \"{\", got \""SV_FMT"\"", SV_ARG(token.text));
        eatUntil(lexer, TOKEN_LBRACE);
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
        printError(lexer->fileName, spanToken(token), "Expected identifier, but got \""SV_FMT"\"", SV_ARG(token.text));
        eatUntil(lexer, TOKEN_IDENT_OR_KEYWORD);
        *success = false;
    }
    String_View name = token.text;

    token = getToken(lexer);
    if (token.type != TOKEN_DCOLON) {
        Arrow_Span span = spanAndEatUntil(lexer, token, TOKEN_DCOLON); 
        printError(lexer->fileName, span, "Junk between function name and \"::\"");
        *success = false;
    }

    token = getToken(lexer);
    if (token.type != TOKEN_IDENT_OR_KEYWORD || !svEqualsCStr(token.text, "func")) {
        Arrow_Span span = spanAndEatUntilKeyword(lexer, token, "func");
        printError(lexer->fileName, span, "Junk between \"::\" and \"func\" keyword");
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
