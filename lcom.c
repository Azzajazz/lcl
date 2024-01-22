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

bool isKeyword(Token token, char* keyword) {
    return (token.type == TOKEN_IDENT_OR_KEYWORD && svEqualsCStr(token.text, keyword));
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

// TODO: Continue filling out the parser struct. 
// This might make the API a lot cleaner for no extra overhead. For example:
//   - MAYBE_PTR(AST_Node) (and therefore the clunky MAYBE stuff in general) is no longer needed, since the parser could keep a flag as to whether parsing was successful.
//   - Keeping track of the node scopes becomes less cumbersome and removes the need for out parameters in `eatUntil*`
typedef struct {
    Lexer          lexer;
    AST_Node_Arena arena;
    Token          current;
//    Token          before_current; // This is used for scopes that pass over multiple tokens.
    bool           success;        // Denotes whether parsing was successful.
} Parser;

Parser makeParser(char* code, char* fileName) {
    return (Parser){
        .lexer = makeLexer(code, fileName),
        .arena = makeNodeArena(10),
        .success = true,
    };
}

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

Lex_Scope scopeAfter(Token token) {
    return (Lex_Scope){
        .lineNumStart = token.lineNum,
        .charNumStart = token.charNum + token.text.length + 1,
        .lineStart = token.line,
        .lineNumEnd = token.lineNum,
        .charNumEnd = token.charNum + token.text.length + 2,
        .lineEnd = token.line,
    };
}

void eatUntil(Parser* parser, Token_Type wanted) {
    parser->current = getToken(&parser->lexer);
    while (parser->current.type != wanted && parser->current.type != TOKEN_EOF) {
        parser->current = getToken(&parser->lexer);
    }
}

void eatUntilKeyword(Parser* parser, char* wanted) {
    parser->current = getToken(&parser->lexer);
    while (!isKeyword(parser->current, wanted) && parser->current.type != TOKEN_EOF) {
        parser->current = getToken(&parser->lexer);
    }
}

// nocheckin: The parse* functions will not compile right now.

AST_Node* parseArgList(Parser* parser) {
    Token funcKeyword = parser->current;
    parser->current = getToken(&parser->lexer);
    if (parser->current.type != TOKEN_LPAREN) {
        if (parser->current.type == TOKEN_EOF) {
            printf("%s:%zu:%zu: Expected \"(\" after \"func\" keyword:\n",
                    parser->lexer.fileName, parser->current.lineNum, parser->current.charNum);
            printScope(scopeAfter(funcKeyword));
            exit(1);
        }
        else {
            printf("%s:%zu:%zu: Expected \"(\" after \"func\" keyword, but got\""SV_FMT"\":\n",
                    parser->lexer.fileName, parser->current.lineNum, parser->current.charNum, SV_ARG(parser->current.text));
            printScope(scopeToken(parser->current));
        }
        parser->success = false;
    }


    Token openParen = parser->current;
    parser->current = getToken(&parser->lexer);
    if (parser->current.type == TOKEN_RPAREN) {
        return NULL;
    }
    if (parser->current.type != TOKEN_IDENT_OR_KEYWORD) {
        if (parser->current.type == TOKEN_EOF) {
            printf("%s:%zu:%zu: Expected \")\" in argument list:\n",
                    parser->lexer.fileName, parser->current.lineNum, parser->current.charNum);
            printScope(scopeAfter(openParen));
            exit(1);
        }
        else {
            printf("%s:%zu:%zu: Expected argument name or \")\" in argument list, got \""SV_FMT"\":\n",
                    parser->lexer.fileName, parser->current.lineNum, parser->current.charNum, SV_ARG(parser->current.text));
            printScope(scopeToken(parser->current));
            eatUntil(parser, TOKEN_IDENT_OR_KEYWORD);
            if (parser->current.type == TOKEN_EOF) {
                exit(1);
            }
        }
        parser->success = false;
    }

    //OPTIMIZATION: If parser->success is false, we could avoid some allocations here.
    AST_Node* head = allocateNode(&parser->arena);
    head->type = NODE_ARG_LIST;
    head->data.arg = parser->current.text;
    head->data.nextArg = NULL;
    AST_Node* curr = head;

    Token arg = parser->current;
    parser->current = getToken(&parser->lexer);
    while (parser->current.type != TOKEN_RPAREN) {
        if (parser->current.type != TOKEN_COMMA) {
            if (parser->current.type == TOKEN_EOF) {
                printf("%s:%zu:%zu: Expected \",\" or \")\" after argument in argument list:\n",
                        parser->lexer.fileName, arg.lineNum, arg.charNum);
                printScope(scopeAfter(arg));
                exit(1);
            }
            else {
                printf("%s:%zu:%zu: Expected \",\" or \")\" after argument in argument list, got\""SV_FMT"\":\n",
                        parser->lexer.fileName, parser->current.lineNum, parser->current.charNum, SV_ARG(parser->current.text));
                printScope(scopeToken(parser->current));
                eatUntil(parser, TOKEN_COMMA);
                if (parser->current.type == TOKEN_EOF) {
                    exit(1);
                }
            }
            parser->success = false;
        }

        Token comma = parser->current;
        parser->current = getToken(&parser->lexer);

        if (parser->current.type != TOKEN_IDENT_OR_KEYWORD) {
            if (parser->current.type == TOKEN_EOF) {
                printf("%s:%zu:%zu: Expected an identifier after \",\" in argument list:\n",
                        parser->lexer.fileName, comma.lineNum, comma.charNum);
                printScope(scopeAfter(comma));
                exit(1);
            }
            else {
                printf("%s:%zu:%zu: Expected an identifier after \",\" in argument list, got \""SV_FMT"\":\n",
                        parser->lexer.fileName, parser->current.lineNum, parser->current.charNum, SV_ARG(parser->current.text));
                printScope(scopeToken(parser->current));
                eatUntil(parser, TOKEN_IDENT_OR_KEYWORD);
                if (parser->current.type == TOKEN_EOF) {
                    exit(1);
                }
            }
            parser->success = false;
        }
        curr->data.nextArg = allocateNode(&parser->arena);
        curr = curr->data.nextArg;
        curr->type = NODE_ARG_LIST;
        curr->data.arg = parser->current.text;
        curr->data.nextArg = NULL;

        arg = parser->current;
        parser->current = getToken(&parser->lexer);
    }
    return head;
}

AST_Node* parseScope(Parser* parser) {
    Token closeParen = parser->current;
    parser->current = getToken(&parser->lexer);
    if (parser->current.type != TOKEN_LBRACE) {
        if (parser->current.type == TOKEN_EOF) {
            printf("%s:%zu:%zu: error! Expected \"{\" after argument list:\n",
                    parser->lexer.fileName, closeParen.lineNum, closeParen.charNum);
            printScope(scopeAfter(closeParen));
            exit(1);
        }
        else {
            printf("%s:%zu:%zu: error! Expected \"{\" after argument list, but got \""SV_FMT"\":\n",
                    parser->lexer.fileName, parser->current.lineNum, parser->current.charNum, SV_ARG(parser->current.text));
            printScope(scopeToken(parser->current));
            eatUntil(parser, TOKEN_LBRACE);
            if (parser->current.type == TOKEN_EOF) {
                exit(1);
            }
        }
        parser->success = false;
    }
    //TODO: Temporary. We want to parse and ExprList here:
    parser->current = getToken(&parser->lexer);
    assert(parser->current.type == TOKEN_RBRACE);
    if (parser->success) {
        AST_Node* node = allocateNode(&parser->arena);
        node->type = NODE_SCOPE;
        node->data.exprs = NULL;
        return node;
    }
    return NULL;
}

AST_Node* parseFunction(Parser* parser) {
    parser->current = getToken(&parser->lexer);
    if (parser->current.type != TOKEN_IDENT_OR_KEYWORD) {
        printf("%s:%zu:%zu: error! Expected an identifier, got\""SV_FMT"\":\n",
                parser->lexer.fileName, parser->current.lineNum, parser->current.charNum, SV_ARG(parser->current.text));
        printScope(scopeToken(parser->current));
        eatUntil(parser, TOKEN_IDENT_OR_KEYWORD);
        if (parser->current.type == TOKEN_EOF) {
            exit(1);
        }
        parser->success = false;
    }

    Token nameToken = parser->current;
    String_View name = parser->current.text;

    parser->current = getToken(&parser->lexer);
    if (parser->current.type != TOKEN_DCOLON) {
        if (isKeyword(parser->current, "func")) {
            printf("%s:%zu:%zu: error! Missing \"::\" between function name and \"func\" keyword:\n",
                    parser->lexer.fileName, nameToken.lineNum, nameToken.charNum);
            printScope(scopeBetween(nameToken, parser->current));
        }
        else if (parser->current.type == TOKEN_EOF) {
            printf("%s:%zu:%zu: error! Expected \"::\" after function name:\n",
                    parser->lexer.fileName, nameToken.lineNum, nameToken.charNum);
            printScope(scopeAfter(nameToken));
            exit(1);
        }
        else {
            printf("%s:%zu:%zu: error! Expected \"::\" after function name, but got \""SV_FMT"\":\n",
                    parser->lexer.fileName, parser->current.lineNum, parser->current.charNum, SV_ARG(parser->current.text));
            printScope(scopeToken(parser->current));
        }
        eatUntil(parser, TOKEN_DCOLON);
        if (parser->current.type == TOKEN_EOF) {
            exit(1);
        }
        parser->success = false;
    }

    parser->current = getToken(&parser->lexer);
    if (!isKeyword(parser->current, "func")) {
        printf("%s:%zu:%zu: error! Expected \"func\" keyword after \"::\", but got \""SV_FMT"\":\n",
                parser->lexer.fileName, parser->current.lineNum, parser->current.charNum, SV_ARG(parser->current.text));
        printScope(scopeToken(parser->current));
        eatUntilKeyword(parser, "func");
        if (parser->current.type == TOKEN_EOF) {
            exit(1);
        }
        parser->success = false;
    }

    AST_Node* args = parseArgList(parser);

    AST_Node* body = parseScope(parser);

    if (parser->success) {
        AST_Node* node = allocateNode(&parser->arena);
        node->type = NODE_FUNCTION;
        node->data.name = name;
        node->data.args = args;
        node->data.body = body;
        return node;
    }
    return NULL;
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
    Parser parser = makeParser(code, fileName);
    AST_Node* func = parseFunction(&parser);
    if (parser.success) {
        printNode(*func);
    }
    return 0;
}
