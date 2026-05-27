#ifndef COMPILER_H
#define COMPILER_H

/* ── Token codes ─────────────────────────────────────────── */
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

extern Token *tokens;

/* ── Symbol table types ──────────────────────────────────── */

/* Type base codes */
enum { TB_INT, TB_DOUBLE, TB_CHAR, TB_STRUCT, TB_VOID };

/* Symbol class codes */
enum { CLS_VAR, CLS_FUNC, CLS_EXTFUNC, CLS_STRUCT };

/* Memory placement codes */
enum { MEM_GLOBAL, MEM_ARG, MEM_LOCAL };

/* Forward declaration needed for Type and Symbol */
struct _Symbol;
typedef struct _Symbol Symbol;

/* Dynamic array of Symbol pointers */
typedef struct {
    Symbol **begin;   /* start of allocated block, or NULL  */
    Symbol **end;     /* one past the last stored pointer   */
    Symbol **after;   /* one past the allocated block       */
} Symbols;

/* Type descriptor */
typedef struct {
    int     tb;         /* TB_*                                    */
    Symbol *s;          /* struct definition when tb==TB_STRUCT    */
    int     n;          /* >=0 : array with n elements (0=unsized) */
                        /* <0  : not an array                      */
} Type;

/* Symbol descriptor */
struct _Symbol {
    const char *name;   /* points into the token text              */
    int         cls;    /* CLS_*                                   */
    int         mem;    /* MEM_*                                   */
    Type        type;
    int         depth;  /* 0=global, 1=fn body, 2+=nested blocks   */
    Symbol     *owner;  /* enclosing function/struct, or NULL      */
    union {
        Symbols args;       /* CLS_FUNC  : parameter list          */
        Symbols members;    /* CLS_STRUCT: member list             */
    };
};

/* Global flat symbol table */
extern Symbols symTable;

/* ── Function declarations ───────────────────────────────── */
void err(const char *fmt, ...);
void tkerr(const Token *tk, const char *fmt, ...);

char  *loadFile(const char *path);
void   tokenize(const char *src, int show_output);
void   parse(int show_output);
void   domainAnalysis(int show_output);
void   freeTokens(void);

/* Symbol table helpers (implemented in DomainAnalyzer.c) */
void    initSymbols(Symbols *s);
Symbol *newSymbol(const char *name, int cls);
Symbol *dupSymbol(const Symbol *src);
void    addSymbolToList(Symbols *list, Symbol *sym);
Symbol *addSymbolToDomain(Symbols *domain, Symbol *sym);
Symbol *findSymbol(const char *name);
Symbol *findSymbolInDomain(Symbols *domain, const char *name);
void    pushDomain(void);
void    dropDomain(void);

#endif /* COMPILER_H */