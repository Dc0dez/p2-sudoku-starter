// Sudoku puzzle verifier and solver

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* 
* struct to hold data passed to validation threads
* contains starting position (row/col), puzzle size, grid reference, and result pointer
*/  
typedef struct {
	int row;
	int column;
	int psize;
	int **grid;
	bool *result;
} parameters;

/*
* validates a single row for duplicate numbers (ignores 0s)
* uses boolean array to track which numbers have been seen in the row
* thread function for pthread - sets result to false if duplicates found
*/
void *checkRow(void *arg) {
	parameters *data = (parameters *)arg;
	int row = data->row;
	int psize = data->psize;
	int **grid = data->grid;

	// initialize seen array to track which numbers (1 to psize) appear in this row
	bool seen[psize + 1];
	for (int i = 0; i <= psize; i++) {
		seen[i] = false;
	}
	// iterate through all columns in this row
	for (int col = 1; col <= psize; col++) {
		int value = grid[row][col];

		if (value == 0)	continue;
		// if value has been seen already, row is invalid (duplicate found)
		if (seen[value]) {
			*(data->result) = false;
			pthread_exit(NULL);
		}
		// else, mark value as seen
		seen[value] = true;
	}
	// no duplicates found (row is valid)
	*(data->result) = true;
	pthread_exit(NULL);
}

/*
* validates a single column for duplicate numbers (ignores 0s)
* uses boolean array to track which numbers have been seen in the column
* thread function for pthread - sets result to false if duplicates found
*/
void *checkColumn(void *arg) {
	parameters *data = (parameters *)arg;
	int col = data->column;
	int psize = data->psize;
	int **grid = data->grid;

	bool seen[psize + 1];
	for (int i = 0; i <= psize; i++) {
		seen[i] = false;
	}
	// iterate through all rows in this column
	for (int row = 1; row <= psize; row++) {
		int value = grid[row][col];
		if (value == 0)	continue;

		if (seen[value]) {
			*(data->result) = false;
			pthread_exit(NULL);
		}
		seen[value] = true;
	}
	
	*(data->result) = true;
	pthread_exit(NULL);
}

/*
* validates a single subgrid (box) for duplicate numbers (ignores 0s)
* subgrid size is sqrt(psize) * sqrt(psize), or 3x3 for a 9x9 puzzle
* thread function for pthread - sets result to false if duplicates found
*/
void *checkSubGrid(void *arg) {
	parameters *data = (parameters *)arg;
	int start_col = data->column;
	int start_row = data->row;
	int psize = data->psize;
	int **grid = data->grid;
	int box_size = (int)sqrt(psize);

	bool seen[psize+1];
	for (int i = 0; i <= psize; i++) {
		seen[i] = false;
	}
	// iterate through all cells in this box_size * box_size subgrid
	for (int r = 0; r < box_size; r++) {
		for (int c = 0; c < box_size; c++) {
			int row = start_row + r;
			int col = start_col + c;
			int value = grid[row][col];
			// skip empty cells
			if (value == 0) continue;
			// if value has been seen, subgrid is invalid
			if (seen [value]) {
				*(data->result) = false;
				pthread_exit(NULL);
			}

			seen[value] = true;
		}
	}

	*(data->result) = true;
	pthread_exit(NULL);
}

/*
* creates multiple threads to validate all rows, columns, and subgrids in parallel
* total threads = psize (rows) + psize(columns) + num_subgrids (boxes)
* waits for all threads to complete and aggregates results into *valid
*/
void createThreads(int psize, int **grid, bool *valid) {
	int box_size = (int)sqrt(psize);
	int num_subgrids = psize/box_size;
	int num_threads = psize + psize + (num_subgrids * num_subgrids);
	pthread_t threads[num_threads];
	bool results[num_threads];
	parameters thread_data[num_threads];

	// initialize all results to true
	for (int i = 0; i < num_threads; i++) {
		results[i] = true;
	}

	int thread_index = 0;
	// create one thread per row to validate each row
	for (int row = 1; row <= psize; row++) {
		thread_data[thread_index].row = row;
		thread_data[thread_index].psize = psize;
		thread_data[thread_index].grid = grid;
		thread_data[thread_index].result = &results[thread_index];

		pthread_create(&threads[thread_index], NULL, checkRow, &thread_data[thread_index]);
		thread_index++;
	}
	// create one thread per column to validate each column
	for (int col = 1; col <= psize; col++) {
		thread_data[thread_index].column = col;
		thread_data[thread_index].psize = psize;
		thread_data[thread_index].grid = grid;
		thread_data[thread_index].result = &results[thread_index];
		
		pthread_create(&threads[thread_index], NULL, checkColumn, &thread_data[thread_index]);
		thread_index++;
	}
	// create one thread per subgrid to validate each box
	for (int row = 1; row <= psize; row += box_size) {
		for (int col = 1; col <= psize; col += box_size) {
			thread_data[thread_index].column = col;
			thread_data[thread_index].row = row;
			thread_data[thread_index].psize = psize;
			thread_data[thread_index].grid = grid;
			thread_data[thread_index].result = &results[thread_index];
		
			pthread_create(&threads[thread_index], NULL, checkSubGrid, &thread_data[thread_index]);
			thread_index++;
		}
	}
	// wait for all threads to complete
	for (int i = 0; i < num_threads; i++) {
		pthread_join(threads[i], NULL);
	}

	// aggregate results (puzzle is only valid if all threads report valid)
	*valid = true;
	for (int i = 0; i < num_threads; i++) {
		if (!results[i]) {
			*valid = false;
			break;
		}
	}
}

// takes puzzle size and grid[][] representing sudoku puzzle
// and tow booleans to be assigned: complete and valid.
// row-0 and column-0 is ignored for convenience, so a 9x9 puzzle
// has grid[1][1] as the top-left element and grid[9]9] as bottom right
// A puzzle is complete if it can be completed with no 0s in it
// If complete, a puzzle is valid if all rows/columns/boxes have numbers from 1
// to psize For incomplete puzzles, we cannot say anything about validity

void checkPuzzle(int psize, int **grid, bool *complete, bool *valid) {
	*valid = true;
	*complete = true;

	// scan entire grid for any 0's
	for (int row = 1; row <= psize; row++) {
		for (int col = 1; col <= psize; col++) {
			if (grid[row][col] == 0) {
				*complete = false;
				// puzzle is incomplete
				return; 
			} 
		}
	}
	// puzzle is complete, validate using threads
	createThreads(psize, grid, valid);
}

/*
* checks if placing 'num' at position (row, col) violates Sudoku rules
* returns false if num already exists in the same row, column, or subgrid
* returns true if placement is valid
*/
bool checkPlacement(int psize, int **grid, int row, int col, int num) {
	int box_size = (int)sqrt(psize);
	// check if num already exists in this row
	for (int c = 1; c <= psize; c++) {
		if (grid[row][c] == num) return false;
	}
	// check if num already exists in this column
	for (int r = 1; r <= psize; r++) {
		if (grid[r][col] == num) return false;
	}
	// calculate starting position of the subgrid containing (row, col)
	int start_row = ((row - 1) / box_size) * box_size + 1;
	int start_col = ((col - 1) / box_size) * box_size + 1;
	// check if num already exists in the subgrid
	for (int r = 0; r < box_size; r++) {
		for (int c = 0; c < box_size; c++) {
			if (grid[start_row + r][start_col + c] == num) return false;
		}
	}
	// placement is valid
	return true;
}

/*
* attempts to fill empty cells where only one valid number is possible
* scans entire grid and fills cells that have exactly one valid option
* returns true if at least one cell was filled, false if no progress made
* designed for "easy" puzzles with cells that have only one possibility
*/
bool solveSimple(int psize, int **grid) {
	bool made_progress = false;
	// scan every cell in the grid
	for (int row = 1; row <= psize; row++) {
		for (int col = 1; col <= psize; col++) {
			// found empty cell
			if (grid[row][col] == 0) {
				int poss_num = 0;
				int count = 0;
				// count how many valid numbers could go in this cell
				for (int num = 1; num <= psize; num++) {
					if (checkPlacement(psize, grid, row, col, num)) {
						poss_num = num;
						count++;
					}
				}
				// if exactly one valid number, fill it in
				if (count == 1) {
					grid[row][col] = poss_num;
					made_progress = true;
				}
			}
		}
	}

	return made_progress; 
}

/*
* solves complex puzzles using recursive backtracking algorithm
* tries placing numbers 1 to psize in empty cells, recursively solving forward
* if a placement leads to no solution, backtracks by resetting cell to 0
*/
bool solveComplex(int psize, int **grid) {
    for (int row = 1; row <= psize; row++) {
        for (int col = 1; col <= psize; col++) {
            if (grid[row][col] == 0) { 
                for (int i = 1; i <= psize; i++) {
					// on each iteration, checks if placing i in the current cell is valid
                    if (checkPlacement(psize, grid, row, col, i)) { 
                        grid[row][col] = i;
						// recursively calls 'solveComplex' to solve the puzzle
                        // if recursion solves the puzzle, return true
						if (solveComplex(psize, grid)) return true; 
						// backtrack: reset and try next number
                        grid[row][col] = 0;
                    }
                }
				// returns false if no valid entries can be found for the current cell and triggers backtracking
                return false; 
            }
        }
    }
	// if no more cells in the grid can be filled, returns true since puzzle is solved
    return true; 
}

// takes filename and pointer to grid[][]
// returns size of Sudoku puzzle and fills grid
int readSudokuPuzzle(char *filename, int ***grid) {
	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		printf("Could not open file %s\n", filename);
		exit(EXIT_FAILURE);
	}
	int psize;
	fscanf(fp, "%d", &psize);
	int **agrid = (int **)malloc((psize + 1) * sizeof(int *));
	for (int row = 1; row <= psize; row++) {
		agrid[row] = (int *)malloc((psize + 1) * sizeof(int));
		for (int col = 1; col <= psize; col++) {
			fscanf(fp, "%d", &agrid[row][col]);
		}
	}
	fclose(fp);
	*grid = agrid;
	return psize;
}

// takes puzzle size and grid[][]
// prints the puzzle
void printSudokuPuzzle(int psize, int **grid) {
	printf("%d\n", psize);
	for (int row = 1; row <= psize; row++) {
		for (int col = 1; col <= psize; col++) {
			printf("%d ", grid[row][col]);
	}
		printf("\n");
	}
	printf("\n");
}

// takes puzzle size and grid[][]
// frees the memory allocated
void deleteSudokuPuzzle(int psize, int **grid) {
	for (int row = 1; row <= psize; row++) {
		free(grid[row]);
	}
	free(grid);
}

// expects file name of the puzzle as argument in command line
int main(int argc, char **argv) {
	if (argc != 2) {
		printf("usage: ./sudoku puzzle.txt\n");
		return EXIT_FAILURE;
	}
	// grid is a 2D array
	int **grid = NULL;
	// find grid size and fill grid
	int sudokuSize = readSudokuPuzzle(argv[1], &grid);
	bool valid = false;
	bool complete = false;

	// repeatedly apply simple solving until no more progress
	while (solveSimple(sudokuSize, grid)) {}
	// attempt complex backtracking to solve for remaining cells
	solveComplex(sudokuSize, grid);
	// check if puzzle is now complete and valid
	checkPuzzle(sudokuSize, grid, &complete, &valid);

	printf("Complete puzzle? ");
	printf(complete ? "true\n" : "false\n");
	if (complete) {
		printf("Valid puzzle? ");
		printf(valid ? "true\n" : "false\n");
	}

	printSudokuPuzzle(sudokuSize, grid);
	deleteSudokuPuzzle(sudokuSize, grid);
	return EXIT_SUCCESS;
}
