/*
 * AtomC Compiler — single-file implementation
 *
 * Sections:
 *   1. Includes & token definitions
 *   2. Data structures (Token, Symbol, Symbols, Type)
 *   3. Lexical Analyzer
 *   4. Syntactic Analyzer
 *   5. Domain Analyzer
 *   6. main
 */

/* ═══════════════════════════════════════════════════════════
 * 1. INCLUDES & TOKEN CODES
 * ═══════════════════════════════════════════════════════════ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

enum {
    ID, CT_INT, CT_REAL, CT_CHAR, CT_STRING,
    BREAK, CHAR, DOUBLE, ELSE, FOR, IF, INT, RETURN, STRUCT, VOID, WHILE,
    ADD, SUB, MUL, DIV, DOT,
    AND, OR, NOT,
    ASSIGN, EQUAL, NOTEQ,
    LESS, LESSEQ, GREATER, GREATEREQ,
    COMMA, SEMICOLON,
    LPAR, RPAR,
    LBRACKET, RBRACKET,
    LACC, RACC,
    END
};

/* ═══════════════════════════════════════════════════════════
 * 2. DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════ */

/* ── Token ───────────────────────────────────────────────── */
typedef struct _Token {
    int code;
    union {
        char    *text;
        long int i;
        double   r;
    };
    int line;
    struct _Token *next;
} Token;

/* ── Type base codes ─────────────────────────────────────── */
enum { TB_INT, TB_DOUBLE, TB_CHAR, TB_STRUCT, TB_VOID };

/* ── Symbol class / memory codes ────────────────────────── */
enum { CLS_VAR, CLS_FUNC, CLS_EXTFUNC, CLS_STRUCT };
enum { MEM_GLOBAL, MEM_ARG, MEM_LOCAL };

/* Forward declaration */
struct _Symbol;
typedef struct _Symbol Symbol;

/* ── Dynamic array of Symbol pointers ───────────────────── */
typedef struct {
    Symbol **begin;
    Symbol **end;
    Symbol **after;
} Symbols;

/* ── Type descriptor ─────────────────────────────────────── */
typedef struct {
    int     tb;   /* TB_*                                        */
    Symbol *s;    /* struct definition when tb == TB_STRUCT      */
    int     n;    /* >=0 : array; 0=unsized; <0 : not an array  */
} Type;

/* ── Symbol descriptor ───────────────────────────────────── */
struct _Symbol {
    const char *name;
    int         cls;    /* CLS_*  */
    int         mem;    /* MEM_*  */
    Type        type;
    int         depth;  /* 0=global, 1=fn body, 2+=nested blocks */
    Symbol     *owner;  /* enclosing fn/struct, or NULL          */
    union {
        Symbols args;    /* CLS_FUNC   : parameter list */
        Symbols members; /* CLS_STRUCT : member list    */
    };
};

/* ═══════════════════════════════════════════════════════════
 * 3. LEXICAL ANALYZER
 * ═══════════════════════════════════════════════════════════ */

Token      *tokens    = NULL;
static Token      *lastToken = NULL;
static const char *pCrtCh    = NULL;
static int         line      = 1;

#define SAFEALLOC(var, Type) \
    if (((var) = (Type*)malloc(sizeof(Type))) == NULL) err("not enough memory")

void err(const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    fprintf(stderr, "error: ");
    vfprintf(stderr, fmt, va);
    fputc('\n', stderr);
    va_end(va);
    exit(-1);
}

void tkerr(const Token *tk, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    fprintf(stderr, "error in line %d: ", tk->line);
    vfprintf(stderr, fmt, va);
    fputc('\n', stderr);
    va_end(va);
    exit(-1);
}

Token *addTk(int code) {
    Token *tk;
    SAFEALLOC(tk, Token);
    tk->code = code;
    tk->line = line;
    tk->next = NULL;
    if (lastToken) lastToken->next = tk;
    else           tokens = tk;
    lastToken = tk;
    return tk;
}

char *createString(const char *start, const char *end) {
    size_t len = (size_t)(end - start);
    char *s = (char*)malloc(len + 1);
    if (!s) err("not enough memory");
    memcpy(s, start, len);
    s[len] = '\0';
    return s;
}

int getNextToken(void) {
    int state = 0;
    char ch;
    const char *pStartCh = NULL;
    Token *tk;

    while (1) {
        ch = *pCrtCh;
        switch (state) {

        case 0:
            if (ch == ' ' || ch == '\t' || ch == '\r') {
                pCrtCh++;
            } else if (ch == '\n') {
                line++;
                pCrtCh++;
            }
            else if (isalpha((unsigned char)ch) || ch == '_') {
                pStartCh = pCrtCh++;
                state = 100;
            }
            else if (ch == '0') {
                pStartCh = pCrtCh++;
                state = 11;
            } else if (ch >= '1' && ch <= '9') {
                pStartCh = pCrtCh++;
                state = 3;
            }
            else if (ch == '\'') {
                pCrtCh++;
                state = 12;
            } else if (ch == '"') {
                pStartCh = pCrtCh + 1;
                pCrtCh++;
                state = 15;
            }
            else if (ch == '+')  { pCrtCh++; tk = addTk(ADD);  return ADD;  }
            else if (ch == '-')  { pCrtCh++; tk = addTk(SUB);  return SUB;  }
            else if (ch == '*')  { pCrtCh++; tk = addTk(MUL);  return MUL;  }
            else if (ch == '.')  { pCrtCh++; tk = addTk(DOT);  return DOT;  }
            else if (ch == '!') { pCrtCh++; state = 200; }
            else if (ch == '&') { pCrtCh++; state = 210; }
            else if (ch == '|') { pCrtCh++; state = 220; }
            else if (ch == '=') { pCrtCh++; state = 230; }
            else if (ch == '<') { pCrtCh++; state = 240; }
            else if (ch == '>') { pCrtCh++; state = 250; }
            else if (ch == '/') { pCrtCh++; state = 300; }
            else if (ch == ',')  { pCrtCh++; addTk(COMMA);     return COMMA;     }
            else if (ch == ';')  { pCrtCh++; addTk(SEMICOLON); return SEMICOLON; }
            else if (ch == '(')  { pCrtCh++; addTk(LPAR);      return LPAR;      }
            else if (ch == ')')  { pCrtCh++; addTk(RPAR);      return RPAR;      }
            else if (ch == '[')  { pCrtCh++; addTk(LBRACKET);  return LBRACKET;  }
            else if (ch == ']')  { pCrtCh++; addTk(RBRACKET);  return RBRACKET;  }
            else if (ch == '{')  { pCrtCh++; addTk(LACC);      return LACC;      }
            else if (ch == '}')  { pCrtCh++; addTk(RACC);      return RACC;      }
            else if (ch == '\0') { addTk(END); return END; }
            else { tkerr(addTk(END), "invalid character '%c'", ch); }
            break;

        case 100:
            if (isalnum((unsigned char)ch) || ch == '_') pCrtCh++;
            else state = 101;
            break;
        case 101: {
            int nCh = (int)(pCrtCh - pStartCh);
            if      (nCh==5  && !memcmp(pStartCh,"break",  5)) tk=addTk(BREAK);
            else if (nCh==4  && !memcmp(pStartCh,"char",   4)) tk=addTk(CHAR);
            else if (nCh==6  && !memcmp(pStartCh,"double", 6)) tk=addTk(DOUBLE);
            else if (nCh==4  && !memcmp(pStartCh,"else",   4)) tk=addTk(ELSE);
            else if (nCh==3  && !memcmp(pStartCh,"for",    3)) tk=addTk(FOR);
            else if (nCh==2  && !memcmp(pStartCh,"if",     2)) tk=addTk(IF);
            else if (nCh==3  && !memcmp(pStartCh,"int",    3)) tk=addTk(INT);
            else if (nCh==6  && !memcmp(pStartCh,"return", 6)) tk=addTk(RETURN);
            else if (nCh==6  && !memcmp(pStartCh,"struct", 6)) tk=addTk(STRUCT);
            else if (nCh==4  && !memcmp(pStartCh,"void",   4)) tk=addTk(VOID);
            else if (nCh==5  && !memcmp(pStartCh,"while",  5)) tk=addTk(WHILE);
            else {
                tk = addTk(ID);
                tk->text = createString(pStartCh, pCrtCh);
            }
            return tk->code;
        }

        case 3:
            if (isdigit((unsigned char)ch)) pCrtCh++;
            else if (ch == '.') { pCrtCh++; state = 5; }
            else if (ch=='e'||ch=='E') { pCrtCh++; state = 8; }
            else state = 4;
            break;
        case 4:
            tk = addTk(CT_INT);
            tk->i = strtol(pStartCh, NULL, 10);
            return CT_INT;

        case 5:
            if (isdigit((unsigned char)ch)) { pCrtCh++; state = 6; }
            else tkerr(addTk(END), "invalid real constant");
            break;
        case 6:
            if (isdigit((unsigned char)ch)) pCrtCh++;
            else if (ch=='e'||ch=='E') { pCrtCh++; state = 8; }
            else state = 7;
            break;
        case 7:
            tk = addTk(CT_REAL);
            tk->r = atof(pStartCh);
            return CT_REAL;

        case 8:
            if (ch=='+'||ch=='-') { pCrtCh++; state = 9; }
            else if (isdigit((unsigned char)ch)) { pCrtCh++; state = 10; }
            else tkerr(addTk(END), "invalid exponent in real constant");
            break;
        case 9:
            if (isdigit((unsigned char)ch)) { pCrtCh++; state = 10; }
            else tkerr(addTk(END), "invalid exponent in real constant");
            break;
        case 10:
            if (isdigit((unsigned char)ch)) pCrtCh++;
            else {
                tk = addTk(CT_REAL);
                tk->r = atof(pStartCh);
                return CT_REAL;
            }
            break;

        case 11:
            if (ch=='x'||ch=='X') { pCrtCh++; state = 17; }
            else if (ch>='0'&&ch<='7') { pCrtCh++; state = 20; }
            else if (ch=='.') { pCrtCh++; state = 5; }
            else if (ch=='e'||ch=='E') { pCrtCh++; state = 8; }
            else {
                tk = addTk(CT_INT);
                tk->i = 0;
                return CT_INT;
            }
            break;

        case 17:
            if (isxdigit((unsigned char)ch)) { pCrtCh++; state = 18; }
            else tkerr(addTk(END), "invalid hex constant");
            break;
        case 18:
            if (isxdigit((unsigned char)ch)) pCrtCh++;
            else state = 19;
            break;
        case 19:
            tk = addTk(CT_INT);
            tk->i = strtol(pStartCh, NULL, 16);
            return CT_INT;

        case 20:
            if (ch>='0'&&ch<='7') pCrtCh++;
            else state = 21;
            break;
        case 21:
            if (ch>='8'&&ch<='9')
                tkerr(addTk(END), "invalid octal digit '%c'", ch);
            tk = addTk(CT_INT);
            tk->i = strtol(pStartCh, NULL, 8);
            return CT_INT;

        case 12:
            if (ch == '\'' || ch == '\0' || ch == '\n')
                tkerr(addTk(END), "empty or invalid char constant");
            pStartCh = pCrtCh;
            if (ch == '\\') { pCrtCh += 2; }
            else             { pCrtCh++;    }
            state = 13;
            break;
        case 13:
            if (ch == '\'') { pCrtCh++; state = 14; }
            else tkerr(addTk(END), "missing closing quote in char constant");
            break;
        case 14:
            tk = addTk(CT_CHAR);
            tk->i = (long int)(unsigned char)*pStartCh;
            return CT_CHAR;

        case 15:
            if (ch == '"')       { pCrtCh++; state = 16; }
            else if (ch == '\0') tkerr(addTk(END), "unterminated string");
            else if (ch == '\\') { pCrtCh += 2; }
            else                 { pCrtCh++;    }
            break;
        case 16:
            tk = addTk(CT_STRING);
            tk->text = createString(pStartCh, pCrtCh - 1);
            return CT_STRING;

        case 200:
            if (ch == '=') { pCrtCh++; addTk(NOTEQ); return NOTEQ; }
            else { addTk(NOT); return NOT; }

        case 210:
            if (ch == '&') { pCrtCh++; addTk(AND); return AND; }
            else tkerr(addTk(END), "expected '&&'");
            break;

        case 220:
            if (ch == '|') { pCrtCh++; addTk(OR); return OR; }
            else tkerr(addTk(END), "expected '||'");
            break;

        case 230:
            if (ch == '=') { pCrtCh++; addTk(EQUAL); return EQUAL; }
            else { addTk(ASSIGN); return ASSIGN; }

        case 240:
            if (ch == '=') { pCrtCh++; addTk(LESSEQ); return LESSEQ; }
            else { addTk(LESS); return LESS; }

        case 250:
            if (ch == '=') { pCrtCh++; addTk(GREATEREQ); return GREATEREQ; }
            else { addTk(GREATER); return GREATER; }

        case 300:
            if (ch == '/') { pCrtCh++; state = 301; }
            else { addTk(DIV); return DIV; }
            break;
        case 301:
            if (ch == '\n' || ch == '\0') state = 0;
            else pCrtCh++;
            break;

        default:
            err("unknown state %d", state);
        }
    }
}

static const char *tokenName(int code) {
    switch (code) {
    case ID:         return "ID";
    case CT_INT:     return "CT_INT";
    case CT_REAL:    return "CT_REAL";
    case CT_CHAR:    return "CT_CHAR";
    case CT_STRING:  return "CT_STRING";
    case BREAK:      return "BREAK";
    case CHAR:       return "CHAR";
    case DOUBLE:     return "DOUBLE";
    case ELSE:       return "ELSE";
    case FOR:        return "FOR";
    case IF:         return "IF";
    case INT:        return "INT";
    case RETURN:     return "RETURN";
    case STRUCT:     return "STRUCT";
    case VOID:       return "VOID";
    case WHILE:      return "WHILE";
    case ADD:        return "ADD";
    case SUB:        return "SUB";
    case MUL:        return "MUL";
    case DIV:        return "DIV";
    case DOT:        return "DOT";
    case AND:        return "AND";
    case OR:         return "OR";
    case NOT:        return "NOT";
    case ASSIGN:     return "ASSIGN";
    case EQUAL:      return "EQUAL";
    case NOTEQ:      return "NOTEQ";
    case LESS:       return "LESS";
    case LESSEQ:     return "LESSEQ";
    case GREATER:    return "GREATER";
    case GREATEREQ:  return "GREATEREQ";
    case COMMA:      return "COMMA";
    case SEMICOLON:  return "SEMICOLON";
    case LPAR:       return "LPAR";
    case RPAR:       return "RPAR";
    case LBRACKET:   return "LBRACKET";
    case RBRACKET:   return "RBRACKET";
    case LACC:       return "LACC";
    case RACC:       return "RACC";
    case END:        return "END";
    default:         return "UNKNOWN";
    }
}

void showTokens(void) {
    for (Token *tk = tokens; tk; tk = tk->next) {
        printf("line %d  %-12s", tk->line, tokenName(tk->code));
        if (tk->code == ID || tk->code == CT_STRING)
            printf("  \"%s\"", tk->text);
        else if (tk->code == CT_INT || tk->code == CT_CHAR)
            printf("  %ld", tk->i);
        else if (tk->code == CT_REAL)
            printf("  %g", tk->r);
        printf("\n");
    }
}

void freeTokens(void) {
    Token *tk = tokens;
    while (tk) {
        Token *next = tk->next;
        if (tk->code == ID || tk->code == CT_STRING)
            free(tk->text);
        free(tk);
        tk = next;
    }
    tokens = lastToken = NULL;
}

char *loadFile(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) err("cannot open file: %s", path);
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *buf = (char*)malloc(sz + 1);
    if (!buf) err("not enough memory");
    if (fread(buf, 1, sz, f) != (size_t)sz) err("read error");
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

void tokenize(const char *src, int show_output) {
    pCrtCh = src;
    line = 1;
    while (getNextToken() != END) {}
    if (show_output) showTokens();
}

/* ═══════════════════════════════════════════════════════════
 * 4. SYNTACTIC ANALYZER
 * ═══════════════════════════════════════════════════════════ */

static Token *crtTk;
static Token *consumedTk;

static int consume(int code) {
    if (crtTk->code == code) {
        consumedTk = crtTk;
        crtTk = crtTk->next;
        return 1;
    }
    return 0;
}

/* Forward declarations for syntactic rules */
int syn_expr(void);
int syn_stm(void);
int syn_stmCompound(void);
int syn_typeBase(Type *t);
int syn_arrayDecl(Type *t);
int syn_exprAssign(void);
int syn_exprOr(void);
int syn_exprAnd(void);
int syn_exprEq(void);
int syn_exprRel(void);
int syn_exprAdd(void);
int syn_exprMul(void);
int syn_exprCast(void);
int syn_exprUnary(void);
int syn_exprPostfix(void);
int syn_exprPrimary(void);
int syn_varDef(void);
int syn_fnParam(void);
int syn_fnDef(void);
int syn_structDef(void);

int syn_typeBase(Type *t) {
    t->n = -1; /* default: not an array */
    Token *startTk = crtTk;
    if (consume(INT))    { t->tb = TB_INT;    return 1; }
    if (consume(DOUBLE)) { t->tb = TB_DOUBLE; return 1; }
    if (consume(CHAR))   { t->tb = TB_CHAR;   return 1; }
    if (consume(STRUCT)) {
        if (!consume(ID)) tkerr(crtTk, "expected identifier after 'struct'");
        t->tb = TB_STRUCT;
        t->s  = NULL; /* domain analysis will resolve */
        return 1;
    }
    crtTk = startTk;
    return 0;
}

int syn_arrayDecl(Type *t) {
    if (!consume(LBRACKET)) return 0;
    if (consume(CT_INT)) {
        t->n = (int)consumedTk->i;
    } else {
        t->n = 0; /* unsized */
    }
    if (!consume(RBRACKET))
        tkerr(crtTk, "missing ']' in array declaration");
    return 1;
}

int syn_varDef(void) {
    Token *startTk = crtTk;
    Type t;
    if (!syn_typeBase(&t)) return 0;
    if (!consume(ID)) { crtTk = startTk; return 0; }
    syn_arrayDecl(&t);

    while (consume(COMMA)) {
        if (!consume(ID)) tkerr(crtTk, "expected identifier after ','");
        Type t2 = t; t2.n = -1;
        syn_arrayDecl(&t2);
    }
    if (!consume(SEMICOLON))
        tkerr(crtTk, "missing ';' after variable declaration");
    return 1;
}

int syn_fnParam(void) {
    Token *startTk = crtTk;
    Type t;
    if (!syn_typeBase(&t)) return 0;
    if (!consume(ID)) { crtTk = startTk; return 0; }
    syn_arrayDecl(&t);
    return 1;
}

int syn_stmCompound(void) {
    if (!consume(LACC)) return 0;
    while (1) {
        if (syn_varDef()) continue;
        if (syn_stm())    continue;
        break;
    }
    if (!consume(RACC))
        tkerr(crtTk, "missing '}' or syntax error in compound statement");
    return 1;
}

int syn_expr(void) { return syn_exprAssign(); }

int syn_exprAssign(void) {
    Token *startTk = crtTk;
    if (syn_exprUnary()) {
        if (consume(ASSIGN)) {
            if (!syn_exprAssign()) tkerr(crtTk, "invalid expression after '='");
            return 1;
        }
        crtTk = startTk;
    }
    return syn_exprOr();
}

int syn_exprOr1(void) {
    if (consume(OR)) {
        if (!syn_exprAnd()) tkerr(crtTk, "invalid expression after '||'");
        syn_exprOr1();
        return 1;
    }
    return 0;
}
int syn_exprOr(void) { if (!syn_exprAnd()) return 0; syn_exprOr1(); return 1; }

int syn_exprAnd1(void) {
    if (consume(AND)) {
        if (!syn_exprEq()) tkerr(crtTk, "invalid expression after '&&'");
        syn_exprAnd1();
        return 1;
    }
    return 0;
}
int syn_exprAnd(void) { if (!syn_exprEq()) return 0; syn_exprAnd1(); return 1; }

int syn_exprEq1(void) {
    if (consume(EQUAL) || consume(NOTEQ)) {
        if (!syn_exprRel()) tkerr(crtTk, "invalid expression after equality operator");
        syn_exprEq1();
        return 1;
    }
    return 0;
}
int syn_exprEq(void) { if (!syn_exprRel()) return 0; syn_exprEq1(); return 1; }

int syn_exprRel1(void) {
    if (consume(LESS)||consume(LESSEQ)||consume(GREATER)||consume(GREATEREQ)) {
        if (!syn_exprAdd()) tkerr(crtTk, "invalid expression after relational operator");
        syn_exprRel1();
        return 1;
    }
    return 0;
}
int syn_exprRel(void) { if (!syn_exprAdd()) return 0; syn_exprRel1(); return 1; }

int syn_exprAdd1(void) {
    if (consume(ADD)||consume(SUB)) {
        if (!syn_exprMul()) tkerr(crtTk, "invalid expression after '+' or '-'");
        syn_exprAdd1();
        return 1;
    }
    return 0;
}
int syn_exprAdd(void) { if (!syn_exprMul()) return 0; syn_exprAdd1(); return 1; }

int syn_exprMul1(void) {
    if (consume(MUL)||consume(DIV)) {
        if (!syn_exprCast()) tkerr(crtTk, "invalid expression after '*' or '/'");
        syn_exprMul1();
        return 1;
    }
    return 0;
}
int syn_exprMul(void) { if (!syn_exprCast()) return 0; syn_exprMul1(); return 1; }

int syn_exprCast(void) {
    Token *startTk = crtTk;
    if (consume(LPAR)) {
        Type t;
        if (syn_typeBase(&t)) {
            syn_arrayDecl(&t);
            if (!consume(RPAR)) tkerr(crtTk, "missing ')' in cast expression");
            if (!syn_exprCast()) tkerr(crtTk, "invalid expression after cast");
            return 1;
        }
        crtTk = startTk;
    }
    return syn_exprUnary();
}

int syn_exprUnary(void) {
    if (consume(SUB)||consume(NOT)) {
        if (!syn_exprUnary()) tkerr(crtTk, "invalid expression after unary operator");
        return 1;
    }
    return syn_exprPostfix();
}

int syn_exprPostfix1(void) {
    if (consume(LBRACKET)) {
        if (!syn_expr()) tkerr(crtTk, "invalid expression inside '[]'");
        if (!consume(RBRACKET)) tkerr(crtTk, "missing ']'");
        syn_exprPostfix1();
        return 1;
    }
    if (consume(DOT)) {
        if (!consume(ID)) tkerr(crtTk, "missing identifier after '.'");
        syn_exprPostfix1();
        return 1;
    }
    return 0;
}
int syn_exprPostfix(void) {
    if (!syn_exprPrimary()) return 0;
    syn_exprPostfix1();
    return 1;
}

int syn_exprPrimary(void) {
    Token *startTk = crtTk;
    if (consume(ID)) {
        if (consume(LPAR)) {
            if (syn_expr()) {
                while (consume(COMMA)) {
                    if (!syn_expr()) tkerr(crtTk, "invalid expression after ','");
                }
            }
            if (!consume(RPAR)) tkerr(crtTk, "missing ')' in function call");
        }
        return 1;
    }
    if (consume(CT_INT))    return 1;
    if (consume(CT_REAL))   return 1;
    if (consume(CT_CHAR))   return 1;
    if (consume(CT_STRING)) return 1;
    if (crtTk->code == LPAR) {
        Token *saveTk = crtTk;
        consume(LPAR);
        if (!syn_expr()) { crtTk = saveTk; return 0; }
        if (!consume(RPAR)) tkerr(crtTk, "missing ')'");
        return 1;
    }
    crtTk = startTk;
    return 0;
}

int syn_stm(void) {
    Token *startTk = crtTk;

    if (syn_stmCompound()) return 1;

    if (consume(IF)) {
        if (!consume(LPAR))   tkerr(crtTk, "missing '(' after 'if'");
        if (!syn_expr())      tkerr(crtTk, "missing or invalid expression in 'if'");
        if (!consume(RPAR))   tkerr(crtTk, "missing ')' after 'if' condition");
        if (!syn_stm())       tkerr(crtTk, "missing statement in 'if'");
        if (consume(ELSE)) {
            if (!syn_stm())   tkerr(crtTk, "missing statement after 'else'");
        }
        return 1;
    }
    if (consume(WHILE)) {
        if (!consume(LPAR))   tkerr(crtTk, "missing '(' after 'while'");
        if (!syn_expr())      tkerr(crtTk, "invalid expression in 'while'");
        if (!consume(RPAR))   tkerr(crtTk, "missing ')' after 'while' condition");
        if (!syn_stm())       tkerr(crtTk, "missing statement in 'while'");
        return 1;
    }
    if (consume(FOR)) {
        if (!consume(LPAR))       tkerr(crtTk, "missing '(' after 'for'");
        syn_expr();
        if (!consume(SEMICOLON))  tkerr(crtTk, "missing ';' in 'for'");
        syn_expr();
        if (!consume(SEMICOLON))  tkerr(crtTk, "missing ';' in 'for'");
        syn_expr();
        if (!consume(RPAR))       tkerr(crtTk, "missing ')' in 'for'");
        if (!syn_stm())           tkerr(crtTk, "missing statement in 'for'");
        return 1;
    }
    if (consume(BREAK)) {
        if (!consume(SEMICOLON)) tkerr(crtTk, "missing ';' after 'break'");
        return 1;
    }
    if (consume(RETURN)) {
        syn_expr();
        if (!consume(SEMICOLON)) tkerr(crtTk, "missing ';' after 'return'");
        return 1;
    }
    syn_expr();
    if (consume(SEMICOLON)) return 1;

    crtTk = startTk;
    return 0;
}

int syn_fnDef(void) {
    Token *startTk = crtTk;
    Type t;
    int hasType = syn_typeBase(&t);
    if (!hasType && !consume(VOID)) return 0;
    if (!hasType) t.tb = TB_VOID;
    if (!consume(ID))   { crtTk = startTk; return 0; }
    if (!consume(LPAR)) { crtTk = startTk; return 0; }
    if (syn_fnParam()) {
        while (consume(COMMA)) {
            if (!syn_fnParam()) tkerr(crtTk, "invalid parameter after ','");
        }
    }
    if (!consume(RPAR))       tkerr(crtTk, "missing ')' in function definition");
    if (!syn_stmCompound())   tkerr(crtTk, "missing function body");
    return 1;
}

int syn_structDef(void) {
    Token *startTk = crtTk;
    if (!consume(STRUCT)) return 0;
    if (!consume(ID))     { crtTk = startTk; return 0; }
    if (!consume(LACC))   { crtTk = startTk; return 0; }
    while (syn_varDef()) {}
    if (!consume(RACC))      tkerr(crtTk, "missing '}' in struct definition");
    if (!consume(SEMICOLON)) tkerr(crtTk, "missing ';' after struct definition");
    return 1;
}

static void syn_unit(void) {
    while (1) {
        if (syn_structDef()) continue;
        if (syn_fnDef())     continue;
        if (syn_varDef())    continue;
        break;
    }
    if (!consume(END)) tkerr(crtTk, "unexpected token at top level");
}

void parse(int show_output) {
    crtTk = tokens;
    syn_unit();
    if (show_output) printf("Syntax OK\n");
}

/* ═══════════════════════════════════════════════════════════
 * 5. DOMAIN ANALYZER
 * ═══════════════════════════════════════════════════════════ */

/* ── Global symbol table & depth tracking ────────────────── */
Symbols symTable;       /* scoped flat table used during analysis */
Symbols allSymbols;     /* permanent record of every symbol, for display */
static int crtDepth = 0;
static Symbol *owner = NULL; /* current enclosing fn or struct */

/* ── initSymbols ─────────────────────────────────────────── */
void initSymbols(Symbols *s) {
    s->begin = s->end = s->after = NULL;
}

/* ── addSymbolToList ─────────────────────────────────────── */
void addSymbolToList(Symbols *list, Symbol *sym) {
    if (list->end == list->after) {
        int count = (int)(list->after - list->begin);
        int n = count * 2;
        if (n == 0) n = 1;
        list->begin = (Symbol**)realloc(list->begin, n * sizeof(Symbol*));
        if (!list->begin) err("not enough memory");
        list->end   = list->begin + count;
        list->after = list->begin + n;
    }
    *list->end++ = sym;
}

/* ── newSymbol ───────────────────────────────────────────── */
Symbol *newSymbol(const char *name, int cls) {
    Symbol *s;
    SAFEALLOC(s, Symbol);
    s->name  = name;
    s->cls   = cls;
    s->depth = crtDepth;
    s->owner = owner;
    s->mem   = MEM_GLOBAL;
    s->type.tb = TB_INT;
    s->type.s  = NULL;
    s->type.n  = -1;
    initSymbols(&s->args); /* zero both union fields */
    return s;
}

/* ── dupSymbol ───────────────────────────────────────────── */
Symbol *dupSymbol(const Symbol *src) {
    Symbol *s;
    SAFEALLOC(s, Symbol);
    *s = *src;
    return s;
}

/* ── addSymbolToDomain ───────────────────────────────────── */
/*
 * Adds sym to the scoped flat table AND to the permanent allSymbols list.
 */
Symbol *addSymbolToDomain(Symbols *domain, Symbol *sym) {
    addSymbolToList(domain, sym);
    addSymbolToList(&allSymbols, sym); /* permanent record */
    return sym;
}

/* ── findSymbolInDomain ──────────────────────────────────── */
/*
 * Search backward (right-to-left) in *domain for the symbol
 * with the matching name and depth >= minDepth.
 * For "current domain only" checks pass crtDepth; for global
 * look-ups pass 0.
 */
Symbol *findSymbolInDomain(Symbols *domain, const char *name) {
    if (domain->begin == domain->end) return NULL;
    for (Symbol **p = domain->end - 1; p >= domain->begin; p--) {
        if (strcmp((*p)->name, name) == 0) return *p;
    }
    return NULL;
}

/* ── findSymbol ──────────────────────────────────────────── */
/*
 * Finds any visible symbol (searches the entire flat table
 * right-to-left so the nearest definition takes precedence).
 */
Symbol *findSymbol(const char *name) {
    return findSymbolInDomain(&symTable, name);
}

/* ── findSymbolInCurrentDomain ───────────────────────────── */
/*
 * Returns non-NULL only if a symbol with the given name exists
 * AT the current depth (i.e. is in the current domain scope).
 */
static Symbol *findSymbolInCurrentDomain(const char *name) {
    if (symTable.begin == symTable.end) return NULL;
    for (Symbol **p = symTable.end - 1; p >= symTable.begin; p--) {
        if ((*p)->depth < crtDepth) break; /* stop at shallower depth */
        if (strcmp((*p)->name, name) == 0) return *p;
    }
    return NULL;
}

/* ── pushDomain / dropDomain ─────────────────────────────── */
void pushDomain(void) {
    crtDepth++;
}

void dropDomain(void) {
    /* remove all symbols at crtDepth from the flat table */
    Symbol **p = symTable.begin;
    Symbol **w = symTable.begin;
    /* keep symbols with depth < crtDepth */
    while (p != symTable.end) {
        if ((*p)->depth < crtDepth) *w++ = *p;
        /* symbols at crtDepth are "popped"; memory is intentionally not
           freed here — they may still be referenced from fn->args etc. */
        p++;
    }
    symTable.end = w;
    crtDepth--;
}

/* ── Domain analysis pass: forward declarations ─────────── */
int da_typeBase(Type *t);
int da_arrayDecl(Type *t);
int da_varDef(void);
int da_fnParam(void);
int da_fnDef(void);
int da_structDef(void);
int da_stmCompound(int newDomain);
int da_stm(void);
int da_expr(void);

/* re-use the same consume / crtTk / consumedTk from syntactic pass */

int da_typeBase(Type *t) {
    t->n = -1;
    Token *startTk = crtTk;
    if (consume(INT))    { t->tb = TB_INT;    return 1; }
    if (consume(DOUBLE)) { t->tb = TB_DOUBLE; return 1; }
    if (consume(CHAR))   { t->tb = TB_CHAR;   return 1; }
    if (consume(STRUCT)) {
        if (!consume(ID)) tkerr(crtTk, "expected identifier after 'struct'");
        Token *tkName = consumedTk;
        t->tb = TB_STRUCT;
        /* struct must have been defined before use */
        t->s = findSymbol(tkName->text);
        if (!t->s) tkerr(tkName, "undefined struct: %s", tkName->text);
        return 1;
    }
    crtTk = startTk;
    return 0;
}

int da_arrayDecl(Type *t) {
    if (!consume(LBRACKET)) return 0;
    if (consume(CT_INT)) {
        t->n = (int)consumedTk->i;
    } else {
        t->n = 0; /* unsized */
    }
    if (!consume(RBRACKET))
        tkerr(crtTk, "missing ']' in array declaration");
    return 1;
}

int da_varDef(void) {
    Token *startTk = crtTk;
    Type t;
    if (!da_typeBase(&t)) return 0;
    if (!consume(ID)) { crtTk = startTk; return 0; }
    Token *tkName = consumedTk;

    if (da_arrayDecl(&t)) {
        /* a vector variable must have a specified dimension */
        if (t.n == 0)
            tkerr(tkName, "a vector variable must have a specified dimension");
    }

    /* check for redefinition in current domain */
    if (findSymbolInCurrentDomain(tkName->text))
        tkerr(tkName, "symbol redefinition: %s", tkName->text);

    Symbol *var = newSymbol(tkName->text, CLS_VAR);
    var->type  = t;
    var->owner = owner;

    if (owner) {
        switch (owner->cls) {
        case CLS_FUNC:
            var->mem = MEM_LOCAL;
            addSymbolToList(&owner->args, dupSymbol(var));
            break;
        case CLS_STRUCT:
            var->mem = MEM_LOCAL;
            addSymbolToList(&owner->members, dupSymbol(var));
            break;
        }
    } else {
        var->mem = MEM_GLOBAL;
    }
    addSymbolToDomain(&symTable, var);

    /* handle additional variables in the same declaration: int a, b, c; */
    while (consume(COMMA)) {
        if (!consume(ID)) tkerr(crtTk, "expected identifier after ','");
        tkName = consumedTk;
        Type t2 = t; t2.n = -1;
        if (da_arrayDecl(&t2)) {
            if (t2.n == 0)
                tkerr(tkName, "a vector variable must have a specified dimension");
        }
        if (findSymbolInCurrentDomain(tkName->text))
            tkerr(tkName, "symbol redefinition: %s", tkName->text);
        Symbol *var2 = newSymbol(tkName->text, CLS_VAR);
        var2->type  = t2;
        var2->owner = owner;
        var2->mem   = var->mem;
        if (owner && owner->cls == CLS_FUNC)
            addSymbolToList(&owner->args, dupSymbol(var2));
        else if (owner && owner->cls == CLS_STRUCT)
            addSymbolToList(&owner->members, dupSymbol(var2));
        addSymbolToDomain(&symTable, var2);
    }

    if (!consume(SEMICOLON))
        tkerr(crtTk, "missing ';' after variable declaration");
    return 1;
}

int da_fnParam(void) {
    Token *startTk = crtTk;
    Type t;
    if (!da_typeBase(&t)) return 0;
    if (!consume(ID)) { crtTk = startTk; return 0; }
    Token *tkName = consumedTk;

    /* parameters may be arrays but dimension is erased (int v[10] -> int v[]) */
    if (da_arrayDecl(&t)) {
        t.n = 0; /* erase dimension for parameters */
    }

    /* check for redefinition in current domain */
    if (findSymbolInCurrentDomain(tkName->text))
        tkerr(tkName, "symbol redefinition: %s", tkName->text);

    Symbol *param = newSymbol(tkName->text, CLS_VAR);
    param->type  = t;
    param->mem   = MEM_ARG;
    param->owner = owner;

    /* add to domain and to the function's args list */
    addSymbolToDomain(&symTable, param);
    if (owner && owner->cls == CLS_FUNC)
        addSymbolToList(&owner->args, dupSymbol(param));

    return 1;
}

int da_stmCompound(int newDomain) {
    if (!consume(LACC)) return 0;
    if (newDomain) pushDomain();
    while (1) {
        if (da_varDef()) continue;
        if (da_stm())    continue;
        break;
    }
    if (!consume(RACC))
        tkerr(crtTk, "missing '}' in compound statement");
    if (newDomain) dropDomain();
    return 1;
}

int da_expr(void) { return syn_expr(); } /* expressions have no domain effects */

int da_stm(void) {
    Token *startTk = crtTk;

    /* compound statement creates a new domain */
    if (da_stmCompound(1)) return 1;

    if (consume(IF)) {
        if (!consume(LPAR))   tkerr(crtTk, "missing '(' after 'if'");
        if (!da_expr())       tkerr(crtTk, "missing or invalid expression in 'if'");
        if (!consume(RPAR))   tkerr(crtTk, "missing ')' after 'if' condition");
        if (!da_stm())        tkerr(crtTk, "missing statement in 'if'");
        if (consume(ELSE)) {
            if (!da_stm())    tkerr(crtTk, "missing statement after 'else'");
        }
        return 1;
    }
    if (consume(WHILE)) {
        if (!consume(LPAR))   tkerr(crtTk, "missing '(' after 'while'");
        if (!da_expr())       tkerr(crtTk, "invalid expression in 'while'");
        if (!consume(RPAR))   tkerr(crtTk, "missing ')' after 'while' condition");
        if (!da_stm())        tkerr(crtTk, "missing statement in 'while'");
        return 1;
    }
    if (consume(FOR)) {
        if (!consume(LPAR))       tkerr(crtTk, "missing '(' after 'for'");
        da_expr();
        if (!consume(SEMICOLON))  tkerr(crtTk, "missing ';' in 'for'");
        da_expr();
        if (!consume(SEMICOLON))  tkerr(crtTk, "missing ';' in 'for'");
        da_expr();
        if (!consume(RPAR))       tkerr(crtTk, "missing ')' in 'for'");
        if (!da_stm())            tkerr(crtTk, "missing statement in 'for'");
        return 1;
    }
    if (consume(BREAK)) {
        if (!consume(SEMICOLON)) tkerr(crtTk, "missing ';' after 'break'");
        return 1;
    }
    if (consume(RETURN)) {
        da_expr();
        if (!consume(SEMICOLON)) tkerr(crtTk, "missing ';' after 'return'");
        return 1;
    }
    da_expr();
    if (consume(SEMICOLON)) return 1;

    crtTk = startTk;
    return 0;
}

int da_fnDef(void) {
    Token *startTk = crtTk;
    Type t;
    int hasType = da_typeBase(&t);
    if (!hasType && !consume(VOID)) return 0;
    if (!hasType) t.tb = TB_VOID;

    if (!consume(ID)) { crtTk = startTk; return 0; }
    Token *tkName = consumedTk;

    if (!consume(LPAR)) { crtTk = startTk; return 0; }

    /* function name must be unique in current domain */
    if (findSymbolInCurrentDomain(tkName->text))
        tkerr(tkName, "symbol redefinition: %s", tkName->text);

    Symbol *fn = newSymbol(tkName->text, CLS_FUNC);
    fn->type = t;
    fn->mem  = MEM_GLOBAL;
    addSymbolToDomain(&symTable, fn);

    Symbol *savedOwner = owner;
    owner = fn;
    pushDomain(); /* function's local domain starts right after LPAR */

    if (da_fnParam()) {
        while (consume(COMMA)) {
            if (!da_fnParam()) tkerr(crtTk, "invalid parameter after ','");
        }
    }
    if (!consume(RPAR)) tkerr(crtTk, "missing ')' in function definition");

    /* function body: stmCompound with newDomain=false — the fn domain
       continues directly into the body without a nested push/pop */
    if (!da_stmCompound(0)) tkerr(crtTk, "missing function body");

    dropDomain();
    owner = savedOwner;
    return 1;
}

int da_structDef(void) {
    Token *startTk = crtTk;
    if (!consume(STRUCT)) return 0;
    if (!consume(ID))     { crtTk = startTk; return 0; }
    Token *tkName = consumedTk;
    if (!consume(LACC))   { crtTk = startTk; return 0; }

    /* struct name must be unique */
    if (findSymbolInCurrentDomain(tkName->text))
        tkerr(tkName, "symbol redefinition: %s", tkName->text);

    Symbol *s = newSymbol(tkName->text, CLS_STRUCT);
    s->type.tb = TB_STRUCT;
    s->type.s  = s;
    s->type.n  = -1;
    addSymbolToDomain(&symTable, s);

    Symbol *savedOwner = owner;
    owner = s;
    pushDomain(); /* struct member domain */

    while (da_varDef()) {}

    if (!consume(RACC))      tkerr(crtTk, "missing '}' in struct definition");
    if (!consume(SEMICOLON)) tkerr(crtTk, "missing ';' after struct definition");

    dropDomain();
    owner = savedOwner;
    return 1;
}

static void da_unit(void) {
    while (1) {
        if (da_structDef()) continue;
        if (da_fnDef())     continue;
        if (da_varDef())    continue;
        break;
    }
    if (!consume(END)) tkerr(crtTk, "unexpected token at top level");
}

static const char *clsName(int cls) {
    switch (cls) {
    case CLS_VAR:     return "VAR";
    case CLS_FUNC:    return "FUNC";
    case CLS_EXTFUNC: return "EXTFUNC";
    case CLS_STRUCT:  return "STRUCT";
    default:          return "?";
    }
}

static const char *memName(int mem) {
    switch (mem) {
    case MEM_GLOBAL: return "GLOBAL";
    case MEM_ARG:    return "ARG";
    case MEM_LOCAL:  return "LOCAL";
    default:         return "?";
    }
}

static void printType(const Type *t) {
    switch (t->tb) {
    case TB_INT:    printf("int");    break;
    case TB_DOUBLE: printf("double"); break;
    case TB_CHAR:   printf("char");   break;
    case TB_VOID:   printf("void");   break;
    case TB_STRUCT:
        printf("struct %s", t->s ? t->s->name : "?");
        break;
    default:        printf("?");      break;
    }
    if (t->n >= 0) printf("[%d]", t->n);
}

static void showSymTable(void) {
    int n = (int)(allSymbols.end - allSymbols.begin);
    printf("Symbol table (%d entries):\n", n);
    for (int i = 0; i < n; i++) {
        Symbol *s = allSymbols.begin[i];
        printf("  %-20s  cls=%-8s  mem=%-8s  depth=%-2d  type=",
               s->name, clsName(s->cls), memName(s->mem), s->depth);
        printType(&s->type);
        printf("\n");
    }
}

void domainAnalysis(int show_output) {
    initSymbols(&symTable);
    initSymbols(&allSymbols);
    crtDepth = 0;
    owner    = NULL;
    crtTk    = tokens;
    da_unit();
    if (show_output) showSymTable();
}

/* ═══════════════════════════════════════════════════════════
 * 6. TYPE ANALYZER
 * ═══════════════════════════════════════════════════════════ */

/* ── RetVal: result of evaluating an expression ─────────── */
typedef union {
    long int i;       /* TB_INT, TB_CHAR  */
    double   d;       /* TB_DOUBLE        */
    const char *str;  /* TB_CHAR array    */
} CtVal;

typedef struct {
    Type type;
    int  lval;   /* 1 = left-value (addressable)  */
    int  ct;     /* 1 = compile-time constant     */
    CtVal ctVal;
} RetVal;

/* ── Type helper: create a Type inline ───────────────────── */
static Type mkType(int tb, int n) {
    Type t;
    t.tb = tb;
    t.s  = NULL;
    t.n  = n;
    return t;
}

/* ── getArithType ────────────────────────────────────────── */
/*
 * Given two arithmetic types, return the "wider" common type.
 * Returns a type with tb == TB_VOID if conversion is impossible
 * (e.g. either operand is a struct or an array).
 */
static Type getArithType(const Type *s1, const Type *s2) {
    /* arrays and structs cannot participate in arithmetic */
    if (s1->n >= 0 || s2->n >= 0)          return mkType(TB_VOID, -1);
    if (s1->tb == TB_STRUCT || s2->tb == TB_STRUCT) return mkType(TB_VOID, -1);
    if (s1->tb == TB_VOID   || s2->tb == TB_VOID)   return mkType(TB_VOID, -1);
    /* widen: double > int > char */
    if (s1->tb == TB_DOUBLE || s2->tb == TB_DOUBLE) return mkType(TB_DOUBLE, -1);
    if (s1->tb == TB_INT    || s2->tb == TB_INT)    return mkType(TB_INT,    -1);
    return mkType(TB_CHAR, -1);
}

/* ── canBeScalar ─────────────────────────────────────────── */
/* A value is "scalar" if it is not a struct and not an array */
static int canBeScalar(const RetVal *rv) {
    return rv->type.tb != TB_STRUCT && rv->type.n < 0;
}

/* ── convTo ──────────────────────────────────────────────── */
/*
 * Returns 1 if type *src can be implicitly converted to *dst.
 * Struct-to-struct only if they are the same struct.
 * Array-to-array only if same base type.
 * No array <-> scalar conversion.
 */
static int convTo(const Type *src, const Type *dst) {
    /* array vs scalar mismatch */
    if (src->n >= 0 && dst->n < 0) return 0;
    if (src->n < 0  && dst->n >= 0) return 0;
    /* struct: must be the exact same struct */
    if (dst->tb == TB_STRUCT) {
        return src->tb == TB_STRUCT && src->s == dst->s;
    }
    /* void cannot be a destination in value context */
    if (dst->tb == TB_VOID) return 0;
    /* numeric types (char/int/double) are mutually convertible */
    if (src->tb == TB_STRUCT) return 0;
    if (src->tb == TB_VOID)   return 0;
    return 1;
}

/* ── cast (hard conversion, generates error on failure) ──── */
static void castType(const Token *tk, const Type *dst, const Type *src) {
    if (src->n >= 0) {
        if (dst->n >= 0) {
            if (src->tb != dst->tb)
                tkerr(tk, "an array cannot be converted to an array of another type");
        } else {
            tkerr(tk, "an array cannot be converted to a non-array");
        }
    } else {
        if (dst->n >= 0)
            tkerr(tk, "a non-array cannot be converted to an array");
    }
    switch (src->tb) {
    case TB_CHAR: case TB_INT: case TB_DOUBLE:
        switch (dst->tb) {
        case TB_CHAR: case TB_INT: case TB_DOUBLE:
            return;
        }
        break;
    case TB_STRUCT:
        if (dst->tb == TB_STRUCT) {
            if (src->s != dst->s)
                tkerr(tk, "a structure cannot be converted to another one");
            return;
        }
        break;
    default: break;
    }
    tkerr(tk, "incompatible types");
}

/* ── findSymbolInList ────────────────────────────────────── */
/* Searches a Symbols list (e.g. struct members) by name.    */
static Symbol *findSymbolInList(const Symbols *list, const char *name) {
    if (!list || list->begin == list->end) return NULL;
    for (Symbol **p = list->begin; p != list->end; p++) {
        if (strcmp((*p)->name, name) == 0) return *p;
    }
    return NULL;
}

/* ── addExtFunc / addFuncArg ─────────────────────────────── */
static Symbol *addExtFunc(const char *name, Type type) {
    Symbol *s = newSymbol(name, CLS_EXTFUNC);
    s->type = type;
    s->mem  = MEM_GLOBAL;
    initSymbols(&s->args);
    addSymbolToList(&symTable, s);
    addSymbolToList(&allSymbols, s);
    return s;
}

static void addFuncArg(Symbol *fn, const char *name, Type type) {
    Symbol *a = newSymbol(name, CLS_VAR);
    a->type = type;
    a->mem  = MEM_ARG;
    addSymbolToList(&fn->args, a);
}

/* ── addExtFuncs: register all predefined AtomC functions ── */
static void addExtFuncs(void) {
    Symbol *s;
    s = addExtFunc("put_s",   mkType(TB_VOID,   -1));
        addFuncArg(s, "s",    mkType(TB_CHAR,    0));
    s = addExtFunc("get_s",   mkType(TB_VOID,   -1));
        addFuncArg(s, "s",    mkType(TB_CHAR,    0));
    s = addExtFunc("put_i",   mkType(TB_VOID,   -1));
        addFuncArg(s, "i",    mkType(TB_INT,    -1));
      (void)addExtFunc("get_i", mkType(TB_INT,  -1));   /* no args */
    s = addExtFunc("put_d",   mkType(TB_VOID,   -1));
        addFuncArg(s, "d",    mkType(TB_DOUBLE, -1));
      (void)addExtFunc("get_d", mkType(TB_DOUBLE,-1));
    s = addExtFunc("put_c",   mkType(TB_VOID,   -1));
        addFuncArg(s, "c",    mkType(TB_CHAR,   -1));
      (void)addExtFunc("get_c",   mkType(TB_CHAR,  -1));
      (void)addExtFunc("seconds", mkType(TB_DOUBLE,-1));
}

/* ── Type-analysis forward declarations ──────────────────── */
int ta_expr(RetVal *r);
int ta_exprAssign(RetVal *r);
int ta_exprOr(RetVal *r);
int ta_exprOr1(RetVal *r);
int ta_exprAnd(RetVal *r);
int ta_exprAnd1(RetVal *r);
int ta_exprEq(RetVal *r);
int ta_exprEq1(RetVal *r);
int ta_exprRel(RetVal *r);
int ta_exprRel1(RetVal *r);
int ta_exprAdd(RetVal *r);
int ta_exprAdd1(RetVal *r);
int ta_exprMul(RetVal *r);
int ta_exprMul1(RetVal *r);
int ta_exprCast(RetVal *r);
int ta_exprUnary(RetVal *r);
int ta_exprPostfix(RetVal *r);
int ta_exprPostfix1(RetVal *r);
int ta_exprPrimary(RetVal *r);
int ta_stm(void);
int ta_stmCompound(int newDomain);
int ta_varDef(void);
int ta_fnParam(void);
int ta_fnDef(void);
int ta_structDef(void);
int ta_typeBase(Type *t);
int ta_arrayDecl(Type *t);

/* ── ta_typeBase ─────────────────────────────────────────── */
int ta_typeBase(Type *t) {
    t->n = -1;
    Token *startTk = crtTk;
    if (consume(INT))    { t->tb = TB_INT;    return 1; }
    if (consume(DOUBLE)) { t->tb = TB_DOUBLE; return 1; }
    if (consume(CHAR))   { t->tb = TB_CHAR;   return 1; }
    if (consume(STRUCT)) {
        if (!consume(ID)) tkerr(crtTk, "expected identifier after 'struct'");
        Token *tkName = consumedTk;
        t->tb = TB_STRUCT;
        t->s  = findSymbol(tkName->text);
        if (!t->s) tkerr(tkName, "undefined struct: %s", tkName->text);
        return 1;
    }
    crtTk = startTk;
    return 0;
}

/* ── ta_arrayDecl ────────────────────────────────────────── */
int ta_arrayDecl(Type *t) {
    if (!consume(LBRACKET)) return 0;
    if (consume(CT_INT)) {
        t->n = (int)consumedTk->i;
    } else {
        t->n = 0;
    }
    if (!consume(RBRACKET))
        tkerr(crtTk, "missing ']' in array declaration");
    return 1;
}

/* ── ta_expr ─────────────────────────────────────────────── */
int ta_expr(RetVal *r) { return ta_exprAssign(r); }

/* ── ta_exprAssign ───────────────────────────────────────── */
int ta_exprAssign(RetVal *r) {
    Token *startTk = crtTk;
    RetVal rDst;
    if (ta_exprUnary(&rDst)) {
        if (consume(ASSIGN)) {
            Token *assignTk = consumedTk;
            RetVal rSrc;
            if (!ta_exprAssign(&rSrc))
                tkerr(crtTk, "invalid expression after '='");
            /* semantic checks */
            if (!rDst.lval)
                tkerr(assignTk, "the assign destination must be a left-value");
            if (rDst.ct)
                tkerr(assignTk, "the assign destination cannot be constant");
            if (!canBeScalar(&rDst))
                tkerr(assignTk, "the assign destination must be scalar");
            if (!canBeScalar(&rSrc))
                tkerr(assignTk, "the assign source must be scalar");
            if (!convTo(&rSrc.type, &rDst.type))
                tkerr(assignTk, "the assign source cannot be converted to destination");
            *r = rSrc;
            r->lval = 0;
            r->ct   = 0;
            return 1;
        }
        crtTk = startTk;
    }
    return ta_exprOr(r);
}

/* ── ta_exprOr / ta_exprOr1 ──────────────────────────────── */
int ta_exprOr1(RetVal *r) {
    if (consume(OR)) {
        Token *opTk = consumedTk;
        RetVal right;
        if (!ta_exprAnd(&right))
            tkerr(crtTk, "invalid expression after '||'");
        Type tDst = getArithType(&r->type, &right.type);
        if (tDst.tb == TB_VOID)
            tkerr(opTk, "invalid operand type for ||");
        r->type = mkType(TB_INT, -1);
        r->lval = 0; r->ct = 0;
        ta_exprOr1(r);
        return 1;
    }
    return 0;
}
int ta_exprOr(RetVal *r) {
    if (!ta_exprAnd(r)) return 0;
    ta_exprOr1(r);
    return 1;
}

/* ── ta_exprAnd / ta_exprAnd1 ────────────────────────────── */
int ta_exprAnd1(RetVal *r) {
    if (consume(AND)) {
        Token *opTk = consumedTk;
        RetVal right;
        if (!ta_exprEq(&right))
            tkerr(crtTk, "invalid expression after '&&'");
        Type tDst = getArithType(&r->type, &right.type);
        if (tDst.tb == TB_VOID)
            tkerr(opTk, "invalid operand type for &&");
        r->type = mkType(TB_INT, -1);
        r->lval = 0; r->ct = 0;
        ta_exprAnd1(r);
        return 1;
    }
    return 0;
}
int ta_exprAnd(RetVal *r) {
    if (!ta_exprEq(r)) return 0;
    ta_exprAnd1(r);
    return 1;
}

/* ── ta_exprEq / ta_exprEq1 ──────────────────────────────── */
int ta_exprEq1(RetVal *r) {
    if (consume(EQUAL) || consume(NOTEQ)) {
        Token *opTk = consumedTk;
        RetVal right;
        if (!ta_exprRel(&right))
            tkerr(crtTk, "invalid expression after equality operator");
        Type tDst = getArithType(&r->type, &right.type);
        if (tDst.tb == TB_VOID)
            tkerr(opTk, "invalid operand type for == or !=");
        r->type = mkType(TB_INT, -1);
        r->lval = 0; r->ct = 0;
        ta_exprEq1(r);
        return 1;
    }
    return 0;
}
int ta_exprEq(RetVal *r) {
    if (!ta_exprRel(r)) return 0;
    ta_exprEq1(r);
    return 1;
}

/* ── ta_exprRel / ta_exprRel1 ────────────────────────────── */
int ta_exprRel1(RetVal *r) {
    if (consume(LESS)||consume(LESSEQ)||consume(GREATER)||consume(GREATEREQ)) {
        Token *opTk = consumedTk;
        RetVal right;
        if (!ta_exprAdd(&right))
            tkerr(crtTk, "invalid expression after relational operator");
        Type tDst = getArithType(&r->type, &right.type);
        if (tDst.tb == TB_VOID)
            tkerr(opTk, "invalid operand type for <, <=, >, >=");
        r->type = mkType(TB_INT, -1);
        r->lval = 0; r->ct = 0;
        ta_exprRel1(r);
        return 1;
    }
    return 0;
}
int ta_exprRel(RetVal *r) {
    if (!ta_exprAdd(r)) return 0;
    ta_exprRel1(r);
    return 1;
}

/* ── ta_exprAdd / ta_exprAdd1 ────────────────────────────── */
int ta_exprAdd1(RetVal *r) {
    if (consume(ADD) || consume(SUB)) {
        Token *opTk = consumedTk;
        RetVal right;
        if (!ta_exprMul(&right))
            tkerr(crtTk, "invalid expression after '+' or '-'");
        Type tDst = getArithType(&r->type, &right.type);
        if (tDst.tb == TB_VOID)
            tkerr(opTk, "invalid operand type for + or -");
        r->type = tDst;
        r->lval = 0; r->ct = 0;
        ta_exprAdd1(r);
        return 1;
    }
    return 0;
}
int ta_exprAdd(RetVal *r) {
    if (!ta_exprMul(r)) return 0;
    ta_exprAdd1(r);
    return 1;
}

/* ── ta_exprMul / ta_exprMul1 ────────────────────────────── */
int ta_exprMul1(RetVal *r) {
    if (consume(MUL) || consume(DIV)) {
        Token *opTk = consumedTk;
        RetVal right;
        if (!ta_exprCast(&right))
            tkerr(crtTk, "invalid expression after '*' or '/'");
        Type tDst = getArithType(&r->type, &right.type);
        if (tDst.tb == TB_VOID)
            tkerr(opTk, "invalid operand type for * or /");
        r->type = tDst;
        r->lval = 0; r->ct = 0;
        ta_exprMul1(r);
        return 1;
    }
    return 0;
}
int ta_exprMul(RetVal *r) {
    if (!ta_exprCast(r)) return 0;
    ta_exprMul1(r);
    return 1;
}

/* ── ta_exprCast ─────────────────────────────────────────── */
int ta_exprCast(RetVal *r) {
    Token *startTk = crtTk;
    if (consume(LPAR)) {
        Type t;
        if (ta_typeBase(&t)) {
            ta_arrayDecl(&t);
            if (!consume(RPAR))
                tkerr(crtTk, "missing ')' in cast expression");
            RetVal op;
            if (!ta_exprCast(&op))
                tkerr(crtTk, "invalid expression after cast");
            /* cast rules */
            Token *castTk = consumedTk;
            if (t.tb == TB_STRUCT)
                tkerr(castTk, "cannot convert to a struct type");
            if (op.type.tb == TB_STRUCT)
                tkerr(castTk, "cannot convert a struct");
            if (op.type.n >= 0 && t.n < 0)
                tkerr(castTk, "an array can be converted only to another array");
            if (op.type.n < 0 && t.n >= 0)
                tkerr(castTk, "a scalar can be converted only to another scalar");
            r->type = t;
            r->lval = 0; r->ct = 0;
            return 1;
        }
        crtTk = startTk;
    }
    return ta_exprUnary(r);
}

/* ── ta_exprUnary ────────────────────────────────────────── */
int ta_exprUnary(RetVal *r) {
    if (consume(SUB) || consume(NOT)) {
        Token *opTk = consumedTk;
        if (!ta_exprUnary(r))
            tkerr(crtTk, "invalid expression after unary operator");
        if (!canBeScalar(r))
            tkerr(opTk, "unary - or ! must have a scalar operand");
        r->lval = 0; r->ct = 0;
        return 1;
    }
    return ta_exprPostfix(r);
}

/* ── ta_exprPostfix / ta_exprPostfix1 ────────────────────── */
int ta_exprPostfix1(RetVal *r) {
    if (consume(LBRACKET)) {
        Token *bracketTk = consumedTk;
        RetVal idx;
        if (!ta_expr(&idx))
            tkerr(crtTk, "invalid expression inside '[]'");
        if (!consume(RBRACKET))
            tkerr(crtTk, "missing ']'");
        /* only an array can be indexed */
        if (r->type.n < 0)
            tkerr(bracketTk, "only an array can be indexed");
        Type tInt = mkType(TB_INT, -1);
        if (!convTo(&idx.type, &tInt))
            tkerr(bracketTk, "the index is not convertible to int");
        r->type.n = -1; /* result is element type */
        r->lval = 1; r->ct = 0;
        ta_exprPostfix1(r);
        return 1;
    }
    if (consume(DOT)) {
        Token *dotTk = consumedTk;
        if (!consume(ID))
            tkerr(crtTk, "missing identifier after '.'");
        Token *tkName = consumedTk;
        if (r->type.tb != TB_STRUCT)
            tkerr(dotTk, "a field can only be selected from a struct");
        Symbol *field = findSymbolInList(&r->type.s->members, tkName->text);
        if (!field)
            tkerr(tkName, "the structure %s does not have a field %s",
                  r->type.s->name, tkName->text);
        r->type = field->type;
        r->lval = 1;
        r->ct   = (field->type.n >= 0) ? 0 : 0;
        ta_exprPostfix1(r);
        return 1;
    }
    return 0;
}
int ta_exprPostfix(RetVal *r) {
    if (!ta_exprPrimary(r)) return 0;
    ta_exprPostfix1(r);
    return 1;
}

/* ── ta_exprPrimary ──────────────────────────────────────── */
int ta_exprPrimary(RetVal *r) {
    Token *startTk = crtTk;

    if (consume(ID)) {
        Token *tkName = consumedTk;
        Symbol *s = findSymbol(tkName->text);
        if (!s)
            tkerr(tkName, "undefined id: %s", tkName->text);

        if (consume(LPAR)) {
            /* function call */
            Token *lparTk = consumedTk;
            if (s->cls != CLS_FUNC && s->cls != CLS_EXTFUNC)
                tkerr(lparTk, "only a function can be called");
            /* iterate expected parameters */
            Symbol **param = s->args.begin;
            int nParams = (int)(s->args.end - s->args.begin);
            int argIdx = 0;

            RetVal rArg;
            if (ta_expr(&rArg)) {
                if (argIdx >= nParams)
                    tkerr(lparTk, "too many arguments in function call");
                if (!convTo(&rArg.type, &param[argIdx]->type))
                    tkerr(tkName, "in call, cannot convert argument %d type to parameter type", argIdx+1);
                argIdx++;
                while (consume(COMMA)) {
                    if (!ta_expr(&rArg))
                        tkerr(crtTk, "invalid expression after ','");
                    if (argIdx >= nParams)
                        tkerr(lparTk, "too many arguments in function call");
                    if (!convTo(&rArg.type, &param[argIdx]->type))
                        tkerr(tkName, "in call, cannot convert argument %d type to parameter type", argIdx+1);
                    argIdx++;
                }
            }
            if (argIdx < nParams)
                tkerr(lparTk, "too few arguments in function call");
            if (!consume(RPAR))
                tkerr(crtTk, "missing ')' in function call");
            r->type = s->type;
            r->lval = 0; r->ct = 0;
            return 1;
        }

        /* plain variable / struct reference */
        if (s->cls == CLS_FUNC || s->cls == CLS_EXTFUNC)
            tkerr(tkName, "a function can only be called");
        r->type = s->type;
        r->lval = 1;
        r->ct   = 0;
        return 1;
    }

    if (consume(CT_INT)) {
        r->type = mkType(TB_INT, -1);
        r->lval = 0; r->ct = 1;
        r->ctVal.i = consumedTk->i;
        return 1;
    }
    if (consume(CT_REAL)) {
        r->type = mkType(TB_DOUBLE, -1);
        r->lval = 0; r->ct = 1;
        r->ctVal.d = consumedTk->r;
        return 1;
    }
    if (consume(CT_CHAR)) {
        r->type = mkType(TB_CHAR, -1);
        r->lval = 0; r->ct = 1;
        r->ctVal.i = consumedTk->i;
        return 1;
    }
    if (consume(CT_STRING)) {
        r->type = mkType(TB_CHAR, 0); /* char[] */
        r->lval = 0; r->ct = 1;
        r->ctVal.str = consumedTk->text;
        return 1;
    }
    if (crtTk->code == LPAR) {
        Token *saveTk = crtTk; /* save before consuming */
        consume(LPAR);
        RetVal inner;
        if (!ta_expr(&inner)) {
            /* Not a parenthesised expression (e.g. it's a cast) — backtrack */
            crtTk = saveTk;
            return 0;
        }
        if (!consume(RPAR))
            tkerr(crtTk, "missing ')'");
        *r = inner;
        return 1;
    }

    crtTk = startTk;
    return 0;
}

/* ── ta_stmCompound ──────────────────────────────────────── */
int ta_stmCompound(int newDomain) {
    if (!consume(LACC)) return 0;
    if (newDomain) pushDomain();
    while (1) {
        if (ta_varDef()) continue;
        if (ta_stm())    continue;
        break;
    }
    if (!consume(RACC))
        tkerr(crtTk, "missing '}' in compound statement");
    if (newDomain) dropDomain();
    return 1;
}

/* ── ta_stm ──────────────────────────────────────────────── */
int ta_stm(void) {
    Token *startTk = crtTk;
    RetVal rCond, rExpr;

    if (ta_stmCompound(1)) return 1;

    if (consume(IF)) {
        if (!consume(LPAR))    tkerr(crtTk, "missing '(' after 'if'");
        Token *condTk = crtTk;
        if (!ta_expr(&rCond))  tkerr(crtTk, "missing expression in 'if'");
        if (!canBeScalar(&rCond))
            tkerr(condTk, "the if condition must be a scalar value");
        if (!consume(RPAR))    tkerr(crtTk, "missing ')' after 'if' condition");
        if (!ta_stm())         tkerr(crtTk, "missing statement in 'if'");
        if (consume(ELSE)) {
            if (!ta_stm())     tkerr(crtTk, "missing statement after 'else'");
        }
        return 1;
    }

    if (consume(WHILE)) {
        if (!consume(LPAR))    tkerr(crtTk, "missing '(' after 'while'");
        Token *condTk = crtTk;
        if (!ta_expr(&rCond))  tkerr(crtTk, "invalid expression in 'while'");
        if (!canBeScalar(&rCond))
            tkerr(condTk, "the while condition must be a scalar value");
        if (!consume(RPAR))    tkerr(crtTk, "missing ')' after 'while' condition");
        if (!ta_stm())         tkerr(crtTk, "missing statement in 'while'");
        return 1;
    }

    if (consume(FOR)) {
        if (!consume(LPAR))       tkerr(crtTk, "missing '(' after 'for'");
        RetVal rInit, rStep;
        ta_expr(&rInit);  /* optional init */
        if (!consume(SEMICOLON))  tkerr(crtTk, "missing ';' in 'for'");
        Token *condTk = crtTk;
        if (ta_expr(&rCond)) {
            if (!canBeScalar(&rCond))
                tkerr(condTk, "the for condition must be a scalar value");
        }
        if (!consume(SEMICOLON))  tkerr(crtTk, "missing ';' in 'for'");
        ta_expr(&rStep);  /* optional step */
        if (!consume(RPAR))       tkerr(crtTk, "missing ')' in 'for'");
        if (!ta_stm())            tkerr(crtTk, "missing statement in 'for'");
        return 1;
    }

    if (consume(BREAK)) {
        if (!consume(SEMICOLON)) tkerr(crtTk, "missing ';' after 'break'");
        return 1;
    }

    if (consume(RETURN)) {
        Token *retTk = consumedTk;
        if (ta_expr(&rExpr)) {
            if (owner->type.tb == TB_VOID)
                tkerr(retTk, "a void function cannot return a value");
            if (!canBeScalar(&rExpr))
                tkerr(retTk, "the return value must be a scalar value");
            if (!convTo(&rExpr.type, &owner->type))
                tkerr(retTk, "cannot convert the return expression type to the function return type");
        } else {
            if (owner->type.tb != TB_VOID)
                tkerr(retTk, "a non-void function must return a value");
        }
        if (!consume(SEMICOLON)) tkerr(crtTk, "missing ';' after 'return'");
        return 1;
    }

    /* expression statement (optional expression) */
    ta_expr(&rExpr);
    if (consume(SEMICOLON)) return 1;

    crtTk = startTk;
    return 0;
}

/* ── ta_varDef ───────────────────────────────────────────── */
int ta_varDef(void) {
    Token *startTk = crtTk;
    Type t;
    if (!ta_typeBase(&t)) return 0;
    if (!consume(ID)) { crtTk = startTk; return 0; }
    Token *tkName = consumedTk;

    if (ta_arrayDecl(&t)) {
        if (t.n == 0)
            tkerr(tkName, "a vector variable must have a specified dimension");
    }

    if (findSymbolInCurrentDomain(tkName->text))
        tkerr(tkName, "symbol redefinition: %s", tkName->text);

    Symbol *var = newSymbol(tkName->text, CLS_VAR);
    var->type  = t;
    var->owner = owner;
    if (owner) {
        var->mem = (owner->cls == CLS_STRUCT) ? MEM_LOCAL : MEM_LOCAL;
        if (owner->cls == CLS_STRUCT)
            addSymbolToList(&owner->members, dupSymbol(var));
    } else {
        var->mem = MEM_GLOBAL;
    }
    addSymbolToDomain(&symTable, var);

    while (consume(COMMA)) {
        if (!consume(ID)) tkerr(crtTk, "expected identifier after ','");
        tkName = consumedTk;
        Type t2 = t; t2.n = -1;
        if (ta_arrayDecl(&t2)) {
            if (t2.n == 0)
                tkerr(tkName, "a vector variable must have a specified dimension");
        }
        if (findSymbolInCurrentDomain(tkName->text))
            tkerr(tkName, "symbol redefinition: %s", tkName->text);
        Symbol *var2 = newSymbol(tkName->text, CLS_VAR);
        var2->type  = t2;
        var2->owner = owner;
        var2->mem   = var->mem;
        if (owner && owner->cls == CLS_STRUCT)
            addSymbolToList(&owner->members, dupSymbol(var2));
        addSymbolToDomain(&symTable, var2);
    }

    if (!consume(SEMICOLON))
        tkerr(crtTk, "missing ';' after variable declaration");
    return 1;
}

/* ── ta_fnParam ──────────────────────────────────────────── */
int ta_fnParam(void) {
    Token *startTk = crtTk;
    Type t;
    if (!ta_typeBase(&t)) return 0;
    if (!consume(ID)) { crtTk = startTk; return 0; }
    Token *tkName = consumedTk;

    if (ta_arrayDecl(&t)) t.n = 0; /* erase dimension */

    if (findSymbolInCurrentDomain(tkName->text))
        tkerr(tkName, "symbol redefinition: %s", tkName->text);

    Symbol *param = newSymbol(tkName->text, CLS_VAR);
    param->type  = t;
    param->mem   = MEM_ARG;
    param->owner = owner;
    addSymbolToDomain(&symTable, param);
    if (owner && owner->cls == CLS_FUNC)
        addSymbolToList(&owner->args, dupSymbol(param));
    return 1;
}

/* ── ta_fnDef ────────────────────────────────────────────── */
int ta_fnDef(void) {
    Token *startTk = crtTk;
    Type t;
    int hasType = ta_typeBase(&t);
    if (!hasType && !consume(VOID)) return 0;
    if (!hasType) t.tb = TB_VOID;

    if (!consume(ID))   { crtTk = startTk; return 0; }
    Token *tkName = consumedTk;
    if (!consume(LPAR)) { crtTk = startTk; return 0; }

    if (findSymbolInCurrentDomain(tkName->text))
        tkerr(tkName, "symbol redefinition: %s", tkName->text);

    Symbol *fn = newSymbol(tkName->text, CLS_FUNC);
    fn->type = t; fn->mem = MEM_GLOBAL;
    addSymbolToDomain(&symTable, fn);

    Symbol *savedOwner = owner;
    owner = fn;
    pushDomain();

    if (ta_fnParam()) {
        while (consume(COMMA)) {
            if (!ta_fnParam()) tkerr(crtTk, "invalid parameter after ','");
        }
    }
    if (!consume(RPAR)) tkerr(crtTk, "missing ')' in function definition");
    if (!ta_stmCompound(0)) tkerr(crtTk, "missing function body");

    dropDomain();
    owner = savedOwner;
    return 1;
}

/* ── ta_structDef ────────────────────────────────────────── */
int ta_structDef(void) {
    Token *startTk = crtTk;
    if (!consume(STRUCT)) return 0;
    if (!consume(ID))     { crtTk = startTk; return 0; }
    Token *tkName = consumedTk;
    if (!consume(LACC))   { crtTk = startTk; return 0; }

    if (findSymbolInCurrentDomain(tkName->text))
        tkerr(tkName, "symbol redefinition: %s", tkName->text);

    Symbol *s = newSymbol(tkName->text, CLS_STRUCT);
    s->type.tb = TB_STRUCT; s->type.s = s; s->type.n = -1;
    addSymbolToDomain(&symTable, s);

    Symbol *savedOwner = owner;
    owner = s;
    pushDomain();

    while (ta_varDef()) {}

    if (!consume(RACC))      tkerr(crtTk, "missing '}' in struct definition");
    if (!consume(SEMICOLON)) tkerr(crtTk, "missing ';' after struct definition");

    dropDomain();
    owner = savedOwner;
    return 1;
}

/* ── ta_unit ─────────────────────────────────────────────── */
static void ta_unit(void) {
    while (1) {
        if (ta_structDef()) continue;
        if (ta_fnDef())     continue;
        if (ta_varDef())    continue;
        break;
    }
    if (!consume(END)) tkerr(crtTk, "unexpected token at top level");
}

/* ── typeAnalysis (entry point) ──────────────────────────── */
void typeAnalysis(int show_output) {
    /* reuse and reinitialize the symbol table */
    initSymbols(&symTable);
    initSymbols(&allSymbols);
    crtDepth = 0;
    owner    = NULL;
    crtTk    = tokens;

    /* register predefined functions before parsing */
    addExtFuncs();

    ta_unit();

    if (show_output) {
        showSymTable();
        printf("Type analysis OK\n");
    }
}

/* ═══════════════════════════════════════════════════════════
 * 7. MAIN
 * ═══════════════════════════════════════════════════════════ */

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr,
            "Usage: %s <source_file> [showLex] [showSyn] [showDom] [showType]\n",
            argv[0]);
        return 1;
    }

    int show_lex  = 0, show_syn = 0, show_dom = 0, show_type = 0;
    if (argc > 2) show_lex  = atoi(argv[2]);
    if (argc > 3) show_syn  = atoi(argv[3]);
    if (argc > 4) show_dom  = atoi(argv[4]);
    if (argc > 5) show_type = atoi(argv[5]);

    char *src = loadFile(argv[1]);

    tokenize(src, show_lex);
    parse(show_syn);
    domainAnalysis(show_dom);
    typeAnalysis(show_type);

    freeTokens();
    free(src);
    return 0;
}