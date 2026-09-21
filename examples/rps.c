// Rock, paper, scissors against the computer: best of three rounds.
// A C version of the assembly example in CeresASM's tutorial, using read_choice for the menu and rand for
// the computer's move. The generator has a fixed seed, so a game can be replayed - and so the example has an
// expected output (examples/expected/rps.stdin holds the moves).
#include "stdio.h"
#include "stdlib.h"
#include "ceres/line.h"

static const char* const names[3] = { "rock", "paper", "scissors" };

// 0: a draw; 1: the player wins; -1: the computer wins. Each move beats the one before it (paper beats rock).
static int judge(int player, int computer)
{
    if (player == computer)
        return 0;
    return (player - computer + 3) % 3 == 1 ? 1 : -1;
}

int main(void)
{
    srand(7);
    int player_score = 0, computer_score = 0;
    puts("Best of three. Type the number of your move.");
    for (int round = 1; round <= 3; round++)
    {
        int mine = read_choice("your move", names, 3) - 1;      // it answers 1..3
        int theirs = rand() % 3;
        printf("round %d: you play %s, the computer plays %s: ", round, names[mine], names[theirs]);
        int result = judge(mine, theirs);
        if (result > 0) { puts("you win the round"); player_score++; }
        else if (result < 0) { puts("the computer wins the round"); computer_score++; }
        else puts("a draw");
    }
    printf("final score %d - %d: %s\n", player_score, computer_score,
           player_score > computer_score ? "you win" : (player_score < computer_score ? "the computer wins" : "a tie"));
    return 0;
}
