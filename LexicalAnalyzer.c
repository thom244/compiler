#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "compiler.h"

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
                pStartCh = pCrtCh;
                pCrtCh++;
                state = 100;                       
            }
            else if (ch == '0') {
                pStartCh = pCrtCh;
                pCrtCh++;
                state = 11;                        
            } else if (ch >= '1' && ch <= '9') {
                pStartCh = pCrtCh;
                pCrtCh++;
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
            else if (ch == '!') {
                pCrtCh++;
                state = 200;                       
            }
            else if (ch == '&') {
                pCrtCh++;
                state = 210;                       
            }
            else if (ch == '|') {
                pCrtCh++;
                state = 220;                       
            }
            else if (ch == '=') {
                pCrtCh++;
                state = 230;                       
            }
            else if (ch == '<') {
                pCrtCh++;
                state = 240;                       
            }
            else if (ch == '>') {
                pCrtCh++;
                state = 250;                       
            }
            else if (ch == '/') {
                pCrtCh++;
                state = 300;
            }
            else if (ch == ',')  { pCrtCh++; addTk(COMMA);     return COMMA;     }
            else if (ch == ';')  { pCrtCh++; addTk(SEMICOLON); return SEMICOLON; }
            else if (ch == '(')  { pCrtCh++; addTk(LPAR);      return LPAR;      }
            else if (ch == ')')  { pCrtCh++; addTk(RPAR);      return RPAR;      }
            else if (ch == '[')  { pCrtCh++; addTk(LBRACKET);  return LBRACKET;  }
            else if (ch == ']')  { pCrtCh++; addTk(RBRACKET);  return RBRACKET;  }
            else if (ch == '{')  { pCrtCh++; addTk(LACC);      return LACC;      }
            else if (ch == '}')  { pCrtCh++; addTk(RACC);      return RACC;      }
            else if (ch == '\0') { addTk(END); return END; }
            else {
                tkerr(addTk(END), "invalid character '%c'", ch);
            }
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
        case 4: {
            tk = addTk(CT_INT);
            tk->i = strtol(pStartCh, NULL, 10);
            return CT_INT;
        }

        case 5:
            if (isdigit((unsigned char)ch)) {pCrtCh++; state = 6;}
            else tkerr(addTk(END), "invalid real constant");
            break;
        case 6:
            if (isdigit((unsigned char)ch)) pCrtCh++;
            else if (ch=='e'||ch=='E') { pCrtCh++; state = 8; }
            else state = 7;
            break;
        case 7: {
            tk = addTk(CT_REAL);
            tk->r = atof(pStartCh);
            return CT_REAL;
        }
        case 8:
            if (ch=='+'||ch=='-') {pCrtCh++; state = 9;}
            else if(isdigit((unsigned char)ch)) {pCrtCh++; state = 10;}
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
        case 19: {
            tk = addTk(CT_INT);
            tk->i = strtol(pStartCh, NULL, 16);
            return CT_INT;
        }

        case 20:
            if (ch>='0'&&ch<='7') pCrtCh++;
            else state = 21;
            break;
        case 21: {
            if (ch>='8'&&ch<='9')
                tkerr(addTk(END), "invalid octal digit '%c'", ch);
            tk = addTk(CT_INT);
            tk->i = strtol(pStartCh, NULL, 8);
            return CT_INT;
        }
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
        case 14: {
            tk = addTk(CT_CHAR);
            tk->i = (long int)(unsigned char)*pStartCh;
            return CT_CHAR;
        }
        case 15:
            if (ch == '"')       { pCrtCh++; state = 16; }
            else if (ch == '\0') tkerr(addTk(END), "unterminated string");
            else if (ch == '\\') { pCrtCh += 2; }
            else                 { pCrtCh++;    }
            break;
        case 16: {
            tk = addTk(CT_STRING);
            tk->text = createString(pStartCh, pCrtCh - 1);
            return CT_STRING;
        }

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
            if (ch == '/') {                              
                pCrtCh++;
                state = 301;
            } else {
                addTk(DIV); return DIV;
            }
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
    if (show_output) {
        showTokens();
    }
}