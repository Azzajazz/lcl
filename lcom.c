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

inline void svTryFWrite(String_View sv, FILE* file) {
    tryFWrite(sv.start, 1, sv.length, file);
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
    TOKEN_DCOLON,
    TOKEN_SEMICOLON,

    // Keyword tokens
    TOKEN_FUNC_KEYWORD,
    TOKEN_RETURN_KEYWORD,

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

typedef struct {
    char* code;
    char* fileName;
    int lineNum;
    int charNum;
    String_View line;
} Lexer;

Lexer makeLexer(char* code, char* fileName) {
    return (Lexer){
        .code = code,
        .fileName = fileName,
        .lineNum = 0,
        .charNum = 0,
        .line = svUntil('\n', code)
    };
}

void lexerAdvance(Lexer* lexer, size_t steps) {
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

inline Token makeToken(Lexer* lexer, Token_Type type, size_t textLength) {
    return (Token){
        .type = type,
        .lineNum = lexer->lineNum,
        .charNum = lexer->charNum,
        .line = lexer->line,
        .text = (String_View){lexer->code, textLength},
    };
}

bool isLexemeTerminator(char c) {
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
    size_t intLength = 0;
    bool negative = false;
    if (*lexer->code == '+') {
        intLength++;
    }
    else if (*lexer->code == '-') {
        negative = true;
        intLength++;
    }
    bool zero = lexer->code[intLength == '0'] && isLexemeTerminator(lexer->code[intLength + 1]);
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
    }
    return (Token){TOKEN_ERROR};
}

Token getIdentToken(Lexer* lexer) {
    size_t identLength = 0;
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
            if (lexer->code[1] == ':') {
                Token result = makeToken(lexer, TOKEN_DCOLON, 2);
                lexerAdvance(lexer, 2);
                return result;
            }
            //TODO: This is where the lexer will need error handling
            assert(lexer->code[1] == ':');
            break;
        }
        case ';': {
            Token result = makeToken(lexer, TOKEN_SEMICOLON, 1);
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
    assert(false && "unreachable");
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
        case TOKEN_SEMICOLON:
            typeString = "SEMICOLON";
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

typedef enum {
    NODE_FUNCTION,
    NODE_ARGS,          // Argument lists to functions
    NODE_SCOPE,         // Lists of expressions wrapped in braces.

    // Expressions
    NODE_STATEMENTS,    // Linked lists of statements
    NODE_RETURN,

    // Literals
    NODE_INT,
} Node_Type;

typedef union {
    struct {                 // NODE_FUNCTION
        String_View functionName;
        size_t functionArgs;
        size_t functionBody;
    };
    // Represents a linked list of function arguments.
    struct {                 // NODE_ARGS
        String_View argArg;
        size_t argNext;
    };
    size_t scopeStatements;  // NODE_SCOPE
    // Represents a linked list of expressions in a scope.
    struct {                 // NODE_STATEMENTS
        size_t statementStatement;
        size_t statementNext;
    };
    size_t returnExpr;       // NODE_RETURN
    String_View identName;   // NODE_IDENT
    int intValue;            // NODE_INT
} Node_Data;

typedef struct {
    Node_Type type;
    Node_Data data;
} AST_Node;

typedef struct {
    AST_Node* nodes;
    size_t nodeCount;
    size_t capacity;
} AST_Node_List;

AST_Node_List makeNodeList(size_t capacity) {
    return (AST_Node_List){
        .nodes = malloc(capacity * sizeof(AST_Node)),
        .nodeCount = 0,
        .capacity = capacity
    };
}

size_t addNode(AST_Node_List* list, AST_Node node) {
    if (list->nodeCount == list->capacity) {
        list->capacity *= 2;
        list->nodes = realloc(list->nodes, list->capacity * sizeof(AST_Node));
    }
    list->nodes[list->nodeCount++] = node;
    return list->nodeCount - 1;
}

size_t parseExpr(AST_Node_List* list, Lexer* lexer);
size_t parseStatement(AST_Node_List* list, Lexer* lexer);
size_t parseStatments(AST_Node_List* list, Lexer* lexer, bool* success);
size_t parseScope(AST_Node_List* list, Lexer* lexer);
size_t parseArgs(AST_Node_List* list, Lexer* lexer, bool* success);
size_t parseFunction(AST_Node_List* list, Lexer* lexer);

size_t parseExpr(AST_Node_List* list, Lexer* lexer) {
    Token token = getToken(lexer);
    switch (token.type) {
        case TOKEN_INT: {
            //TODO: Look to see if there is an operator
            AST_Node node;
            node.type = NODE_INT;
            node.data.intValue = token.intValue;
            return addNode(list, node);
        }
        default:
            printf("Unexpected token type: %d\n", token.type);
            assert(false && "This isn't an expression (parseExpr)");
    }
}

size_t parseStatement(AST_Node_List* list, Lexer* lexer) {
    Token token = getToken(lexer);
    switch (token.type) {
        case TOKEN_RETURN_KEYWORD: {
            AST_Node node;
            node.type = NODE_RETURN;
            node.data.returnExpr = parseExpr(list, lexer);
            size_t result = addNode(list, node);
            token = getToken(lexer);
            assert(token.type == TOKEN_SEMICOLON);
            return result;
        }
        default:
            assert(false && "This isn't a statement (parseStatement)");
    }
}

size_t parseStatements(AST_Node_List* list, Lexer* lexer, bool* success) {
    if (peekToken(lexer).type == TOKEN_RBRACE) {
        getToken(lexer); // Eat the '}'
        *success = true;
        return SIZE_MAX;
    }

    AST_Node node;
    node.type = NODE_STATEMENTS;
    node.data.statementStatement = parseStatement(list, lexer);
    assert(node.data.statementStatement != SIZE_MAX);
    size_t head = addNode(list, node);
    size_t curr = head;

    while (peekToken(lexer).type != TOKEN_RBRACE) {
        node.data.statementStatement = parseStatement(list, lexer);
        list->nodes[curr].data.statementNext = addNode(list, node);
        curr = list->nodes[curr].data.statementNext;
    }
    list->nodes[curr].data.statementNext = SIZE_MAX;
    *success = true;
    return head;
}

size_t parseScope(AST_Node_List* list, Lexer* lexer) {
    AST_Node node;
    node.type = NODE_SCOPE;

    bool success;
    node.data.scopeStatements = parseStatements(list, lexer, &success);
    assert(success);

    return addNode(list, node);
}

size_t parseArgs(AST_Node_List* list, Lexer* lexer, bool* success) {
    Token token = getToken(lexer);
    if (token.type == TOKEN_RPAREN) {
        *success = true;
        return SIZE_MAX;
    }
    assert(token.type == TOKEN_IDENT);

    AST_Node node;
    node.type = NODE_ARGS;
    node.data.argArg = token.text;
    size_t head = addNode(list, node);
    size_t curr = head;

    token = getToken(lexer);
    while (token.type != TOKEN_RPAREN) {
        assert(token.type == TOKEN_COMMA);

        token = getToken(lexer);
        assert(token.type == TOKEN_IDENT);
        
        node.data.argArg = token.text;
        list->nodes[curr].data.argNext = addNode(list, node);
        curr = list->nodes[curr].data.argNext;

        token = getToken(lexer);
    }
    list->nodes[curr].data.argNext = SIZE_MAX;
    *success = true;
    return head;
}

size_t parseFunction(AST_Node_List* list, Lexer* lexer) {
    AST_Node result;
    result.type = NODE_FUNCTION;

    Token token = getToken(lexer);
    assert(token.type == TOKEN_IDENT);
    result.data.functionName = token.text;

    token = getToken(lexer);
    assert(token.type == TOKEN_DCOLON);

    token = getToken(lexer);
    assert(token.type == TOKEN_FUNC_KEYWORD);

    token = getToken(lexer);
    assert(token.type == TOKEN_LPAREN);

    bool success;
    result.data.functionArgs = parseArgs(list, lexer, &success);
    assert(success);

    token = getToken(lexer);
    assert(token.type == TOKEN_LBRACE);

    result.data.functionBody = parseScope(list, lexer);
    assert(success);

    return addNode(list, result);
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
            printIndented(indent, "type=FUNCTION, name="SV_FMT", args=", SV_ARG(node.data.functionName));
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
            printIndented(indent, SV_FMT",\n", SV_ARG(node.data.argArg));
            while (node.data.argNext != SIZE_MAX) {
                node = list.nodes[node.data.argNext];
                printIndented(indent, SV_FMT",\n", SV_ARG(node.data.argArg));
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
        case NODE_INT: {
            printIndented(indent, "type=INT, value=%d\n", node.data.intValue);
            break;
        }
        default:
            printf("%d\n", node.type);
            assert(false && "Non-exhaustive cases in printASTIndented");
    }
}

void printAST(AST_Node_List list, size_t root) {
    printASTIndented(0, list, root);
}



/////////////////
// Emitter API //
/////////////////

void emitExpr(FILE* file, AST_Node_List list, size_t root);
void emitStatement(int indent, FILE* file, AST_Node_List list, size_t root);
void emitStatements(int indent, FILE* file, AST_Node_List list, size_t root);
void emitScope(int indent, FILE* file, AST_Node_List list, size_t root);
void emitFunction(FILE* file, AST_Node_List list, size_t root);

void emitExpr(FILE* file, AST_Node_List list, size_t root) {
    AST_Node expr = list.nodes[root];
    switch (expr.type) {
        case NODE_INT: {
            tryFPrintf(file, "%d", expr.data.intValue);
            break;
        }
        default:
            printf("Unexpected node type: %d\n", expr.type);
            assert(false && "This is not an expression! (emitExpr)");
    }
}

void emitStatement(int indent, FILE* file, AST_Node_List list, size_t root) {
    AST_Node statement = list.nodes[root];
    switch (statement.type) {
        case NODE_RETURN: {
            tryFPutsIndented(indent, "return ", file);
            emitExpr(file, list, statement.data.returnExpr);
            break;
        }
        default:
            printf("Unexpected node type: %d\n", statement.type);
            assert(false && "This is not a statement! (emitStatement)");
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
    char* fileName = "simple.lcl";
    char* code = readEntireFile(fileName);
    Lexer lexer = makeLexer(code, fileName);
    AST_Node_List list = makeNodeList(8);
    size_t function = parseFunction(&list, &lexer);
    if (function == SIZE_MAX) {
        return 1;
    }
    FILE* output = tryFOpen("simple.c", "wb");
    emitFunction(output, list, function);
    return 0;
}
