#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>
#include <assert.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
// IMPORTANT:
// None
////////////////////////////////////////////////////////////////////////////////////////////////////

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

void tryFPuts(char* str, FILE* file) {
    fputs(str, file);
    if (ferror(file)) {
        fprintf(stderr, "[ERROR]: Could not write to file!\nReason: %s\n", strerror(errno));
        exit(1);
    }
}

void tryFPutsIndented(int indent, char* str, FILE* file) {
    for (int i = 0; i < indent; ++i) {
        tryFPuts("    ", file);
    }
    tryFPuts(str, file);
}

void tryFWrite(void* buffer, size_t size, size_t count, FILE* stream) {
    fwrite(buffer, size, count, stream);
    if (ferror(stream)) {
        fprintf(stderr, "[ERROR]: Could not write to file!\nReason: %s\n", strerror(errno));
        exit(1);
    }
}

void tryFWriteIndented(int indent, void* buffer, size_t size, size_t count, FILE* stream) {
    for (int i = 0; i < indent; ++i) {
        tryFPuts("    ", stream);
    }
    tryFWrite(buffer, size, count, stream);
}

void vTryFPrintf(FILE* stream, char* fmt, va_list args) {
    if(vfprintf(stream, fmt, args) < 0) {
        fprintf(stderr, "[ERROR]: Could not write to file!\nReason: %s\n", strerror(errno));
        exit(1);
    }
}

void tryFPrintf(FILE* stream, char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vTryFPrintf(stream, fmt, args);
    va_end(args);
}

void tryFPrintfIndented(int indent, FILE* stream, char* fmt, ...) {
    for (int i = 0; i < indent; ++i) {
        tryFPuts("    ", stream);
    } 
    va_list args;
    va_start(args, fmt);
    vTryFPrintf(stream, fmt, args);
    va_end(args);
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
    fclose(file);
    return contents;
}



/////////////////////
// String View API //
/////////////////////

typedef struct {
    char* start;
    int length;
} String_View;

#define SV_FMT "%.*s"
#define SV_ARG(sv) (sv).length, (sv).start

bool svIsEmpty(String_View sv) {
    return sv.length == 0;
}

String_View svFromCStr(char* cstr) {
    return (String_View){cstr, (int)strlen(cstr)};
}

bool svEquals(String_View sv1, String_View sv2) {
    if (sv1.length != sv2.length) {
        return false;
    }
    return strncmp(sv1.start, sv2.start, sv1.length) == 0;
}

bool svEqualsCStr(String_View sv, char* cstr) {
    for (size_t i = 0; i < sv.length; ++i) {
        if (cstr[i] == '\0' || cstr[i] != sv.start[i]) {
            return false;
        }
    }
    return cstr[sv.length] == '\0';
}

String_View svUntil(char delim, char* cstr) {
    int length = 0;
    for (char* p = cstr; *p && *p != delim; ++p) {
        length++;
    }
    return (String_View){cstr, length};
}

inline void svTryFWrite(String_View sv, FILE* file) {
    tryFWrite(sv.start, 1, sv.length, file);
}



///////////////
// Error API //
///////////////

typedef struct {
    int lineNumStart;
    int charNumStart;
    String_View lineStart;
    int lineNumEnd;
    int charNumEnd;
    String_View lineEnd;
} Lex_Scope;

void printScope(Lex_Scope scope) {
    if (scope.lineNumStart == scope.lineNumEnd) {
        printf("  "SV_FMT"\n", SV_ARG(scope.lineStart));
        printf("  ");
        for (int i = 0; i < scope.charNumStart; ++i) {
            printf(" ");
        }
        for (int i = 0; i < scope.charNumEnd - scope.charNumStart; ++i) {
            printf("^");
        }
    }
    else {
        printf("  Line %5d: "SV_FMT"\n", scope.lineNumStart + 1, SV_ARG(scope.lineStart));
        printf("   ...         ");
        for (int i = 0; i < scope.charNumStart; ++i) {
            printf(" ");
        }
        for (int i = scope.charNumStart; i < scope.lineStart.length; ++i) {
            printf("^");
        }
        printf("  Line %5d: "SV_FMT"\n", scope.lineNumEnd + 1, SV_ARG(scope.lineEnd));
        printf("               ");
        for (size_t i = 0; i < scope.charNumEnd; ++i) {
            printf("^");
        }
    }
    printf("\n");
}

void printErrorMessage(char* fileName, Lex_Scope scope, char* fmt, ...) {
    printf("%s:%d:%d: ERROR! ", fileName, scope.lineNumStart + 1, scope.charNumStart + 1);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf(":\n");
    printScope(scope);
}



///////////////
// Lexer API //
///////////////

#define NUM_BINOP_TOKENS 5
#define NUM_TYPE_TOKENS 2

typedef enum {
    // Special token for EOF. Returned when the lexer has no more tokens.
    TOKEN_EOF,

    // Punctuation tokens
    TOKEN_COMMA,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_COLON,
    TOKEN_DOUBLE_COLON,
    TOKEN_SEMICOLON,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_EQUALS,
    TOKEN_DOUBLE_EQUALS,
    TOKEN_ARROW,

    // Control flow keywords
    TOKEN_FUNC_KEYWORD,
    TOKEN_RETURN_KEYWORD,
    TOKEN_IF_KEYWORD,
    TOKEN_ELSE_KEYWORD,
    TOKEN_WHILE_KEYWORD,

    // Primitive type keywords
    TOKEN_INTTYPE_KEYWORD,
    TOKEN_BOOLTYPE_KEYWORD,

    // Identifiers
    TOKEN_IDENT,

    // Literals
    TOKEN_INT,
    TOKEN_BOOL,

    // Used to communicate that a token tried to be constructed, but was not of the expected type.
    TOKEN_ERROR,

    // Used for static asserts
    TOKEN_COUNT
} Token_Type;

typedef struct {
    Token_Type type;
    int lineNum;
    int charNum;
    String_View line;
    String_View text;
    union {
        int intValue;
        bool boolValue;
    };
} Token;

bool isOperator(Token_Type type) {
    return type == TOKEN_PLUS
        || type == TOKEN_MINUS
        || type == TOKEN_STAR
        || type == TOKEN_SLASH
        || type == TOKEN_DOUBLE_EQUALS;
    static_assert(NUM_BINOP_TOKENS == 5, "Non-exhaustive cases (isOperator)");
}

bool isTypeToken(Token_Type type) {
    return type == TOKEN_INTTYPE_KEYWORD
        || type == TOKEN_BOOLTYPE_KEYWORD;
    static_assert(NUM_TYPE_TOKENS == 2, "Non-exhaustive cases (isTypeToken)");
}

inline Lex_Scope scopeToken(Token token) {
    return (Lex_Scope){
        .lineNumStart = token.lineNum,
        .charNumStart = token.charNum,
        .lineStart = token.line,
        .lineNumEnd = token.lineNum,
        .charNumEnd = token.charNum + token.text.length,
        .lineEnd = token.line,
    };
}

inline Lex_Scope scopeBetween(Token start, Token end) {
    return (Lex_Scope){
        .lineNumStart = start.lineNum,
        .charNumStart = start.charNum,
        .lineStart = start.line,
        .lineNumEnd = end.lineNum,
        .charNumEnd = end.charNum + end.text.length,
        .lineEnd = end.line,
    };
}

inline Lex_Scope scopeAfter(Token token) {
    return (Lex_Scope){
        .lineNumStart = token.lineNum,
        .charNumStart = token.charNum + token.text.length + 1,
        .lineStart = token.line,
        .lineNumEnd = token.lineNum,
        .charNumEnd = token.charNum + token.text.length + 2,
        .lineEnd = token.line,
    };
}


typedef struct {
    char* code;
    char* fileName;
    int lineNum;
    int charNum;
    String_View line;
} Lexer;

inline Token makeToken(Lexer* lexer, Token_Type type, int textLength) {
    return (Token){
        .type = type,
        .lineNum = lexer->lineNum,
        .charNum = lexer->charNum,
        .line = lexer->line,
        .text = (String_View){lexer->code, textLength},
    };
}

inline Lexer makeLexer(char* code, char* fileName) {
    return (Lexer){
        .code = code,
        .fileName = fileName,
        .lineNum = 0,
        .charNum = 0,
        .line = svUntil('\n', code)
    };
}

inline void lexerAdvance(Lexer* lexer, size_t steps) {
    for (size_t i = 0; i < steps; ++i) {
        lexer->charNum++;
        if (*lexer->code == '\n') {
            lexer->lineNum++;
            lexer->charNum = 0;
            lexer->line = svUntil('\n', lexer->code + 1);
        }
        lexer->code++;
    }
}

inline bool isLexemeTerminator(char c) {
    return isspace(c)
        || c == '\0'
        || c == ','
        || c == '('
        || c == ')'
        || c == '{'
        || c == '}'
        || c == ';'
        || c == ':';
}

Token getIntToken(Lexer* lexer) {
    int intLength = 0;
    bool negative = false;
    if (*lexer->code == '+') {
        intLength++;
    }
    else if (*lexer->code == '-') {
        negative = true;
        intLength++;
    }
    bool zero = lexer->code[intLength] == '0' && isLexemeTerminator(lexer->code[intLength + 1]);
    if (!zero && (lexer->code[intLength] < '1' || lexer->code[intLength] > '9')) {
        return (Token){TOKEN_ERROR};
    }
    int value = lexer->code[intLength] - '0';
    intLength++;
    for (; !isLexemeTerminator(lexer->code[intLength]); ++intLength) {
        if (lexer->code[intLength] < '0' || lexer->code[intLength] > '9') {
            return (Token){TOKEN_ERROR};
        }
        value *= 10;
        value += lexer->code[intLength] - '0';
    }
    Token result = makeToken(lexer, TOKEN_INT, intLength);
    lexerAdvance(lexer, intLength);
    result.intValue = negative ? -value : value;
    return result;
}

Token getBoolToken(Lexer* lexer) {
    if (strncmp(lexer->code, "true", 4) == 0 && isLexemeTerminator(lexer->code[4])) {
        Token token = makeToken(lexer, TOKEN_BOOL, 4);
        token.boolValue = true;
        lexerAdvance(lexer, 4);
        return token;
    }
    else if (strncmp(lexer->code, "false", 5) == 0 && isLexemeTerminator(lexer->code[5])) {
        Token token = makeToken(lexer, TOKEN_BOOL, 5);
        token.boolValue = false;
        lexerAdvance(lexer, 5);
        return token;
    }
    return (Token){TOKEN_ERROR};
}

Token getKeywordToken(Lexer* lexer) {
    switch (*lexer->code) {
        case 'b': {
            if (strncmp(lexer->code, "bool", 4) == 0 && isLexemeTerminator(lexer->code[4])) {
                Token token = makeToken(lexer, TOKEN_BOOLTYPE_KEYWORD, 4);
                lexerAdvance(lexer, 4);
                return token;
            }
            break;
        }
        case 'e': {
            if (strncmp(lexer->code, "else", 4) == 0 && isLexemeTerminator(lexer->code[4])) {
                Token token = makeToken(lexer, TOKEN_ELSE_KEYWORD, 4);
                lexerAdvance(lexer, 4);
                return token;
            }
            break;
        }
        case 'f': {
            if (strncmp(lexer->code, "func", 4) == 0 && isLexemeTerminator(lexer->code[4])) {
                Token token = makeToken(lexer, TOKEN_FUNC_KEYWORD, 4);
                lexerAdvance(lexer, 4);
                return token;
            }
            break;
        }
        case 'i': {
            if (strncmp(lexer->code, "int", 3) == 0 && isLexemeTerminator(lexer->code[3])) {
                Token token = makeToken(lexer, TOKEN_INTTYPE_KEYWORD, 3);
                lexerAdvance(lexer, 3);
                return token;
            }
            else if (strncmp(lexer->code, "if", 2) == 0 && isLexemeTerminator(lexer->code[2])) {
                Token token = makeToken(lexer, TOKEN_IF_KEYWORD, 2);
                lexerAdvance(lexer, 2);
                return token;
            }
            break;
        }
        case 'r': {
            if (strncmp(lexer->code, "return", 6) == 0 && isLexemeTerminator(lexer->code[6])) {
                Token token = makeToken(lexer, TOKEN_RETURN_KEYWORD, 6);
                lexerAdvance(lexer, 6);
                return token;
            }
            break;
        }
        case 'w': {
            if (strncmp(lexer->code, "while", 5) == 0 && isLexemeTerminator(lexer->code[5])) {
                Token token = makeToken(lexer, TOKEN_WHILE_KEYWORD, 5);
                lexerAdvance(lexer, 5);
                return token;
            }
        }
    }
    return (Token){TOKEN_ERROR};
}

inline Token getIdentToken(Lexer* lexer) {
    int identLength = 0;
    for (; !isLexemeTerminator(lexer->code[identLength]); ++identLength);
    Token token = makeToken(lexer, TOKEN_IDENT, identLength);
    lexerAdvance(lexer, identLength);
    return token;
}

Token getToken(Lexer* lexer) {
    while (isspace(*lexer->code)) {
        lexerAdvance(lexer, 1);
    }

    switch(*lexer->code) {
        case '\0': {
            return makeToken(lexer, TOKEN_EOF, 0);
            break;
        }
        case ',': {
            Token result = makeToken(lexer, TOKEN_COMMA, 1);
            lexerAdvance(lexer, 1);
            return result;
        }
        case '(': {
            Token result = makeToken(lexer, TOKEN_LPAREN, 1);
            lexerAdvance(lexer, 1);
            return result;
        }
        case ')': {
            Token result = makeToken(lexer, TOKEN_RPAREN, 1);
            lexerAdvance(lexer, 1);
            return result;
        }
        case '{': {
            Token result = makeToken(lexer, TOKEN_LBRACE, 1);
            lexerAdvance(lexer, 1);
            return result;
        }
        case '}': {
            Token result = makeToken(lexer, TOKEN_RBRACE, 1);
            lexerAdvance(lexer, 1);
            return result;
        }
        case ':': {
            Token result;
            if (lexer->code[1] != ':') {
                result = makeToken(lexer, TOKEN_COLON, 1);
                lexerAdvance(lexer, 1);
            }
            else {
                result = makeToken(lexer, TOKEN_DOUBLE_COLON, 2);
                lexerAdvance(lexer, 2);
            }
            return result;
        }
        case ';': {
            Token result = makeToken(lexer, TOKEN_SEMICOLON, 1);
            lexerAdvance(lexer, 1);
            return result;
        }
        case '+': {
            Token result = makeToken(lexer, TOKEN_PLUS, 1);
            lexerAdvance(lexer, 1);
            return result;
        }
        case '-': {
            Token result;
            if (lexer->code[1] != '>') {
                result = makeToken(lexer, TOKEN_MINUS, 1);
                lexerAdvance(lexer, 1);
            }
            else {
                result = makeToken(lexer, TOKEN_ARROW, 2);
                lexerAdvance(lexer, 2);
            }
            return result;
        }
        case '*': {
            Token result = makeToken(lexer, TOKEN_STAR, 1);
            lexerAdvance(lexer, 1);
            return result;
        }
        case '/': {
            Token result = makeToken(lexer, TOKEN_SLASH, 1);
            lexerAdvance(lexer, 1);
            return result;
        }
        case '=': {
            Token result;
            if (lexer->code[1] != '=') {
                result = makeToken(lexer, TOKEN_EQUALS, 1);
                lexerAdvance(lexer, 1);
            }
            else {
                result = makeToken(lexer, TOKEN_DOUBLE_EQUALS, 2);
                lexerAdvance(lexer, 2);
            }
            return result;
        }
        default: {
            Token result = getIntToken(lexer);
            if (result.type != TOKEN_ERROR) {
                return result;
            }
            result = getBoolToken(lexer);
            if (result.type != TOKEN_ERROR) {
                return result;
            }
            result = getKeywordToken(lexer);
            if (result.type != TOKEN_ERROR) {
                return result;
            }
            result = getIdentToken(lexer);
            return result;
        } 
    }
}

inline Token peekToken(Lexer* lexer) {
    Lexer newLexer = *lexer;
    Token token = getToken(&newLexer);
    return token;
}

void printToken(Token token) {
    switch (token.type) {
        case TOKEN_EOF:
            printf("Type: EOF, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
        case TOKEN_COMMA:
            printf("Type: COMMA, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
        case TOKEN_LPAREN:
            printf("Type: LPAREN, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
        case TOKEN_RPAREN:
            printf("Type: RPAREN, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
        case TOKEN_LBRACE:
            printf("Type: LBRACE, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
        case TOKEN_RBRACE:
            printf("Type: RBRACE, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
        case TOKEN_DOUBLE_COLON:
            printf("Type: DOUBLE_COLON, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
        case TOKEN_EQUALS:
            printf("Type: EQUALS, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
        case TOKEN_DOUBLE_EQUALS:
            printf("Type: DOUBLE_EQUALS, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
        case TOKEN_SEMICOLON:
            printf("Type: SEMICOLON, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
        case TOKEN_PLUS:
            printf("Type: PLUS, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
        case TOKEN_MINUS:
            printf("Type: MINUS, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
        case TOKEN_STAR:
            printf("Type: STAR, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
        case TOKEN_SLASH:
            printf("Type: SLASH, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
        case TOKEN_FUNC_KEYWORD:
            printf("Type: FUNC_KEYWORD, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
        case TOKEN_RETURN_KEYWORD:
            printf("Type: RETURN_KEYWORD, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
        case TOKEN_IF_KEYWORD:
            printf("Type: IF_KEYWORD, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
        case TOKEN_ELSE_KEYWORD:
            printf("Type: ELSE_KEYWORD, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
        case TOKEN_WHILE_KEYWORD:
            printf("Type: WHILE_KEYWORD, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
        case TOKEN_INTTYPE_KEYWORD:
            printf("Type: INTTYPE_KEYWORD, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
        case TOKEN_BOOLTYPE_KEYWORD:
            printf("Type: BOOLTYPE_KEYWORD, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
        case TOKEN_IDENT:
            printf("Type: IDENT, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
        case TOKEN_INT:
            printf("Type: INT, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
        case TOKEN_BOOL:
            printf("Type: BOOL, text: "SV_FMT"\n", SV_ARG(token.text));
            break;
    }
    static_assert(TOKEN_COUNT == 27, "Non-exhaustive cases (printToken)");
}



////////////////
// Parser API //
////////////////

#define NUM_BINOP_NODES 5

typedef enum {
    NODE_FUNCTION,
    NODE_ARGS,       // Argument lists to functions
    NODE_SCOPE,      // Lists of statements wrapped in braces.

    // Statements
    NODE_STATEMENTS, // Linked lists of statements
    NODE_RETURN,
    NODE_DECLARATION,
    NODE_ASSIGNMENT,

    // Arithmetic operators 
    NODE_PLUS,
    NODE_MINUS,
    NODE_TIMES,
    NODE_DIVIDE,

    // Boolean operators
    NODE_IS_EQUAL,

    // Literals
    NODE_INT,
    NODE_BOOL,

    // Control flow
    NODE_IF,
    NODE_ELSE,
    NODE_WHILE,

    // Identifiers
    NODE_IDENT,

    //Used for static asserts
    NODE_COUNT,
} Node_Type;

typedef struct AST_Node AST_Node;

typedef union {
    struct {                 // NODE_FUNCTION
        String_View functionName;
        AST_Node* functionArgs;
        AST_Node* functionBody;
        String_View functionRetType;
    };
    // Represents a linked list of function arguments.
    struct {                 // NODE_ARGS
        String_View argName;
        String_View argType;
        AST_Node* argNext;
    };
    struct {                 // NODE_SCOPE
        AST_Node* scopeStatements;
        int scopeId; // Necessary for variable info lookup in symbol table
    };
    // Represents a linked list of expressions in a scope.
    struct {                 // NODE_STATEMENTS
        AST_Node* statementStatement;
        AST_Node* statementNext;
    };
    struct {                 // Binary operations (e.g. NODE_PLUS, NODE_MINUS, ...)
        AST_Node* binaryOpLeft;
        AST_Node* binaryOpRight;
    };
    struct {                 // Control statements (NODE_IF, NODE_WHILE)
        AST_Node* controlCondition;
        AST_Node* controlScope;
    };
    struct {                 // NODE_ELSE
        AST_Node* elseScope;
    };
    AST_Node* returnExpr;       // NODE_RETURN
    struct {                 // NODE_DELCARATION
        String_View declarationName;
        String_View declarationType;
    };
    struct {                 // NODE_ASSIGNMENT
        String_View assignmentName;
        AST_Node* assignmentExpr;
    };
    String_View identName;   // NODE_IDENT
    int intValue;            // NODE_INT
    bool boolValue;          // NODE_BOOL
} Node_Data;

struct AST_Node {
    Node_Type type;
    Node_Data data;
};

#define INIT_PROGRAM_CAPACITY 128
typedef struct {
    AST_Node** nodes;
    int length;
    int capacity;
} Program;

Program makeProgram() {
    return (Program){
        .nodes = NULL,
        .length = 0,
        .capacity = 0,
    };
}

void printAST(AST_Node* node); // Forward declaration, since this is needed for `printProgram`

void printProgram(Program program) {
    for (int i = 0; i < program.length; ++i) {
        printAST(program.nodes[i]);
        printf("\n");
    }
}

void programAddNode(Program* program, AST_Node* node) {
    if (program->nodes == NULL) {
        program->capacity = INIT_PROGRAM_CAPACITY;
        program->nodes = malloc(program->capacity * sizeof(AST_Node*));
    }
    else if (program->length == program->capacity) {
        program->capacity *= 2;
        program->nodes = realloc(program->nodes, program->capacity * sizeof(AST_Node*));
    }
    program->nodes[program->length++] = node;
}

int getScopeId() {
    static int id = 0;
    return id++;
}

bool isNodeOperator(Node_Type type) {
    return type == NODE_PLUS
        || type == NODE_MINUS
        || type == NODE_TIMES
        || type == NODE_DIVIDE
        || type == NODE_IS_EQUAL;
    static_assert(NUM_BINOP_NODES == 5, "Non-exhaustive cases (isNodeOperator)");
}

typedef struct {
    AST_Node* nodes;
    int length;
} AST_Node_Bucket;

#define INIT_LIST_CAPACITY 128

typedef struct {
    AST_Node_Bucket* buckets; // The bucket array. This contains pointers to `AST_Node_Bucket`s in order to preserve pointer stability for the nodes in a given bucket.
    int length;                // Number of valid pointers to buckets
    int capacity;              // Capacity of the `buckets` array. Used to decide when `buckets` needs to be resized.

    int bucketCapacity;        // The capacity of a single bucket
} AST_Node_List;


inline AST_Node_List makeNodeList(int bucketCapacity) {
    return (AST_Node_List){
        .buckets = NULL,
        .length = 0,
        .capacity = 0,
        .bucketCapacity = bucketCapacity,
    };
}

inline AST_Node* nodeListAddNode(AST_Node_List* list, AST_Node node) {
    if (list->capacity == 0) {
        assert(list->length == 0);
        list->capacity = INIT_LIST_CAPACITY;
        list->buckets = malloc(list->capacity * sizeof(AST_Node_Bucket));
        list->buckets[0] = (AST_Node_Bucket){
            .nodes = malloc(list->bucketCapacity * sizeof(AST_Node)),
            .length = 0,
        };
        list->length = 1;
    }

    if (list->buckets[list->length - 1].length == list->bucketCapacity) {
        if (list->length == list->capacity) {
            list->capacity *= 2;
            list->buckets = realloc(list->buckets, list->capacity * sizeof(AST_Node_Bucket));
        }
        list->buckets[list->length++] = (AST_Node_Bucket){
            .nodes = malloc(list->bucketCapacity * sizeof(AST_Node)),
            .length = 0,
        };
    }

    AST_Node_Bucket* lastBucket = &list->buckets[list->length - 1];
    assert(lastBucket->length < list->bucketCapacity);
    lastBucket->nodes[lastBucket->length++] = node;
    return &lastBucket->nodes[lastBucket->length - 1];
}

inline AST_Node* addDeclarationNode(AST_Node_List* list, String_View name, String_View type) {
    AST_Node node;
    node.type = NODE_DECLARATION;
    node.data.declarationName = name;
    node.data.declarationType = type;
    return nodeListAddNode(list, node);
}

inline AST_Node* addAssignmentNode(AST_Node_List* list, String_View name, AST_Node* expr) {
    AST_Node node;
    node.type = NODE_ASSIGNMENT;
    node.data.assignmentName = name;
    node.data.assignmentExpr = expr;
    return nodeListAddNode(list, node);
}

inline AST_Node* addIntNode(AST_Node_List* list, int value) {
    AST_Node node;
    node.type = NODE_INT;
    node.data.intValue = value;
    return nodeListAddNode(list, node);
}

inline AST_Node* addBoolNode(AST_Node_List* list, bool value) {
    AST_Node node;
    node.type = NODE_BOOL;
    node.data.boolValue = value;
    return nodeListAddNode(list, node);
}

inline AST_Node* addIdentNode(AST_Node_List* list, String_View name) {
    AST_Node node;
    node.type = NODE_IDENT;
    node.data.identName = name;
    return nodeListAddNode(list, node);
}

inline AST_Node* addArgNode(AST_Node_List* list, String_View name, String_View type) {
    AST_Node node;
    node.type = NODE_ARGS;
    node.data.argName = name;
    node.data.argType = type;
    return nodeListAddNode(list, node);
}

inline AST_Node* addReturnNode(AST_Node_List* list, AST_Node* expr) {
    AST_Node node;
    node.type = NODE_RETURN;
    node.data.returnExpr = expr;
    return nodeListAddNode(list, node);
}

inline AST_Node* addElseNode(AST_Node_List* list, AST_Node* scope) {
    AST_Node node;
    node.type = NODE_ELSE;
    node.data.elseScope = scope;
    return nodeListAddNode(list, node);
}

inline AST_Node* addControlNode(AST_Node_List* list, Node_Type type, AST_Node* condition, AST_Node* scope) {
    AST_Node node;
    node.type = type;
    node.data.controlCondition = condition;
    node.data.controlScope = scope;
    return nodeListAddNode(list, node);
}

inline AST_Node* addBinaryOpNode(AST_Node_List* list, AST_Node* left, Token token, AST_Node* right) {
    assert(isOperator(token.type));
    AST_Node node;
    switch (token.type) {
        case TOKEN_PLUS:
            node.type = NODE_PLUS;
            break;
        case TOKEN_MINUS:
            node.type = NODE_MINUS;
            break;
        case TOKEN_STAR:
            node.type = NODE_TIMES;
            break;
        case TOKEN_SLASH:
            node.type = NODE_DIVIDE;
            break;
        case TOKEN_DOUBLE_EQUALS:
            node.type = NODE_IS_EQUAL;
            break;
        default:
            printf("Unknown token operator type: %d\n", token.type);
            assert(false && "Unexhausted cases (addBinaryOpNode)");
    }
    static_assert(NUM_BINOP_TOKENS == 5, "Non-exhaustive cases (addBinaryOpNode)");
    node.data.binaryOpLeft = left;
    node.data.binaryOpRight = right;
    return nodeListAddNode(list, node);
}

int getPrecedence(Token_Type type) {
    switch (type) {
        case TOKEN_DOUBLE_EQUALS:
            return 10;
        case TOKEN_PLUS:
        case TOKEN_MINUS:
            return 20;
        case TOKEN_STAR:
        case TOKEN_SLASH:
            return 30;
        default:
            printf("Unknown token operator type: %d\n", type);
            assert(false && "Called with a non-operator token or non-exhaustive cases (getPrecedence)");
    }
    static_assert(NUM_BINOP_TOKENS == 5, "Non-exhaustive cases (getPrecedence)");
}

int getNodePrecedence(Node_Type type) {
    switch (type) {
        case NODE_IS_EQUAL:
            return 10;
        case NODE_PLUS:
        case NODE_MINUS:
            return 20;
        case NODE_TIMES:
        case NODE_DIVIDE:
            return 30;
        default:
            printf("Unknown node operator type: %d\n", type);
            assert(false && "Called with a non-operator node or non-exhaustive cases (getNodePrecedence)");
    }
    static_assert(NUM_BINOP_NODES == 5, "Non-exhaustive cases (getNodePrecedence)");
}

void recoverByEatUntil(Lexer* lexer, Token_Type wanted) {
    Token token = getToken(lexer);
    while (token.type != wanted && token.type != TOKEN_EOF) {
        token = getToken(lexer);
    }
    if (token.type == TOKEN_EOF) {
        exit(1);
    }
}

void recoverByEatUpTo(Lexer* lexer, Token_Type wanted) {
    Token peeked = peekToken(lexer);
    while (peeked.type != wanted && peeked.type != TOKEN_EOF) {
        getToken(lexer);
        peeked = peekToken(lexer);
    }
    if (peeked.type == TOKEN_EOF) {
        exit(1);
    }
}

AST_Node* parseTerm(AST_Node_List* list, Lexer* lexer);
AST_Node* parseBracketedExpr(AST_Node_List* list, Lexer* lexer);
AST_Node* parseExpr(AST_Node_List* list, Lexer* lexer, int precedence);
AST_Node* parseStatement(AST_Node_List* list, Lexer* lexer);
AST_Node* parseStatments(AST_Node_List* list, Lexer* lexer, bool* success);
AST_Node* parseScope(AST_Node_List* list, Lexer* lexer);
AST_Node* parseArgs(AST_Node_List* list, Lexer* lexer, bool* success);
AST_Node* parseFunction(AST_Node_List* list, Lexer* lexer);

Program parseProgram(AST_Node_List* list, Lexer* lexer, bool* success);

AST_Node* parseTerm(AST_Node_List* list, Lexer* lexer) {
    Token token = getToken(lexer);
    switch (token.type) {
        case TOKEN_INT:
            return addIntNode(list, token.intValue);
        case TOKEN_BOOL:
            return addBoolNode(list, token.boolValue);
        case TOKEN_IDENT:
            return addIdentNode(list, token.text);
        default:
            printErrorMessage(lexer->fileName, scopeToken(token), "Expected an integer or identifier, but got \""SV_FMT"\"", SV_ARG(token.text));
            recoverByEatUpTo(lexer, TOKEN_SEMICOLON);
    }
    return addIntNode(list, token.intValue);
}

AST_Node* parseIncreasingPrecedence(AST_Node_List* list, Lexer* lexer, AST_Node* left, int precedence) {
    Token token = peekToken(lexer);
    assert(isOperator(token.type));

    int thisPrecedence = getPrecedence(token.type);
    if (thisPrecedence > precedence) {
        getToken(lexer); // Eat the operator
        AST_Node* right = parseExpr(list, lexer, thisPrecedence);
        if (right == NULL) {
            return NULL;
        }
        return addBinaryOpNode(list, left, token, right);
    }
    return left;
}

AST_Node* parseExpr(AST_Node_List* list, Lexer* lexer, int precedence) {
    Token peeked = peekToken(lexer);
    if (peeked.type == TOKEN_LPAREN) {
        getToken(lexer); // Eat the '('
        AST_Node* inner = parseExpr(list, lexer, -1);
        if (inner == NULL) {
            return NULL;
        }

        Token token = getToken(lexer);
        if (token.type != TOKEN_RPAREN) {
            printErrorMessage(lexer->fileName, scopeToken(token), "Expected \")\", but got \""SV_FMT"\"", SV_ARG(token.text));
            recoverByEatUpTo(lexer, TOKEN_SEMICOLON);
            return NULL;
        }
        return inner;
    }

    AST_Node* term = parseTerm(list, lexer);
    if (term == NULL) {
        return NULL;
    }

    peeked = peekToken(lexer);
    while (isOperator(peeked.type)) {
        AST_Node* op = parseIncreasingPrecedence(list, lexer, term, precedence);
        if (op == NULL) {
            return NULL;
        }
        if (op == term) {
            return term;
        }
        term = op;
        peeked = peekToken(lexer);
    }
    
    if (peeked.type == TOKEN_INT || peeked.type == TOKEN_IDENT) {
        printErrorMessage(lexer->fileName, scopeToken(peeked), "Expected an operator, but got %d", peeked.intValue);
        recoverByEatUpTo(lexer, TOKEN_SEMICOLON);
        return NULL;
    }
   
    return term;
}

AST_Node* parseStatement(AST_Node_List* list, Lexer* lexer) {
    Token token = peekToken(lexer);
    switch (token.type) {
        case TOKEN_RETURN_KEYWORD: {
            getToken(lexer); // Eat the "return"
            AST_Node* inner = parseExpr(list, lexer, -1);
            if (inner == NULL) {
               recoverByEatUntil(lexer, TOKEN_SEMICOLON);
               return NULL;
            }
            AST_Node* result = addReturnNode(list, inner);
            token = getToken(lexer);
            if (token.type != TOKEN_SEMICOLON) {
                printErrorMessage(lexer->fileName, scopeAfter(token), "Expected a \";\", but got none");
                return NULL;
            }
            return result;
        }
        case TOKEN_LBRACE: {
            return parseScope(list, lexer);
        }
        case TOKEN_IDENT: {
            getToken(lexer); // Eat the ident
            String_View name = token.text;
            token = getToken(lexer);
            switch (token.type) {
                case TOKEN_COLON: {
                    token = getToken(lexer);
                    if (!isTypeToken(token.type)) {
                        printErrorMessage(lexer->fileName, scopeToken(token), "Expected a type name, but got \""SV_FMT"\"", SV_ARG(token.text));
                        recoverByEatUntil(lexer, TOKEN_SEMICOLON);
                        return NULL;
                    }
                    String_View type = token.text;
                    token = getToken(lexer);
                    if (token.type != TOKEN_SEMICOLON) {
                        printErrorMessage(lexer->fileName, scopeAfter(token), "Expected a \";\", but got none");
                        return NULL;
                    }
                    return addDeclarationNode(list, name, type);
                }
                case TOKEN_EQUALS: {
                    AST_Node* expr = parseExpr(list, lexer, -1);
                    if (expr == NULL) {
                        recoverByEatUntil(lexer, TOKEN_SEMICOLON);
                        return NULL;
                    }
                    token = getToken(lexer);
                    if (token.type != TOKEN_SEMICOLON) {
                        printErrorMessage(lexer->fileName, scopeAfter(token), "Expected a \";\", but got none");
                        return NULL;
                    }
                    return addAssignmentNode(list, name, expr);
                }
                default: {
                    printErrorMessage(lexer->fileName, scopeToken(token), "Expected a declaration or an assignment, but got neither");
                    recoverByEatUntil(lexer, TOKEN_SEMICOLON);
                    break;
                }
            }
            break;
        }
        case TOKEN_IF_KEYWORD: {
            getToken(lexer); // Eat the "if"
            //TODO: Error recovery from parseExpr will eat until a semicolon, but that's not great here.
            AST_Node* condition = parseExpr(list, lexer, -1);
            if (condition == NULL) {
                recoverByEatUntil(lexer, TOKEN_RBRACE);
                return NULL;
            }
            AST_Node* scope = parseScope(list, lexer);
            if (scope == NULL) {
                return NULL;
            }
            return addControlNode(list, NODE_IF, condition, scope);
        }
        case TOKEN_ELSE_KEYWORD: {
            getToken(lexer); // Eat the "else"
            AST_Node* scope = parseScope(list, lexer);
            if (scope == NULL) {
                return NULL;
            }
            return addElseNode(list, scope);
        }
        case TOKEN_WHILE_KEYWORD: {
            getToken(lexer); // Eat the "while"
            AST_Node* condition = parseExpr(list, lexer, -1);
            if (condition == NULL) {
                recoverByEatUntil(lexer, TOKEN_RBRACE);
                return NULL;
            }
            AST_Node* scope = parseScope(list, lexer);
            if (scope == NULL) {
                return NULL;
            }
            return addControlNode(list, NODE_WHILE, condition, scope);
            break;
        }
        default: {
            printErrorMessage(lexer->fileName, scopeToken(token), "Expected the start of a valid statement, got \""SV_FMT"\"", SV_ARG(token.text));
            recoverByEatUntil(lexer, TOKEN_SEMICOLON);
            return NULL;
       }
    }
}

AST_Node* parseStatements(AST_Node_List* list, Lexer* lexer, bool* success) {
    *success = true;

    if (peekToken(lexer).type == TOKEN_RBRACE) {
        getToken(lexer); // Eat the '}'
        return NULL;
    }

    AST_Node node;
    node.type = NODE_STATEMENTS;
    node.data.statementStatement = parseStatement(list, lexer);
    if (node.data.statementStatement == NULL) {
        *success = false;
    }
    AST_Node* head = nodeListAddNode(list, node);
    AST_Node* curr = head;

    while (peekToken(lexer).type != TOKEN_RBRACE) {
        node.data.statementStatement = parseStatement(list, lexer);
        if (node.data.statementStatement == NULL) {
            *success = false;
        }
        curr->data.statementNext = nodeListAddNode(list, node);
        curr = curr->data.statementNext;
    }
    getToken(lexer); // Eat the '}'
    curr->data.statementNext = NULL;
    return head;
}

AST_Node* parseScope(AST_Node_List* list, Lexer* lexer) {
    Token token = getToken(lexer);
    assert(token.type == TOKEN_LBRACE);

    AST_Node node;
    node.type = NODE_SCOPE;
    node.data.scopeId = getScopeId();

    bool success;
    node.data.scopeStatements = parseStatements(list, lexer, &success);

    return success ? nodeListAddNode(list, node) : NULL;
}

AST_Node* parseArgs(AST_Node_List* list, Lexer* lexer, bool* success) {
    *success = true;

    Token token = getToken(lexer);
    assert(token.type == TOKEN_LPAREN);

    token = getToken(lexer);
    if (token.type == TOKEN_RPAREN) {
        return NULL;
    }
    if (token.type != TOKEN_IDENT) {
        printErrorMessage(lexer->fileName, scopeToken(token), "Expected an identifier in argument list, got \""SV_FMT"\"", SV_ARG(token.text));
        recoverByEatUntil(lexer, TOKEN_IDENT);
        *success = false;
    }
    String_View name = token.text;

    token = getToken(lexer);
    if (token.type != TOKEN_COLON) {
        printErrorMessage(lexer->fileName, scopeToken(token), "Expected \":\" in argument list, got \""SV_FMT"\"", SV_ARG(token.text));
        recoverByEatUntil(lexer, TOKEN_COLON);
        *success = false;
    }

    token = getToken(lexer);
    if (!isTypeToken(token.type)) {
        printErrorMessage(lexer->fileName, scopeToken(token), "Expected a type name in argument list, got \""SV_FMT"\"", SV_ARG(token.text));
        recoverByEatUntil(lexer, TOKEN_INTTYPE_KEYWORD);
        *success = false;
    }
    String_View type = token.text;
    AST_Node* head = addArgNode(list, name, type);
    AST_Node* curr = head;

    token = getToken(lexer);
    while (token.type != TOKEN_RPAREN) {
        if (token.type != TOKEN_COMMA) {
            printErrorMessage(lexer->fileName, scopeToken(token), "Expected \",\" separating arguments in argument list, got \""SV_FMT"\"", SV_ARG(token.text));
            recoverByEatUntil(lexer, TOKEN_COMMA);
            *success = false;
        }

        token = getToken(lexer);
        if (token.type != TOKEN_IDENT) {
            printErrorMessage(lexer->fileName, scopeToken(token), "Expected an identifier in argument list, got \""SV_FMT"\"", SV_ARG(token.text));
            recoverByEatUntil(lexer, TOKEN_IDENT);
            *success = false;
        }
        name = token.text;

        token = getToken(lexer);
        if (token.type != TOKEN_COLON) {
            printErrorMessage(lexer->fileName, scopeToken(token), "Expected \":\" in argument list, got \""SV_FMT"\"", SV_ARG(token.text));
            recoverByEatUntil(lexer, TOKEN_COLON);
            *success = false;
        }

        token = getToken(lexer);
        if (token.type != TOKEN_INTTYPE_KEYWORD) {
            printErrorMessage(lexer->fileName, scopeToken(token), "Expected a type name in argument list, got \""SV_FMT"\"", SV_ARG(token.text));
            recoverByEatUntil(lexer, TOKEN_INTTYPE_KEYWORD);
            *success = false;
        }
        type = token.text;
        
        curr->data.argNext = addArgNode(list, name, type);
        curr = curr->data.argNext;

        token = getToken(lexer);
    }
    curr->data.argNext = NULL;
    return head;
}

AST_Node* parseFunction(AST_Node_List* list, Lexer* lexer) {
    bool success = true;
    AST_Node node;
    node.type = NODE_FUNCTION;

    Token token = getToken(lexer);
    if (token.type != TOKEN_IDENT) {
        printErrorMessage(lexer->fileName, scopeToken(token), "Expected an identifier in function definition, got \""SV_FMT"\"", SV_ARG(token.text));
        recoverByEatUntil(lexer, TOKEN_IDENT);
        success = false;
    }
    node.data.functionName = token.text;

    token = getToken(lexer);
    if (token.type != TOKEN_DOUBLE_COLON) {
        printErrorMessage(lexer->fileName, scopeToken(token), "Expected \"::\" in function definition, got \""SV_FMT"\"", SV_ARG(token.text));
        recoverByEatUntil(lexer, TOKEN_DOUBLE_COLON);
        success = false;
    }

    token = getToken(lexer);
    if (token.type != TOKEN_FUNC_KEYWORD) {
        printErrorMessage(lexer->fileName, scopeToken(token), "Expected \"func\" keyword in function definition, got \""SV_FMT"\"", SV_ARG(token.text));
        recoverByEatUntil(lexer, TOKEN_FUNC_KEYWORD);
        success = false;
    }

    token = peekToken(lexer);
    if (token.type != TOKEN_LPAREN) {
        printErrorMessage(lexer->fileName, scopeToken(token), "Expected argument list (starting with \"(\") in function definition, got \""SV_FMT"\"", SV_ARG(token.text));
        recoverByEatUntil(lexer, TOKEN_LPAREN);
        success = false;
    }

    bool argsSuccess;
    node.data.functionArgs = parseArgs(list, lexer, &argsSuccess);
    if (!argsSuccess) {
        success = false;
    }

    token = peekToken(lexer);
    if (token.type == TOKEN_ARROW) {
        getToken(lexer); // Eat the "->"
        token = getToken(lexer);
        if (!isTypeToken(token.type)) {
            printErrorMessage(lexer->fileName, scopeToken(token), "Expected a return type in function definition, got \""SV_FMT"\"", SV_ARG(token.text));
            recoverByEatUpTo(lexer, TOKEN_LBRACE);
            success = false;
        }
        node.data.functionRetType = token.text;
        token = peekToken(lexer);
    }
    else {
        node.data.functionRetType = svFromCStr("unit");
    }

    if (token.type != TOKEN_LBRACE) {
        printErrorMessage(lexer->fileName, scopeToken(token), "Expected function body (starting with \"{\") in function definition, got \""SV_FMT"\"", SV_ARG(token.text));
        recoverByEatUntil(lexer, TOKEN_LBRACE);
        success = false;
    }

    node.data.functionBody = parseScope(list, lexer);
    if (node.data.functionBody == NULL) {
        success = false;
    }

    return success ? nodeListAddNode(list, node) : NULL;
}

Program parseProgram(AST_Node_List* list, Lexer* lexer, bool* success) {
    *success = true;
    Program program = makeProgram();

    Token token = peekToken(lexer);
    while(token.type != TOKEN_EOF) {
        AST_Node* function = parseFunction(list, lexer);
        if (function == NULL) {
            *success = false;
        }
        if (*success) {
            programAddNode(&program, function);
        }

        token = peekToken(lexer);
    }
    return program;
}

void printIndented(int indent, char* fmt, ...) {
    for (int i = 0; i < indent; ++i) {
        printf("  ");
    }
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

void printASTIndented(int indent, AST_Node* root) {
    switch (root->type) {
        case NODE_FUNCTION: {
            printIndented(indent, "type=FUNCTION, name="SV_FMT", rettype="SV_FMT", args=", SV_ARG(root->data.functionName), SV_ARG(root->data.functionRetType));
            if (root->data.functionArgs == NULL) {
                printf("NONE, body=");
            }
            else {
                printf("(\n");
                printASTIndented(indent + 1, root->data.functionArgs);
                printIndented(indent, "), body=");
            }
            if (root->data.functionBody == NULL) {
                printf("NONE\n");
            }
            else {
                printf("(\n");
                printASTIndented(indent + 1, root->data.functionBody);
                printIndented(indent, ")\n");
            }
            break;
        }
        case NODE_ARGS: {
            printIndented(indent, "name="SV_FMT", type="SV_FMT"\n", SV_ARG(root->data.argName), SV_ARG(root->data.argType));
            while (root->data.argNext != NULL) {
                root = root->data.argNext;
                printIndented(indent, "name="SV_FMT", type="SV_FMT"\n", SV_ARG(root->data.argName), SV_ARG(root->data.argType));
            }
            break;
        }
        case NODE_SCOPE: {
            printIndented(indent, "type=SCOPE, statements=");
            if (root->data.scopeStatements == NULL) {
                printf("NONE\n");
            }
            else {
                printf("(\n");
                printASTIndented(indent + 1, root->data.scopeStatements);
                printIndented(indent, ")\n");
            }
            break;
        }
        case NODE_STATEMENTS: {
            printASTIndented(indent, root->data.statementStatement);
            while (root->data.statementNext != NULL) {
                root = root->data.statementNext;
                printASTIndented(indent, root->data.statementStatement);
            }
            break;
        }
        case NODE_RETURN: {
            printIndented(indent, "type=RETURN, expr=");
            if (root->data.returnExpr == NULL) {
                printf("NONE\n");
            }
            else {
                printf("(\n");
                printASTIndented(indent + 1, root->data.returnExpr);
                printIndented(indent, ")\n");
            }
            break;
        }
        case NODE_DECLARATION: {
            printIndented(indent, "type=DECLARATION, name="SV_FMT", type="SV_FMT"\n", SV_ARG(root->data.declarationName), SV_ARG(root->data.declarationType));
            break;
        }
        case NODE_ASSIGNMENT: {
            printIndented(indent, "type=ASSIGNMENT, name="SV_FMT", expr=(\n", SV_ARG(root->data.declarationName));
            printASTIndented(indent + 1, root->data.assignmentExpr);
            printIndented(indent, ")\n");
            break;
        }
        case NODE_PLUS: {
            printIndented(indent, "type=PLUS, left=(\n");
            printASTIndented(indent + 1, root->data.binaryOpLeft);
            printIndented(indent, "), right=(\n");
            printASTIndented(indent + 1, root->data.binaryOpRight);
            printIndented(indent, ")\n");
            break;
        }
        case NODE_MINUS: {
            printIndented(indent, "type=MINUS, left=(\n");
            printASTIndented(indent + 1, root->data.binaryOpLeft);
            printIndented(indent, "), right=(\n");
            printASTIndented(indent + 1, root->data.binaryOpRight);
            printIndented(indent, ")\n");
            break;
        }
        case NODE_TIMES: {
            printIndented(indent, "type=TIMES, left=(\n");
            printASTIndented(indent + 1, root->data.binaryOpLeft);
            printIndented(indent, "), right=(\n");
            printASTIndented(indent + 1, root->data.binaryOpRight);
            printIndented(indent, ")\n");
            break;
        }
        case NODE_DIVIDE: {
            printIndented(indent, "type=DIVIDE, left=(\n");
            printASTIndented(indent + 1, root->data.binaryOpLeft);
            printIndented(indent, "), right=(\n");
            printASTIndented(indent + 1, root->data.binaryOpRight);
            printIndented(indent, ")\n");
            break;
        }
        case NODE_IS_EQUAL: {
            printIndented(indent, "type=IS_EQUAL, left=(\n");
            printASTIndented(indent + 1, root->data.binaryOpLeft);
            printIndented(indent, "), right=(\n");
            printASTIndented(indent + 1, root->data.binaryOpRight);
            printIndented(indent, ")\n");
            break;
        }
        case NODE_IF: {
            printIndented(indent, "type=IF, condition=(\n");
            printASTIndented(indent + 1, root->data.controlCondition);
            printIndented(indent, "), body=(\n");
            printASTIndented(indent + 1, root->data.controlScope);
            printIndented(indent, ")\n");
            break;
        }
        case NODE_ELSE: {
            printIndented(indent, "type=ELSE, body=(\n");
            printASTIndented(indent + 1, root->data.elseScope);
            printIndented(indent, ")\n");
            break;
        }
        case NODE_WHILE: {
            printIndented(indent, "type=WHILE, condition=(\n");
            printASTIndented(indent + 1, root->data.controlCondition);
            printIndented(indent, "), body=(\n");
            printASTIndented(indent + 1, root->data.controlScope);
            printIndented(indent, ")\n");
            break;
        }
        case NODE_INT: {
            printIndented(indent, "type=INT, value=%d\n", root->data.intValue);
            break;
        }
        case NODE_BOOL: {
            printIndented(indent, "type=BOOL, value=%d\n", root->data.boolValue);
            break;
        }
        case NODE_IDENT: {
            printIndented(indent, "type=IDENT, name="SV_FMT"\n", SV_ARG(root->data.identName));
            break;
        }
    }
    static_assert(NODE_COUNT == 18, "Non-exhaustive cases (printASTIndented)");
}

inline void printAST(AST_Node* root) {
    printASTIndented(0, root);
}



//////////////////////
// Symbol table API //
//////////////////////

typedef struct {
    int scopeId;
    String_View name;
    String_View type;
} Symbol_Entry;

typedef struct {
    int id;
    int parentId; // ID of the immediate parent of the scope with ID `id`.
} Scope_Parent_Entry;

typedef struct {
    // TODO: This should be a hash map
    Symbol_Entry* symbols;
    int symbolsLength;
    int symbolsCapacity;

    
    // TODO: This should be a hash map
    Scope_Parent_Entry* scopeParents;
    int parentsLength;
    int parentsCapacity;
} Symbol_Table;

Symbol_Table makeSymbolTable(int capacity) {
    return (Symbol_Table){
        .symbols = malloc(capacity * sizeof(Symbol_Entry)),
        .symbolsLength = 0,
        .symbolsCapacity = capacity,

        .scopeParents = malloc(capacity * sizeof(Scope_Parent_Entry)),
        .parentsLength = 0,
        .parentsCapacity = capacity,
    };
}

void printSymbolTable(Symbol_Table table) {
    printf("Symbols:\n");
    for (int i = 0; i < table.symbolsLength; ++i) {
        Symbol_Entry entry = table.symbols[i];
        printf("Scope id: %d, Name: "SV_FMT", Type: "SV_FMT"\n", entry.scopeId, SV_ARG(entry.name), SV_ARG(entry.type));
    }

    printf("--------------------------------------------------\n");

    printf("Parent data:\n");
    for (int i = 0; i < table.parentsLength; ++i) {
        Scope_Parent_Entry entry = table.scopeParents[i];
        printf("This id: %d, Parent id: %d\n", entry.id, entry.parentId);
    }
}

void addSymbol(Symbol_Table* table, int scopeId, String_View name, String_View type) {
    if (table->symbolsLength == table->symbolsCapacity) {
        table->symbolsCapacity *= 2;
        table->symbols = realloc(table->symbols, table->symbolsCapacity * sizeof(Symbol_Entry));
    }
    table->symbols[table->symbolsLength++] = (Symbol_Entry){scopeId, name, type};
}

void addScopeParent(Symbol_Table* table, int id, int parentId) {
    if (table->parentsLength == table->parentsCapacity) {
        table->parentsCapacity *= 2;
        table->scopeParents = realloc(table->scopeParents, table->parentsCapacity * sizeof(Scope_Parent_Entry));
    }
    table->scopeParents[table->parentsLength++] = (Scope_Parent_Entry){id, parentId};
}

void addScopeData(Symbol_Table* table, AST_Node* root, int parentId) {
    assert(root->type == NODE_SCOPE);

    AST_Node* statements = root->data.scopeStatements;
    // If the root is empty, there's no data to add.
    if (statements == NULL) {
        return;
    }

    addScopeParent(table, root->data.scopeId, parentId);
    while (statements != NULL) {
        AST_Node* statement = statements->data.statementStatement;

        switch (statement->type) {
            case NODE_DECLARATION: {
                addSymbol(table, root->data.scopeId, statement->data.declarationName, statement->data.declarationType);
                break;
            }
            case NODE_SCOPE: {
                addScopeData(table, statements->data.statementStatement, root->data.scopeId);
                break;
            }
            case NODE_IF:
            case NODE_WHILE: {
                addScopeData(table, statement->data.controlScope, root->data.scopeId);
                break;
            }
            case NODE_ELSE: {
                addScopeData(table, statement->data.elseScope, root->data.scopeId);
                break;
            }
        }
        statements = statements->data.statementNext;
    }
}

void addFunctionData(Symbol_Table* table, AST_Node* root, int parentId) {
    assert(root->type == NODE_FUNCTION);

    AST_Node* body = root->data.functionBody;
    // If the body of the function is empty, then we don't need to add any of the heirarchy data or argument symbols.
    if (body->data.scopeStatements == NULL) {
        return;
    }

    AST_Node* arg = root->data.functionArgs;
    while (arg != NULL) {
        addSymbol(table, body->data.scopeId, arg->data.argName, arg->data.argType);
        arg = arg->data.argNext;
    }

    addScopeData(table, root->data.functionBody, parentId);
}

void initSymbolTable(Symbol_Table* table, Program program) {
    for (int i = 0; i < program.length; ++i) {
        addFunctionData(table, program.nodes[i], -1);
    }
}

Scope_Parent_Entry tableLookupParent(Symbol_Table* table, int scopeId) {
    for (int i = 0; i < table->parentsLength; ++i) {
        Scope_Parent_Entry entry = table->scopeParents[i];
        if (entry.id == scopeId) {
            return entry;
        }
    }
    assert(false && "Unreachable (tableLookupParent)");
}

typedef struct {
    bool exists;
    Symbol_Entry entry;
} Symbol_Lookup_Result;

Symbol_Lookup_Result tableLookupSymbol(Symbol_Table* table, int scopeId, String_View name) {
    while (scopeId != -1) {
        for (int i = 0; i < table->symbolsLength; ++i) {
            Symbol_Entry entry = table->symbols[i];
            if (entry.scopeId == scopeId && svEquals(entry.name, name)) {
                return (Symbol_Lookup_Result){true, entry};
            }
        }
        scopeId = tableLookupParent(table, scopeId).parentId;
    }
    return (Symbol_Lookup_Result){false};
}



//////////////////////
// Verification API //
//////////////////////

bool verifyProgram(Symbol_Table* table, Program program);
bool verifyFunction(Symbol_Table* table, AST_Node* root);
bool verifyScope(Symbol_Table* table, AST_Node* root);
bool verifyExpr(Symbol_Table* table, AST_Node* root, int scopeId);

bool verifyProgram(Symbol_Table* table, Program program) {
    bool success = true;

    for (int i = 0; i < program.length; ++i) {
        bool functionSuccess = verifyFunction(table, program.nodes[i]);
        if (!functionSuccess) {
            success = false;
        }
    }
    return success;
}

bool verifyFunction(Symbol_Table* table, AST_Node* root) {
    assert(root->type == NODE_FUNCTION);

    return verifyScope(table, root->data.functionBody);
}

bool verifyScope(Symbol_Table* table, AST_Node* root) {
    bool success = true;

    assert(root->type == NODE_SCOPE);

    AST_Node* statements = root->data.scopeStatements;
    while (statements != NULL) {
        AST_Node* statement = statements->data.statementStatement;

        switch (statement->type) {
            case NODE_RETURN: {
                if (!verifyExpr(table, statement->data.returnExpr, root->data.scopeId)) {
                    success = false;
                }
                break;
            }
            case NODE_ASSIGNMENT: {
                Symbol_Lookup_Result result = tableLookupSymbol(table, root->data.scopeId, statement->data.assignmentName);
                if (!result.exists) {
                    fprintf(stderr, "ERROR! Use of undeclared identifier \""SV_FMT"\"\n", SV_ARG(statement->data.assignmentName));
                    success = false;
                }
                break;
            }
            case NODE_SCOPE: {
                if (!verifyScope(table, statements->data.statementStatement)) {
                    success = false;
                }
                break;
            }
            case NODE_IF:
            case NODE_WHILE: {
                if (!verifyExpr(table, statement->data.controlCondition, root->data.scopeId)) {
                    success = false;
                }
                if (!verifyScope(table, statement->data.controlScope)) {
                    success = false;
                }
                break;
            }
            case NODE_ELSE: {
                if (!verifyScope(table, statement->data.elseScope)) {
                    success = false;
                }
                break;
            }
            case NODE_DECLARATION: {
                break;
            }
            default:
                printf("Unknown statement type: %d\n", statement->type);
                assert(false && "Not a statement type or non-exhaustive cases (verifyScope)");
        }

        statements = statements->data.statementNext;
    }

    return success;
}

bool verifyExpr(Symbol_Table* table, AST_Node* root, int scopeId) {
    switch (root->type) {
        case NODE_INT:
        case NODE_BOOL: {
            return true;
        }

        case NODE_IDENT: {
            Symbol_Lookup_Result result = tableLookupSymbol(table, scopeId, root->data.identName);
            if (!result.exists) {
                fprintf(stderr, "ERROR! Variable \""SV_FMT"\" used before it was declared\n", SV_ARG(root->data.identName));
                return false;
            }
            return true;
        }

        case NODE_PLUS:
        case NODE_MINUS:
        case NODE_TIMES:
        case NODE_DIVIDE:
        case NODE_IS_EQUAL: {
            if (!verifyExpr(table, root->data.binaryOpLeft, scopeId)) {
                return false;
            }
            if (!verifyExpr(table, root->data.binaryOpRight, scopeId)) {
                return false;
            }
            return true;
        }
        default:
            printf("Unknown expression node: %d\n", root->type);
            assert(false && "Not an expression type or non-exhaustive cases (verifyExpr)");
    }
}



///////////////////////
// Type checking API //
///////////////////////

//TODO: Using String_View for types seems inefficient and sloppy. This really should be an enum for primitive types and reworked to allow for user-defined types.

bool typeCheckProgram(Symbol_Table* table, Program program);
bool typeCheckFunction(Symbol_Table* table, AST_Node* root);
bool expectScopeType(Symbol_Table* table, AST_Node* root, String_View expected);

String_View getExprType(Symbol_Table* table, AST_Node* root, int scopeId);

bool typeCheckProgram(Symbol_Table* table, Program program) {
    bool success = true;

    for (int i = 0; i < program.length; ++i) {
        bool functionSuccess = typeCheckFunction(table, program.nodes[i]);
        if (!functionSuccess) {
            success = false;
        }
    }
    return success;
}

bool typeCheckFunction(Symbol_Table* table, AST_Node* root) {
    assert(root->type == NODE_FUNCTION);
    
    return expectScopeType(table, root->data.functionBody, root->data.functionRetType);
}

bool expectScopeType(Symbol_Table* table, AST_Node* root, String_View expected) {
    bool success = true;
    assert(root->type == NODE_SCOPE);

    AST_Node* statements = root->data.scopeStatements;
    while (statements != NULL) {
        AST_Node* statement = statements->data.statementStatement;

        switch (statement->type) {
            case NODE_RETURN: {
                String_View exprType = getExprType(table, statement->data.returnExpr, root->data.scopeId);
                if (svIsEmpty(exprType)) {
                    fprintf(stderr, "ERROR! Could not evaluate type of return expression.\n");
                    success = false;
                }
                if (!svEquals(exprType, expected)) {
                    fprintf(stderr, "ERROR! Type mismatch. Expected "SV_FMT", got "SV_FMT"\n", SV_ARG(expected), SV_ARG(exprType));
                    success = false;
                }
                break;
            }
            case NODE_ASSIGNMENT: {
                Symbol_Lookup_Result var = tableLookupSymbol(table, root->data.scopeId, statement->data.assignmentName);
                assert(var.exists);
                String_View varType = var.entry.type;
                String_View exprType = getExprType(table, statement->data.assignmentExpr, root->data.scopeId);
                if (svIsEmpty(exprType)) {
                    //TODO: IMPROVE THIS ERROR MESSAGE!!!!! Lexical scoping of AST_Nodes
                    fprintf(stderr, "ERROR! Could not evalutate the right hand side of assignment\n");
                    success = false;
                }
                if (!svEquals(varType, exprType)) {
                    fprintf(stderr, "ERROR! Type mismatch. Expected "SV_FMT", got "SV_FMT"\n", SV_ARG(varType), SV_ARG(exprType));
                    success = false;
                }
                break;
            }
            case NODE_IF:
            case NODE_WHILE: {
                String_View conditionType = getExprType(table, statement->data.controlCondition, root->data.scopeId);
                if (svIsEmpty(conditionType)) {
                    fprintf(stderr, "ERROR! Could not evaluate type of control condition\n");
                    success = false;
                }
                if (!svEqualsCStr(conditionType, "bool")) {
                    fprintf(stderr, "ERROR! Type mismatch. Expected bool, got "SV_FMT"\n", SV_ARG(conditionType));
                    success = false;
                }

                expectScopeType(table, statement->data.controlScope, svFromCStr("unit"));
                break;
            }
            case NODE_ELSE: {
                if (!expectScopeType(table, statement->data.elseScope, svFromCStr("unit"))) {
                    success = false;
                }
                break;
            }
            case NODE_SCOPE: {
                if (!expectScopeType(table, statements->data.statementStatement, svFromCStr("unit"))) {
                    success = false;
                }
                break;
            }
        }

        statements = statements->data.statementNext;
    }
    return success;
}

String_View getExprType(Symbol_Table* table, AST_Node* root, int scopeId) {
    switch (root->type) {
        case NODE_INT: {
            return svFromCStr("int");
        }
        case NODE_BOOL: {
            return svFromCStr("bool");
        }
        case NODE_IDENT: {
            Symbol_Lookup_Result result = tableLookupSymbol(table, scopeId, root->data.identName);
            assert(result.exists);
            return result.entry.type;
        }
        case NODE_PLUS:
        case NODE_MINUS:
        case NODE_TIMES:
        case NODE_DIVIDE: {
            String_View left = getExprType(table, root->data.binaryOpLeft, scopeId);
            String_View right = getExprType(table, root->data.binaryOpRight, scopeId);
            if (svIsEmpty(left) || svIsEmpty(right)) {
                fprintf(stderr, "ERROR! Could not evaluate expression type\n");
                return svFromCStr("");
            }
            if (!svEquals(left, right)) {
                fprintf(stderr, "ERROR! Type mismatch. Left is "SV_FMT", right is "SV_FMT"\n", SV_ARG(left), SV_ARG(right));
                return svFromCStr("");
            }
            return left;
        }
        case NODE_IS_EQUAL: {
            String_View left = getExprType(table, root->data.binaryOpLeft, scopeId);
            String_View right = getExprType(table, root->data.binaryOpRight, scopeId);
            if (svIsEmpty(left) || svIsEmpty(right)) {
                fprintf(stderr, "ERROR! Could not evaluate expression type\n");
                return svFromCStr("");
            }
            if (!svEquals(left, right)) {
                fprintf(stderr, "ERROR! Type mismatch. Left is "SV_FMT", right is "SV_FMT"\n", SV_ARG(left), SV_ARG(right));
                return svFromCStr("");
            }
            return svFromCStr("bool");
        }
        default:
            printf("Unknown expression node: %d\n", root->type);
            assert(false && "Not an expression type or non-exhaustive cases (getExprType)");
    }
}



/////////////////
// Emitter API //
/////////////////

void emitTerm(FILE* file, AST_Node* root);
void emitExpr(FILE* file, AST_Node* root, int precedence);
void emitStatement(int indent, FILE* file, AST_Node* root);
void emitStatements(int indent, FILE* file, AST_Node* root);
void emitScope(int leadingIndent, int indent, FILE* file, AST_Node* root);
void emitArgs(FILE* file, AST_Node* root);
void emitFunction(FILE* file, AST_Node* root);
void emitProgram(FILE* file, Program program);

void emitTerm(FILE* file, AST_Node* root) {
    switch (root->type) {
        case NODE_INT: {
            tryFPrintf(file, "%d", root->data.intValue);
            break;
        }
        case NODE_BOOL: {
            tryFPrintf(file, "%d", root->data.boolValue);
            break;
        }
        case NODE_IDENT: {
            tryFPrintf(file, SV_FMT, SV_ARG(root->data.identName));
            break;
        }
        default:
            printf("Unknown node term type: %d\n", root->type);
            assert(false && "Called with a non-term node or non-exhaustive cases (emitTerm)");
    }
}

void emitExpr(FILE* file, AST_Node* root, int precedence) {
    if (isNodeOperator(root->type)) {
        int thisPrecedence = getNodePrecedence(root->type);
        if (thisPrecedence < precedence) {
            tryFPuts("(", file);
        }

        switch (root->type) {
            case NODE_PLUS: {
                emitExpr(file, root->data.binaryOpLeft, thisPrecedence);
                tryFPuts(" + ", file);
                emitExpr(file, root->data.binaryOpRight, thisPrecedence);
                break;
            }
            case NODE_MINUS: {
                emitExpr(file, root->data.binaryOpLeft, thisPrecedence);
                tryFPuts(" - ", file);
                emitExpr(file, root->data.binaryOpRight, thisPrecedence);
                break;
            }
            case NODE_TIMES: {
                emitExpr(file, root->data.binaryOpLeft, thisPrecedence);
                tryFPuts(" * ", file);
                emitExpr(file, root->data.binaryOpRight, thisPrecedence);
                break;
            }
            case NODE_DIVIDE: {
                emitExpr(file, root->data.binaryOpLeft, thisPrecedence);
                tryFPuts(" / ", file);
                emitExpr(file, root->data.binaryOpRight, thisPrecedence);
                break;
            }
            case NODE_IS_EQUAL: {
                emitExpr(file, root->data.binaryOpLeft, thisPrecedence);
                tryFPuts(" == ", file);
                emitExpr(file, root->data.binaryOpRight, thisPrecedence);
                break;
            }
        }
        static_assert(NUM_BINOP_NODES == 5, "Non-exhaustive cases (emitExpr)");

        if (thisPrecedence < precedence) {
            tryFPuts(")", file);
        }
    }
    else {
        emitTerm(file, root);
    }
}

void emitStatement(int indent, FILE* file, AST_Node* root) {
    switch (root->type) {
        case NODE_RETURN: {
            tryFPutsIndented(indent, "return ", file);
            emitExpr(file, root->data.returnExpr, -1);
            tryFPuts(";\n", file);
            break;
        }
        case NODE_DECLARATION: {
            tryFPrintfIndented(indent, file, SV_FMT" "SV_FMT, SV_ARG(root->data.declarationType), SV_ARG(root->data.declarationName));
            tryFPuts(";\n", file);
            break;
        }
        case NODE_ASSIGNMENT: {
            tryFPrintfIndented(indent, file, SV_FMT" = ", SV_ARG(root->data.assignmentName));
            emitExpr(file, root->data.assignmentExpr, -1);
            tryFPuts(";\n", file);
            break;
        }
        case NODE_IF: {
            tryFPutsIndented(indent, "if (", file);
            emitExpr(file, root->data.controlCondition, -1);
            tryFPuts(") ", file);
            emitScope(0, indent, file, root->data.controlScope);
            break;
        }
        case NODE_ELSE: {
            tryFPutsIndented(indent, "else ", file);
            emitScope(0, indent, file, root->data.elseScope);
            break;
        }
        case NODE_WHILE: {
            tryFPutsIndented(indent, "while (", file);
            emitExpr(file, root->data.controlCondition, -1);
            tryFPuts(") ", file);
            emitScope(0, indent, file, root->data.controlScope);
            break;
        }
        case NODE_SCOPE: {
            emitScope(indent, indent, file, root);
            break;
        }
        default:
            printf("Unexpected node type: %d\n", root->type);
            assert(false && "Not a statement type or non-exhaustive cases (emitStatement)");
    }
}

void emitStatements(int indent, FILE* file, AST_Node* root) {
    if (root == NULL) {
        return;
    }
    do {
        assert(root->type == NODE_STATEMENTS);

        emitStatement(indent, file, root->data.statementStatement);

        root = root->data.statementNext;
    } while (root != NULL);
}

void emitScope(int leadingIndent, int indent, FILE* file, AST_Node* root) {
    assert(root->type == NODE_SCOPE);

    tryFPutsIndented(leadingIndent, "{\n", file);
    emitStatements(indent + 1, file, root->data.scopeStatements);
    tryFPutsIndented(indent, "}\n", file);
}

void emitArgs(FILE* file, AST_Node* root) {
    if (root == NULL) {
        tryFPuts("()", file);
        return;
    }

    tryFPuts("(", file);
    tryFPrintf(file, SV_FMT" "SV_FMT, SV_ARG(root->data.argType), SV_ARG(root->data.argName));
    root = root->data.argNext;
    while (root != NULL) {
        tryFPrintf(file, ", "SV_FMT" "SV_FMT, SV_ARG(root->data.argType), SV_ARG(root->data.argName));
        root = root->data.argNext;
    }
    tryFPuts(")", file);
}

void emitFunction(FILE* file, AST_Node* root) {
    assert(root->type == NODE_FUNCTION);

    if (svEqualsCStr(root->data.functionName, "main")) {
        tryFPuts("int ", file);
    }
    else if (svEqualsCStr(root->data.functionRetType, "unit")) {
        tryFPuts("void ", file);
    }
    else {
        tryFPrintf(file, SV_FMT" ", SV_ARG(root->data.functionRetType));
    }
    tryFPrintf(file, SV_FMT, SV_ARG(root->data.functionName));
    emitArgs(file, root->data.functionArgs);
    tryFPuts(" ", file);
    emitScope(0, 0, file, root->data.functionBody);
}

void emitProgram(FILE* file, Program program) {
    tryFPuts("#include <stdbool.h>\n\n", file); // Needed for bool types in the emitted C program

    if (program.length == 0) {
        return;
    }

    emitFunction(file, program.nodes[0]);
    for (int i = 1; i < program.length; ++i) {
        tryFPuts("\n", file);
        emitFunction(file, program.nodes[i]);
    }
}

// Read in file simple.lcl - DONE
// Have an iterator lexer - DONE
// Write a simple grammar - DONE* (Will be expanded)
// Write a simple parser for that grammar - DONE* (Needs error handling)
// Convert to C code - DONE
// Compile C code to executable

int main() {
    char* fileName = "examples/simple.lcl";
    char* code = readEntireFile(fileName);
    Lexer lexer = makeLexer(code, fileName);
    AST_Node_List list = makeNodeList(512);

    bool parseSuccess;
    Program program = parseProgram(&list, &lexer, &parseSuccess);
    if (!parseSuccess) {
        return 1;
    }
    printProgram(program);
    
    Symbol_Table table = makeSymbolTable(8);
    initSymbolTable(&table, program);

    printf("\n\n\n");
    printSymbolTable(table);
    printf("\n\n\n");

    bool verified = verifyProgram(&table, program);
    if (!verified) {
        return 1;
    }

    bool typeChecked = typeCheckProgram(&table, program);
    if (!typeChecked) {
        return 1;
    }

    FILE* output = tryFOpen("examples/simple.c", "wb");
    emitProgram(output, program);
    return 0;
}
