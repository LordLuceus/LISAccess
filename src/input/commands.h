#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace lis::input {
using std::size_t;
enum class Command : size_t { Previous, Next, Describe, Beacon, Help, Obstacles, Preview, Face, Action1, Action2, Action3, Action4, Count };
constexpr size_t Count=static_cast<size_t>(Command::Count);
// End belongs to the native diary. Shift+Home extends our existing Home family.
constexpr std::array<int,Count> Keys={0x21,0x22,0x24,0x24,0x70,0x71,0x72,'F','1','2','3','4'};
struct Modifiers {bool shift=false,control=false,alt=false;};
using Presses=std::array<bool,Count>;
struct Keyboard {
    Presses held{};
    std::array<uint64_t,Count> repeatAt{};
    Presses Update(const Presses& raw,Modifiers modifiers,bool foreground,uint64_t now) {
        Presses result{};
        for(size_t i=0;i<Count;++i) {
            const bool allowed=!modifiers.control&&!modifiers.alt&&
                (i==static_cast<size_t>(Command::Beacon) ? modifiers.shift : !modifiers.shift);
            const bool edge=raw[i]&&!held[i];
            const bool repeatable=i==static_cast<size_t>(Command::Previous)||i==static_cast<size_t>(Command::Next);
            if(edge)repeatAt[i]=now+450;
            if(foreground&&allowed&&raw[i]) {
                result[i]=edge||(repeatable&&repeatAt[i]&&now>=repeatAt[i]);
                if(result[i]&&!edge)repeatAt[i]=now+180;
            } else if(!foreground||!allowed) repeatAt[i]=0;
            if(!raw[i])repeatAt[i]=0;
            held[i]=raw[i];
        }
        return result;
    }
};
struct Controller {
    uint16_t previous=0;
    bool thumbCommand=false,thumbStarted=false;
    uint64_t leftRepeat=0,rightRepeat=0;
    Presses Update(uint16_t buttons,bool active,uint64_t now) {
        constexpr uint16_t Up=1,Down=2,Left=4,Right=8,Thumb=0x40;
        Presses out{};
        const uint16_t edge=buttons&~previous;
        const bool thumb=(buttons&Thumb)!=0;
        if(edge&Thumb){thumbCommand=false;thumbStarted=active;}
        if(thumb&&(buttons&(Up|Down|Left|Right)))thumbCommand=true;
        if(!active){thumbStarted=false;leftRepeat=rightRepeat=0;}
        if(active) {
            if((previous&Thumb)&&!thumb&&thumbStarted&&!thumbCommand)out[static_cast<size_t>(Command::Face)]=true;
            if(thumb) {
                if(edge&Up)out[static_cast<size_t>(Command::Help)]=true;
                if(edge&Down)out[static_cast<size_t>(Command::Obstacles)]=true;
                if(edge&(Left|Right))out[static_cast<size_t>(Command::Preview)]=true;
                leftRepeat=rightRepeat=0;
            } else {
                if(edge&Up)out[static_cast<size_t>(Command::Describe)]=true;
                if(edge&Down)out[static_cast<size_t>(Command::Beacon)]=true;
                auto cycle=[&](uint16_t button,uint64_t& repeat,Command command) {
                    if(edge&button){out[static_cast<size_t>(command)]=true;repeat=now+450;}
                    else if((buttons&button)&&repeat&&now>=repeat){out[static_cast<size_t>(command)]=true;repeat=now+180;}
                    if(!(buttons&button))repeat=0;
                };
                cycle(Left,leftRepeat,Command::Previous);cycle(Right,rightRepeat,Command::Next);
            }
        }
        previous=buttons;return out;
    }
};
}
