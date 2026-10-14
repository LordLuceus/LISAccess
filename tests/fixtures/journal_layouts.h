#pragma once
// Extracted from original LIS GP_Character_E1_SF.upk (build2304417).
// Unique export paths, not ambiguous UIObjectDescription_N short names.
// Only text placements listed; indices/counts retain the original full arrays.
namespace journal_fixtures {
struct Placement {int index;const wchar_t* resource;float x,y;};
// Export8: gp_character_e1.CharacterDef2.UIObjectDescription_1, page0,10 elements.
inline constexpr Placement Victoria[]={
    {5,L"$GT_Menus_Diary_Chara_Victoria_A",30,130},
    {6,L"$GT_Menus_Diary_Chara_Victoria_B",-580,210},
    {7,L"$GT_Menus_Diary_Chara_Victoria_C",-610,-160},
    {8,L"$GT_Menus_Diary_Chara_Victoria_D",15,-250},
    {9,L"$GT_Menus_Diary_Chara_Victoria_E",50,50}
};
// Export9: gp_character_e1.CharacterDef2.UIObjectDescription_3, page0,9 elements.
inline constexpr Placement Principal[]={
    {6,L"$GT_Menus_Diary_Chara_Principal_A",20,-80},
    {7,L"$GT_Menus_Diary_Chara_Principal_B",-300,-280},
    {8,L"$GT_Menus_Diary_Chara_Principal_C",70,110}
};
// ExampleGame GP_Journal.JournalInfo.JournalPerDay: PREQUEL=-1, MONDAY=0.
inline constexpr int PrequelDay=-1;
}
