#include "features/menus/narration.h"
#include <iostream>
#include <cstdlib>
void check(bool condition, const char* test) {
    if (!condition) { std::cerr << "FAILED: " << test << '\n'; std::exit(1); }
}
int main() {
    check(lis::DescribeMenu({L"Press Any Button"})==L"Press Any Button","title has no position suffix");
    check(lis::DescribeMenu({L"New Game"})==L"New Game","menu has no position suffix");
    check(lis::DescribeMenu({L"Savegame 1",L"Episode 1\u00a0",L"Nightmare"})==L"Savegame 1, Episode 1, Nightmare","slot names preserved without position suffix");
    check(lis::DescribeMenu({L"",L"$GT_Menus_Placeholder"}).empty(),"no invented labels");
    check(lis::DescribeMenu({L"Quit?"},0)==L"Quit? Enter to confirm. Escape to cancel.","modal has no invented yes/no selection");
    check(lis::DescribeMenu({L"Continue.",L"Continue."})==L"Continue.","deduplication and punctuation");
    check(lis::DescribeMenu({L"Wait"},4)==L"Wait","buttonless modal");
    std::cout << "PASS: 7 narration cases\n";
}
