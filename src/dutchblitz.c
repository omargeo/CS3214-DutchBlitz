/*
 * A multi-threaded simulation of the game "Dutchblitz"
 * (https://www.dutchblitz.com/)
 *
 * Player are threads and the game state is expressed in
 * global variables.
 *
 * The challenge is to ensure a consistent and fair game.
 *
 * First version written for CS3214, Spring 2022
 *
 * @author gback
 */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "pile.h"


#include "cards.h"

/* A Fisher-Yates shuffle */
static void 
fisher_yates(uint8_t *deck, uint8_t n)
{
    for (int i = n-1; i > 0; i--) {
        int j = random() % (i+1);
        uint8_t tmp = deck[j];
        deck[j] = deck[i];
        deck[i] = tmp;
    }
}

/* Prepare a standard dutchblitz deck with a given bgcolor and shuffle it. */
static void
prepare_deck(uint8_t *deck, enum Color bgcolor)
{
    for (int fgcolor = 0; fgcolor < 4; fgcolor++)
        for (int num = 0; num < 10; num++)
            deck[10*fgcolor + num] = make_card(bgcolor, fgcolor, num);

    fisher_yates(deck, 40);
}

// the play state of a player
struct player_state {
    uint8_t deck[40];           // deck from which the piles were dealt
    struct pile woodpiledraw;   // wood pile in hand to draw from
    struct pile woodpilediscard;// wood pile on table to discard to
    struct pile post[3];        // post piles: stack
    struct pile blitz;          // blitz pile: stack
    uint8_t bgcolor;            // that player's bgcolor
    const char *name;           // name of player based on background
                                // color of their deck
};

struct player_state players[4]; // state of each player
struct pile dutch[16];          // 16 dutch piles: 4 of each color (4x threads)
int nextdutch = 0;              // index of next dutch pile to be started
bool blitzed = false;           // true if someone blitzed in this game
struct player_state *winner;    // winner who has blitzed
int deadlocked = 0;             // how many players are currently deadlocked
static _Thread_local const char *threadname;    // name of each player

// a barrier in an attempt to let threads start at roughly the same time
static pthread_barrier_t readysetgo;   

FILE *logfile;  // logfile to write log output, or NULL

pthread_mutex_t lock;
struct timespec ts = {0, 1};

static void
reset_simulation()
{
    winner = NULL;
    deadlocked = 0;
    blitzed = false;
    nextdutch = 0;
}

const int BLITZED_FROM_POST = 256;  // player blitzed by moving cards to post pile
const int PLAY_WOOD = 257;  // player is going to put a wood pile card to the dutch pile
const int PLAY_BLITZ = 258; // player is going to put a blitz pile card to the dutch pile
const int PLAY_POST = 259;  // player is going to put a post pile card to the dutch pile

// check the consistency of the dutch piles started so far
static void 
validate_dutch()
{
    for (int i = 0; i < nextdutch; i++) {
        for (int pos = 0; pos < pile_size(&dutch[i]); pos++) {
            uint8_t cp = dutch[i]._cards[pos];
            // check that dutch piles are in order 0, 1, 2 and have the
            // same front color
            assert (get_card_number(cp) == pos);        
            assert (get_front_color(cp) == get_front_color(dutch[i]._cards[0]));
        }
    }
}

// output dutch piles' content to file
static void
dump_dutch(FILE *out, bool full)
{
    fprintf(out, "Dutch pile sizes:");
    for (int i = 0; i < nextdutch; i++)
        fprintf(out, " %2d", pile_size(&dutch[i]));
    fprintf(out, "\n");
    if (full) {
        for (int i = 0; i < nextdutch; i++) {
            pile_dump(dutch+i, out);
            fprintf(out, "\n");
        }
    }
}

// does card fit on dutch pile? 
// if `play` is true, card will be added to dutch pile on which it fits
// return true/false
static bool
fits_on_dutch_pile(uint8_t card, bool play, FILE *out)
{
    if (get_card_number(card) == 0) {
        if (play) {
            assert(nextdutch < 16);
            pile_init(&dutch[nextdutch], 10);
            pile_push(&dutch[nextdutch], card);
            if (out) {
                fprintf(out, "%s puts ", threadname);
                print_card(card, false, out);
                fprintf(out, " on dutch\n");
            }
            nextdutch++;
        }

        return true;
    }

    for (int i = 0; i < nextdutch; i++) {
        uint8_t dtopcard = pile_top(&dutch[i]);
        if (get_card_number(card) == get_card_number(dtopcard) + 1
            && get_front_color(card) == get_front_color(dtopcard))
        {
            if (play) {
                pile_push(&dutch[i], card);
                if (out) {
                    fprintf(out, "%s puts ", threadname);
                    print_card(card, false, out);
                    fprintf(out, "on dutch\n");
                }
            }
            return true;
        }
    }
    return false;
}

// print this player's state.
// should be called only if out != NULL
static void
player_print_state(struct player_state *player, FILE *out)
{
    fprintf(out, "Piles for player %s\n", player->name); 
    fprintf(out, "Blitzpile: ");
    pile_dump(&player->blitz, out); 
    fprintf(out, "\n");
    for (int i = 0; i < 3; i++) {
        fprintf(out, "Postpile#%d: ", i);
        pile_dump(&player->post[i], out); 
        fprintf(out, "\n");
    }
    fprintf(out, "Total cards in wood piles %d\n", 
        pile_size(&player->woodpilediscard) + pile_size(&player->woodpiledraw));
    fprintf(out, "Woodpile (discard): ");
    pile_dump(&player->woodpilediscard, out); 
    fprintf(out, "\n");
    fprintf(out, "Woodpile (draw): ");
    pile_dump(&player->woodpiledraw, out); 
    fprintf(out, "\n");
}

// validate single post pile consistency
static void 
validate_post_pile(struct pile *pile)
{
    for (int i = pile->top - 1; i > 0; i--) {
        uint8_t c1 = pile->_cards[i];
        uint8_t c2 = pile->_cards[i-1];
        assert(opposite_colors(get_front_color(c1), get_front_color(c2)));
        assert(get_card_number(c1) + 1 == get_card_number(c2));
        assert(get_back_color(c1) == get_back_color(c2));
    }
}

// validate post piles consistency
static void
validate_post_piles(struct player_state *player)
{
    for (int i = 0; i < 3; i++)
        if (!pile_empty(&player->post[i]))
            validate_post_pile(&player->post[i]);
}

// validate (and possibly output) global state when someone blitzed
static void
global_state_on_win(struct player_state *winner, FILE *out)
{
    for (int i = 0; i < 4; i++) {
        validate_post_piles(&players[i]);
    }
    validate_dutch();

    if (out) {
        if (winner) {
            fprintf(out, "Winner is:  %s\n", winner->name);
            player_print_state(winner, out);
            fprintf(out, "\nOther players:\n"); 
        } else {
            fprintf(out, "There was no winner:\n"); 
        }
        for (int i = 0; i < 4; i++) 
            if (players + i != winner) {
                player_print_state(&players[i], out);
                fprintf(out, "\n");
            }
        dump_dutch(out, winner == NULL);
    }
}

// prepare a player's deck by dealing blitz, post, and wood piles
static void
player_deal(struct player_state *player)
{
    prepare_deck(player->deck, player->bgcolor);

    int nextcard = 0;
    for (int i = 0; i < 3; i++) {
        pile_init(&player->post[i], 10);
        pile_push(&player->post[i], player->deck[nextcard++]);
    }
    pile_init(&player->blitz, 10);
    for (int i = 0; i < 10; i++) {
        pile_push(&player->blitz, player->deck[nextcard++]);
    }
    pile_init(&player->woodpiledraw, 30);
    pile_init(&player->woodpilediscard, 30);
    for (int i = 0; i < 27; i++) {
        pile_push(&player->woodpiledraw, player->deck[nextcard++]);
    }
}

// Play my deck, being able to read from, but not write to,
// the dutch piles
// Returns 
//  - a 1 card to put on dutch pile if one is available to play
//  - PLAY_BLITZ or PLAY_POST+0, +1, +2 if an attempt should be made to
//      play blitz or the first, second, or third post pile.
//  - PLAY_WOOD if the top of the wood pile can be dutched
//
//  -1 if no actions are possible until something moves in the dutch piles
static uint32_t
player_find_possible_move(struct player_state *player, FILE *out)
{
    const int NROUNDS = 500;
    int resetsleft = 3;

    for (int rounds = 0; rounds < NROUNDS; rounds++) {
        // check if any blitz card can be put on the dutch pile
        if (!pile_empty(&player->blitz) && fits_on_dutch_pile(pile_top(&player->blitz), false, out))
            return PLAY_BLITZ;

        // check if any post pile cards can be placed onto the dutch pile
        for (int j = 0; j < 3; j++) {
            if (!pile_empty(&player->post[j])) {
                if (fits_on_dutch_pile(pile_top(&player->post[j]), false, out))
                    return PLAY_POST + j;
            }
        }

        // see if any blitz cards can be moved onto post pile.
        bool moved = true;
        while (moved) {
            moved = false;
            for (int i = 0; !pile_empty(&player->blitz) && i < 3; i++) {
                uint8_t btopcard = pile_top(&player->blitz);
                if (pile_empty(&player->post[i])) {
                    pile_push(&player->post[i], pile_pop(&player->blitz));
                    moved = true;
                } else {
                    uint8_t to = pile_top(&player->post[i]);
                    if (get_card_number(to) == get_card_number(btopcard) + 1
                        && opposite_colors(get_front_color(btopcard), get_front_color(to))) {
                        pile_push(&player->post[i], pile_pop(&player->blitz));
                        moved = true;
                    }
                }
            }
        }
        if (pile_empty(&player->blitz))
            return BLITZED_FROM_POST;

        // now we have a choice to make - serve the wood pile first,
        // or try to consolidate the post piles.
        //
        // Check if post piles can be consolidated, starting with 
        // the post pile with the highest number.
        // For instance, if we have 3red, 4yellow, 5red we want to
        // move 4yellow onto 5red first, then 3red onto 4yellow.
        for (int cnum = 8; cnum >= 1; cnum--) {
            for (int i = 0; i < 3; i++) {
                if (pile_size(&player->post[i]) == 1) { // can move only single cards
                    uint8_t from = pile_top(&player->post[i]);
                    if (get_card_number(from) == cnum) {
                        for (int j = 0; j < 3; j++) if (i != j) {
                            if (pile_size(&player->post[j]) == 1) {
                                uint8_t to = pile_top(&player->post[j]);
                                if (get_card_number(to) == cnum + 1 
                                    && opposite_colors(get_front_color(from), get_front_color(to))) {
                                    pile_push(&player->post[j], pile_pop(&player->post[i]));
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }

        if (pile_size(&player->woodpiledraw) == 0 && pile_size(&player->woodpilediscard) == 0) {
            if (out)
                fprintf(out, "player %s ran out of wood piles\n", player->name);
            return -1;
        }

        // now it's time to sift through the wood pile. We must go in steps of 3.
        // we may run out at any step and may need to flip the woodpile draw over 
        for (int i = 0; i < 3; i++) {
            // replenish drawing pile from discard pile if out of drawing cards
            if (pile_size(&player->woodpiledraw) == 0) {
                while (pile_size(&player->woodpilediscard) > 0) {
                    pile_push(&player->woodpiledraw, pile_pop(&player->woodpilediscard));
                }
            }
            // flip card over
            pile_push(&player->woodpilediscard, pile_pop(&player->woodpiledraw));
        }

        // now check if the top card of the woodpile discard can be put on the dutch pile.
        if (fits_on_dutch_pile(pile_top(&player->woodpilediscard), false, out))
            return PLAY_WOOD;

        // at this point, we could try to place the top of the wood pile onto
        // a post pile.  However, this is risky - the rules manual actually advises
        // against it since it pads the post piles, making it less likely to accommodate
        // cards from the blitz pile.  But it may be needed to get a game unstuck,
        // particularly if the number of cards on the wood post pile is a multiple
        // of 3, that is, only 1/3 of the cards are looked at.
        // let's do that only after we've played through the complete woodpile at least once.
        //
        if (rounds > (pile_size(&player->woodpiledraw) + pile_size(&player->woodpilediscard))) {
            uint8_t wtopcard = pile_top(&player->woodpilediscard);

            for (int i = 0; i < 3; i++) {
                // should not be empty since we would have placed blitz card here
                assert (!pile_empty(&player->post[i]));

                uint8_t to = pile_top(&player->post[i]);
                if (get_card_number(to) == get_card_number(wtopcard) + 1
                    && opposite_colors(get_front_color(wtopcard), get_front_color(to))) {
                    pile_push(&player->post[i], pile_pop(&player->woodpilediscard));
                    break;
                }
            }
        }

        // As the rules say, if a player believes they are stuck, they can move the top 
        // of the wood discard pile and place it on the bottom.
        // we do this only after having recycle the wood pile a few times
        if (rounds > 2 * (pile_size(&player->woodpiledraw) + pile_size(&player->woodpilediscard))) {
            if (!pile_empty(&player->woodpiledraw))
                if (resetsleft > 0) {
                    pile_rotate_top_card_down(&player->woodpiledraw);
                    resetsleft--;
                    rounds = 0;
                }
        }
    }
    // we run out of rounds - we conclude that we must wait for some action on the dutch piles
    return -1;
}

// try to play your pile and make up to one move related to the dutch pile
// return true if a move was made
//        false if no move could be made
//
// May set blitzed if move led to this player blitzing
static bool
player_try_to_make_one_move(struct player_state *player, FILE *out)
{
    uint32_t action = player_find_possible_move(player, out);
    bool iblitzed = false;
    bool madeplay = false;
    if (action == -1) {
        if (out)
            fprintf(out, "player %s stuck deadlocked %d\n", player->name, deadlocked);
    } else {
        if (action == BLITZED_FROM_POST) {
            iblitzed = true;
        } else
        if (action == PLAY_WOOD) {
            if (fits_on_dutch_pile(pile_top(&player->woodpilediscard), true, out)) {
                pile_pop(&player->woodpilediscard);
                madeplay = true;
            }
        } else
        if (action == PLAY_BLITZ) {
            if (fits_on_dutch_pile(pile_top(&player->blitz), true, out)) {
                pile_pop(&player->blitz);
                if (pile_empty(&player->blitz)) {
                    iblitzed = true;
                }
                madeplay = true;
            }
        } else 
        if (PLAY_POST <= action && action <= PLAY_POST + 2) {
            if (fits_on_dutch_pile(pile_top(&player->post[action-PLAY_POST]), true, out)) {
                pile_pop(&player->post[action-PLAY_POST]);
                madeplay = true;
            }
        }

        if (iblitzed && !blitzed) {
            blitzed = true;
            winner = player;
            madeplay = true;
        }
    }
    return madeplay;
}

// try to take a turn and return true if the game is not over yet
bool
player_can_take_turns_and_game_not_over(struct player_state *player, FILE *out)
{
    while (!blitzed) {
        pthread_mutex_lock(&lock);
        bool madeplay = player_try_to_make_one_move(player, out);
        pthread_mutex_unlock(&lock);
        nanosleep(&ts, NULL);
        if (madeplay) {
            if (player == winner)
                global_state_on_win(player, out);
        } else {
            break;
        }
    }
    return !blitzed;
}

// main player function
void *
player_function(void *_arg)
{
    struct player_state *player = _arg;
    threadname = player->name;

    // this barrier allows threads to start at about the same time
    pthread_barrier_wait(&readysetgo);
    //pthread_mutex_lock(&lock);
    while (player_can_take_turns_and_game_not_over(player, logfile)) {
        // this player cannot make a turn right now, but the game is also
        // not over.  Mark this player as having deadlocked - if 4 players
        // deadlock (which occurs rarely, but does happen), then the
        // game should stop
        if (++deadlocked == 4) {
            break;
        }
        pthread_mutex_lock(&lock);
        deadlocked--;
        pthread_mutex_unlock(&lock);
        nanosleep(&ts, NULL);
    }
    //pthread_mutex_unlock(&lock);
    return NULL;
}

// compute score for this player
static int
score_player(struct player_state *player)
{
    int s = -2 * pile_size(&player->blitz);     // -2 for each card left in blitz pile
    for (int i = 0; i < nextdutch; i++) {
        for (int j = 0; j < dutch[i].top; j++)
            if (get_back_color(dutch[i]._cards[j]) == player->bgcolor)
                s++;        // +1 for each card in the dutch pile
    }
    return s;
}

// compute score for all players
static void
score_all_players(int scores[4], FILE *out)
{
    for (int i = 0; i < 4; i++) {
        scores[i] = score_player(&players[i]);
        if (out)
            fprintf(out, "%d ", scores[i]);
    }
    if (out)
        fprintf(out, "\n");
}

// simulate a full game and write results to `scores`
static void
simulate_one_game(int scores[4], FILE *out)
{
    for (int bgcolor = 0; bgcolor < 4; bgcolor++) {
        players[bgcolor].name = colors[bgcolor];
        players[bgcolor].bgcolor = bgcolor;
        player_deal(&players[bgcolor]);
    }

    pthread_t t[4];
    pthread_barrier_init(&readysetgo, NULL, 4);

    uint8_t startorder[4] = {0, 1, 2, 3};
    //fisher_yates(startorder, 4);
    for (int i = 0; i < 4; i++) {
        int rc = pthread_create(&t[i], NULL, player_function, players + startorder[i]);
        if (rc != 0) {
            errno = rc;
            perror("pthread_create");
        }
    }

    for (int i = 0; i < 4; i++)
    {
        pthread_join(t[i], NULL);
    }

    pthread_barrier_destroy(&readysetgo);

    if (!blitzed)
        global_state_on_win(NULL, out);

    score_all_players(scores, out);
}

int
main(int ac, char *av[])
{
    int N_GAMES = ac > 1 ? atoi(av[1]) : 1000;
    char *output = getenv("OUTPUT");
    logfile = output && !strcmp(output, "stdout") ? stdout : NULL;

    srand(time(NULL));

    int total_scores[4] = { 0 };
    for (int i = 0; i < N_GAMES; i++) {
        int scores[4];
        reset_simulation();
        simulate_one_game(scores, logfile);
        for (int j = 0; j < 4; j++)
            total_scores[j] += scores[j];
    }
    for (int i = 0; i < 4; i++) {
        fprintf(stdout, "%d ", total_scores[i]);
    }
    fprintf(stdout, "\n");
}
