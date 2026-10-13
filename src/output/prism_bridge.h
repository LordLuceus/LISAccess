#pragma once
#include <prism.h>

namespace lis::output::bridge {
// The RPC-backed readers can disappear mid-call. Keep native exception handling
// outside frames containing C++ objects, and do not let it reach the game worker.
PrismContext* Init();
void Shutdown(PrismContext* context);
size_t Count(PrismContext* context);
PrismBackendId Id(PrismContext* context,size_t index);
int Priority(PrismContext* context,PrismBackendId id);
PrismBackend* Acquire(PrismContext* context,PrismBackendId id);
const char* Name(PrismBackend* backend);
uint64_t Features(PrismBackend* backend);
PrismError Initialize(PrismBackend* backend);
PrismError Speak(PrismBackend* backend,const char* text,bool interrupt);
PrismError Braille(PrismBackend* backend,const char* text);
void Stop(PrismBackend* backend);
void Free(PrismBackend* backend);
}
