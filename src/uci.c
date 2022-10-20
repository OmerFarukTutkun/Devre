#include "uci.h"
int string_compare(char* str1, char* str2, int size);
void go(char* line, SearchInfo *info, Position *pos) {

	int depth = MAX_DEPTH, movestogo = 8 ,movetime = -1 ;
	int time = 20000000, inc = 0;
	uint64_t fixed_nodes = ((uint64_t)1 ) << 40;
    char *ptr = NULL;
	info->quit = 0;

	if ((ptr = strstr(line,"infinite"))) {
		movetime =200000000;
	}
	if ((ptr = strstr(line,"binc")) && pos->side == BLACK) {
		inc = atoi(ptr + 5);
	}

	if ((ptr = strstr(line,"winc")) && pos->side == WHITE) {
		inc = atoi(ptr + 5);
	}

	if ((ptr = strstr(line,"wtime")) &&pos->side == WHITE) {
		info->search_type = NORMAL_GAME;
		time = atoi(ptr + 6);
	}

	if ((ptr = strstr(line,"btime")) &&pos->side == BLACK) {
		info->search_type = NORMAL_GAME;
		time = atoi(ptr + 6);
	}

	if ((ptr = strstr(line,"movestogo"))) {
		movestogo = MIN(25, atoi(ptr + 10)/2 );
	}

	if ((ptr = strstr(line,"movetime"))) {
		info->search_type = FIX_TIME;
		movetime = atoi(ptr + 9);
	}

	if ((ptr = strstr(line,"depth"))) {
		info->search_type = FIX_DEPTH;
		depth = atoi(ptr + 6);
		if(depth > MAX_DEPTH)
			depth= MAX_DEPTH;
	}
	if ((ptr = strstr(line,"nodes"))) {
		info->search_type = FIX_NODES;
		if(atoi(ptr +6) > 0)
			fixed_nodes = atoi(ptr + 6);
	}
	info->start_time = timeInMilliseconds();
	info->fixed_depth = depth;
	info->stopped = FALSE;
	info->fixed_nodes = fixed_nodes;
	if(inc == 0 && movestogo==8)
	{
		movestogo = 25;
	}
	if(time != -1) {
		time /= movestogo+1;
		info->stop_time = info->start_time + time + inc;
	}
	if(movetime != -1) {
		info->stop_time = info->start_time + movetime;
	}
	search(pos, info);
	fflush(stdout);
}

// position fen fenstr
// position startpos
// ... moves e2e4 e7e5 b7b8q
void set_position(char* lineIn, Position *pos) {
	lineIn += 9;
    char *ptrChar = lineIn;
    if(strncmp(lineIn, "startpos", 8) == 0){
        fen_to_board(pos,STARTING_FEN);
		pos->threat = allAttackedSquares(pos, !pos->side); 
    } else {
        ptrChar = strstr(lineIn, "fen");
        if(ptrChar == NULL) {
            fen_to_board(pos, STARTING_FEN);
			pos->threat = allAttackedSquares(pos, !pos->side); 
        } else {
            ptrChar+=4;
            fen_to_board(pos , ptrChar);
			pos->threat = allAttackedSquares(pos, !pos->side); 
        }
    }
	initZobristKey(pos);
	pos->pos_history.index = 0;
	pos->pos_history.keys[pos->pos_history.index++] = pos->key;
	ptrChar = strstr(lineIn, "moves");
	uint16_t move;
	if(ptrChar != NULL) {
        ptrChar += 6;
        while(*ptrChar) {
              move = string_to_move(pos, ptrChar);
			  make_move(pos, move);
			  if(pos->half_move== 0)
			  {
				  	pos->pos_history.index = 0;
					pos->pos_history.keys[pos->pos_history.index++] = pos->key;
			  }
			  while(*ptrChar && *ptrChar!= ' ') ptrChar++;
              ptrChar++;
        }
    }
	fflush(stdout);
}
void Uci_Loop() {
	setbuf(stdin, NULL);
	setbuf(stdout, NULL);
	char line[8000];
	char *ptr = NULL;
	fflush(stdout);

    init_attacks();
    set_weights();
    init_keys();

    Position pos;
    fen_to_board(&pos, STARTING_FEN); 

    initZobristKey(&pos);

    SearchInfo info;
	memset(&info, 0, sizeof(SearchInfo));
    info.quit = FALSE;

	tt_init(16);

	set_position("position startpos\n", &pos);
	printf("id name Devre %s\n" ,VERSION);
    printf("id author Omer Faruk Tutkun\n");
	fflush(stdout);

	while (TRUE) {
		memset(&line[0], 0, sizeof(line));
		fflush(stdout);
		if(info.quit)
			break;
        if (!fgets(line, 8000, stdin))
        	continue;
        if (line[0] == '\n')
        	continue;
        else if (string_compare(line, "isready", 7)) {
            printf("readyok\n");
			fflush(stdout);
            continue;
        }else if (string_compare(line, "position", 8)) {
            set_position(line, &pos);
        }else  if (string_compare(line, "ucinewgame", 10)) {
			tt_clear();
			memset(&pos, 0 ,sizeof(Position));
			memset(&pos, 0 ,sizeof(SearchInfo));
			fen_to_board(&pos, STARTING_FEN); 
			initZobristKey(&pos);

        }else if (string_compare(line, "go", 2)) {
            go(line , &info, &pos);
		}
		else if (string_compare(line, "see move", 8)) {
            uint16_t move = string_to_move(&pos, line +9);
			assert(move);
			printf("see value:  %d\n", SEE(&pos, move));
        }else if (string_compare(line, "quit", 4)) {
            break;
        }else if (string_compare(line, "uci", 3)) {
			printf("id name Devre %s\n", VERSION);
    		printf("id author Omer Faruk Tutkun\n");
			printf("option name Hash type spin default 16 min 1 max 4096\n");
			printf("option name UCI_Chess960 type check default false\n");
            printf("uciok\n");
			fflush(stdout);
        }
		 else if (string_compare(line, "setoption name Hash value ", 26)) {
             tt_init(atoi(line + 26));
		}
		else if (( ptr = strstr(line,"perft"))) 
		{
			perft_test(&pos,atoi(ptr + 6));
		}
		else  if ((ptr = strstr(line,"print"))) 
		{
			print_Board(&pos);
		}
		else if (string_compare(line, "setoption name UCI_Chess960 value ", 34)) {
			if( string_compare( line + 34 ,"true" ,4) )
			{
            	pos.frc = TRUE;
			}
			else
				pos.frc = FALSE;
		}
		else if ((ptr = strstr(line,"eval"))) 
		{
			int score = evaluate_nnue(&pos);
			char fen_notation[15]="PNBRQKpnbrqk";
			for(int i=7 ; i >=0 ;i--)
			{
				printf("\n  |-------|-------|-------|-------|-------|-------|-------|-------|\n" );
				for(int j = 0; j <8 ;j++)
				{
					if(pos.board[8*i + j] != EMPTY)
						printf("%8c",fen_notation[ pos.board[ 8*i + j]]);
					else
						printf("%8c",' ');
				}
				printf("%8d\n", i+1);
				for(int j = 0; j <8 ;j++)
				{
					uint8_t sq = 8*i+ j;
					if(pos.board[sq] != EMPTY && pos.board[sq] != BLACK_KING && pos.board[sq] != KING)
					{
						uint8_t piece = pos.board[sq];
						remove_piece(&pos, piece , sq);
						printf("%8.2f",(score - evaluate_nnue(&pos))/100.0);
						add_piece(&pos, piece , sq);
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
		else
		{
			printf("unknown command\n");
		}
    }
	tt_free();
}
int string_compare(char* str1, char* str2, int size)
{
	for(int i=0; i<size ; i++ )
	{
		if(str1[i] != str2[i])
			return FALSE;
	}
	return TRUE;
}