#include <stdio.h>
#include <stdlib.h>
#include "compiler.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <source_file> [showLex] [showSyn] [showDom]\n", argv[0]);
        return 1;
    }

    int show_lex = 0;
    int show_syn = 0;
    int show_dom = 0;
    if (argc > 2) show_lex = atoi(argv[2]);
    if (argc > 3) show_syn = atoi(argv[3]);
    if (argc > 4) show_dom = atoi(argv[4]);

    char *src = loadFile(argv[1]);

    tokenize(src, show_lex);
    parse(show_syn);
    domainAnalysis(show_dom);

    freeTokens();
    free(src);
    return 0;
}