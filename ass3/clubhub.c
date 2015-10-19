#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <limits.h>

#include "utils.h"

enum ecode {
    OK = 0,
    BADARG,
    BADSCORE,
    BADFILE,
    BADDECK,
    BADPROC,
    QUITTER,
    BADMSG,
    BADPLAY,
    SIG,
    SYSCALL,
};

struct deck {
    int *cards;
    int num;
    int pos;
};

struct game {
    int all_alive;
    pid_t alive_children[4];

    FILE *children[4][2];
    int thresh;
    int players;
    struct deck deck;

    int card_allocation[52];
    int new_trick;
    int tricks;
    int lead;
    int next_player;
    int played[4];
    int scores[4];
};

#define FROM_CHILD(g, c) ((g)->children[c][0])
#define TO_CHILD(g, c) ((g)->children[c][1])

void error(enum ecode e);
struct deck read_deck(char *file);
int check_for_dot(FILE *f);
void init_child(int *to, int *from, int num, struct game *g);
void make_children(int num, char **progs, struct game *g);
void shutdown_children(void);
void init_signal_handler(void);
void send_decks(struct game *g);
int read_play(int lead, int player, struct game *g);
int can_play(int card, int lead, int player, struct game *g);
void do_trick(struct game *g);
int has_card_in_suit(int suit, int player, struct game *g);
void send_played(int card, struct game *g);
void update_scores(struct game *g, int send);
void play_rounds(struct game *g);

/* 
 * Needed to kill the children from inside signal handler.
 * I prefer to pass the actual game around during normal gameplay, so this
 * just points to a game on the stack.
 */
struct game *game;

int
main(int argc, char **argv)
{
    struct game g;
    char *err;
    int thresh;

    init_signal_handler();

    memset(&g, 0, sizeof(struct game));
    game = &g;
    
    if (argc < 5 || argc > 7)
        error(BADARG);
    g.players = argc - 3;

    thresh = strtol(argv[2], &err, 10);
    if (thresh < 0 || *err != '\0')
        error(BADSCORE);
    g.thresh = thresh;
    g.new_trick = 1;

    g.deck = read_deck(argv[1]);
    g.deck.pos = 0;

    make_children(argc - 3, argv + 3, &g);

    play_rounds(&g);

    error(OK);
}

int
have_winner(struct game *g)
{
    int max = 0, min = INT_MAX, ind;

    for (int i = 0; i < g->players; ++i) {
        if (g->scores[i] > max)
            max = g->scores[i];

        if (g->scores[i] < min) {
            ind = i;
            min = g->scores[i];
        }
    }

    if (max >= g->thresh) {
        printf("Winner(s): %c", ind + 'A');
        for (int i = ind + 1; i < g->players; ++i) {
            if (g->scores[i] == min)
                printf(" %c", i + 'A');
        }
        printf("\n");

        return 1;
    }

    return 0;
}

void
play_rounds(struct game *g)
{

    do {
        send_decks(g);
        for (int i = 0; i < 52 / g->players; ++i) {
            do_trick(g);
            update_scores(g, i == 52 / g->players - 1);
        }
    } while (have_winner(g) != 1);
}

void
do_trick(struct game *g)
{
    int player = g->next_player;
    int played;
    char *msg;

    for (int i = 0; i < g->players; ++i) {
        if (i == 0)
            msg = "newtrick\n";
        else
            msg = "yourturn\n";
        fprintf(TO_CHILD(g, player), "%s", msg);
        fflush(TO_CHILD(g, player));

        played = read_play(i == 0, player, g);

        for (int i = 0; i < g->players; ++i) {
            fprintf(TO_CHILD(g, i), "played %s\n", get_card_string(played));
            fflush(TO_CHILD(g, i));
        }

        player = (player + 1) % g->players;
    }
}

void
print_scores(FILE *f, struct game *g)
{
    fprintf(f, "scores %d,%d", g->scores[0], g->scores[1]);
    switch (g->players) {
        case 2:
            fprintf(f, "\n");
            break;
        case 3:
            fprintf(f, ",%d\n", g->scores[2]);
            break;
        case 4:
            fprintf(f, ",%d,%d\n", g->scores[2], g->scores[3]);
            break;
        default:
            break;
    }

    fflush(f);
}

void
update_scores(struct game *g, int send)
{
    int clubs = 0, max = -1, winner;

    for (int i = 0; i < g->players; ++i) {
        if (g->played[i] / 13 == g->lead && g->played[i] > max) {
            winner = i;
            max = g->played[i];
        } 

        if (g->played[i] / 13 == 1)
            ++clubs;
    }

    g->scores[winner] += clubs;
    g->next_player = winner;

    /* Always send trickover - sometimes send the rest. */
    for (int i = 0; i < g->players; ++i) {
        fprintf(TO_CHILD(g, i), "trickover\n");

        if (send == 1)
            print_scores(TO_CHILD(g, i), g);
        fflush(TO_CHILD(g, i));
    }

    if (send == 1)
        print_scores(stdout, g);
}

int
read_play(int lead, int player, struct game *g)
{
    char buf[3] = { 0 };
    int c;

    for (int i = 0; i < 3; ++i) {
        c = fgetc(FROM_CHILD(g, player));
        if (c == EOF)
            error(QUITTER);
        buf[i] = c;
        if (c == '\n')
            break;
    }

    if (buf[2] != '\n')
        error(BADMSG);
    buf[2] = '\0';

    if ((c = is_valid_card(buf)) == -1)
        error(BADMSG);

    if (can_play(c, lead, player, g) == 0)
        error(BADPLAY);

    printf("Player %c %s %s\n", player + 'A', lead ? "led" : "played", buf);

    g->played[player] = c;
    g->card_allocation[c] = -1;

    return c;
}

int
can_play(int card, int lead, int player, struct game *g)
{
    if (g->card_allocation[card] != player)
        return 0;

    if (lead == 1)
        g->lead = card / 13;
    else if (has_card_in_suit(g->lead, player, g) == 1 && 
            card / 13 != g->lead)
        return 0;

    return 1;
}

int
has_card_in_suit(int suit, int player, struct game *g)
{
    int pos = suit * 13;

    for (; pos < (suit + 1) * 13; ++pos) {
        if (g->card_allocation[pos] == player)
            return 1;
    }

    return 0;
}

void
send_decks(struct game *g)
{
    int cards[4][26], card;
    char msg[100], *pos;

    /* make them all -1 */
    memset(g->card_allocation, 0xff, 52 * sizeof(int));

    for (int i = 0; i < 51; i += g->players) {
        for (int j = 0; j < g->players; ++j) {
            if (g->players == 3 && g->deck.cards[g->deck.pos] == 26)
                ++g->deck.pos;
            card = g->deck.cards[g->deck.pos++];
            cards[j][i / g->players] = card;
            g->card_allocation[card] = j;
        }
    }

    for (int i = 0; i < g->players; ++i) {
        sort_cards(cards[i], 52 / g->players);
    }

    for (int i = 0; i < g->players; ++i) {
        memset(msg, 0, sizeof(msg));
        strcat(msg, "newround ");
        pos = msg + strlen("newround ");
        for (int j = 0; j < 52 / g->players; ++j) {
            strcat(pos, get_card_string(cards[i][j]));
            strcat(pos, ",");
            pos += 3;
        }
        *(pos - 1) = '\n';
        fprintf(TO_CHILD(g, i), "%s", msg);
        fflush(TO_CHILD(g, i));
        printf("Player (%c): %s", i + 'A', msg + strlen("newround "));
    }

    if (g->deck.pos == g->deck.num)
        g->deck.pos = 0;
}

void
sigint_handler(int sig)
{
    (void)sig;

    error(SIG);
}

void
init_signal_handler(void)
{
    struct sigaction sig;

    memset(&sig, 0, sizeof(struct sigaction));
    sig.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sig, 0);
    sig.sa_handler = sigint_handler;
    sigaction(SIGINT, &sig, 0);
}

void
make_children(int num, char **progs, struct game *g)
{
    int to[2], from[2], pit;
    pid_t child;
    char id[2] = { '\0' }, num_p[2] = { num + '0', '\0' };

    for (int i = 0; i < num; i++) {
        if (pipe(to) != 0 || pipe(from) != 0)
            error(BADPROC);
        if ((child = fork()) == 0) {
            /* child - we're lazy here and assume these all work.. */
            pit = open("/dev/null", O_WRONLY);
            dup2(to[0], STDIN_FILENO);
            dup2(from[1], STDOUT_FILENO);
            dup2(pit, STDERR_FILENO);
            close(to[0]);
            close(to[1]);
            close(from[0]);
            close(from[1]);
            close(pit);

            /* Otherwise kernel man thinks they can still talk */
            for (int j = 0; j < i; ++j) {
                fclose(TO_CHILD(g, j));
                fclose(FROM_CHILD(g, j));
            }

            id[0] = i + 'A';
            execlp(progs[i], progs[i], num_p, id, (char *)NULL);
            exit(20); // it borked
        } else if (child > 0) {
            /* parent */
            g->alive_children[i] = child;
            init_child(to, from, i, g);
            if (i == num - 1)
                g->all_alive = 1;
        } else {
            /* not good. */
            error(BADPROC);
        }
    }
}

void
init_child(int *to, int *from, int num, struct game *g)
{
    FILE *f;

    if ((f = fdopen(to[1], "w")) == NULL)
        error(BADPROC);
    TO_CHILD(g, num) = f;
    if ((f = fdopen(from[0], "r")) == NULL)
        error(BADPROC);
    FROM_CHILD(g, num) = f;
    close(to[0]);
    close(from[1]);

    if (fgetc(FROM_CHILD(g, num)) != '-')
        error(BADPROC);
}

struct deck
read_deck(char *file)
{
    FILE *f;
    struct deck d = { .cards = NULL, .num = 0, .pos = 0 };
    int n = 1, c, tmp; 

    if ((f = fopen(file, "r")) == NULL)
        error(BADFILE);

    while (1) {
        if (d.num < n * 52) {
            d.num = n * 52;
            d.cards = realloc(d.cards, sizeof(int) * d.num);
        }

        for (int i = 0; i < 52; i++) {
            c = read_card(f, 1);
            if (c == -1) 
                error(BADDECK);
            d.cards[d.pos++] = c;

            c = fgetc(f);

            /* Because Joel changed the rules and I'm lazy */
            tmp = fgetc(f);
            if (tmp != '\n')
                ungetc(tmp, f);

            if (i != 51 && (c != ',' && c != '\n'))
                error(BADDECK);
            else if (i == 51 && (c != EOF && c != '\n' && c != ','))
                error(BADDECK);
        } 
        
        if (all_unique_cards(&(d.cards[(n - 1) * 52]), 52) == 0)
            error(BADDECK);

        if (check_for_dot(f) == 0)
            return d;

        /* 
         * If we get here then there is another deck. We make sure there is a
         * newline after the dot. 
         */
        if (fgetc(f) != '\n')
            error(BADDECK);

        ++n;
    }
}

int
check_for_dot(FILE *f)
{
    int c;

    while (1) {
        /* At this point we're at the start of a fresh line or EOF. */
        c = fgetc(f);
        if (c == EOF)
            return 0;

        if (c == '.')
            return 1;

        if (isspace(c) && c != '\n' && eat_line(f, 1) != 0)
            error(BADDECK);
        else if (c == '#')
            eat_line(f, 0);
        else
            error(BADDECK);
    }
}

/*
 * Try and shutdown any active children. 
 *
 * NOTE: If alive_children is not 0 then g must already be valid, so don't
 * need to check that. However it could be we got interrupted before setting
 * the FD's up, so we better check those.
 */
void
shutdown_children(void)
{
    int status = -1;
    pid_t ret;
    struct sigaction sig;

    /* We don't want to end up in here twice - disable sigint. */
    memset(&sig, 0, sizeof(struct sigaction));
    sig.sa_handler = SIG_IGN;
    sigaction(SIGINT, &sig, 0);

    /* First we try to tell each one nicely. */
    for (int i = 0; i < 4; i++) {
        if (game->alive_children[i] == 0)
            continue;

        if (TO_CHILD(game, i) != NULL) {
            fprintf(TO_CHILD(game, i), "end\n");
            fflush(TO_CHILD(game, i));
        }
    }
    
    /* Give them time. */
    usleep(100000);

    for (int i = 0; i < 4; i++) {
        if (game->alive_children[i] == 0)
            continue;
        
        /* If it didn't shut down. */
        if ((ret = waitpid(game->alive_children[i], &status, WNOHANG)) <= 0) {
            /* We need to be a bit more serious. */
            kill(game->alive_children[i], SIGKILL);
            /* This can't block, we just sigkill'd */
            ret = waitpid(game->alive_children[i], &status, 0);
        }

        if (game->all_alive == 1) {
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
                fprintf(stderr, "Player %c exited with status %d\n", 'A' + i,
                        WEXITSTATUS(status));
            if (WIFSIGNALED(status))
                fprintf(stderr, "Player %c terminated due to signal %d\n", 
                        'A' + i, WTERMSIG(status));
        }
    }
}

void
error(enum ecode e)
{
    shutdown_children();

    switch (e) {
        case OK:
            break;
        case BADARG:
            fprintf(stderr, "Usage: clubhub deckfile winscore prog1 prog2 [prog3 [prog4]]\n");
            break;
        case BADSCORE:
            fprintf(stderr, "Invalid score\n");
            break;
        case BADFILE:
            fprintf(stderr, "Unable to access deckfile\n");
            break;
        case BADDECK:
            fprintf(stderr, "Error reading deck\n");
            break;
        case BADPROC:
            fprintf(stderr, "Unable to start subprocess\n");
            break;
        case QUITTER:
            fprintf(stderr, "Player quit\n");
            break;
        case BADMSG:
            fprintf(stderr, "Invalid message received from player\n");
            break;
        case BADPLAY:
            fprintf(stderr, "Invalid play by player\n");
            break;
        case SIG:
            fprintf(stderr, "SIGINT caught\n");
            break;
        case SYSCALL:
            perror("Syscall failed: ");
            break;
        default:
            fprintf(stderr, "What is this?\n");
    }

    exit(e);
}
