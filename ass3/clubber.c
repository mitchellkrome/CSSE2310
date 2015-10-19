#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "utils.h"

enum ecode {
    OK = 0,
    BADARG,
    BADPLAYERS,
    BADID,
    DEADHUB,
    BADHUB,
};

enum game_state {
    NEWROUND = 0,
    NEWTRICK,
    PLAYING,
    TRICKOVER,
    SCORES,
    FINISHED,
};

struct game {
    enum game_state state;
    int cards[52];
    int num_cards;
    int lead_suit;
    int players;
    int current_player;
    int turns_left;
    int tricks_left;
    int played_card;
    int my_move;
    int me;
    int played_cards[52];
    int scores[4];
    int played[52];
};

void error(enum ecode e);
char *read_line(FILE *f);
void print_line(char *line);
void process_line(char *line, struct game *g);
void newround(char *line, struct game *g);
void print_status(struct game *g);
void newtrick(struct game *g);
void trickover(struct game *g);
void yourturn(struct game *g);
void played(char *line, struct game *g);
int pick_card(char *order, int lowest, struct game *g);
void scores(char *line, struct game *g);
int try_follow_suit(struct game *g);
int get_lowest_club(struct game *g);
void init_signal_handler(void);

int
main(int argc, char **argv)
{
    struct game g;

    memset(&g, 0, sizeof(struct game));

    init_signal_handler();

    if (argc != 3)
        error(BADARG);

    if (argv[1][1] != '\0' || argv[1][0] < '2' || argv[1][0] > '4')
        error(BADPLAYERS);
    g.players = argv[1][0] - '0';

    if (argv[2][1] != '\0' || argv[2][0] < 'A' || 
            argv[2][0] >= 'A' + g.players)
        error(BADID);
    g.me = argv[2][0] - 'A';

    printf("-");
    fflush(stdout);

    g.state = NEWROUND;
    g.turns_left = -1;

    if (g.players == 3)
        g.played[is_valid_card("2D")] = 1;

    while (1) {
        char *l = read_line(stdin);
        process_line(l, &g);
    }

    exit(10);
}

void
init_signal_handler(void)
{
    struct sigaction sig;

    memset(&sig, 0, sizeof(struct sigaction));
    sig.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sig, 0);
}

void
print_status(struct game *g)
{
    int first = 1;
    char suites[] = "SCDH";

    fprintf(stderr, "Hand: ");
    for (int i = 0; i < 52; ++i) {
        if (g->cards[i] == 1) {
            if (first == 1)
                first = 0;
            else
                fprintf(stderr, ",");
            fprintf(stderr, "%s", get_card_string(i));
        }
    }

    for (int i = 0; i < 4; ++i) {
        fprintf(stderr, "\nPlayed (%c): ", suites[i]);
        first = 1;
        for (int j = i * 13; j < i * 13 + 13; ++j) {
            if (g->played[j] == 1) {
                if (first == 1)
                    first = 0;
                else
                    fprintf(stderr, ",");
                fprintf(stderr, "%c", get_card_char(j));
            }
        }
    }

    fprintf(stderr, "\nScores: ");
    first = 1;
    for (int i = 0; i < g->players; ++i) {
        if (first == 1)
            first = 0;
        else
            fprintf(stderr, ",");
        fprintf(stderr, "%d", g->scores[i]);
    }
    fprintf(stderr, "\n");
}

void
process_line(char *line, struct game *g)
{
    if (strncmp(line, "newround ", strlen("newround ")) == 0) {
        newround(line + strlen("newround "), g);
    } else if (strcmp(line, "newtrick") == 0) {
        newtrick(g);
    } else if (strcmp(line, "trickover") == 0) {
        trickover(g);
    } else if (strcmp(line, "yourturn") == 0) {
        yourturn(g);
    } else if (strncmp(line, "played ", strlen("played ")) == 0) {
        played(line + strlen("played "), g);
    } else if (strncmp(line, "scores ", strlen("scores ")) == 0) {
        scores(line + strlen("scores "), g);
    } else if (strcmp(line, "end") == 0) {
        error(OK);
    } else {
        error(BADHUB);
    }
    print_status(g);
}

void
played(char *line, struct game *g)
{
    int c;

    if (g->state != PLAYING)
        error(BADHUB);

    if ((c = is_valid_card(line)) == -1 || 
            (g->my_move == 0 && g->played[c] != 0))
        error(BADHUB);

    g->played[c] = 1;

    if (g->turns_left == -1) {
        g->lead_suit = c / 13;
        g->turns_left = g->players - 1;
    } else {
        --g->turns_left;
    }
    
    if (g->turns_left == 0)
        g->state = TRICKOVER;
    
    g->my_move = 0;
}

void
scores(char *line, struct game *g)
{
    char *end;
    int score;

    if (g->state != SCORES)
        error(BADHUB);

    for (int i = 0; i < g->players; ++i) {
        score = strtol(line, &end, 10);

        if (score < 0 || (i != g->players - 1 && *end != ',') ||
                (i == g->players - 1 && *end != '\0'))
            error(BADHUB);
        g->scores[i] = score;
        line = end + 1;
    }

    g->state = NEWROUND;
}

void
trickover(struct game *g)
{
    if (g->state != TRICKOVER || g->turns_left != 0 || g->played_card == 0)
        error(BADHUB);
    
    g->turns_left = -1;
    g->played_card = 0;
    --g->tricks_left;

    if (g->tricks_left == 0)
        g->state = SCORES;
    else
        g->state = PLAYING;
}

void
yourturn(struct game *g)
{
    int play;

    if (g->state != PLAYING || g->turns_left == 0)
        error(BADHUB);

    g->played_card = 1;
    g->my_move = 1;
    
    /* Make a guess. */
    play = try_follow_suit(g);

    if (play == -1) {
        if (g->turns_left == 1)
            play = pick_card("HDCS", 0, g);
        else
            play = pick_card("CDHS", 0, g);
    }

    g->played[play] = 1;

    printf("%s\n", get_card_string(play));
    fflush(stdout);
}

int
try_follow_suit(struct game *g)
{
    int suit = g->lead_suit * 13;

    for (int i = suit; i < suit + 13; ++i) {
        if (g->cards[i] == 1) {
            g->cards[i] = 0;
            return i;
        }
    }

    return -1;
}

/* 
 * Find the next card we have available to play, using order to define the
 * order which suits are searched.
 * Order is a string like "SDCH".
 */
int
pick_card(char *order, int lowest, struct game *g)
{
    int suit;

    for (int i = 0; i < 4; ++i) {
        suit = order[i] == 'S' ? 0 : order[i] == 'C' ? 13 : 
                order[i] == 'D' ? 26 : 39;
        if (lowest == 1) {
            for (int j = suit; j < suit + 13; ++j) {
                if (g->cards[j] == 1) {
                    g->cards[j] = 0;
                    return j;
                }
            }
        } else {
            for (int j = suit + 12; j >= suit; --j) {
                if (g->cards[j] == 1) {
                    g->cards[j] = 0;
                    return j;
                }
            }
        }
    }

    return 0;
}

void
newtrick(struct game *g)
{
    int play;

    if (g->state != PLAYING || g->turns_left != -1)
        error(BADHUB);

    g->played_card = 1;
    g->my_move = 1;

    play = get_lowest_club(g);

    if (play == -1)
        play = pick_card("DHSC", 1, g);

    g->played[play] = 1;

    printf("%s\n", get_card_string(play));
    fflush(stdout);
}

int
get_lowest_club(struct game *g)
{
    for (int i = 13; i < 26; i++) {
        if (g->played[i] == 0 && g->cards[i] == 1) {
            g->cards[i] = 0;
            return i;
        } else if (g->played[i] == 0) {
            return -1;
        }
    }

    return -1;
}

void
newround(char *line, struct game *g)
{
    int c;
    int cards = 52 / g->players;
    size_t len = cards * 2 + cards - 1;

    g->num_cards = cards;

    if (g->state != NEWROUND || strlen(line) != len)
        error(BADHUB);

    for (int i = 0; i < cards; i++) {
        if (i != cards - 1 && line[2] != ',')
            error(BADHUB);

        line[2] = '\0';
        c = is_valid_card(line);
        if (c != -1 || g->cards[c] != 0)
            g->cards[c] = 1;
        else 
            error(BADHUB);

        line += 3;
    }

    g->state = PLAYING;
    g->tricks_left = 52 / g->players;
    memset(g->played_cards, 0, 52 * sizeof(int));
    memset(g->played, 0, 52 * sizeof(int));
}

char *
read_line(FILE *f)
{
    char *line;
    int c, pos = 0;

    /* Should be long enough for any valid message. */
    line = malloc(sizeof(char) * 101);

    while (pos != 100) {
        c = fgetc(f);
        
        if (c == EOF || c == '\n')
            break;

        line[pos++] = c;
    }

    /* Don't print if we only got EOF. */
    if (c == EOF && pos == 0)
        error(DEADHUB);

    /* Terminate the line and print it before error checking. */
    line[pos] = '\0';
    print_line(line);

    if (c == EOF)
        error(DEADHUB);

    if (pos == 100)
        error(BADHUB);

    return line;
}

void
print_line(char *line)
{
    char copy[21] = { 0 };

    /* Cheap way to get the first 20. */
    strncpy(copy, line, 20);
    
    fprintf(stderr, "From hub:%s\n", copy);
}

void
error(enum ecode e)
{
    switch (e) {
        case OK:
            break;
        case BADARG:
            fprintf(stderr, "Usage: player number_of_players myid\n");
            break;
        case BADPLAYERS:
            fprintf(stderr, "Invalid player count\n");
            break;
        case BADID:
            fprintf(stderr, "Invalid player ID\n");
            break;
        case DEADHUB:
            fprintf(stderr, "Unexpected loss of hub\n");
            break;
        case BADHUB:
            fprintf(stderr, "Bad message from hub\n");
            break;
        default:
            fprintf(stderr, "What is this?\n");
    }

    exit(e);
}
