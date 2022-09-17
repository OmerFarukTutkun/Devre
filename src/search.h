#ifndef _SEARCH_H_
#define _SEARCH_H_

#include "move.h"
#include "nnue.h"
#include "tt.h"
#include "history.h"
#define INF  31000
#define MATE 30000

enum {NORMAL_GAME=0, FIX_DEPTH , FIX_NODES, FIX_TIME 
};

typedef struct {
  int fixed_depth;
  int fixed_nodes;
  int search_depth;
  uint64_t node_history[MAX_DEPTH];
  int16_t  bestmove_history[MAX_DEPTH];
  int16_t  score_history[MAX_DEPTH];
  int seldepth;
  int quit;
  long long int start_time;
  long long int stop_time;
  int stopped;
  int search_type;
} SearchInfo; 

int16_t qsearch(int alpha, int beta, Position* pos,SearchInfo* info);
int AlphaBeta(int alpha, int beta, Position* pos, int depth,SearchInfo* info);
void search(Position* pos, SearchInfo* info );
int UciCheck(SearchInfo* info);
#endif