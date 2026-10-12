#include "engine_abi.h"
#include <iostream>
#include <cstdint>
struct Fixture {
    void* function=nullptr; void* parameters=nullptr; void* legacy=reinterpret_cast<void*>(1);
    virtual __declspec(noinline) void Dispatch(void* f, void* p, void* r) {function=f;parameters=p;legacy=r;}
};
int main() {
    static_assert(sizeof(void*)==4);
    Fixture object;int function=17,parameters=29;
    auto slot=(*reinterpret_cast<uintptr_t**>(&object))[0];
    // Same x86 thiscall arity as the two disassembled engine dispatchers.
    reinterpret_cast<lis::engine::ProcessEvent>(slot)(&object,&function,&parameters,nullptr);
    if(object.function!=&function || object.parameters!=&parameters || object.legacy!=nullptr) return 1;
    std::cout<<"PASS: x86 ProcessEvent this plus three stack arguments\n";
}
