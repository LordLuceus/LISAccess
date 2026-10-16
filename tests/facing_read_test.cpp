#include "features/camera/facing_read.h"
#include <cstring>
#include <iostream>
#include <limits>
using namespace lis::world::facing_read;
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL "<<__LINE__<<"\n";return 1;}}while(false)
struct Descriptor {
    bool known=true;int offset=12;
    bool Offset(const char* owner,const char* name,const char* kind,int size,int& out) const {
        out=offset;
        return known&&std::strcmp(owner,"Camera")==0&&std::strcmp(name,"Rotation")==0&&
            std::strcmp(kind,"StructProperty")==0&&size==12;
    }
};
int main() {
    struct Rotation {int pitch,yaw,roll;} out{};
    Descriptor d;int copies=0;bool readable=true;
    auto copy=[&](void* to,uintptr_t from,size_t size) {
        ++copies;if(!readable||from!=0x1000c||size!=12)return false;
        Rotation source{1,2,3};std::memcpy(to,&source,12);return true;
    };
    CHECK(DescriptorNativeRead(d,copy,0x10000,"Camera","Rotation","StructProperty",out));
    CHECK(out.pitch==1&&out.yaw==2&&copies==1);
    d.known=false;CHECK(!DescriptorNativeRead(d,copy,0x10000,"Camera","Rotation","StructProperty",out));
    d.known=true;
    CHECK(!DescriptorNativeRead(d,copy,0x10000,"Pawn","Rotation","StructProperty",out));
    CHECK(!DescriptorNativeRead(d,copy,0x10000,"Camera","Rotation","IntProperty",out));
    int wrongSize=0;CHECK(!DescriptorNativeRead(d,copy,0x10000,"Camera","Rotation","StructProperty",wrongSize));
    for(int offset:{-1,65536}) {d.offset=offset;CHECK(!DescriptorNativeRead(d,copy,0x10000,"Camera","Rotation","StructProperty",out));}
    d.offset=12;CHECK(!DescriptorNativeRead(d,copy,0,"Camera","Rotation","StructProperty",out));
    CHECK(!DescriptorNativeRead(d,copy,(std::numeric_limits<uintptr_t>::max)()-4,"Camera","Rotation","StructProperty",out));
    CHECK(copies==1);readable=false;
    CHECK(!DescriptorNativeRead(d,copy,0x10000,"Camera","Rotation","StructProperty",out)&&copies==2);
    CHECK(!Span(0x10000,0,0)&&Span(0x10000,0,12));
    std::cout<<"PASS facing reads\n";
}
