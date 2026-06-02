#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"

Symbols symTable;
static int crtDepth = 0;

static Symbol *owner = NULL;

#define MAX_DOMAIN_DEPTH 128
static int domainMarks[MAX_DOMAIN_DEPTH];
static int domainTop = 0;

void initSymbols(Symbols *s) {
    s->begin = s->end = s->after = NULL;
}

void addSymbolToList(Symbols *list, Symbol *sym) {
    if (list->end == list->after) {
        int count = (int)(list->after - list->begin);
        int n = count ? count * 2 : 1;
        list->begin = (Symbol **)realloc(list->begin, (size_t)n * sizeof(Symbol *));
        if (!list->begin) err("not enough memory");
        list->end = list->begin + count;
        list->after = list->begin + n;
    }
    *list->end++ = sym;
}

Symbol *newSymbol(const char *name, int cls) {
    Symbol *s = (Symbol *)malloc(sizeof(Symbol));
    if (!s) err("not enough memory");
    memset(s, 0, sizeof(Symbol));
    s->name  = name;
    s->cls   = cls;
    s->depth = crtDepth;
    s->owner = owner;
    initSymbols(&s->args);
    return s;
}

Symbol *dupSymbol(const Symbol *src) {
    Symbol *d = (Symbol *)malloc(sizeof(Symbol));
    if (!d) err("not enough memory");
    *d = *src;
    return d;
}

Symbol *addSymbolToDomain(Symbols *domain, Symbol *sym) {
    addSymbolToList(domain, sym);
    return sym;
}

Symbol *findSymbolInDomain(Symbols *domain, const char *name) {
    if (domain->begin == domain->end) return NULL;
    Symbol **p;
    for (p = domain->end - 1; p >= domain->begin; p--) {
        if ((*p)->depth == crtDepth && strcmp((*p)->name, name) == 0)
            return *p;
    }
    return NULL;
}

Symbol *findSymbol(const char *name) {
    if (symTable.begin == symTable.end) return NULL;
    Symbol **p;
    for (p = symTable.end - 1; p >= symTable.begin; p--) {
        if (strcmp((*p)->name, name) == 0)
            return *p;
    }
    return NULL;
}

void pushDomain(void) {
    if (domainTop >= MAX_DOMAIN_DEPTH) err("domain stack overflow");
    domainMarks[domainTop++] = (int)(symTable.end - symTable.begin);
    crtDepth++;
}

void dropDomain(void) {
    if (domainTop <= 0) err("domain stack underflow");
    int markIdx = domainMarks[--domainTop];
    Symbol **mark = symTable.begin + markIdx;
    Symbol **p;
    for (p = mark; p < symTable.end; p++) {
        free(*p);
    }
    symTable.end = mark;
    crtDepth--;
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
static const char *tbName(int tb) {
    switch (tb) {
    case TB_INT:    return "int";
    case TB_DOUBLE: return "double";
    case TB_CHAR:   return "char";
    case TB_STRUCT: return "struct";
    case TB_VOID:   return "void";
    default:        return "?";
    }
}

static void printSymbols(void) {
    Symbol **p;
    printf("\nSymbol table (%d symbols):\n",
           (int)(symTable.end - symTable.begin));
    for (p = symTable.begin; p < symTable.end; p++) {
        Symbol *s = *p;
        printf("  %-20s  cls=%-8s  mem=%-8s  type=%s",
               s->name, clsName(s->cls), memName(s->mem), tbName(s->type.tb));
        if (s->type.tb == TB_STRUCT && s->type.s)
            printf("(%s)", s->type.s->name);
        if (s->type.n >= 0)
            printf("[%d]", s->type.n);
        printf("  depth=%d\n", s->depth);
    }
}

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

static int da_typeBase(Type *t);
static int da_arrayDecl(Type *t);
static int da_varDef(void);
static int da_fnParam(void);
static int da_stmCompound(int newDomain);
static int da_stm(void);
static int da_expr(void);
static int da_exprAssign(void);
static int da_exprOr(void);
static int da_exprOr1(void);
static int da_exprAnd(void);
static int da_exprAnd1(void);
static int da_exprEq(void);
static int da_exprEq1(void);
static int da_exprRel(void);
static int da_exprRel1(void);
static int da_exprAdd(void);
static int da_exprAdd1(void);
static int da_exprMul(void);
static int da_exprMul1(void);
static int da_exprCast(void);
static int da_exprUnary(void);
static int da_exprPostfix(void);
static int da_exprPostfix1(void);
static int da_exprPrimary(void);

static int da_typeBase(Type *t) {
    t->n = -1;
    if (consume(INT))    { t->tb = TB_INT;    return 1; }
    if (consume(DOUBLE)) { t->tb = TB_DOUBLE; return 1; }
    if (consume(CHAR))   { t->tb = TB_CHAR;   return 1; }
    if (consume(STRUCT)) {
        if (!consume(ID)) tkerr(crtTk, "expected identifier after 'struct'");
        Token *tkName = consumedTk;
        t->tb = TB_STRUCT;
        t->s  = findSymbol(tkName->text);
        if (!t->s)
            tkerr(tkName, "undefined struct: %s", tkName->text);
        return 1;
    }
    return 0;
}

static int da_arrayDecl(Type *t) {
    if (!consume(LBRACKET)) return 0;
    if (consume(CT_INT)) {
        t->n = (int)consumedTk->i;
    } else {
        t->n = 0;   /* unsized array */
    }
    if (!consume(RBRACKET)) tkerr(crtTk, "missing ']' in array declaration");
    return 1;
}

static int da_varDef(void) {
    Token *startTk = crtTk;
    Type t;

    if (!da_typeBase(&t)) return 0;
    if (!consume(ID)) { crtTk = startTk; return 0; }


#define ADD_VAR(tkName_, localType_) do {                               \
    Symbol *var = findSymbolInDomain(&symTable, (tkName_)->text);       \
    if (var) tkerr((tkName_), "symbol redefinition: %s", (tkName_)->text); \
    var = newSymbol((tkName_)->text, CLS_VAR);                          \
    var->type = (localType_);                                           \
    if (owner) {                                                        \
        if (owner->cls == CLS_STRUCT) {                                 \
            var->mem = MEM_LOCAL;                                       \
            addSymbolToList(&owner->members, dupSymbol(var));           \
        } else {                                                        \
            var->mem = MEM_LOCAL;                                       \
        }                                                               \
    } else {                                                            \
        var->mem = MEM_GLOBAL;                                          \
    }                                                                   \
    addSymbolToDomain(&symTable, var);                                  \
} while(0)

    Token *tkName = consumedTk;
    Type   localType = t;

    if (da_arrayDecl(&localType)) {
        if (localType.n == 0 && owner && owner->cls != CLS_FUNC)
            tkerr(crtTk, "a vector variable must have a specified dimension");
    }
    ADD_VAR(tkName, localType);

    while (consume(COMMA)) {
        if (!consume(ID)) tkerr(crtTk, "expected identifier after ','");
        tkName    = consumedTk;
        localType = t;
        if (da_arrayDecl(&localType)) {
            if (localType.n == 0 && owner && owner->cls != CLS_FUNC)
                tkerr(crtTk, "a vector variable must have a specified dimension");
        }
        ADD_VAR(tkName, localType);
    }
#undef ADD_VAR

    if (!consume(SEMICOLON)) tkerr(crtTk, "missing ';' after variable declaration");
    return 1;
}

static int da_fnParam(void) {
    Token *startTk = crtTk;
    Type t;

    if (!da_typeBase(&t)) return 0;
    if (!consume(ID)) { crtTk = startTk; return 0; }
    Token *tkName = consumedTk;

    if (da_arrayDecl(&t)) {
        t.n = 0;
    }

    Symbol *param = findSymbolInDomain(&symTable, tkName->text);
    if (param) tkerr(tkName, "symbol redefinition: %s", tkName->text);

    param       = newSymbol(tkName->text, CLS_VAR);
    param->mem  = MEM_ARG;
    param->type = t;
    addSymbolToDomain(&symTable, param);

    if (owner && owner->cls == CLS_FUNC)
        addSymbolToList(&owner->args, dupSymbol(param));

    return 1;
}

static int da_stmCompound(int newDomain) {
    if (!consume(LACC)) return 0;
    if (newDomain) pushDomain();
    while (1) {
        if (da_varDef()) continue;
        if (da_stm())    continue;
        break;
    }
    if (!consume(RACC)) tkerr(crtTk, "missing '}' or syntax error in compound statement");
    if (newDomain) dropDomain();
    return 1;
}

/* ── expression predicates (no semantic actions needed here) ─ */
static int da_expr(void)       { return da_exprAssign(); }

static int da_exprAssign(void) {
    Token *startTk = crtTk;
    if (da_exprUnary()) {
        if (consume(ASSIGN)) {
            if (!da_exprAssign()) tkerr(crtTk, "invalid expression after '='");
            return 1;
        }
        crtTk = startTk;
    }
    return da_exprOr();
}

static int da_exprOr1(void) {
    if (consume(OR)) {
        if (!da_exprAnd()) tkerr(crtTk, "invalid expression after '||'");
        da_exprOr1();
        return 1;
    }
    return 0;
}
static int da_exprOr(void)  { if (!da_exprAnd()) return 0; da_exprOr1(); return 1; }

static int da_exprAnd1(void) {
    if (consume(AND)) {
        if (!da_exprEq()) tkerr(crtTk, "invalid expression after '&&'");
        da_exprAnd1();
        return 1;
    }
    return 0;
}
static int da_exprAnd(void) { if (!da_exprEq()) return 0; da_exprAnd1(); return 1; }

static int da_exprEq1(void) {
    if (consume(EQUAL) || consume(NOTEQ)) {
        if (!da_exprRel()) tkerr(crtTk, "invalid expression after equality operator");
        da_exprEq1();
        return 1;
    }
    return 0;
}
static int da_exprEq(void)  { if (!da_exprRel()) return 0; da_exprEq1(); return 1; }

static int da_exprRel1(void) {
    if (consume(LESS)||consume(LESSEQ)||consume(GREATER)||consume(GREATEREQ)) {
        if (!da_exprAdd()) tkerr(crtTk, "invalid expression after relational operator");
        da_exprRel1();
        return 1;
    }
    return 0;
}
static int da_exprRel(void) { if (!da_exprAdd()) return 0; da_exprRel1(); return 1; }

static int da_exprAdd1(void) {
    if (consume(ADD) || consume(SUB)) {
        if (!da_exprMul()) tkerr(crtTk, "invalid expression after '+'/'-'");
        da_exprAdd1();
        return 1;
    }
    return 0;
}
static int da_exprAdd(void) { if (!da_exprMul()) return 0; da_exprAdd1(); return 1; }

static int da_exprMul1(void) {
    if (consume(MUL) || consume(DIV)) {
        if (!da_exprCast()) tkerr(crtTk, "invalid expression after '*'/'/'");
        da_exprMul1();
        return 1;
    }
    return 0;
}
static int da_exprMul(void) { if (!da_exprCast()) return 0; da_exprMul1(); return 1; }

static int da_exprCast(void) {
    Token *startTk = crtTk;
    Type t;
    if (consume(LPAR)) {
        if (da_typeBase(&t)) {
            da_arrayDecl(&t);
            if (!consume(RPAR)) tkerr(crtTk, "missing ')' in cast");
            if (!da_exprCast()) tkerr(crtTk, "invalid expression after cast");
            return 1;
        }
        crtTk = startTk;
    }
    return da_exprUnary();
}

static int da_exprUnary(void) {
    if (consume(SUB) || consume(NOT)) {
        if (!da_exprUnary()) tkerr(crtTk, "invalid expression after unary operator");
        return 1;
    }
    return da_exprPostfix();
}

static int da_exprPostfix1(void) {
    if (consume(LBRACKET)) {
        if (!da_expr()) tkerr(crtTk, "invalid expression inside '[]'");
        if (!consume(RBRACKET)) tkerr(crtTk, "missing ']'");
        da_exprPostfix1();
        return 1;
    }
    if (consume(DOT)) {
        if (!consume(ID)) tkerr(crtTk, "missing identifier after '.'");
        da_exprPostfix1();
        return 1;
    }
    return 0;
}
static int da_exprPostfix(void) {
    if (!da_exprPrimary()) return 0;
    da_exprPostfix1();
    return 1;
}

static int da_exprPrimary(void) {
    Token *startTk = crtTk;
    if (consume(ID)) {
        if (consume(LPAR)) {
            if (da_expr()) {
                while (consume(COMMA))
                    if (!da_expr()) tkerr(crtTk, "invalid expression after ','");
            }
            if (!consume(RPAR)) tkerr(crtTk, "missing ')' in function call");
        }
        return 1;
    }
    if (consume(CT_INT))    return 1;
    if (consume(CT_REAL))   return 1;
    if (consume(CT_CHAR))   return 1;
    if (consume(CT_STRING)) return 1;
    if (consume(LPAR)) {
        if (!da_expr()) tkerr(crtTk, "invalid expression after '('");
        if (!consume(RPAR)) tkerr(crtTk, "missing ')'");
        return 1;
    }
    crtTk = startTk;
    return 0;
}

/* ── da_stm ──────────────────────────────────────────────── */
/*
 * stm: stmCompound[true]   <- each {...} statement opens its own domain
 *    | IF ...
 *    | WHILE ...
 *    | FOR ...
 *    | BREAK ;
 *    | RETURN expr? ;
 *    | expr? ;
 */
static int da_stm(void) {
    Token *startTk = crtTk;

    /* compound statement – opens a new domain */
    if (da_stmCompound(1)) return 1;

    if (consume(IF)) {
        if (!consume(LPAR))  tkerr(crtTk, "missing '(' after 'if'");
        if (!da_expr())      tkerr(crtTk, "invalid expression in 'if'");
        if (!consume(RPAR))  tkerr(crtTk, "missing ')' after 'if' condition");
        if (!da_stm())       tkerr(crtTk, "missing statement in 'if'");
        if (consume(ELSE))
            if (!da_stm())   tkerr(crtTk, "missing statement after 'else'");
        return 1;
    }

    if (consume(WHILE)) {
        if (!consume(LPAR))  tkerr(crtTk, "missing '(' after 'while'");
        if (!da_expr())      tkerr(crtTk, "invalid expression in 'while'");
        if (!consume(RPAR))  tkerr(crtTk, "missing ')' after 'while' condition");
        if (!da_stm())       tkerr(crtTk, "missing statement in 'while'");
        return 1;
    }

    if (consume(FOR)) {
        if (!consume(LPAR))          tkerr(crtTk, "missing '(' after 'for'");
        da_expr();
        if (!consume(SEMICOLON))     tkerr(crtTk, "missing ';' in 'for'");
        da_expr();
        if (!consume(SEMICOLON))     tkerr(crtTk, "missing ';' in 'for'");
        da_expr();
        if (!consume(RPAR))          tkerr(crtTk, "missing ')' in 'for'");
        if (!da_stm())               tkerr(crtTk, "missing statement in 'for'");
        return 1;
    }

    if (consume(BREAK)) {
        if (!consume(SEMICOLON)) tkerr(crtTk, "missing ';' after 'break'");
        return 1;
    }

    if (consume(RETURN)) {
        da_expr();   /* optional */
        if (!consume(SEMICOLON)) tkerr(crtTk, "missing ';' after 'return'");
        return 1;
    }

    da_expr();   /* optional expression statement */
    if (consume(SEMICOLON)) return 1;

    crtTk = startTk;
    return 0;
}

/* ── da_fnDef ────────────────────────────────────────────── */
/*
 * fnDef: ( typeBase[&t] | VOID {t.tb=TB_VOID;} ) ID[tkName] LPAR
 *   { check uniqueness; create CLS_FUNC symbol; pushDomain; owner=fn; }
 *   ( fnParam ( COMMA fnParam )* )? RPAR
 *   stmCompound[false]   <- function body shares the parameter domain
 *   { dropDomain; owner=NULL; }
 */
static int da_fnDef(void) {
    Token *startTk = crtTk;
    Type t;

    int hasType = da_typeBase(&t);
    if (!hasType) {
        if (!consume(VOID)) return 0;
        t.tb = TB_VOID;
        t.n  = -1;
        t.s  = NULL;
    }

    if (!consume(ID)) { crtTk = startTk; return 0; }
    Token *tkName = consumedTk;

    if (!consume(LPAR)) { crtTk = startTk; return 0; }

    /* Semantic: create function symbol */
    Symbol *fn = findSymbolInDomain(&symTable, tkName->text);
    if (fn) tkerr(tkName, "symbol redefinition: %s", tkName->text);

    fn       = newSymbol(tkName->text, CLS_FUNC);
    fn->type = t;
    fn->mem  = MEM_GLOBAL;
    addSymbolToDomain(&symTable, fn);

    /* Open the function's domain and set owner */
    owner = fn;
    pushDomain();

    /* Optional parameter list */
    if (da_fnParam()) {
        while (consume(COMMA)) {
            if (!da_fnParam()) tkerr(crtTk, "invalid parameter after ','");
        }
    }

    if (!consume(RPAR)) tkerr(crtTk, "missing ')' in function definition");

    /* Function body – does NOT open a new domain (newDomain=false) */
    if (!da_stmCompound(0)) tkerr(crtTk, "missing function body");

    dropDomain();
    owner = NULL;

    return 1;
}

/* ── da_structDef ────────────────────────────────────────── */
/*
 * structDef: STRUCT ID[tkName] LACC
 *   { check uniqueness; create CLS_STRUCT symbol; pushDomain; owner=s; }
 *   varDef* RACC SEMICOLON
 *   { owner=NULL; dropDomain(); }
 */
static int da_structDef(void) {
    Token *startTk = crtTk;
    if (!consume(STRUCT)) return 0;
    if (!consume(ID))     { crtTk = startTk; return 0; }
    Token *tkName = consumedTk;
    if (!consume(LACC))   { crtTk = startTk; return 0; }

    Symbol *s = findSymbolInDomain(&symTable, tkName->text);
    if (s) tkerr(tkName, "symbol redefinition: %s", tkName->text);

    s          = newSymbol(tkName->text, CLS_STRUCT);
    s->type.tb = TB_STRUCT;
    s->type.s  = s;
    s->type.n  = -1;
    s->mem     = MEM_GLOBAL;
    addSymbolToDomain(&symTable, s);

    owner = s;
    pushDomain();

    while (da_varDef()) {}

    if (!consume(RACC))      tkerr(crtTk, "missing '}' in struct definition");
    if (!consume(SEMICOLON)) tkerr(crtTk, "missing ';' after struct definition");

    owner = NULL;
    dropDomain();

    return 1;
}

/* ── da_unit ─────────────────────────────────────────────── */
static void da_unit(void) {
    while (1) {
        if (da_structDef()) continue;
        if (da_fnDef())     continue;
        if (da_varDef())    continue;
        break;
    }
    if (!consume(END)) tkerr(crtTk, "unexpected token at top level");
}

void domainAnalysis(int show_output) {
    initSymbols(&symTable);
    crtDepth = 0;
    owner    = NULL;
    domainTop = 0;

    crtTk = tokens;
    da_unit();

    if (show_output) {
        printSymbols();
    }
}