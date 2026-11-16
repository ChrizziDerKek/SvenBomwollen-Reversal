#pragma once
#include <windows.h>

LONG CALLBACK HandleException(EXCEPTION_POINTERS* e)
{
    DWORD code = e->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION) {
        char text[256];
        std::snprintf(text, sizeof(text), "First-chance native exception 0x%08lX at %p", code, e->ExceptionRecord->ExceptionAddress);
        Log(text);
        if (code == EXCEPTION_ACCESS_VIOLATION) {
            MEMORY_BASIC_INFORMATION memory{};
            if (VirtualQuery(e->ExceptionRecord->ExceptionAddress, &memory, sizeof(memory))) {
                char module[MAX_PATH]{};
                GetModuleFileNameA(static_cast<HMODULE>(memory.AllocationBase), module, MAX_PATH);
                std::snprintf(text, sizeof(text), "  Fault module %s base %p access %lu address %p", module, memory.AllocationBase, static_cast<DWORD>(e->ExceptionRecord->ExceptionInformation[0]), reinterpret_cast<any>(e->ExceptionRecord->ExceptionInformation[1]));
                Log(text);
            }
            any frames[24]{};
            u16 count = CaptureStackBackTrace(0, 24, frames, nullptr);
            for (u16 i = 0; i < count; ++i) {
                HMODULE module = nullptr;
                char path[MAX_PATH]{};
                GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<cstr>(frames[i]), &module);
                GetModuleFileNameA(module, path, MAX_PATH);
                std::snprintf(text, sizeof(text), "  frame %u: %p RVA %08lX", i, frames[i], static_cast<DWORD>(reinterpret_cast<uintptr_t>(frames[i]) - reinterpret_cast<uintptr_t>(module)));
                Log(std::string(text) + " " + path);
            }
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}