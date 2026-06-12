#include <iostream>
#include "MinHook.h"
#include <string>
#include <fstream>
#pragma comment(lib, "minhook.lib")
#define SVEN_ZWO

#pragma pack(push, 1)
struct BmpFileHeader
{
    union {
        struct {
            union {
                struct {
                    uint8_t header_0x42;
                    uint8_t header_0x4D;
                };
                uint16_t header;
            };
            uint32_t file_size;
            uint16_t reserved_1;
            uint16_t reserved_2;
            uint32_t pixel_array_offset;
        };
        char buffer[14];
    };
};
class iIOStream
{
public:
    virtual ~iIOStream() { } //0x00
    virtual void func1() { } //0x04
    virtual void func2() { } //0x08
    virtual void func3() { } //0x0C
    virtual void func4() { } //0x10
    //returns this+0xC
    virtual int32_t get_location() { } //0x14
    virtual void func6() { } //0x18
    //sets this+0xC and does some exception handling we don't care about
    virtual void seek(int32_t value, int32_t alwaysZero) { } //0x1C
    virtual void func8() { } //0x20
    //copies this+0x10 with memcpy (doing that manually does nothing)
    virtual void read(char* buffer, int32_t size) { } //0x24
public:
    char pad_0004[8]; //0x04
    DWORD location; //0x0C setting this does absolute jack shit for some reason
    DWORD* some_array; //0x10 according to ida, this is correct but does nothing when reading it
public:
    //custom read function that doesn't modify the read location
    void custom_read(char* buffer, int32_t size) {
        int32_t pos = this->get_location();
        this->read(buffer, size);
        this->seek(pos, 0);
    }
};
struct CString
{
    char pad[4]; //0x00
    char* data; //0x04
    int32_t size; //0x08
};
#pragma pack(pop)

using fpGetFile = CString(__thiscall*)(PVOID thisptr);
fpGetFile get_file;
using fpLoadSprite = bool(__thiscall*)(PVOID thisptr, PVOID curl, PVOID sprite1, int32_t unk1, int32_t unk2, PVOID sprite2);
fpLoadSprite load_sprite;
using fpBmpFileCodec = void(__thiscall*)(PVOID codec, iIOStream* stream, PVOID picture);
fpBmpFileCodec bmp_file_codec;

std::string texture_name_to_dump = "";
static void remove_filepath(std::string& str)
{
    for (int32_t i = 0; i < str.size(); i++)
        if (str[i] == '/') str[i] = '\\';
    str = str.substr(str.find_last_of('\\') + 1, str.size() - str.find_last_of('\\') + 1);
}

//loads a csprite textures from a curl instance aka a virtual filepath
fpLoadSprite OG_LOAD_SPRITE;
static bool __fastcall HK_LOAD_SPRITE(PVOID CApplication, PVOID reserved, PVOID crl, PVOID sprite1, int32_t unk1, int32_t unk2, PVOID sprite2)
{
    //get the sprite's name from the curl class
    texture_name_to_dump = get_file(crl).data;
    //remove the filepath before the name
    remove_filepath(texture_name_to_dump);
    //call the og function and load the sprite - this will call our bmp file codec hook somewhere in cpicture::load
    return OG_LOAD_SPRITE(CApplication, crl, sprite1, unk1, unk2, sprite2);
}

//converts a bmp file stream to a cpicture struct
fpBmpFileCodec OG_BMP_FILE_CODEC;
static void __fastcall HK_BMP_FILE_CODEC(PVOID codec, PVOID reserved, iIOStream* stream, PVOID picture)
{
    //bmp file header (https://en.wikipedia.org/wiki/BMP_file_format)
    BmpFileHeader bfh{};
    //read the header
    stream->custom_read(bfh.buffer, 14);
    //now we know the file size, so we can create a buffer for the actual file
    char* buffer = new char[bfh.file_size + 1];
    //now we read out the whole file into the buffer
    stream->custom_read(buffer, bfh.file_size);
    //save bmp files in a textures folder
    static int counter = 1;
    if (texture_name_to_dump == "")
        texture_name_to_dump = "_unnamed" + std::to_string(counter++) + ".bmp";
    std::string file = "textures/" + texture_name_to_dump;
    //write the buffer to the file
    std::ofstream os; os.open(file, std::ios::binary);
    os.write(buffer, bfh.file_size);
    //close it and delete the buffer to avoid memory leaks
    os.close();
    delete[] buffer;
    std::cout << "Dumped Texture: " << texture_name_to_dump << std::endl;
    texture_name_to_dump = "";
    //continue with loading the sprite normally
    OG_BMP_FILE_CODEC(codec, stream, picture);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        //delete all hooks and the console
        MH_DisableHook(MH_ALL_HOOKS);
        MH_RemoveHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        FreeConsole();
    }
    else if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        //create a console
        if (!AllocConsole()) return FALSE;
        freopen_s((FILE**)(stdout), "CONOUT$", "w", stdout);
        SetConsoleCP(CP_UTF8);
        SetConsoleOutputCP(CP_UTF8);
        //get the functions that we need for dumping
#ifdef SVEN_ZWO
        load_sprite = (fpLoadSprite)((DWORD)GetModuleHandleA("mudGE.dll") + 0x1D440);
        get_file = (fpGetFile)((DWORD)GetModuleHandleA("wtnlib.dll") + 0xA5A0);
        bmp_file_codec = (fpBmpFileCodec)((DWORD)GetModuleHandleA("pluginpack.dll") + 0x14360);
#else
        load_sprite = (fpLoadSprite)((DWORD)GetModuleHandleA("mudGE.dll") + 0x1CEF0);
        get_file = (fpGetFile)((DWORD)GetModuleHandleA("wtnlib.dll") + 0xA120);
        bmp_file_codec = (fpBmpFileCodec)((DWORD)GetModuleHandleA("pluginpack.dll") + 0x13FD0);
#endif
        //hook the 2 functions
        MH_Initialize();
        MH_CreateHook(load_sprite, HK_LOAD_SPRITE, (PVOID*)&OG_LOAD_SPRITE);
        MH_CreateHook(bmp_file_codec, HK_BMP_FILE_CODEC, (PVOID*)&OG_BMP_FILE_CODEC);
        MH_EnableHook(MH_ALL_HOOKS);
    }
    return TRUE;
}