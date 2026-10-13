#include "output/speech.h"
#include "output/prism_bridge.h"
#include <iostream>
#include <string>

namespace {
bool outputFailure=false;
void PRISM_CALL Capture(void* data,const float*,size_t samples,size_t,size_t){*static_cast<size_t*>(data)+=samples;}
}
namespace lis {void Log(const std::string& text){std::cout<<text<<'\n';if(text.find("_failed")!=std::string::npos)outputFailure=true;}}
int main(int argc,char** argv) {
    static_assert(sizeof(void*)==4,"Prism smoke check must exercise x86");
    namespace bridge=lis::output::bridge;
    auto* context=bridge::Init();if(!context)return 1;
    bool hasNvda=false,hasSapi=false;
    const auto count=bridge::Count(context);
    for(size_t i=0;i<count;++i){const auto id=bridge::Id(context,i);hasNvda|=id==PRISM_BACKEND_NVDA;hasSapi|=id==PRISM_BACKEND_SAPI;}
    std::cout<<"x86 registered_backends="<<count<<" nvda="<<hasNvda<<" sapi="<<hasSapi<<'\n';
    if(!hasNvda||!hasSapi){bridge::Shutdown(context);return 2;}
    auto* sapi=bridge::Acquire(context,PRISM_BACKEND_SAPI);
    const auto result=sapi?bridge::Initialize(sapi):PRISM_ERROR_BACKEND_NOT_AVAILABLE;
    std::cout<<"sapi_initialize="<<static_cast<int>(result)<<'\n';
    size_t samples=0;
    const auto literal=result==PRISM_OK?prism_backend_speak_to_memory(sapi,"<not valid XML",Capture,&samples):result;
    std::cout<<"sapi_literal_text="<<static_cast<int>(literal)<<" captured_samples="<<samples<<'\n';
    if(sapi)bridge::Free(sapi);
    bridge::Shutdown(context);
    if(result!=PRISM_OK&&result!=PRISM_ERROR_ALREADY_INITIALIZED)return 3;
    if(literal!=PRISM_OK||samples==0)return 5;
    lis::InitializeSpeech(nullptr);
    const bool available=*lis::SpeechBackendName()!=0;
    if(available&&argc==2&&std::string(argv[1])=="--output") {
        lis::Speak(L"Life Is Strange Prism speech and braille test.");
        Sleep(4000);
    }
    lis::ShutdownSpeech();
    return available&&!outputFailure?0:4;
}
