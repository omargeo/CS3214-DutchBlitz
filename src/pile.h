
struct pile {
    int top; 
    int cap;
    uint8_t *_cards;
};

void pile_init(struct pile *pile, int cap);
void pile_push(struct pile *pile, uint8_t card);
uint8_t pile_pop(struct pile *pile);
void pile_rotate_top_card_down(struct pile *pile);
uint8_t pile_top(struct pile *pile);
bool pile_empty(struct pile *pile);
int pile_size(struct pile *pile);
void pile_dump(struct pile *pile, FILE *out);
