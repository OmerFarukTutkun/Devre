#ifndef _UCI_H_
#define _UCI_H_

#include "search.h"
#include "hash.h"
#include "test.h"
// The library forked from Vice chess engine.
void go(char* line, search_info *info, Position *pos ,Stack* stack) {

	int depth = MAX_DEPTH, movestogo = 30 ,movetime = -1;
	int time = 20000000, inc = 0;
    char *ptr = NULL;
	info->quit = 0;

	if ((ptr = strstr(line,"infinite"))) {
		movetime =200000000;
	}
	if ((ptr = strstr(line,"binc")) && pos->side_to_move == 1) {
		inc = atoi(ptr + 5);
	}

	if ((ptr = strstr(line,"winc")) && pos->side_to_move == 0) {
		inc = atoi(ptr + 5);
	}

	if ((ptr = strstr(line,"wtime")) &&pos->side_to_move == 0) {
		time = atoi(ptr + 6);
	}

	if ((ptr = strstr(line,"btime")) &&pos->side_to_move == 1) {
		time = atoi(ptr + 6);
	}

	if ((ptr = strstr(line,"movestogo"))) {
		movestogo = atoi(ptr + 10);
	}

	if ((ptr = strstr(line,"movetime"))) {
		movetime = atoi(ptr + 9);
	}

	if ((ptr = strstr(line,"depth"))) {
		depth = atoi(ptr + 6);
		if(depth > MAX_DEPTH)
			depth= MAX_DEPTH;
	}
	info->start_time = clock();
	info->depth = depth;
	info->stopped = FALSE;

	if(time != -1) {
		time /= movestogo+1;
		info->stop_time = info->start_time + time + inc;
	}
	if(movetime != -1) {
		info->stop_time = info->start_time + movetime;
	}
	search(pos,stack,info);
	fflush(stdout);
}

// position fen fenstr
// position startpos
// ... moves e2e4 e7e5 b7b8q
void set_position(char* lineIn, Position *pos ,Stack* stack) {
	stack->top = 0;
	lineIn += 9;
    char *ptrChar = lineIn;
    if(strncmp(lineIn, "startpos", 8) == 0){
        fen_to_board(pos,STARTING_FEN);
    } else {
        ptrChar = strstr(lineIn, "fen");
        if(ptrChar == NULL) {
            fen_to_board(pos, STARTING_FEN);
        } else {
            ptrChar+=4;
            fen_to_board(pos , ptrChar);
        }
    }
	set_PieceList(pos);
	initZobristKey(pos);
	pos_history.n=0;
	pos_history.position_keys[pos_history.n++] = pos->key;
	ptrChar = strstr(lineIn, "moves");
	uint16_t move;
	int k=0;
	if(ptrChar != NULL) {
        ptrChar += 6;
        while(*ptrChar) {
              move = string_to_move(pos, ptrChar);
			  make_move(pos, move ,stack);
			  if(pos->half_move== 0)
			  {
				  	pos_history.n=0;
					pos_history.position_keys[pos_history.n++] = pos->key;
			  }
			  while(*ptrChar && *ptrChar!= ' ') ptrChar++;
              ptrChar++;
        }
    }
	stack->top = 0;
	fflush(stdout);
}

void Uci_Loop() {

	setbuf(stdin, NULL);
	setbuf(stdout, NULL);
	
	char line[2500];
	char *ptr = NULL;
	fflush(stdout);
	Position * pos= (Position*)malloc(sizeof(Position));
	search_info *info=(search_info*)malloc(sizeof(search_info));
	Stack* stack=createStack();
	int length;

	init_keys(); 
	set_weights();
	hash_table= (TTentry * )malloc(sizeof(TTentry)*HASH_SIZE);
	memset(hash_table,0, 16*HASH_SIZE);
	printf("id name Devre 1.6\n");
    printf("id author Omer Faruk Tutkun\n");
	fflush(stdout);
	int l=0;

	while (TRUE) {
		memset(&line[0], 0, sizeof(line));
		fflush(stdout);
        if (!fgets(line, 2500, stdin))
        	continue;
        if (line[0] == '\n')
        	continue;
        if (!strncmp(line, "isready", 7)) {
            printf("readyok\n");
			fflush(stdout);
            continue;
        } else if (!strncmp(line, "position", 8)) {
            set_position(line, pos,stack);
        } else if (!strncmp(line, "ucinewgame", 10)) {
			for(int i=0; i<HASH_SIZE ; i++)//clear hash table
			{
				hash_table[i].key = 0;
			}
			for(int i=0 ; i<2 ; i++)//clear history heuristic
			{
				for(int j=0 ; j<64; j++)
				{
					for(int k=0 ; k<64 ; k++)
					{
						pos->history[i][j][k] =0;
						pos->counter_moves[i][j][k] = 0;
					}
				}
			}
        } else if (!strncmp(line, "go", 2)) {
            go(line , info,pos,stack );
        } else if (!strncmp(line, "quit", 4)) {
            break;
        } else if (!strncmp(line, "uci", 3)) {
			printf("id name Devre 1.6\n");
    		printf("id author Omer Faruk Tutkun\n");
			printf("option name Hash type spin default 16 min 2 max 512\n");
            printf("uciok\n");
			fflush(stdout);
        }
		else if (!strncmp(line, "setoption name Hash value ", 26)) {
			int hash_size = atoi(line + 26);//in mb
			if(hash_size < 2)
				hash_size =2;
			else if(hash_size > 512)
				hash_size=512;
			hash_size = 1<<(int)(log(hash_size + 0.1)/log(2.0));
			HASH_SIZE= hash_size*1024*1024 / 16;// HASH_SIZE = number of TT entry,  size of one entry is 16 byte.
			HASH_SHIFT= 64 - 16 - (int)(log(hash_size + 0.1)/log(2.0));
			free(hash_table);
			hash_table =(TTentry*) malloc( HASH_SIZE*sizeof(TTentry));	
			if(hash_table == NULL)
			{
				printf("malloc returns NULL\n");
			}	
			else
			{
				for(int i=0; i<HASH_SIZE ; i++)//clear hash table
				{
					hash_table[i].key = 0;
				}
				printf("Hash table size %dMB\n" ,hash_size);
				fflush(stdout);
			}	
		}
		else if (( ptr = strstr(line,"perft"))) 
		{
			perft_test(pos, stack,atoi(ptr + 6));
		}
		else if ((ptr = strstr(line,"print"))) 
		{
			print_Board(pos);
		}
		else if ((ptr = strstr(line,"eval"))) 
		{
			int score=evaluate_network(pos);
			char fen_notation[15]=" PNBRQK  pnbrqk";
			for(int i=9 ; i>1 ;i--)
			{
				printf("\n  |-------|-------|-------|-------|-------|-------|-------|-------|\n");
				for(int j = 8; j >0 ;j--)
					printf("%8c",fen_notation[ pos->board[ 10*i + j]]);
				printf("\n");
				for(int j = 8; j >0 ;j--)
				{
					uint8_t sq = 10*i+ j;
					if(pos -> board[sq] != EMPTY && pos->board[sq] != B_KING && pos->board[sq] != KING)
					{
						uint8_t piece = pos->board[sq];
						pos->board[sq] = 0;
						delete_piece(piece, sq);
						printf("%8.2f",(score -evaluate_network(pos))/100.0);
						pos->board[sq] = piece;
						add_piece(piece, sq);
					}
					else
					{
						printf("%8c",' ');
					}
				}
			}
			printf("\n  |-------|-------|-------|-------|-------|-------|-------|-------|\n");
			printf("\n%8c%8c%8c%8c%8c%8c%8c%8c", 'a','b','c', 'd', 'e','f', 'g' , 'h');
			printf("\n\nScore: %.2f\n" ,score/100.0);
		}
    }
	free(hash_table);
	free(pos);
	free(stack);
	free(info);
}
#endif
