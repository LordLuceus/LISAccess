#include "features/world/target_list.h"
#include <cstdlib>
#include <iostream>
using namespace lis::world;
void Check(bool ok,const char* what){if(!ok){std::cerr<<what<<'\n';std::exit(1);}}
std::wstring Label(const Snapshot& s,const std::string& id){for(const auto& t:s.targets)if(t.stableId==id)return t.label;return {};}
int main() {
    Check(TargetLetter(0)==L"A"&&TargetLetter(25)==L"Z"&&TargetLetter(26)==L"AA"&&
        TargetLetter(27)==L"AB"&&TargetLetter(701)==L"ZZ"&&TargetLetter(702)==L"AAA","Hortator-style alphabetic rollover");
    Snapshot original;original.focusedId="photo-b";
    original.targets={{"photo-b",L"Photo",{10,0,0},Kind::Object},
                      {"photo-a",L"Photo",{100,0,0},Kind::Object},
                      {"bag",L"Bag",{5,0,1000},Kind::Object},
                      {"destination",L"Exit",{50,0,0},Kind::Destination}};
    auto first=original;PrepareTargets(first);
    Check(first.targets[0].stableId=="bag"&&first.targets[1].stableId=="photo-b"&&
          first.targets[2].stableId=="destination","all kinds sort by ground distance; elevation separate");
    Check(Label(first,"photo-a")==L"Photo, A"&&Label(first,"photo-b")==L"Photo, B","letters follow identity, not distance");
    Check(Label(first,"bag")==L"Bag"&&first.focusedLabel==L"Photo, B","unique unchanged; real focus uses same letter");
    auto moved=original;moved.player={110,0,0};PrepareTargets(moved);
    Check(moved.targets[0].stableId=="photo-a"&&Label(moved,"photo-a")==Label(first,"photo-a")&&
          Label(moved,"photo-b")==Label(first,"photo-b"),"walking changes order, not duplicate identities");
    Snapshot tied;tied.targets={{"z",L"Z",{1,0,0}},{"a",L"A",{-1,0,0}}};PrepareTargets(tied);
    Check(tied.targets[0].stableId=="a","distance ties deterministic");
    Snapshot many;many.player={-418.795624f,173.444305f,47.5f};
    // Exit identity/position from the real classroom capture; other entries are synthetic.
    const std::string door="e1_2a_corridor_ld.TheWorld.PersistentLevel.WhatIfInteractionActor_ArtClassDoor_0";
    many.targets.push_back({door,L"Door",{344.988037f,148.639694f,109.626457f}});
    for(int i=0;i<40;++i)many.targets.push_back({"synthetic-"+std::to_string(i),L"Photo",{float(i),0,0}});
    PrepareTargets(many);
    Check(many.targets.size()==41&&Label(many,door)==L"Door","presentation retains more than32 and the distant captured exit");
    Check(many.targets.back().stableId==door,"actual distant exit remains ordered, not promoted to focus");
    Snapshot single;single.targets={{"photo-b",L"Photo",{}}};PrepareTargets(single);
    Check(single.targets[0].label==L"Photo","unique group has no duplicate suffix");
    Snapshot empty;PrepareTargets(empty);Check(empty.targets.empty(),"empty list safe");
}
