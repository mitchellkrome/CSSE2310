#ifndef UTILS_H_
#define UTILS_H_

#include <stdio.h>

int is_valid_card(char const *);
char const *get_card_string(int);
int all_unique_cards(int const *cards, int n);
int eat_line(FILE *f, int must_space);
int read_card(FILE *f, int allow_blanks);
void sort_cards(int *cards, int n);
char get_card_char(int card);

#endif
