#include "output/prism_bridge.h"

namespace lis::output::bridge {
static_assert(sizeof(PrismBackendFeature)==8,"Prism feature enum requires /Zc:enumTypes");
namespace {
template<class R,class F,class... Args> R Guard(R failure,F function,Args... args) {
    __try {return function(args...);}
    __except(EXCEPTION_EXECUTE_HANDLER){return failure;}
}
template<class F,class... Args> void GuardVoid(F function,Args... args) {
    __try {function(args...);}
    __except(EXCEPTION_EXECUTE_HANDLER){}
}
}
PrismContext* Init() {
    auto config=prism_config_init();
    return Guard(static_cast<PrismContext*>(nullptr),prism_init,&config);
}
void Shutdown(PrismContext* context){GuardVoid(prism_shutdown,context);}
size_t Count(PrismContext* context){return Guard(size_t{0},prism_registry_count,context);}
PrismBackendId Id(PrismContext* context,size_t index){return Guard(PRISM_BACKEND_INVALID,prism_registry_id_at,context,index);}
int Priority(PrismContext* context,PrismBackendId id){return Guard(0,prism_registry_priority,context,id);}
PrismBackend* Acquire(PrismContext* context,PrismBackendId id){return Guard(static_cast<PrismBackend*>(nullptr),prism_registry_acquire,context,id);}
const char* Name(PrismBackend* backend){return Guard(static_cast<const char*>("unknown"),prism_backend_name,backend);}
uint64_t Features(PrismBackend* backend){return Guard(uint64_t{0},prism_backend_get_features,backend);}
PrismError Initialize(PrismBackend* backend){return Guard(PRISM_ERROR_BACKEND_NOT_AVAILABLE,prism_backend_initialize,backend);}
PrismError Speak(PrismBackend* backend,const char* text,bool interrupt){return Guard(PRISM_ERROR_BACKEND_NOT_AVAILABLE,prism_backend_speak,backend,text,interrupt);}
PrismError Braille(PrismBackend* backend,const char* text){return Guard(PRISM_ERROR_BACKEND_NOT_AVAILABLE,prism_backend_braille,backend,text);}
void Stop(PrismBackend* backend){(void)Guard(PRISM_ERROR_BACKEND_NOT_AVAILABLE,prism_backend_stop,backend);}
void Free(PrismBackend* backend){GuardVoid(prism_backend_free,backend);}
}
