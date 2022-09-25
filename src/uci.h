#ifndef _UCI_H_
#define _UCI_H_

#include "search.h"
#include "tt.h"
#include "perft.h"

static const char VERSION[] = "4.12";
void Uci_Loop();
int string_compare(char* str1, char* str2, int size);
void go(char* line, SearchInfo *info, Position *pos);
#endif
