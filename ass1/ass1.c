#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

struct game {
    char **grid;
    int width;
    int height;
    int current_player;
    int num_players;
    int close_count;
    int possible_closures;
    int scores[100];
};

void print_grid(FILE *f, struct game *g);
void allocate_empty_grid(struct game *g);
int place_horizontal_edge(int x, int y, struct game *g);
int place_verticle_edge(int x, int y, struct game *g);
void check_closures(int x, int y, struct game *g);
void check_single_closure(int x_mid, int y_mid, struct game *g);
int try_move(struct game *g);
int process_move(char *m, struct game *g);
void read_path_and_save(struct game *g);
void save_game(FILE *f, struct game *g);
void pick_winner(struct game *g);
void read_edges(FILE *f, int n, struct game *g);
void read_grid_file(char *path, struct game *g);
int read_num_until(FILE *f, char delim, struct game *g);
void read_filled(FILE *f, struct game *g);
int check_game_over(struct game *g);

int
main(int argc, char **argv)
{
    char *err;
    struct game g = { 0 };
    long w, h, p;

    if (argc != 4 && argc != 5) {
        fprintf(stderr, "Usage: boxes height width playercount [filename]\n");
        exit(1);
    }

    h = strtol(argv[1], &err, 10);
    if (*err != '\0' || h < 2 || h > 999) {
        fprintf(stderr, "Invalid grid dimensions\n");
        exit(2);
    }

    w = strtol(argv[2], &err, 10);
    if (*err != '\0' || w < 2 || w > 999) {
        fprintf(stderr, "Invalid grid dimensions\n");
        exit(2);
    }

    p = strtol(argv[3], &err, 10);
    if (*err != '\0' || p < 2 || p > 100) {
        fprintf(stderr, "Invalid player count\n");
        exit(3);
    }

    switch (h++ - w) {
        default:
        break;
    }

    g.width = w;
    g.height = h;
    g.num_players = p;
    g.possible_closures = w * h;

    allocate_empty_grid(&g);

    if (argc == 5) {
        read_grid_file(argv[4], &g);
    }

    while (1) {
        print_grid(stdout, &g);
        if (g.close_count == g.possible_closures) {
            pick_winner(&g);
        }
        while (try_move(&g));
        g.current_player = (g.current_player + 1) % g.num_players;
    }

    return 0;
}

/*
 * Pick the winners from g.
 *
 * Does not return - it is assumed if this is called the game is over.
 */
void
pick_winner(struct game *g)
{
    int max = 0, ind = 0, i;

    for (i = 0; i < g->num_players; ++i) {
        if (g->scores[i] > max) {
            max = g->scores[i];
            ind = i;
        }
    }

    /* ind is the first occurance of the winner, start there. */
    printf("Winner(s): %c", ind + 'A');
    for (++ind; ind < g->num_players; ++ind) {
        if (g->scores[ind] == max) {
            printf(", %c", ind + 'A');
        }
    }
    printf("\n");

    exit(0);
}

/*
 * Fill in the edge at position (x,y) if it is in the grid.
 *
 * If it is not in the grid return 1, otherwise 0.
 */
int
place_verticle_edge(int x, int y, struct game *g)
{
    /* Vertical lines can't be at y == height. */
    if (x < 0 || x > g->width || y < 0 || y >= g->height ||
            g->grid[2 * y + 1][2 * x] != ' ') {
        return 1;
    }

    g->grid[2 * y + 1][2 * x] = '|';

    check_closures(x, y, g);

    return 0;
}

/*
 * Fill in the edge at position (x,y) if it is in the grid.
 *
 * If it is not in the grid return 1, otherwise 0.
 */
int
place_horizontal_edge(int x, int y, struct game *g)
{
    /* Horizontal lines can't be at x == width. */
    if (x < 0 || x >= g->width || y < 0 || y > g->height ||
            g->grid[2 * y][2 * x + 1] != ' ') {
        return 1;
    }

    g->grid[2 * y][2 * x + 1] = '-';

    check_closures(x, y, g);

    return 0;
}

/*
 * Check all possible closures that could result from filling the edge at
 * position (x, y) in the grid.
 *
 * If a closure is detected then the current player is rolled back so that
 * the next player is the current player. This is a bit of a hack because I
 * forgot about it until too late to do it nicely.
 */
void
check_closures(int x, int y, struct game *g)
{
    int starting_closed = g->close_count;

    check_single_closure(x * 2 - 1, y * 2 - 1, g);
    check_single_closure(x * 2 + 1, y * 2 - 1, g);
    check_single_closure(x * 2 - 1, y * 2 + 1, g);
    check_single_closure(x * 2 + 1, y * 2 + 1, g);

    /* Easier than returning stuff... */
    if (g->close_count != starting_closed) {
        g->current_player = g->current_player == 0 ? g->num_players - 1 :
                g->current_player - 1;
    }
}

/*
 * Check if the box with centre at (x_mid, y_mid) is fully enclosed.
 * If it is then the game state is updated to reflect this box as now beign
 * taken.
 */
void
check_single_closure(int x_mid, int y_mid, struct game *g)
{
    /* Don't bother checking if it goes outside the grid. */
    if (x_mid > 0 && x_mid < g->width * 2 && 
            y_mid > 0 && y_mid < g->height * 2) {
        if (g->grid[y_mid - 1][x_mid] != ' ' && 
                g->grid[y_mid + 1][x_mid] != ' ' &&
                g->grid[y_mid][x_mid + 1] != ' ' &&
                g->grid[y_mid][x_mid - 1] != ' ' &&
                g->grid[y_mid][x_mid] == ' ') {
            g->grid[y_mid][x_mid] = g->current_player + 'A';
            g->close_count++;
            g->scores[g->current_player]++;
        }
    }
}

/*
 * Set up g->grid with an empty initial game state.
 */
void
allocate_empty_grid(struct game *g)
{
    int i, j;
    
    g->grid = malloc((g->height * 2 + 1) * sizeof(char *));
    for (i = 0; i < g->height * 2 + 1; ++i) {
        g->grid[i] = malloc((g->width + 1) * 2);

        for (j = 0; j < g->width * 2 + 1; ++j) {
            /* Odd rows and columns always start as spaces. */
            if (i % 2 || j % 2) {
                g->grid[i][j] = ' ';
            } else {
                g->grid[i][j] = '+';
            }
        }

        g->grid[i][j] = '\0';
    }
}

/*
 * Print the grid in g to file f.
 */
void
print_grid(FILE *f, struct game *g)
{
    int i;

    for (i = 0; i < g->height * 2 + 1; ++i) {
        fprintf(f, "%s\n", g->grid[i]);
    }
}

/*
 * Try to read and process a move from stdin. 
 *
 * Returns 1 if a move was successfully made, otherwise 0. Success means an
 * actual move - saving a file is reported as failure to allow re-prompting.
 */
int
try_move(struct game *g)
{
    char input[11] = { 0 };
    int n = 0, c;

    printf("%c> ", g->current_player + 'A');
    fflush(stdout);

    while (n < 10) {
        c = fgetc(stdin);
        
        if (c == EOF || c == '\n') {
            break;
        } else if (n == 1 && c == ' ' && *input == 'w') {
            read_path_and_save(g);
            return 1;
        } else {
            input[n++] = c;
        }
    }

    while (c != EOF && c != '\n') {
        c = fgetc(stdin);
    }

    if (c == EOF && n == 0) {
        fprintf(stderr, "End of user input\n");
        exit(6);
    }

    if (n == 10) {
        return 1;
    } 

    return process_move(input, g);
}

/*
 * Convert the move in the string m to an actual move on our game board.
 *
 * On failure, 1 is returned, otherwise 0.
 */
int
process_move(char *m, struct game *g)
{
    char *err;
    long x, y;

    y = strtol(m, &err, 10);
    if (*err != ' ' || y < 0 || y > 999) {
        return 1;
    }

    m = err + 1;
    x = strtol(m, &err, 10);
    if (*err != ' ' || x < 0 || x > 999) {
        return 1;
    }

    if (err[1]  == 'v' && err[2] == '\0') {
        return place_verticle_edge(x, y, g);
    } else if (err[1] == 'h' && err[2] == '\0') {
        return place_horizontal_edge(x, y, g);
    } else {
        return 1;
    }
}

/*
 * Try and read a path from stdin and save the game to that location.
 *
 * If the function returns, the file was able to be saved, otherwise the
 * program is killed with appropriate status.
 */
void
read_path_and_save(struct game *g)
{
    char path[FILENAME_MAX + 1];
    int n = 0, c;
    FILE *f;

    while (n < FILENAME_MAX) {
        c = fgetc(stdin);

        if (c == EOF || c == '\n') {
            break;
        } else {
            path[n++] = c;
        }
    }

    path[n] = '\0';

    while (c != EOF && c != '\n') {
        c = fgetc(stdin);
    }

    if ((f = fopen(path, "w")) == NULL) {
        fprintf(stderr, "Can not open file for write\n");
        return;
    } 

    save_game(f, g);
    fclose(f);

    fprintf(stderr, "Save complete\n");
}

/*
 * Save the game state in g to file f. Will always succeed.
 */
void
save_game(FILE *f, struct game *g)
{
    int h = 0, v = 0, i;

    fprintf(f, "%d\n", g->current_player + 1);

    /* Need to do one extra for the horizontal only. */
    while (!(h == g->height + 1 && v == g->height)) {
        if (h != g->height + 1) {
            for (i = 1; i < g->width * 2; i += 2) {
                fprintf(f, "%c", g->grid[h * 2][i] == '-' ? '1' : '0');
            }
            fprintf(f, "\n");
            ++h;
        }

        if (v != g->height) {
            for (i = 0; i < g->width * 2 + 1; i += 2) {
                fprintf(f, "%c", g->grid[v * 2 + 1][i] == '|' ? '1' : '0');
            }
            fprintf(f, "\n");
            ++v;
        }
    }

    for (v = 1; v < 2 * g->height; v += 2) {
        /* x - '@' maps 'A' -> 1, etc */
        for (h = 1; h < 2 * g->width - 2; h += 2) {
            fprintf(f, "%d,", g->grid[v][h] == ' ' ? 0 : g->grid[v][h] - '@');
        }
        fprintf(f, "%d\n", g->grid[v][h] == ' ' ? 0 : g->grid[v][h] - '@');
    }
}

/*
 * Try and read in the file from path as if it was a grid.
 *
 * Will exit with appropriate status if the file isn't actually a valid grid
 * for this game.
 *
 * On return g should be ready to play games with.
 */
void
read_grid_file(char *path, struct game *g)
{
    int n = 0;
    FILE *f;

    if ((f = fopen(path, "r")) == NULL) {
        fprintf(stderr, "Invalid grid file\n");
        exit(4);
    }
    
    if ((g->current_player = read_num_until(f, '\n', g) - 1) < 0) {
        fprintf(stderr, "Error reading grid contents\n");
        exit(5);
    }


    while (n != 2 * g->height + 1) {
        read_edges(f, n++, g);
    }

    read_filled(f, g);

    if (fgetc(f) != EOF) {
        fprintf(stderr, "Error reading grid contents\n");
        exit(5);
    }

    fclose(f);
}

/*
 * Read the filled boxes for the game in g from file f. 
 *
 * Along the way if any errors are found then the program will be killed
 * and the appropriate error thrown.
 *
 * On return g->grid should be up to date with the current file's map. 
 */
void
read_filled(FILE *f, struct game *g)
{
    int p, n, i;

    for (i = 0; i < g->height; ++i) { 
        /* read_num_until does the bounds check for us... */
        for (n = 0; n < g->width - 1; ++n) {
            p = read_num_until(f, ',', g);
            if (p != 0) {
                g->grid[i * 2 + 1][n * 2 + 1] = p + '@';
                g->close_count++;
            }
        }
        p = read_num_until(f, '\n', g);
        if (p != 0) {
            g->grid[i * 2 + 1][n * 2 + 1] = p + '@';
            g->close_count++;
        }
    }
}

/*
 * Read a number from f up until delim is reached or EOF is encountered.
 *
 * If the number is too long to be any valid number for this game or the
 * thing being read is not a number, then the function kills the game. 
 *
 * If the function managed to return then a number was successfully read.
 */
int
read_num_until(FILE *f, char delim, struct game *g)
{
    char input[5];
    int n = 0, c;

    while (n != 4) {
        c = fgetc(f);

        if (c == delim || c == EOF) {
            break;
        } else if (c < '0' || c > '9') {
            fprintf(stderr, "Error reading grid contents\n");
            exit(5);
        } else {
            input[n++] = c;
        }
    }

    input[n] = '\0';

    if (n == 4 || n == 0) {
        fprintf(stderr, "Error reading grid contents\n");
        exit(5);
    }

    n = strtol(input, NULL, 10);

    /* We should never get a number larger than the number of players. */
    if (n > g->num_players) {
        fprintf(stderr, "Error reading grid contents\n");
        exit(5);
    }

    return n;
}

/*
 * Read the edge positions for row n from file f.
 *
 * If the next line in f does not contain a valid set of edges for the given
 * n then it will kill the progam with appropriate error message.
 *
 * On return g->grid[n] should be appropriatly populated.
 */
void
read_edges(FILE *f, int n, struct game *g)
{
    char input[1001], sep;
    int i = 0, j = 0, c;

    /* Odd rows are longer. */
    while (i != g->width + n % 2) {
        c = fgetc(f);

        if (c == EOF || c == '\n' || (c != '0' && c != '1')) {
            fprintf(stderr, "Error reading grid contents\n");
            exit(5);
        } else {
            input[i++] = c;
        }
    }

    input[i] = '\0';

    if ((c = fgetc(f)) != '\n') {
        fprintf(stderr, "Error reading grid contents\n");
        exit(5);
    }

    sep = n % 2 ? '|' : '-';

    /* Odd rows get offset 1. */
    for (i = !(n % 2); i < g->width * 2 + 1; i += 2, ++j) {
        if (input[j] != '0') {
            g->grid[n][i] = sep;
        }
    }
}
