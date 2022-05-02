# CS3214-DutchBlitz

**Dutch Blitz**

(Dutch Blitz is a registered trademark by the company who makes and distributes
the card game.)
According to https://dutchblitz.com/:
"Dutch Blitz® is a highly interactive, highly energetic, family-friendly
card game that will test your skills, smarts and speed."
You can find a Youtube video introducing the game here:
https://www.youtube.com/watch?v=6n3VyC0ngY8

**Introduction**

Dutch Blitz is a concurrent game.  4 players play simultaneously with their
own deck in a manner that resembles a Solitaire-style game:
Player have 3 temporary piles of cards (the post piles) where cards can
be temporarily placed, they have a pile of cards (the blitz pile) that they
try to get rid of, and they have a pile of cards in their hands (the wood
pile) from which they draw cards in an attempt to place them.
Each player is given a color (red, green, yellow, or blue) that is also
shown on the back of each card of that player's deck.
On the front, each player's deck consists of 4 x 10 = 40 different faces
representing each of the 4 colors, combined with the numbers from 1 to 10.
Cards are to be placed onto the so-called dutch piles in the middle of
the table - these piles are shared between players.  Thus, players need
to synchronize when placing cards there.  For instance, if there is a stack
with a yellow 1, then only one player can place a yellow 2 on top of it,
even if two players are ready to play a yellow 2.
In real game play, this synchronization takes place using a system in
which the fastest player wins (adults, of course, should make sure to let
children go first in the event that two players try to play the same card
at the same moment).  Each player may, however, play only one card at a
time so that all players get a chance to access the dutch pile to play a card.
Throwing cards or using both hands is not allowed!

**Goal**

The goal of this guided exercise is to simulate this game using multiple
threads. These threads will run concurrently, and likely in parallel,
on multiple cores or CPU.  In order to ensure consistency, you will need
to learn how to use mutexes (aka locks) and condition variables in the
monitor pattern.
