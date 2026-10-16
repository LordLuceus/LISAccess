#include "features/navigation/face_reply.h"
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<"\n";return 1;}}while(false)
int main(){
    lis::navigation::detail::FaceReplies gate;
    CHECK(!gate.Current(0));
    const auto first=gate.Advance();
    CHECK(gate.Current(first));
    const auto repeatedSameTarget=gate.Advance();
    CHECK(!gate.Current(first)); // Old completion cannot clear the repeated request.
    CHECK(gate.Current(repeatedSameTarget));
    gate.Advance(); // Retarget, menu, foreground loss or explicit cancellation.
    CHECK(!gate.Current(repeatedSameTarget));
    const auto resumed=gate.Advance();
    CHECK(gate.Current(resumed)&&!gate.Current(first));
    std::cout<<"PASS: late facing replies cannot complete a superseding or cancelled request\n";
}
