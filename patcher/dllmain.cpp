#include <Windows.h>
#include <intrin.h>
#include <fstream>
#include "MinHook.h"
#pragma comment(lib, "minhook.lib")

using fpCryptVerifySignature = BOOL(__stdcall*)(HCRYPTHASH, const BYTE*, DWORD, HCRYPTKEY, LPCSTR, DWORD);
fpCryptVerifySignature crypt_verify_signature;
fpCryptVerifySignature OG_CRYPT_VERIFY_SIGNATURE;
BOOL __stdcall HK_CRYPT_VERIFY_SIGNATURE(HCRYPTHASH hHash, const BYTE* pbSignature, DWORD dwSigLen, HCRYPTKEY hPubKey, LPCSTR szDescription, DWORD dwFlags)
{
    PVOID caller = _ReturnAddress();
    std::ofstream logger("logfile.txt", std::ios::app | std::ios::out);
    logger << "Called CryptVerifySignature at " << caller << "\n";
    //OG_CRYPT_VERIFY_SIGNATURE(hHash, pbSignature, dwSigLen, hPubKey, szDescription, dwFlags);
    return TRUE;
}

__declspec(dllexport) int Dummy()
{
    return 1;
}

BOOL APIENTRY DllMain(HMODULE hmod, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        MH_Initialize();
        MH_CreateHook(CryptVerifySignatureA, HK_CRYPT_VERIFY_SIGNATURE, (PVOID*)&OG_CRYPT_VERIFY_SIGNATURE);
        MH_EnableHook(MH_ALL_HOOKS);
        std::ofstream logger("logfile.txt", std::ios::app | std::ios::out);
        logger << "Started\n";
    }
    return TRUE;
}