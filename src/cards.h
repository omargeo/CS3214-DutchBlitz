/*
 * Red, Green, Blue, Yellow (on back)
 * Green/Yellow - Girl
 * Blue/Red - Boy
 * Total 4 x 40 = 160 cards
 * Each card is encoded as follows:
 *
 * back1 back0 front1 front0 n3 n2 n1 n0
 */
enum Color { RED, GREEN, BLUE, YELLOW };
static const char *colors[] = { "R", "G", "B", "Y" };

static enum Color 
get_back_color(uint8_t card)
{
    return (enum Color)(card >> 6);
}

static enum Color 
get_front_color(uint8_t card)
{
    return (enum Color)((card >> 4) & 0x3);
}

static int 
get_card_number(uint8_t card)
{
    return (card & 0xf);
}

static uint8_t make_card(enum Color back, enum Color front, int number) __attribute__((__unused__));
static bool opposite_colors(enum Color c1, enum Color c2) __attribute__((__unused__));
static bool 
opposite_colors(enum Color c1, enum Color c2)
{
    return (c1 & 2) != (c2 & 2);    // bit 2 is 0 for GREEN/RED, but 1 for YELLOW/BLUE
}

static uint8_t 
make_card(enum Color back, enum Color front, int number)
{
    return back << 6 | front << 4 | number;
}

static void
print_card(uint8_t card, bool includeback, FILE *file)
{
    enum Color front = get_front_color(card);
    enum Color back = get_back_color(card);
    int num = get_card_number(card);
    if (includeback)
        fprintf(file, "%s%d|%s", colors[front], num, colors[back]);
    else
        fprintf(file, "%s%d ", colors[front], num);
}

