#include <stdio.h>
#include <stdlib.h>
#include "compiler.h"
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

int expr(void);
int stm(void);
int stmCompound(void);
int typeBase(void);
int arrayDecl(void);
int exprAssign(void);
int exprOr(void);
int exprAnd(void);
int exprEq(void);
int exprRel(void);
int exprAdd(void);
int exprMul(void);
int exprCast(void);
int exprUnary(void);
int exprPostfix(void);
int exprPrimary(void);

int typeBase(void) {
    Token *startTk = crtTk;
    if (consume(INT))    return 1;
    if (consume(DOUBLE)) return 1;
    if (consume(CHAR))   return 1;
    if (consume(STRUCT)) {
        if (!consume(ID)) tkerr(crtTk, "expected identifier after 'struct'");
        return 1;
    }
    crtTk = startTk;
    return 0;
}

int arrayDecl(void) {
    if (!consume(LBRACKET)) return 0;

    if (!expr()) {
    }

    if (!consume(RBRACKET)) {
        tkerr(crtTk, "missing ']' in array declaration");
    }
    
    return 1;
}

int varDef(void) {
    Token *startTk = crtTk;

    if (!typeBase()) return 0;

    if (!consume(ID)) {
        crtTk = startTk;
        return 0;
    }
    arrayDecl();

    while (consume(COMMA)) {
        if (!consume(ID)) {
            tkerr(crtTk, "expected identifier after ','");
        }
        arrayDecl();
    }

    if (!consume(SEMICOLON)) {
        tkerr(crtTk, "missing ';' after variable declaration");
    }

    return 1;
}

int fnParam(void) {
    Token *startTk = crtTk;
    if (!typeBase()) return 0;
    if (!consume(ID)) { crtTk = startTk; return 0; }
    arrayDecl();
    return 1;
}

int stmCompound(void) {
    if (!consume(LACC)) return 0;
    while (1) {
        if (varDef()) continue;
        if (stm())    continue;
        break;
    }
    if (!consume(RACC)) tkerr(crtTk, "missing '}' or syntax error in compound statement");
    return 1;
}

int expr(void) {
    return exprAssign();
}

int exprAssign(void) {
    Token *startTk = crtTk;
    if (exprUnary()) {
        if (consume(ASSIGN)) {
            if (!exprAssign()) tkerr(crtTk, "invalid expression after '='");
            return 1;
        }
        crtTk = startTk;
    }
    return exprOr();
}

int exprOr1(void) {
    if (consume(OR)) {
        if (!exprAnd()) tkerr(crtTk, "invalid expression after '||'");
        exprOr1();
        return 1;
    }
    return 0;
}
int exprOr(void) {
    if (!exprAnd()) return 0;
    exprOr1();
    return 1;
}

int exprAnd1(void) {
    if (consume(AND)) {
        if (!exprEq()) tkerr(crtTk, "invalid expression after '&&'");
        exprAnd1();
        return 1;
    }
    return 0;
}
int exprAnd(void) {
    if (!exprEq()) return 0;
    exprAnd1();
    return 1;
}

int exprEq1(void) {
    if (consume(EQUAL) || consume(NOTEQ)) {
        if (!exprRel()) tkerr(crtTk, "invalid expression after equality operator");
        exprEq1();
        return 1;
    }
    return 0;
}
int exprEq(void) {
    if (!exprRel()) return 0;
    exprEq1();
    return 1;
}

int exprRel1(void) {
    if (consume(LESS) || consume(LESSEQ) || consume(GREATER) || consume(GREATEREQ)) {
        if (!exprAdd()) tkerr(crtTk, "invalid expression after relational operator");
        exprRel1();
        return 1;
    }
    return 0;
}
int exprRel(void) {
    if (!exprAdd()) return 0;
    exprRel1();
    return 1;
}

int exprAdd1(void) {
    if (consume(ADD) || consume(SUB)) {
        if (!exprMul()) tkerr(crtTk, "invalid expression after '+' or '-'");
        exprAdd1();
        return 1;
    }
    return 0;
}
int exprAdd(void) {
    if (!exprMul()) return 0;
    exprAdd1();
    return 1;
}

int exprMul1(void) {
    if (consume(MUL) || consume(DIV)) {
        if (!exprCast()) tkerr(crtTk, "invalid expression after '*' or '/'");
        exprMul1();
        return 1;
    }
    return 0;
}
int exprMul(void) {
    if (!exprCast()) return 0;
    exprMul1();
    return 1;
}

int exprCast(void) {
    Token *startTk = crtTk;
    if (consume(LPAR)) {
        if (typeBase()) {
            arrayDecl();
            if (!consume(RPAR)) tkerr(crtTk, "missing ')' in cast expression");
            if (!exprCast()) tkerr(crtTk, "invalid expression after cast");
            return 1;
        }
        crtTk = startTk;
    }
    return exprUnary();
}

int exprUnary(void) {
    if (consume(SUB) || consume(NOT)) {
        if (!exprUnary()) tkerr(crtTk, "invalid expression after unary operator");
        return 1;
    }
    return exprPostfix();
}

int exprPostfix1(void) {
    if (consume(LBRACKET)) {
        if (!expr()) tkerr(crtTk, "invalid expression inside '[]'");
        if (!consume(RBRACKET)) tkerr(crtTk, "missing ']'");
        exprPostfix1();
        return 1;
    }
    if (consume(DOT)) {
        if (!consume(ID)) tkerr(crtTk, "missing identifier after '.'");
        exprPostfix1();
        return 1;
    }
    return 0;
}
int exprPostfix(void) {
    if (!exprPrimary()) return 0;
    exprPostfix1();
    return 1;
}

int exprPrimary(void) {
    Token *startTk = crtTk;

    if (consume(ID)) {
        if (consume(LPAR)) {
            if (expr()) {
                while (consume(COMMA)) {
                    if (!expr()) tkerr(crtTk, "invalid expression after ','");
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

    if (consume(LPAR)) {
        if (!expr()) tkerr(crtTk, "invalid expression after '('");
        if (!consume(RPAR)) tkerr(crtTk, "missing ')'");
        return 1;
    }

    crtTk = startTk;
    return 0;
}


int stm(void) {
    Token *startTk = crtTk;

    if (stmCompound()) return 1;

    if (consume(IF)) {
        if (!consume(LPAR))   tkerr(crtTk, "missing '(' after 'if'");
        if (!expr())          tkerr(crtTk, "missing or invalid expression in 'if'");
        if (!consume(RPAR))   tkerr(crtTk, "missing ')' after 'if' condition");
        if (!stm())           tkerr(crtTk, "missing statement in 'if'");
        if (consume(ELSE)) {
            if (!stm())       tkerr(crtTk, "missing statement after 'else'");
        }
        return 1;
    }

    if (consume(WHILE)) {
        if (!consume(LPAR))   tkerr(crtTk, "missing '(' after 'while'");
        if (!expr())          tkerr(crtTk, "invalid expression in 'while'");
        if (!consume(RPAR))   tkerr(crtTk, "missing ')' after 'while' condition");
        if (!stm())           tkerr(crtTk, "missing statement in 'while'");
        return 1;
    }

    if (consume(FOR)) {
        if (!consume(LPAR))   tkerr(crtTk, "missing '(' after 'for'");
        expr();              
        if (!consume(SEMICOLON)) tkerr(crtTk, "missing ';' in 'for'");
        expr();               
        if (!consume(SEMICOLON)) tkerr(crtTk, "missing ';' in 'for'");
        expr();               
        if (!consume(RPAR))   tkerr(crtTk, "missing ')' in 'for'");
        if (!stm())           tkerr(crtTk, "missing statement in 'for'");
        return 1;
    }

    if (consume(BREAK)) {
        if (!consume(SEMICOLON)) tkerr(crtTk, "missing ';' after 'break'");
        return 1;
    }

    if (consume(RETURN)) {
        expr();  
        if (!consume(SEMICOLON)) tkerr(crtTk, "missing ';' after 'return'");
        return 1;
    }

    expr();
    if (consume(SEMICOLON)) return 1;

    crtTk = startTk;
    return 0;
}

int fnDef(void) {
    Token *startTk = crtTk;
    int hasType = typeBase();
    if (!hasType && !consume(VOID)) return 0;
    if (!consume(ID)) { crtTk = startTk; return 0; }
    if (!consume(LPAR)) { crtTk = startTk; return 0; }

    if (fnParam()) {
        while (consume(COMMA)) {
            if (!fnParam()) tkerr(crtTk, "invalid parameter after ','");
        }
    }
    if (!consume(RPAR)) tkerr(crtTk, "missing ')' in function definition");
    if (!stmCompound()) tkerr(crtTk, "missing function body");
    return 1;
}

int structDef(void) {
    Token *startTk = crtTk;
    if (!consume(STRUCT)) return 0;
    if (!consume(ID)) { crtTk = startTk; return 0; }
    if (!consume(LACC)) { crtTk = startTk; return 0; }
    while (varDef()) {}
    if (!consume(RACC))      tkerr(crtTk, "missing '}' in struct definition");
    if (!consume(SEMICOLON)) tkerr(crtTk, "missing ';' after struct definition");
    return 1;
}

void unit(void) {
    while (1) {
        if (structDef()) continue;
        if (fnDef())     continue;
        if (varDef())    continue;
        break;
    }
    if (!consume(END)) tkerr(crtTk, "unexpected token at top level");
}


void parse(int show_output) {
    crtTk = tokens;
    unit();
    if (show_output) {
        printf("Syntax OK\n");
    }
}
