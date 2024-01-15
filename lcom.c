#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>
#include <assert.h>



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
    TOKEN_IDENT,
} Token_Type;

typedef struct {
    Token_Type type;
    String_View text;
} Token;

bool isLexemeTerminator(char c) {
    return c == '('
        || c == ')'
        || c == '{'
        || c == '}'
        || c == ':';
}

Token getToken(char** input) {
    while (isspace(**input)) (*input)++;
    switch(**input) {
        case '\0':
            return (Token){TOKEN_EOF, (String_View){*input, 0}};
        case ',':
            (*input)++;
            return (Token){TOKEN_COMMA, (String_View){*input - 1, 1}};
        case '(':
            (*input)++;
            return (Token){TOKEN_LPAREN, (String_View){*input - 1, 1}};
        case ')':
            (*input)++;
            return (Token){TOKEN_RPAREN, (String_View){*input - 1, 1}};
        case '{':
            (*input)++;
            return (Token){TOKEN_LBRACE, (String_View){*input - 1, 1}};
        case '}':
            (*input)++;
            return (Token){TOKEN_RBRACE, (String_View){*input - 1, 1}};
        case ':': {
            if ((*input)[1] == ':') {
                *input += 2;
                return (Token){TOKEN_DCOLON, (String_View){*input - 2, 2}};
            }
        }
        default: {
            size_t identLength = 0;
            while(!isLexemeTerminator((*input)[identLength])) {
                identLength++;
            }
            String_View text = {*input, identLength};
            *input += identLength;
            return (Token){TOKEN_IDENT, text};
        } 
    }
}

void printToken(Token token) {
    char* typeString;
    switch (token.type) {
        case TOKEN_EOF:
            typeString = "EOF";
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
        case TOKEN_IDENT:
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

AST_Node* parseArgList(AST_Node_Arena* arena, char** code) {
    Token token = getToken(code);
    assert(token.type == TOKEN_LPAREN);

    token = getToken(code);
    if (token.type == TOKEN_RPAREN) {
        return NULL;
    }
    assert(token.type == TOKEN_IDENT);
    AST_Node* head = allocateNode(arena);
    head->type = NODE_ARG_LIST;
    head->data.arg = token.text;
    head->data.nextArg = NULL;
    AST_Node* curr = head;

    token = getToken(code);
    while (token.type != TOKEN_RPAREN) {
        assert(token.type == TOKEN_COMMA);
        token = getToken(code);
        assert(token.type == TOKEN_IDENT);
        curr->data.nextArg = allocateNode(arena);
        curr = curr->data.nextArg;
        curr->type = NODE_ARG_LIST;
        curr->data.arg = token.text;
        curr->data.nextArg = NULL;
        token = getToken(code);
    }
    return head;
}

AST_Node* parseScope(AST_Node_Arena* arena, char** code) {
    Token token = getToken(code);
    assert(token.type == TOKEN_LBRACE);

    //TODO: Parse expression list

    token = getToken(code);
    assert(token.type == TOKEN_RBRACE);

    AST_Node* node = allocateNode(arena);
    node->type = NODE_SCOPE;
    node->data.exprs = NULL;
    return node;
}

AST_Node* parseFunction(AST_Node_Arena* arena, char** code) {
    Token token = getToken(code);
    assert(token.type == TOKEN_IDENT);
    String_View name = token.text;

    token = getToken(code);
    assert(token.type == TOKEN_DCOLON);

    token = getToken(code);
    assert(token.type == TOKEN_IDENT && svEqualsCStr(token.text, "func"));

    AST_Node* args = parseArgList(arena, code);

    AST_Node* body = parseScope(arena, code);

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
    char* code = readEntireFile("simple.lcl");
    AST_Node_Arena arena = makeNodeArena(10);
    AST_Node* func = parseFunction(&arena, &code);
    printNode(*func);
    return 0;
}
