#include "features/journal/reader.h"
#include "fixtures/journal_layouts.h"
#include <iostream>
#include <map>
#include <utility>
using namespace lis::journal;

// Shapes from DNEUIPage, DNEUIPageElement.TextFunc, DNESMS.FilterElements,
// DNEUISMSElement.ClipFactory and DNEThumbnailSelect (not live captures).
struct Fixture {
    std::map<std::string, std::wstring> strings, texts;
    std::map<std::string, float> numbers, getters;
    std::map<std::wstring, std::wstring> translations;
    std::vector<std::string> calls;
    Query query() {
        return {
            [this](const std::string& p) { calls.push_back(p); return strings[p]; },
            [this](const std::string& p) { calls.push_back(p); auto i = numbers.find(p);
                return i == numbers.end() ? std::numeric_limits<float>::quiet_NaN() : i->second; },
            [this](const std::string& p) { calls.push_back(p); return texts[p]; },
            [this](const std::string& p) { calls.push_back(p); auto i = getters.find(p);
                return i == getters.end() ? std::numeric_limits<float>::quiet_NaN() : i->second; },
            [this](const std::wstring& p) { auto i = translations.find(p);
                return i == translations.end() ? p : i->second; }
        };
    }
    Fixture(const wchar_t* page) {
        strings["p.PageID"] = page;
        numbers["p.LoadingFinished"] = 1;
        numbers["p.OldPageElements.length"] = 0;
        numbers["p.PageElements.length"] = 0;
    }
    Page read() { return ReadPage(query(), "p"); }
    void sms(int count) {
        numbers["p.PageElements.length"] = static_cast<float>(count);
        strings["p.ReceiverDesc.DataName"] = L"MAX";
        strings["p.EmitterDesc.DataName"] = L"KATE";
        strings["p.ReceiverDesc.DataTitle"] = L"$GT_Max";
        strings["p.EmitterDesc.DataTitle"] = L"$GT_Kate";
        translations[L"$GT_Max"] = L"Max"; translations[L"$GT_Kate"] = L"Kate";
        for (int i = 0; i < count; ++i) {
            auto e = "p.PageElements." + std::to_string(i);
            strings[e + ".From"] = L"kate";
            strings[e + ".DataTitle"] = L"$GT_Body";
        }
        translations[L"$GT_Body"] = L"Complete old message, including its last sentence.";
    }
};
int main() {
    int errors = 0;
    auto check = [&](bool ok, const char* what) {
        std::cout << (ok ? "PASS: " : "FAIL: ") << what << '\n'; errors += !ok;
    };
    Fixture journal(L"CMD_JOURNAL");
    journal.numbers["p.BrowseInfo.CurrentDay"] = 1;
    journal.numbers["p.BrowseInfo.CurrentPage"] = 2;
    journal.numbers["p.PageElements.length"] = 3;
    journal.texts["p.PageIndexMovie.TEXT"] = L"Page 3 of 8";
    for (int i = 0; i < 3; ++i) {
        auto e = "p.PageElements." + std::to_string(i);
        journal.numbers[e + ".movie_clip._visible"] = 1;
        journal.numbers[e + ".ElementType"] = i == 1 ? 2.0f : 3.0f;
        journal.strings[e + ".ResourceValue"] = L"$GT_UnrenderedStoryVariant";
    }
    journal.texts["p.PageElements.0.movie_clip.root_frame"] = L"First authored paragraph.";
    journal.texts["p.PageElements.2.movie_clip.root_frame"] = L"Second authored paragraph.";
    auto a = journal.read();
    check(a.ready && a.kind == View::Journal && a.selected == 0 && a.items.size() == 2,
        "journal paragraphs are not buried under decorative-art entries");
    check(a.items[0].text == L"First authored paragraph." &&
        a.items[1].text == L"Second authored paragraph.",
        "authored order retained; artwork never described as transcribed");
    check(a.title==L"Journal"&&a.position==L"Page 3 of 8", "native page label is separate for end-position formatting");
    journal.strings["p.PageElements.0.ResourceValue"] = L"$GT_AnotherVariant";
    check(journal.read().id != a.id, "same page with new story variant has a distinct identity");
    journal.numbers["p.LoadingFinished"] = 0;
    check(!journal.read().ready, "loading suppressed");
    journal.numbers["p.LoadingFinished"] = 1;
    journal.numbers["p.OldPageElements.length"] = 3;
    check(!journal.read().ready, "transition suppressed");
    journal.numbers["p.OldPageElements.length"] = 0;
    journal.numbers["p.PageElements.length"] = 257;
    journal.calls.clear();
    check(!journal.read().ready && journal.calls.size() < 8, "oversize arrays rejected before walking");
    journal.numbers["p.PageElements.length"] = 1.5f;
    check(!journal.read().ready, "fractional counts rejected");
    journal.numbers.erase("p.BrowseInfo.CurrentPage");
    journal.numbers["p.PageElements.length"] = 0;
    check(!journal.read().ready, "missing journal index fails closed");

    Fixture sms(L"CMD_SMS"); sms.sms(4);
    sms.texts["p.PageElements.1.movie_clip.AUTOUPDATETEXT.LABEL"] = L"Rendered completed message.";
    sms.texts["p.PageElements.1.movie_clip.AUTOUPDATETEXT.DATE"] = L"October 7, 10:30";
    sms.numbers["p.PageElements.2.UpdateTextTimer"] = 0;
    sms.numbers["p.PageElements.3.fade_in_message"] = 1;
    sms.strings["p.FadeInSMS.0.DataTitle"] = L"Future spoiler";
    auto b = sms.read();
    check(b.ready && b.items.size() == 2 && b.selected == 1,
        "latest completed SMS selected; zero timer and fade flag suppress premature replies");
    check(b.items[0].text == L"Kate: Complete old message, including its last sentence.",
        "undefined old-message flags accepted; virtualized history fully localized with sender");
    check(b.items[1].text == L"Kate, October 7, 10:30: Rendered completed message.",
        "rendered SMS body and date preferred");
    bool unsafe = false;
    for (const auto& c : sms.calls)
        unsafe |= c.find("FadeInSMS") != c.npos || c.find("ResponseContent") != c.npos ||
            c.find("SMSDate") != c.npos || c.find("Named.") != c.npos;
    check(!unsafe, "never query queued replies, raw date arrays, or invented Named storage");
    sms.texts.erase("p.PageElements.1.movie_clip.AUTOUPDATETEXT.DATE");
    check(sms.read().items[1].id == b.items[1].id, "virtualized date does not change message identity");
    sms.numbers.erase("p.PageElements.2.UpdateTextTimer");
    sms.numbers["p.PageElements.2.fade_in_message"] = 0;
    check(sms.read().items.size() == 3 && sms.read().selected == 2, "completed reply becomes available");
    sms.translations.erase(L"$GT_Body");
    check(sms.read().items[0].text == L"Kate: Message text unavailable", "unresolved identifiers never narrated");
    sms.strings["p.EmitterDesc.DataName"] = L"CHLOE";
    check(sms.read().id != b.id, "contact identity separates conversations");

    Fixture contacts(L"CMD_SMSCHARACTERSELECT");
    contacts.numbers["p.PageElements.length"] = 3;
    contacts.numbers["p.selection_clips.length"] = 2;
    contacts.getters["p.GetCurrentSelectionIndex"] = 1;
    contacts.strings["p.SelectedName"] = L"STALE";
    contacts.numbers["p.selection_clips.1._visible"] = 1;
    contacts.numbers["p.selection_clips.1.ui_element.ValidData"] = 1;
    contacts.numbers["p.selection_clips.1.ui_element.UnreadSMSCount"] = 4;
    contacts.strings["p.selection_clips.1.ui_element.DataName"] = L"KATE";
    contacts.texts["p.selection_clips.1.FromText"] = L"Kate";
    auto c = contacts.read();
    check(c.ready && c.kind == View::List && c.items[0].text == L"Kate. 4 unread",
        "native selection_clips getter drives selected contact, not restored SelectedName or element index");
    contacts.numbers["p.selection_clips.1.ui_element.ValidData"] = 0;
    check(contacts.read().items[0].text == L"Locked", "invalid entry does not expose its title");
    contacts.getters["p.GetCurrentSelectionIndex"] = 2;
    check(!contacts.read().ready, "out of range selection fails closed");
    contacts.getters.clear();
    check(!contacts.read().ready, "undefined getter cannot silently select entry zero");

    Fixture character(L"CMD_CHARACTERVIEWERPAGE");
    character.strings["p.BrowseInfo.CharacterName"] = L"KATE";
    character.numbers["p.PageElements.length"] = 1;
    character.numbers["p.PageElements.0.ElementType"] = 3;
    character.numbers["p.PageElements.0.movie_clip._visible"] = 1;
    character.texts["p.PageElements.0.movie_clip.root_frame"] = L"Current character prose.";
    check(character.read().items[0].text == L"Current character prose.", "character UIPage uses rendered text");
    character.texts["p.PageElements.0.movie_clip.root_frame"] = L"$GT_Missing";
    check(character.read().items[0].text == L"Text unavailable", "rendered unresolved key suppressed");
    Fixture unknown(L"CMD_NOT_DIARY");
    check(!unknown.read().ready && !ReadPage({}, "p").ready &&
        !ReadPage(character.query(), "p;action()").ready, "unknown pages, absent adapters and unsafe paths rejected");
    check(detail::Clean(L"Love you <3")==L"Love you <3","SMS heart is not discarded as markup");
    Fixture file(L"CMD_FILESELECT");
    file.numbers["p.scroll_locked"]=0;file.numbers["p.ObjectImages.length"]=2;file.numbers["p.current_page"]=1;
    file.strings["p.ObjectDescriptions.1"]=L"$GT_HiddenDocument";
    file.texts["p.DescriptionMovieClip.Label"]=L"Rendered document transcription.";
    auto image=file.read();
    check(image.ready&&image.document&&image.items[0].text.find(L"text version is available")!=std::wstring::npos,"file viewer offers native transcription without exposing hidden text");
    file.numbers["p.DescriptionVisible"]=1;file.numbers["p.DescriptionMovieClip._visible"]=1;
    check(file.read().items[0].text==L"Rendered document transcription."&&file.read().id!=image.id,"native Enter/A text overlay is narrated only when shown");
    file.numbers["p.scroll_locked"]=1;check(!file.read().ready,"document image transition is not read early");
    file.numbers["p.scroll_locked"]=0;
    file.strings["p.PageID"]=L"CMD_OBJECT_VIEWER";
    file.strings["p.ObjectName"]=L"E1_3A_CampusA_Board01";
    const auto poster=file.read();
    check(poster.ready&&poster.inspection&&poster.document&&poster.title==L"Object inspection"&&
        poster.items[0].text==L"Rendered document transcription.","world inspection uses the working Files text-view path, not diary fallback");
    file.numbers["p.DescriptionVisible"]=0;file.calls.clear();
    check(file.read().ready&&file.read().items[0].text.find(L"text version is available")!=std::wstring::npos,"world image view offers existing text without opening it automatically");
    check(std::find(file.calls.begin(),file.calls.end(),"p.DescriptionMovieClip.Label")==file.calls.end(),"closed transcription is not read early");
    file.strings["p.ObjectDescriptions.1"]=L"";
    check(file.read().items[0].text==L"Image. No text version is available.","image-only world objects honestly report missing text");
    file.numbers["p.scroll_locked"]=1;
    check(!file.read().ready&&file.read().inspection,"inspection domain survives loading");

    Fixture prequel(L"CMD_JOURNAL");
    prequel.numbers["p.BrowseInfo.CurrentDay"]=journal_fixtures::PrequelDay;
    prequel.numbers["p.BrowseInfo.CurrentPage"]=0;
    check(prequel.read().ready&&prequel.read().id.find(":-1:0:")!=std::string::npos,"real PREQUEL day -1 is valid");
    prequel.numbers["p.BrowseInfo.CurrentPage"]=-1;
    check(!prequel.read().ready,"prequel exception does not admit a negative page index");
    prequel.numbers["p.BrowseInfo.CurrentPage"]=0;
    for(float invalid:{-2.f,-1.5f,std::numeric_limits<float>::quiet_NaN()}) {
        prequel.numbers["p.BrowseInfo.CurrentDay"]=invalid;
        check(!prequel.read().ready,"invalid day still fails closed");
    }
    auto layout=[&](const auto& placements,int size,const wchar_t* name,const wchar_t* expected) {
        Fixture f(L"CMD_CHARACTERVIEWERPAGE");
        f.strings["p.BrowseInfo.CharacterName"]=name;f.numbers["p.PageElements.length"]=static_cast<float>(size);
        for(int i=0;i<size;++i) {
            const auto e="p.PageElements."+std::to_string(i);
            f.numbers[e+".movie_clip._visible"]=1;f.numbers[e+".ElementType"]=2;
        }
        for(const auto& place:placements) {
            const auto e="p.PageElements."+std::to_string(place.index);
            f.strings[e+".ResourceValue"]=place.resource;f.numbers[e+".ElementType"]=3;
            f.numbers[e+".PositionX"]=place.x;f.numbers[e+".PositionY"]=place.y;
            // Short synthetic captions identify real resource placements without
            // copying game prose into fixtures or sorting by those suffixes.
            f.texts[e+".movie_clip.root_frame"]=std::wstring(place.resource).back();
        }
        auto result=f.read();std::wstring order;
        for(const auto& item:result.items)order+=item.text;
        check(result.ready&&order==expected,"real character layout reads left page then right, top to bottom");
        f.strings["p.PageID"]=L"CMD_JOURNAL";f.numbers["p.BrowseInfo.CurrentDay"]=0;f.numbers["p.BrowseInfo.CurrentPage"]=0;
        order.clear();for(const auto& item:f.read().items)order+=item.text;
        check(order==expected,"journal shares the same book-layout policy");
        const auto hidden="p.PageElements."+std::to_string(placements[0].index)+".movie_clip._visible";
        f.numbers[hidden]=0;
        check(f.read().items.size()+1==result.items.size(),"ordering never brings hidden story elements into speech");
    };
    layout(journal_fixtures::Victoria,10,L"VIctoria",L"CBDEA");
    layout(journal_fixtures::Principal,9,L"Principal",L"BAC");
    std::vector<Item> mixed={{"postit",L"Objective"},{"right",L"Right",30,-280},{"left",L"Left",-580,210}};
    detail::BookOrder(mixed);
    check(mixed[0].id=="postit"&&mixed[1].id=="left"&&mixed[2].id=="right","unknown-position objective retains its slot; left page precedes higher right-page text");
    return errors ? 1 : 0;
}
