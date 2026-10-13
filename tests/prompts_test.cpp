#include "features/prompts/policy.h"
#include "features/prompts/extraction.h"
#include <iostream>
using namespace lis::prompts;
int main() {
    int errors=0;
    auto check=[&](bool ok,const char* what){std::cout<<(ok?"PASS: ":"FAIL: ")<<what<<'\n';errors+=!ok;};
    check(ClipPath(L"/main_mc/tooltip_4")=="_root.main_mc.tooltip_4","AS2 target paths are translated without invocation");
    check(ClipPath(L"/bad[0]").empty()&&ClipPath(L"_root.foo").empty(),"unrecognized target syntax fails closed");
    check(Adapt(L"$GT_HUD_TUTO_TUTO30",L"mouse instruction").find(L"announced number")!=std::wstring::npos,"real photo tutorial ID teaches verified action controls");
    const std::wstring selfie=L"Aim at the camera, hold Left Mouse Button, drag downwards and release Left Mouse Button to take a selfie.";
    check(Adapt(L"$GT_HUD_TUTO_TUTO25",selfie).find(L"announced number")!=std::wstring::npos,"recorded failing selfie ID gets accessible wording");
    check(Adapt(L"$GT_HUD_Tuto_Tuto25",selfie)==Adapt(L"$GT_HUD_TUTO_TUTO25",selfie),"shipped mixed case and notification uppercase IDs agree");
    check(Adapt(L"$GT_HUD_ANNOUNCEMENT_Tuto25",selfie)==selfie,"the formerly invented namespace is not treated as a tutorial ID");
    check(Adapt(L"other",L"drag a photo")==L"drag a photo","unrelated prose is never rewritten by substring");
    check(Adapt(L"$GT_HUD_TUTO_TUTO30",L"").empty(),"known ID alone cannot announce unavailable text");
    History h;
    h.Observe({{"tooltip",L"key",L"Look around."}});
    check(h.Take()==L"Look around.","new visible prompt is announced");
    h.Observe({{"tooltip",L"key",L"Look around."}});check(h.Take().empty(),"steady prompt does not repeat every frame");
    h.Observe({});check(h.Repeat()==L"Look around.","timed prompt remains repeatable after disappearing");
    h.Observe({{"tooltip",L"key",L"Look around."}});check(h.Take()==L"Look around.","reopening a prompt is a new event");
    h.Observe({{"tooltip",L"key",L"Updated."}});check(h.Take()==L"Updated.","changed visible text is announced");
    h.Observe({{"quotes",L"one",L"First."},{"tooltip",L"two",L"Second."}});
    check(h.Take()==L"First."&&h.Take()==L"Second.","simultaneous channels retain both messages");
    h.Observe({{"prompt",L"new",L"Background."}});h.Background();check(h.Take().empty(),"background backlog is discarded");
    History empty;check(empty.Repeat()==L"No prompt has been read yet.","repeat without a prompt is explicit");
    std::map<std::string,std::wstring> strings;
    std::map<std::string,float> numbers;
    auto string=[&](const std::string& path){auto p=strings.find(path);return p==strings.end()?std::wstring{}:p->second;};
    auto number=[&](const std::string& path){auto p=numbers.find(path);return p==numbers.end()?0.f:p->second;};
    strings["root._visible"]=L"true";strings["root._alpha"]=L"100";
    numbers["root.opening_state"]=3;numbers["root.open_seq.sub_sequences.length"]=2;
    const std::string leaf="root.open_seq.sub_sequences.0";
    strings[leaf+".composition_name"]=L"text_mc";
    strings[leaf+".SP_Label.textField._visible"]=L"true";
    strings[leaf+".SP_Label.textField._alpha"]=L"100";
    strings[leaf+".SP_Label.textField.text"]=L"The actual localized instruction.";
    strings[leaf+".wanted_text"]=L"$GT_HUD_TUTO_TUTO30";
    strings["root.open_seq.sub_sequences.1.shadow_text"]=L"true";
    strings["root.open_seq.sub_sequences.1.SP_Label.textField.text"]=L"Shadow must not be read.";
    auto caption=ReadComposition("root",string,number);
    check(caption.text==Adapt(L"$GT_HUD_TUTO_TUTO30",L"The actual localized instruction.")&&caption.key==L"$GT_HUD_TUTO_TUTO30","rendered tutorial leaf is adapted using its actual source ID");
    const std::string other="root.open_seq.sub_sequences.1";
    strings[other+".shadow_text"]=L"false";
    strings[other+".SP_Label.textField._visible"]=L"true";
    strings[other+".SP_Label.textField._alpha"]=L"100";
    strings[other+".SP_Label.textField.text"]=L"An unrelated notice.";
    check(ReadComposition("root",string,number).text.find(L"An unrelated notice.")!=std::wstring::npos,"adapting a tutorial preserves other notifications in the same container");
    strings[other+".shadow_text"]=L"true";
    numbers["root.opening_state"]=1;
    check(ReadComposition("root",string,number).text.empty(),"closing widgets cannot produce new announcements");
    numbers["root.opening_state"]=2;strings[leaf+".SP_Label.textField._visible"]=L"false";
    check(ReadComposition("root",string,number).text.empty(),"hidden caption text is not spoken");
    return errors?1:0;
}
