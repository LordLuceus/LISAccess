#pragma once
#include "features/journal/reader.h"
#include <algorithm>
#include <set>

namespace lis::journal {
inline bool ItemReading(View view){return view==View::Journal||view==View::Messages;}
inline std::wstring Join(const std::wstring& a,const std::wstring& b) {
    if(a.empty())return b;if(b.empty())return a;
    return a+((a.back()==L'.'||a.back()==L'!'||a.back()==L'?')?L" ":L". ")+b;
}
struct Cursor {
    std::string context,selected;
    std::set<std::string> seen;
    int index=-1;
    void Reset(){context.clear();selected.clear();seen.clear();index=-1;}
    void Select(const Page& p,int value) {
        index=value;selected=index>=0&&index<static_cast<int>(p.items.size())?p.items[index].id:std::string{};
    }
    std::wstring Describe(const Page& p) const {
        if(index<0||index>=static_cast<int>(p.items.size()))return Join(p.title,L"No readable text on this page.");
        const int at=p.nativeIndex>=0?p.nativeIndex:index;
        const int count=p.nativeCount>0?p.nativeCount:static_cast<int>(p.items.size());
        return Join(Join(p.items[index].text,std::to_wstring(at+1)+L" of "+std::to_wstring(count)+L"."),p.position);
    }
    std::wstring Update(const Page& p,const std::string& identity) {
        if(!p.ready)return {};
        if(context!=identity) {
            Reset();context=identity;
            Select(p,p.selected>=0&&p.selected<static_cast<int>(p.items.size())?p.selected:p.items.empty()?-1:0);
            for(const auto& item:p.items)seen.insert(item.id);
            return p.items.empty()?Describe(p):Join(p.title,Describe(p));
        }
        const auto found=std::find_if(p.items.begin(),p.items.end(),[&](const auto& item){return item.id==selected;});
        const auto oldSelected=selected;
        if(p.kind==View::List)Select(p,p.selected);
        else if(found!=p.items.end())Select(p,static_cast<int>(found-p.items.begin()));
        else Select(p,p.items.empty()?-1:0);
        std::wstring incoming;
        for(size_t i=0;i<p.items.size();++i)if(seen.insert(p.items[i].id).second&&p.kind==View::Messages) {
            // Completed new replies are announced once. Reading older history
            // does not move the cursor merely because a new reply arrives.
            const bool atEnd=index==static_cast<int>(i)-1||index<0;
            if(atEnd)Select(p,static_cast<int>(i));
            incoming=Join(incoming,p.items[i].text);
        }
        if(!incoming.empty())return Join(L"New message",incoming);
        return p.kind==View::List&&selected!=oldSelected?Describe(p):std::wstring{};
    }
    std::wstring Move(const Page& p,int direction) {
        if(!p.ready||!ItemReading(p.kind)||p.items.empty())return L"No readable items on this page.";
        const int wanted=index+direction;
        if(wanted<0)return L"First item.";
        if(wanted>=static_cast<int>(p.items.size()))return L"Last item.";
        Select(p,wanted);return Describe(p);
    }
};
// Edge/repeat policy is separate from hardware and ignores keys held on entry.
struct PadReader {
    uint16_t previous=0;uint64_t next=0;bool active=false;
    int Update(uint16_t buttons,bool enabled,bool connected,uint64_t now) {
        const uint16_t edges=buttons&~previous;previous=buttons;
        if(!enabled||!connected){active=false;next=0;return 0;}
        if(!active){active=true;return 0;}
        if(edges&0x40)return 3; // left-stick click: reread
        if(edges&0x80)return 4; // right-stick click: help
        const unsigned held=buttons&3;
        if(held!=1&&held!=2){next=0;return 0;}
        if(edges&held){next=now+450;return held==1?1:2;}
        if(next&&now>=next){next=now+180;return held==1?1:2;}
        return 0;
    }
};
}
