#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "pile.h"
#include "cards.h"

void pile_init(struct pile *pile, int cap)
{
    pile->top = 0;
    pile->cap = cap;
    pile->_cards = malloc(cap * sizeof(uint8_t));
}

void pile_push(struct pile *pile, uint8_t card)
{
    assert (pile->top < pile->cap);
    pile->_cards[pile->top++] = card;
}

uint8_t pile_pop(struct pile *pile)
{
    assert (pile->top > 0);
    return pile->_cards[--pile->top];
}

// place top card down
void pile_rotate_top_card_down(struct pile *pile)
{
    assert (pile->top > 0);
    uint8_t top = pile->_cards[pile->top - 1];
    for (int i = pile->top - 1; i >= 1; i--)
        pile->_cards[i] = pile->_cards[i-1];
    pile->_cards[0] = top;
}

uint8_t pile_top(struct pile *pile)
{
    assert (pile->top > 0);
    return pile->_cards[pile->top - 1];
}

bool pile_empty(struct pile *pile)
{
    return pile->top == 0;
}

int pile_size(struct pile *pile)
{
    return pile->top;
}

void
pile_dump(struct pile *pile, FILE *out)
{
    for (int i = pile->top - 1; i >= 0; i--) {
        print_card(pile->_cards[i], false, out);
        fprintf(out, " <= ");
    }
}
