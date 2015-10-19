#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>

#include "utils.h"

static char const *const cards[] = {
        "2S", "3S", "4S", "5S", "6S", "7S", "8S", "9S", "TS", "JS", "QS", "KS", "AS", 
        "2C", "3C", "4C", "5C", "6C", "7C", "8C", "9C", "TC", "JC", "QC", "KC", "AC", 
        "2D", "3D", "4D", "5D", "6D", "7D", "8D", "9D", "TD", "JD", "QD", "KD", "AD", 
        "2H", "3H", "4H", "5H", "6H", "7H", "8H", "9H", "TH", "JH", "QH", "KH", "AH", 
};

static char const valid_cards[] = "23456789TJQKA";
static char const valid_suites[] = "SCDH";

/*
 * Return card mapping if the string card contains a valid card else 0.
 */
int
is_valid_card(char const *card)
{
    int ret = -1;

    assert(card);

    if (strlen(card) == 2 && strchr(valid_cards, card[0]) != NULL &&
            strchr(valid_suites, card[1]) != NULL) {
        /* It's valid, map it into the cards array. */
        switch (card[0]) {
            case 'T':
                ret = 8;
                break;
            case 'J':
                ret = 9;
                break;
            case 'Q':
                ret = 10;
                break;
            case 'K':
                ret = 11;
                break;
            case 'A':
                ret = 12;
                break;
            default:
                ret = card[0] - '2';
        }

        switch (card[1]) {
            case 'C':
                ret += 13 * 1;
                break;
            case 'D':
                ret += 13 * 2;
                break;
            case 'H':
                ret += 13 * 3;
                break;
            default:
                break;
        }
    }

    return ret;
}

/*
 * Map card num to a string.
 */
char const *
get_card_string(int card)
{
    assert(card < 52);

    return cards[card];
}

char
get_card_char(int card)
{
    int num = card % 13;

    switch (num) {
        case 12:
            return 'A';
        case 11:
            return 'K';
        case 10:
            return 'Q';
        case 9:
            return 'J';
        case 8:
            return 'T';
        default:
            return '2' + num;
    }
}

int
all_unique_cards(int const *cards, int n)
{
    int seen[52] = { 0 };

    assert(n <= 52);

    for (int i = 0; i < n; ++i) {
        if (seen[cards[i]] != 0)
            return 0;
        seen[cards[i]] = 1;
    }

    return 1;
}

int compare_cards(void const *ap, void const *bp)
{
    int a = *((int *)ap);
    int b = *((int *)bp);

    if (a < b)
        return -1;
    else if (a == b)
        return 0;
    else
        return 1;
}

void
sort_cards(int *cards, int n)
{
    qsort(cards, n, sizeof(int), compare_cards);
}

/*
 * Read a single card from file f. If allow_blanks is true then comment and
 * blank lines will be allowed before the card is found, otherwise it must
 * be the next character in the file.
 */
int
read_card(FILE *f, int allow_blanks)
{
    char in[3] = { 0 };
    int c, started = 0;

    while (started == 0) {
        c = fgetc(f);
        
        if (c == EOF)
            return -1;

        if (allow_blanks && isspace(c) && c != '\n') {
            /* 
             * If we can have blank lines and we read space then enforce the
             * rest is a space. eat_line will be -1 if there is non-space.
             */
            if (eat_line(f, 1) == -1)
                return -1;
        } else if (allow_blanks && c == '#') {
            /* Don't enforce space on comments. */
            eat_line(f, 0);
        } else if (allow_blanks && c == '\n') {
            continue;
        } else {
            started = 1;
        }
    }

    /* At this point, the next 2 things should be our card. */
    in[0] = c;
    c = fgetc(f);
    if (c == EOF)
        return -1;
    in[1] = c;

    return is_valid_card(in);
}

/*
 * Return -1 if find non-space and must_space == 1. 
 */
int
eat_line(FILE *f, int must_space)
{
    int c;

    do {
        c = fgetc(f);
        if (must_space && !isspace(c))
            return -1;
    } while (c != '\n' && c != EOF);

    return 0;
}
