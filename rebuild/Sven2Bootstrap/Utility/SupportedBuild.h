#pragma once
#include "Types.h"
#include <windows.h>
#include <wincrypt.h>
#include <fstream>

// Raw offsets/interface slots are valid only for the researched Sven2 build.
inline void RequireBuild(const fpath& path, cstr expected)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Required game library not found: " + path.string());
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    if (!CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        throw std::runtime_error("Cannot acquire SHA-256 provider.");
    if (!CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) {
        CryptReleaseContext(provider, 0);
        throw std::runtime_error("Cannot create SHA-256 hash.");
    }
    char* buffer = new char[USHRT_MAX];
    bool ok = true;
    while (input.read(buffer, USHRT_MAX) || input.gcount()) {
        if (!CryptHashData(hash, reinterpret_cast<u8*>(buffer), static_cast<DWORD>(input.gcount()), 0)) {
            ok = false;
            break;
        }
    }
    delete[] buffer;
    BYTE digest[32]{};
    DWORD length = sizeof(digest);
    ok = ok && !input.bad() && CryptGetHashParam(hash, HP_HASHVAL, digest, &length, 0);
    CryptDestroyHash(hash);
    CryptReleaseContext(provider, 0);
    if (!ok)
        throw std::runtime_error("Cannot hash engine library: " + path.string());
    std::string actual;
    constexpr char hex[] = "0123456789abcdef";
    for (BYTE b : digest) {
        actual += hex[b >> 4];
        actual += hex[b & 15];
    }
    if (actual != expected)
        throw std::runtime_error("Unsupported engine build: " + path.string() + ". This ABI bridge supports the researched Sven2 DLLs only.");
}