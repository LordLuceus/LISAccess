#include "engine/abi.h"
#include <iostream>
#include <cstdint>
struct Fixture {
    void* function=nullptr; void* parameters=nullptr; void* legacy=reinterpret_cast<void*>(1);
    virtual __declspec(noinline) void Dispatch(void* f, void* p, void* r) {function=f;parameters=p;legacy=r;}
};
struct BindingFixture {
    lis::engine::Name binding{};void* source=nullptr;
    virtual __declspec(noinline) uint32_t Press(lis::engine::Name value,void* origin) {binding=value;source=origin;return 23;}
    virtual __declspec(noinline) int Eligible(void* action) {source=action;return 2;}
};
int main() {
    static_assert(sizeof(void*)==4);
    Fixture object;int function=17,parameters=29;
    auto slot=(*reinterpret_cast<uintptr_t**>(&object))[0];
    // Same x86 thiscall arity as the two disassembled engine dispatchers.
    reinterpret_cast<lis::engine::ProcessEvent>(slot)(&object,&function,&parameters,nullptr);
    if(object.function!=&function || object.parameters!=&parameters || object.legacy!=nullptr) return 1;
    BindingFixture input;auto table=*reinterpret_cast<uintptr_t**>(&input);
    if(reinterpret_cast<lis::engine::InteractionPress>(table[0])(&input,{17,29},&parameters)!=23 || input.binding.index!=17 || input.binding.number!=29 || input.source!=&parameters)return 1;
    if(reinterpret_cast<lis::engine::ActionEligibility>(table[1])(&input,&function)!=2 || input.source!=&function)return 1;
    std::cout<<"PASS: x86 ProcessEvent this plus three stack arguments\n";
    std::cout<<"PASS: x86 interaction FName by value plus source; eligibility thiscall\n";
}
