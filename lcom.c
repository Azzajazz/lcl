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

String_View svFromCStr(char* cstr) {
    return (String_View){cstr, (int)strlen(cstr)};
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
    TOKEN_DCOLON,
    TOKEN_SEMICOLON,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_EQUALS,
    TOKEN_ARROW,

    // Keyword tokens
    TOKEN_FUNC_KEYWORD,
    TOKEN_RETURN_KEYWORD,
    TOKEN_INTTYPE_KEYWORD,

    // Identifiers
    TOKEN_IDENT,

    // Literals
    TOKEN_INT,

    // Used to communicate that a token tried to be constructed, but was not of the expected type.
    TOKEN_ERROR,
} Token_Type;

typedef struct {
    Token_Type type;
    int lineNum;
    int charNum;
    String_View line;
    String_View text;
    int intValue;
} Token;

bool isOperator(Token token) {
    return token.type == TOKEN_PLUS
        || token.type == TOKEN_MINUS
        || token.type == TOKEN_STAR
        || token.type == TOKEN_SLASH;
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

Token getKeywordToken(Lexer* lexer) {
    switch (*lexer->code) {
        case 'f': {
            if (strncmp(lexer->code, "func", 4) == 0 && isLexemeTerminator(lexer->code[4])) {
                Token token = makeToken(lexer, TOKEN_FUNC_KEYWORD, 4);
                lexerAdvance(lexer, 4);
                return token;
            }
        }
        case 'r': {
            if (strncmp(lexer->code, "return", 6) == 0 && isLexemeTerminator(lexer->code[6])) {
                Token token = makeToken(lexer, TOKEN_RETURN_KEYWORD, 6);
                lexerAdvance(lexer, 6);
                return token;
            }
        }
        case 'i': {
            if (strncmp(lexer->code, "int", 3) == 0 && isLexemeTerminator(lexer->code[3])) {
                Token token = makeToken(lexer, TOKEN_INTTYPE_KEYWORD, 3);
                lexerAdvance(lexer, 3);
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
                result = makeToken(lexer, TOKEN_DCOLON, 2);
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
            Token result = makeToken(lexer, TOKEN_EQUALS, 1);
            lexerAdvance(lexer, 1);
            return result;
        }
        default: {
            Token result = getIntToken(lexer);
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
        case TOKEN_SEMICOLON:
            typeString = "SEMICOLON";
            break;
        case TOKEN_PLUS:
            typeString = "PLUS";
            break;
        case TOKEN_MINUS:
            typeString = "MINUS";
            break;
        case TOKEN_STAR:
            typeString = "STAR";
            break;
        case TOKEN_SLASH:
            typeString = "SLASH";
            break;
        case TOKEN_FUNC_KEYWORD:
            typeString = "FUNC_KEYWORD";
            break;
        case TOKEN_RETURN_KEYWORD:
            typeString = "RETURN_KEYWORD";
            break;
        case TOKEN_IDENT:
            typeString = "IDENT";
            break;
        case TOKEN_INT:
            typeString = "INT";
            break;
        default:
            assert(false && "Non-exhaustive cases in printToken");
    }
    printf("Type: %s, text: "SV_FMT"\n", typeString, SV_ARG(token.text));
}



////////////////
// Parser API //
////////////////

typedef enum {
    NODE_FUNCTION,
    NODE_ARGS,       // Argument lists to functions
    NODE_SCOPE,      // Lists of statements wrapped in braces.

    // Statements
    NODE_STATEMENTS, // Linked lists of statements
    NODE_RETURN,
    NODE_DECLARATION,
    NODE_ASSIGNMENT,

    // Expressions
    NODE_PLUS,
    NODE_MINUS,
    NODE_TIMES,
    NODE_DIVIDE,

    // Literals
    NODE_INT,

    // Identifiers
    NODE_IDENT
} Node_Type;

typedef union {
    struct {                 // NODE_FUNCTION
        String_View functionName;
        size_t functionArgs;
        size_t functionBody;
        String_View functionRetType;
    };
    // Represents a linked list of function arguments.
    struct {                 // NODE_ARGS
        String_View argName;
        String_View argType;
        size_t argNext;
    };
    size_t scopeStatements;  // NODE_SCOPE
    // Represents a linked list of expressions in a scope.
    struct {                 // NODE_STATEMENTS
        size_t statementStatement;
        size_t statementNext;
    };
    struct {                 // Binary operations (e.g. NODE_PLUS, NODE_MINUS, ...)
        size_t binaryOpLeft;
        size_t binaryOpRight;
    };
    size_t returnExpr;       // NODE_RETURN
    struct {                 // NODE_DELCARATION
        String_View declarationName;
        String_View declarationType;
    };
    struct {                 // NODE_ASSIGNMENT
        String_View assignmentName;
        size_t assignmentExpr;
    };
    String_View identName;   // NODE_IDENT
    int intValue;            // NODE_INT
} Node_Data;

typedef struct {
    Node_Type type;
    Node_Data data;
} AST_Node;

bool isNodeOperator(Node_Type type) {
    return type == NODE_PLUS
        || type == NODE_MINUS
        || type == NODE_TIMES
        || type == NODE_DIVIDE;
}

typedef struct {
    AST_Node* nodes;
    size_t nodeCount;
    size_t capacity;
} AST_Node_List;

inline AST_Node_List makeNodeList(size_t capacity) {
    return (AST_Node_List){
        .nodes = malloc(capacity * sizeof(AST_Node)),
        .nodeCount = 0,
        .capacity = capacity
    };
}

inline size_t addNode(AST_Node_List* list, AST_Node node) {
    if (list->nodeCount == list->capacity) {
        list->capacity *= 2;
        list->nodes = realloc(list->nodes, list->capacity * sizeof(AST_Node));
    }
    list->nodes[list->nodeCount++] = node;
    return list->nodeCount - 1;
}

inline size_t addDeclarationNode(AST_Node_List* list, String_View name, String_View type) {
    AST_Node node;
    node.type = NODE_DECLARATION;
    node.data.declarationName = name;
    node.data.declarationType = type;
    return addNode(list, node);
}

inline size_t addAssignmentNode(AST_Node_List* list, String_View name, size_t expr) {
    AST_Node node;
    node.type = NODE_ASSIGNMENT;
    node.data.assignmentName = name;
    node.data.assignmentExpr = expr;
    return addNode(list, node);
}

inline size_t addIntNode(AST_Node_List* list, int value) {
    AST_Node node;
    node.type = NODE_INT;
    node.data.intValue = value;
    return addNode(list, node);
}

inline size_t addIdentNode(AST_Node_List* list, String_View name) {
    AST_Node node;
    node.type = NODE_IDENT;
    node.data.identName = name;
    return addNode(list, node);
}

inline size_t addArgNode(AST_Node_List* list, String_View name, String_View type) {
    AST_Node node;
    node.type = NODE_ARGS;
    node.data.argName = name;
    node.data.argType = type;
    return addNode(list, node);
}

inline size_t addUnaryNode(AST_Node_List* list, Node_Type type, size_t child) {
    AST_Node node;
    node.type = type;
    switch (node.type) {
        case NODE_RETURN:
            node.data.returnExpr = child;
            break;
        default:
            printf("Unknown unary node: %d\n", node.type);
            assert(false && "Called with a node type that is not unary, or non-exhaustive cases (addUnaryNode)");
    }
    return addNode(list, node);
}

inline size_t addBinaryOpNode(AST_Node_List* list, size_t left, Token token, size_t right) {
    assert(isOperator(token));
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
        default:
            printf("Unknown token operator type: %d\n", token.type);
            assert(false && "Unexhausted cases (addBinaryOpNode)");
    }
    node.data.binaryOpLeft = left;
    node.data.binaryOpRight = right;
    return addNode(list, node);
}

int getPrecedence(Token_Type type) {
    switch (type) {
        case TOKEN_PLUS:
        case TOKEN_MINUS:
            return 10;
        case TOKEN_STAR:
        case TOKEN_SLASH:
            return 20;
        default:
            printf("Unknown token operator type: %d\n", type);
            assert(false && "Called with a non-operator token or non-exhaustive cases (getPrecedence)");
    }
}

int getNodePrecedence(Node_Type type) {
    switch (type) {
        case NODE_PLUS:
        case NODE_MINUS:
            return 10;
        case NODE_TIMES:
        case NODE_DIVIDE:
            return 20;
        default:
            printf("Unknown node operator type: %d\n", type);
            assert(false && "Called with a non-operator node or non-exhaustive cases (getNodePrecedence)");
    }
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

size_t parseTerm(AST_Node_List* list, Lexer* lexer);
size_t parseBracketedExpr(AST_Node_List* list, Lexer* lexer);
size_t parseExpr(AST_Node_List* list, Lexer* lexer, int precedence);
size_t parseStatement(AST_Node_List* list, Lexer* lexer);
size_t parseStatments(AST_Node_List* list, Lexer* lexer, bool* success);
size_t parseScope(AST_Node_List* list, Lexer* lexer);
size_t parseArgs(AST_Node_List* list, Lexer* lexer, bool* success);
size_t parseFunction(AST_Node_List* list, Lexer* lexer);

size_t parseTerm(AST_Node_List* list, Lexer* lexer) {
    Token token = getToken(lexer);
    switch (token.type) {
        case TOKEN_INT:
            return addIntNode(list, token.intValue);
        case TOKEN_IDENT:
            return addIdentNode(list, token.text);
        default:
            printErrorMessage(lexer->fileName, scopeToken(token), "Expected an integer or identifier, but got \""SV_FMT"\"", SV_ARG(token.text));
            recoverByEatUpTo(lexer, TOKEN_SEMICOLON);
    }
    return addIntNode(list, token.intValue);
}

size_t parseIncreasingPrecedence(AST_Node_List* list, Lexer* lexer, size_t left, int precedence) {
    Token token = peekToken(lexer);
    assert(isOperator(token));

    int thisPrecedence = getPrecedence(token.type);
    if (thisPrecedence > precedence) {
        getToken(lexer); // Eat the operator
        size_t right = parseExpr(list, lexer, thisPrecedence);
        if (right == SIZE_MAX) {
            return SIZE_MAX;
        }
        return addBinaryOpNode(list, left, token, right);
    }
    return left;
}

size_t parseExpr(AST_Node_List* list, Lexer* lexer, int precedence) {
    Token peeked = peekToken(lexer);
    if (peeked.type == TOKEN_LPAREN) {
        getToken(lexer); // Eat the '('
        size_t inner = parseExpr(list, lexer, -1);
        if (inner == SIZE_MAX) {
            return SIZE_MAX;
        }

        Token token = getToken(lexer);
        if (token.type != TOKEN_RPAREN) {
            printErrorMessage(lexer->fileName, scopeToken(token), "Expected \")\", but got \""SV_FMT"\"", SV_ARG(token.text));
            recoverByEatUpTo(lexer, TOKEN_SEMICOLON);
            return SIZE_MAX;
        }
        return inner;
    }

    size_t term = parseTerm(list, lexer);
    if (term == SIZE_MAX) {
        return SIZE_MAX;
    }

    peeked = peekToken(lexer);
    while (isOperator(peeked)) {
        size_t op = parseIncreasingPrecedence(list, lexer, term, precedence);
        if (op == SIZE_MAX) {
            return SIZE_MAX;
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
        return SIZE_MAX;
    }
   
    return term;
}

size_t parseStatement(AST_Node_List* list, Lexer* lexer) {
    Token token = getToken(lexer);
    switch (token.type) {
        case TOKEN_RETURN_KEYWORD: {
            size_t inner = parseExpr(list, lexer, -1);
            if (inner == SIZE_MAX) {
               recoverByEatUntil(lexer, TOKEN_SEMICOLON);
               return SIZE_MAX;
            }
            size_t result = addUnaryNode(list, NODE_RETURN, inner);
            token = getToken(lexer);
            if (token.type != TOKEN_SEMICOLON) {
                printErrorMessage(lexer->fileName, scopeAfter(token), "Expected a \";\", but got none");
                return SIZE_MAX;
            }
            return result;
        }
        case TOKEN_IDENT: {
            String_View name = token.text;
            token = getToken(lexer);
            switch (token.type) {
                case TOKEN_COLON: {
                    token = getToken(lexer);
                    if (token.type != TOKEN_INTTYPE_KEYWORD) {
                        printErrorMessage(lexer->fileName, scopeToken(token), "Expected a type name, but got \""SV_FMT"\"", SV_ARG(token.text));
                        recoverByEatUntil(lexer, TOKEN_SEMICOLON);
                        return SIZE_MAX;
                    }
                    String_View type = token.text;
                    token = getToken(lexer);
                    if (token.type != TOKEN_SEMICOLON) {
                        printErrorMessage(lexer->fileName, scopeAfter(token), "Expected a \";\", but got none");
                        return SIZE_MAX;
                    }
                    return addDeclarationNode(list, name, type);
                }
                case TOKEN_EQUALS: {
                    size_t expr = parseExpr(list, lexer, -1);
                    if (expr == SIZE_MAX) {
                        recoverByEatUntil(lexer, TOKEN_SEMICOLON);
                        return SIZE_MAX;
                    }
                    token = getToken(lexer);
                    if (token.type != TOKEN_SEMICOLON) {
                        printErrorMessage(lexer->fileName, scopeAfter(token), "Expected a \";\", but got none");
                        return SIZE_MAX;
                    }
                    return addAssignmentNode(list, name, expr);
                }
            }
        }
        default: {
            printErrorMessage(lexer->fileName, scopeToken(token), "Expected the start of a valid statement, got \""SV_FMT"\"", SV_ARG(token.text));
            recoverByEatUntil(lexer, TOKEN_SEMICOLON);
            return SIZE_MAX;
       }
    }
}

size_t parseStatements(AST_Node_List* list, Lexer* lexer, bool* success) {
    *success = true;

    if (peekToken(lexer).type == TOKEN_RBRACE) {
        getToken(lexer); // Eat the '}'
        return SIZE_MAX;
    }

    AST_Node node;
    node.type = NODE_STATEMENTS;
    node.data.statementStatement = parseStatement(list, lexer);
    if (node.data.statementStatement == SIZE_MAX) {
        *success = false;
    }
    size_t head = addNode(list, node);
    size_t curr = head;

    while (peekToken(lexer).type != TOKEN_RBRACE) {
        node.data.statementStatement = parseStatement(list, lexer);
        if (node.data.statementStatement == SIZE_MAX) {
            *success = false;
        }
        list->nodes[curr].data.statementNext = addNode(list, node);
        curr = list->nodes[curr].data.statementNext;
    }
    list->nodes[curr].data.statementNext = SIZE_MAX;
    return head;
}

size_t parseScope(AST_Node_List* list, Lexer* lexer) {
    AST_Node node;
    node.type = NODE_SCOPE;

    bool success;
    node.data.scopeStatements = parseStatements(list, lexer, &success);

    return success ? addNode(list, node) : SIZE_MAX;
}

size_t parseArgs(AST_Node_List* list, Lexer* lexer, bool* success) {
    *success = true;

    Token token = getToken(lexer);
    if (token.type == TOKEN_RPAREN) {
        return SIZE_MAX;
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
    if (token.type != TOKEN_INTTYPE_KEYWORD) {
        printErrorMessage(lexer->fileName, scopeToken(token), "Expected a type name in argument list, got \""SV_FMT"\"", SV_ARG(token.text));
        recoverByEatUntil(lexer, TOKEN_INTTYPE_KEYWORD);
        *success = false;
    }
    String_View type = token.text;
    size_t head = addArgNode(list, name, type);
    size_t curr = head;

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
        
        list->nodes[curr].data.argNext = addArgNode(list, name, type);
        curr = list->nodes[curr].data.argNext;

        token = getToken(lexer);
    }
    list->nodes[curr].data.argNext = SIZE_MAX;
    return head;
}

size_t parseFunction(AST_Node_List* list, Lexer* lexer) {
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
    if (token.type != TOKEN_DCOLON) {
        printErrorMessage(lexer->fileName, scopeToken(token), "Expected \"::\" in function definition, got \""SV_FMT"\"", SV_ARG(token.text));
        recoverByEatUntil(lexer, TOKEN_DCOLON);
        success = false;
    }

    token = getToken(lexer);
    if (token.type != TOKEN_FUNC_KEYWORD) {
        printErrorMessage(lexer->fileName, scopeToken(token), "Expected \"func\" keyword in function definition, got \""SV_FMT"\"", SV_ARG(token.text));
        recoverByEatUntil(lexer, TOKEN_FUNC_KEYWORD);
        success = false;
    }

    token = getToken(lexer);
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

    token = getToken(lexer);
    if (token.type == TOKEN_ARROW) {
        token = getToken(lexer);
        if (token.type != TOKEN_INTTYPE_KEYWORD) {
            printErrorMessage(lexer->fileName, scopeToken(token), "Expected a return type in function definition, got \""SV_FMT"\"", SV_ARG(token.text));
            recoverByEatUpTo(lexer, TOKEN_LBRACE);
            success = false;
        }
        node.data.functionRetType = token.text;
        token = getToken(lexer);
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
    if (node.data.functionBody == SIZE_MAX) {
        success = false;
    }

    return success ? addNode(list, node) : SIZE_MAX;
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

void printASTIndented(int indent, AST_Node_List list, size_t root) {
    AST_Node node = list.nodes[root];
    switch (node.type) {
        case NODE_FUNCTION: {
            printIndented(indent, "type=FUNCTION, name="SV_FMT", rettype="SV_FMT", args=", SV_ARG(node.data.functionName), SV_ARG(node.data.functionRetType));
            if (node.data.functionArgs == SIZE_MAX) {
                printf("NONE, body=");
            }
            else {
                printf("(\n");
                printASTIndented(indent + 1, list, node.data.functionArgs);
                printIndented(indent, "), body=");
            }
            if (node.data.functionBody == SIZE_MAX) {
                printf("NONE\n");
            }
            else {
                printf("(\n");
                printASTIndented(indent + 1, list, node.data.functionBody);
                printIndented(indent, ")\n");
            }
            break;
        }
        case NODE_ARGS: {
            printIndented(indent, "name="SV_FMT", type="SV_FMT"\n", SV_ARG(node.data.argName), SV_ARG(node.data.argType));
            while (node.data.argNext != SIZE_MAX) {
                node = list.nodes[node.data.argNext];
                printIndented(indent, "name="SV_FMT", type="SV_FMT"\n", SV_ARG(node.data.argName), SV_ARG(node.data.argType));
            }
            break;
        }
        case NODE_SCOPE: {
            printIndented(indent, "type=SCOPE, statements=");
            if (node.data.scopeStatements == SIZE_MAX) {
                printf("NONE\n");
            }
            else {
                printf("(\n");
                printASTIndented(indent + 1, list, node.data.scopeStatements);
                printIndented(indent, ")\n");
            }
            break;
        }
        case NODE_STATEMENTS: {
            printASTIndented(indent, list, node.data.statementStatement);
            while (node.data.statementNext != SIZE_MAX) {
                node = list.nodes[node.data.statementNext];
                printASTIndented(indent, list, node.data.statementStatement);
            }
            break;
        }
        case NODE_RETURN: {
            printIndented(indent, "type=RETURN, expr=");
            if (node.data.returnExpr == SIZE_MAX) {
                printf("NONE\n");
            }
            else {
                printf("(\n");
                printASTIndented(indent + 1, list, node.data.returnExpr);
                printIndented(indent, ")\n");
            }
            break;
        }
        case NODE_DECLARATION: {
            printIndented(indent, "type=DECLARATION, name="SV_FMT", type="SV_FMT"\n", SV_ARG(node.data.declarationName), SV_ARG(node.data.declarationType));
            break;
        }
        case NODE_ASSIGNMENT: {
            printIndented(indent, "type=ASSIGNMENT, name="SV_FMT", expr=(\n", SV_ARG(node.data.declarationName));
            printASTIndented(indent + 1, list, node.data.assignmentExpr);
            printIndented(indent, ")\n");
            break;
        }
        case NODE_PLUS: {
            printIndented(indent, "type=PLUS, left=(\n");
            printASTIndented(indent + 1, list, node.data.binaryOpLeft);
            printIndented(indent, "), right=(\n");
            printASTIndented(indent + 1, list, node.data.binaryOpRight);
            printIndented(indent, ")\n");
            break;
        }
        case NODE_MINUS: {
            printIndented(indent, "type=MINUS, left=(\n");
            printASTIndented(indent + 1, list, node.data.binaryOpLeft);
            printIndented(indent, "), right=(\n");
            printASTIndented(indent + 1, list, node.data.binaryOpRight);
            printIndented(indent, ")\n");
            break;
        }
        case NODE_TIMES: {
            printIndented(indent, "type=TIMES, left=(\n");
            printASTIndented(indent + 1, list, node.data.binaryOpLeft);
            printIndented(indent, "), right=(\n");
            printASTIndented(indent + 1, list, node.data.binaryOpRight);
            printIndented(indent, ")\n");
            break;
        }
        case NODE_DIVIDE: {
            printIndented(indent, "type=DIVIDE, left=(\n");
            printASTIndented(indent + 1, list, node.data.binaryOpLeft);
            printIndented(indent, "), right=(\n");
            printASTIndented(indent + 1, list, node.data.binaryOpRight);
            printIndented(indent, ")\n");
            break;
        }
        case NODE_INT: {
            printIndented(indent, "type=INT, value=%d\n", node.data.intValue);
            break;
        }
        case NODE_IDENT: {
            printIndented(indent, "type=IDENT, name="SV_FMT"\n", SV_ARG(node.data.identName));
            break;
        }
        default:
            printf("Unknown node type: %d\n", node.type);
            assert(false && "Non-exhaustive cases in printASTIndented");
    }
}

inline void printAST(AST_Node_List list, size_t root) {
    printASTIndented(0, list, root);
}



/////////////////
// Emitter API //
/////////////////

void emitTerm(FILE* file, AST_Node_List list, size_t root);
void emitExpr(FILE* file, AST_Node_List list, size_t root, int precedence);
void emitStatement(int indent, FILE* file, AST_Node_List list, size_t root);
void emitStatements(int indent, FILE* file, AST_Node_List list, size_t root);
void emitScope(int indent, FILE* file, AST_Node_List list, size_t root);
void emitFunction(FILE* file, AST_Node_List list, size_t root);

void emitTerm(FILE* file, AST_Node_List list, size_t root) {
    AST_Node term = list.nodes[root];
    switch (term.type) {
        case NODE_INT: {
            tryFPrintf(file, "%d", term.data.intValue);
            break;
        }
        case NODE_IDENT: {
            tryFPrintf(file, SV_FMT, SV_ARG(term.data.identName));
            break;
        }
        default:
            printf("Unknown node term type: %d\n", term.type);
            assert(false && "Called with a non-term node or non-exhaustive cases (emitTerm)");
    }
}

void emitExpr(FILE* file, AST_Node_List list, size_t root, int precedence) {
    AST_Node expr = list.nodes[root];
    if (isNodeOperator(expr.type)) {
        int thisPrecedence = getNodePrecedence(expr.type);
        if (thisPrecedence < precedence) {
            tryFPuts("(", file);
        }

        switch (expr.type) {
            case NODE_PLUS: {
                emitExpr(file, list, expr.data.binaryOpLeft, thisPrecedence);
                tryFPuts(" + ", file);
                emitExpr(file, list, expr.data.binaryOpRight, thisPrecedence);
                break;
            }
            case NODE_MINUS: {
                emitExpr(file, list, expr.data.binaryOpLeft, thisPrecedence);
                tryFPuts(" - ", file);
                emitExpr(file, list, expr.data.binaryOpRight, thisPrecedence);
                break;
            }
            case NODE_TIMES: {
                emitExpr(file, list, expr.data.binaryOpLeft, thisPrecedence);
                tryFPuts(" * ", file);
                emitExpr(file, list, expr.data.binaryOpRight, thisPrecedence);
                break;
            }
            case NODE_DIVIDE: {
                emitExpr(file, list, expr.data.binaryOpLeft, thisPrecedence);
                tryFPuts(" / ", file);
                emitExpr(file, list, expr.data.binaryOpRight, thisPrecedence);
                break;
            }
        }

        if (thisPrecedence < precedence) {
            tryFPuts(")", file);
        }
    }
    else {
        emitTerm(file, list, root);
    }
}

void emitStatement(int indent, FILE* file, AST_Node_List list, size_t root) {
    AST_Node statement = list.nodes[root];
    switch (statement.type) {
        case NODE_RETURN: {
            tryFPutsIndented(indent, "return ", file);
            emitExpr(file, list, statement.data.returnExpr, -1);
            break;
        }
        case NODE_DECLARATION: {
            tryFPrintfIndented(indent, file, SV_FMT" "SV_FMT, SV_ARG(statement.data.declarationType), SV_ARG(statement.data.declarationName));
            break;
        }
        case NODE_ASSIGNMENT: {
            tryFPrintfIndented(indent, file, SV_FMT" = ", SV_ARG(statement.data.assignmentName));
            emitExpr(file, list, statement.data.assignmentExpr, -1);
            break;
        }
        default:
            printf("Unexpected node type: %d\n", statement.type);
            assert(false && "Not a statement type or non-exhaustive cases (emitStatement)");
    }
    tryFPuts(";\n", file);
}

void emitStatements(int indent, FILE* file, AST_Node_List list, size_t root) {
    if (root == SIZE_MAX) {
        return;
    }
    do {
        AST_Node statements = list.nodes[root];
        assert(statements.type == NODE_STATEMENTS);

        emitStatement(indent, file, list, statements.data.statementStatement);

        root = statements.data.statementNext;
    } while (root != SIZE_MAX);
}

void emitScope(int indent, FILE* file, AST_Node_List list, size_t root) {
    AST_Node scope = list.nodes[root];
    assert(scope.type == NODE_SCOPE);

    tryFPutsIndented(indent, "{\n", file);
    emitStatements(indent + 1, file, list, scope.data.scopeStatements);
    tryFPutsIndented(indent, "}\n", file);
}

void emitFunction(FILE* file, AST_Node_List list, size_t root) {
    AST_Node function = list.nodes[root];
    assert(function.type == NODE_FUNCTION);

    tryFPuts("int ", file);
    svTryFWrite(function.data.functionName, file);
    tryFPuts("() ", file);
    emitScope(0, file, list, function.data.functionBody);
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
    AST_Node_List list = makeNodeList(8);
    size_t function = parseFunction(&list, &lexer);
    if (function == SIZE_MAX) {
        return 1;
    }
    printAST(list, function);
    FILE* output = tryFOpen("examples/simple.c", "wb");
    emitFunction(output, list, function);
    return 0;
}
