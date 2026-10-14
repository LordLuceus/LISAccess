#include "features/prompts/controls.h"
#include "features/prompts/extraction.h"
#include <iostream>
using namespace lis::prompts;
Controls Authored() {
    // ExampleGame.upk Default__DynamicInputSwitchManager, export3560:
    // serialized GamepadActionList, not guessed defaults or tutorial special cases.
    Controls c;c.mode=1;c.ready=true;
    c.actions={{L"action_move",L"[L]"},{L"action_jog_fastrewind",L"[R2]"},
        {L"action_rewind",L"[L2]"},{L"action_skip",L"[R1]"},
        {L"action_pause",L"[Start]"},{L"action_diary",L"[Select]"},
        {L"action_replacecamera",L"[R3]"},{L"action_quickrewind",L"[L1]"},
        {L"action_movecamera",L"[R]"},{ControlKey(L"ACTION_INTERACT"),L"[CROSS]"},
        {L"action_getup",L"[CIRCLE]"},{L"action_skip_credits",L"[R1]"}};
    return c;
}
int main() {
    int errors=0;auto check=[&](bool ok,const char* message){std::cout<<(ok?"PASS: ":"FAIL: ")<<message<<'\n';errors+=!ok;};
    auto controls=Authored();
    // Actual shipped HUD.INT Tuto01/Tuto04 and the user's exact spoken symptom.
    const std::wstring rewind=L"Hold [action_rewind] to rewind.",faster=L"Hold [action_jog_fastrewind] to move faster.";
    auto r=ControllerCaption(rewind,L"Hold to rewind.",controls);
    check(r.text==L"Hold LT to rewind."&&r.changed&&!r.unavailable,"Tuto01 restores authored LT in the reported missing-button sentence");
    check(ControllerCaption(faster,L"Hold to move faster.",controls).text==L"Hold RT to move faster.","Tuto04 restores authored RT without matching English sentences");
    // Actual shipped French and German Tuto01: localized prose is not replaced.
    check(ControllerCaption(L"Maintenez [action_rewind] enfoncée pour remonter le temps.",
        L"Maintenez enfoncée pour remonter le temps.",controls).text==L"Maintenez LT enfoncée pour remonter le temps.","French tutorial prose remains intact");
    check(ControllerCaption(L"Halte [action_rewind], um die Zeit zurückzudrehen.",
        L"Halte , um die Zeit zurückzudrehen.",controls).text==L"Halte LT, um die Zeit zurückzudrehen.","German tutorial prose and punctuation remain intact");
    controls.actions[L"action_rewind"]=L"[R2]";
    check(ControllerCaption(rewind,L"Hold to rewind.",controls).text==L"Hold RT to rewind.","runtime description overrides the authored default");
    controls=Authored();controls.mode=0;
    check(ControllerCaption(rewind,L"Hold Q to rewind.",controls).text==L"Hold Q to rewind.","keyboard mode preserves the already-rendered remapped key");
    controls.mode=-1;
    check(!ControllerCaption(rewind,L"Hold to rewind.",controls).changed,"unknown input mode cannot assume gamepad");
    controls=Authored();
    check(ExpandControls(L"[ACTION_INTERACT] [Action_Skip] [TRIANGLE] [R] [L3]",controls).text==L"A RB Y right stick left stick click","case-insensitive actions and direct native glyph vocabulary");
    for(const auto& action:controls.actions){const auto e=ExpandControls(L"["+action.first+L"]",controls);check(e.complete&&!e.text.empty()&&e.text.find(L'[')==std::wstring::npos,"each authored action expands to a physical control");}
    check(ControllerCaption(rewind,L"Hold \ufffc to rewind.",controls).text==L"Hold LT to rewind.","embedded image replacement glyph does not lose its button name");
    check(ControllerCaption(rewind,L"Hold \ue001 to rewind.",controls).text==L"Hold LT to rewind.","private-use control glyph is replaced, not sent silently to speech");
    check(ControllerCaption(rewind,L"Hold LT to rewind.",controls).text==L"Hold LT to rewind.","already-readable button is not duplicated");
    check(ControllerCaption(rewind,L"An unrelated localized caption.",controls).unavailable,"mismatched prose cannot replace visible text");
    check(ControllerCaption(L"An ordinary notice.",L"An ordinary notice.",controls).text==L"An ordinary notice.","unrelated notices stay unchanged");
    controls.ready=false;r=ControllerCaption(rewind,L"Hold to rewind.",controls);
    check(r.unavailable&&r.text.find(L"Controller button names are unavailable.")!=std::wstring::npos,"missing binding data explicitly reports unavailable button names");
    controls=Authored();controls.actions[L"action_rewind"]=L"[action_rewind][action_rewind]";
    check(!ExpandControls(rewind,controls).complete,"cyclic descriptions are bounded and do not expand exponentially");
    controls=Authored();controls.actions.erase(L"action_rewind");
    check(ControllerCaption(rewind,L"Hold to rewind.",controls).unavailable,"unknown action has no invented default binding");
    controls=Authored();
    std::map<std::string,std::wstring> strings={{"root._visible",L"true"},{"root._alpha",L"100"},
        {"root.open_seq.sub_sequences.0.SP_Label.textField._visible",L"true"},
        {"root.open_seq.sub_sequences.0.SP_Label.textField._alpha",L"100"},
        {"root.open_seq.sub_sequences.0.SP_Label.textField.text",L"Hold to rewind."},
        {"root.open_seq.sub_sequences.0.wanted_text",L"$GT_HUD_TUTO_TUTO01"}};
    auto string=[&](const std::string& key){return strings[key];};
    auto number=[](const std::string& key){return key=="root.opening_state"?3.f:key=="root.open_seq.sub_sequences.length"?1.f:0.f;};
    int calls=0;auto restore=[&](const std::wstring& key,const std::wstring& text){++calls;return ControllerCaption(key==L"$GT_HUD_TUTO_TUTO01"?rewind:L"",text,controls).text;};
    auto caption=ReadComposition("root",string,number,restore);
    check(caption.text==L"Hold LT to rewind."&&caption.key==L"$GT_HUD_TUTO_TUTO01","visible caption extraction routes its actual selected ID through the resolver");
    History history;history.Observe({{"prompt",caption.key,caption.text}});check(history.Take()==L"Hold LT to rewind.","resolved buttons reach automatic narration");
    history.Observe({{"prompt",caption.key,caption.text}});check(history.Take().empty()&&history.Repeat()==L"Hold LT to rewind.","repeat retains control names without automatic frame spam");
    strings["root.open_seq.sub_sequences.0.SP_Label.textField._visible"]=L"false";calls=0;
    check(ReadComposition("root",string,number,restore).text.empty()&&calls==0,"hidden text cannot be resurrected from localization");
    return errors?1:0;
}
