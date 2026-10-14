#pragma once
#include "engine/gfx_value.h"
#include <initializer_list>

namespace lis::journal {
// DNEFocusUtils.DispatchKeyFunc: top-down, normal then nonrepeat key. Covered
// pages become DNEMenuPageFocusProxy objects with NO onKeyFunc; skip them.
template<class Invoke>
engine::gfx::KeyResult DispatchTabKeys(int count,int direction,const Invoke& invoke) {
    using engine::gfx::KeyResult;
    if(count<1||count>16||(direction!=-1&&direction!=1))return KeyResult::Failed;
    for(int i=count-1;i>=0;--i)for(int extra:{0,1000}) {
        const auto result=invoke(i,(direction<0?100:103)+extra);
        if(result!=KeyResult::Unhandled)return result;
    }
    return KeyResult::Unhandled;
}
}
