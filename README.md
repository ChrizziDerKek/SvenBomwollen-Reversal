# SvenBomwollen-Reversal

Reversing an old game from my childhood for fun

![Alt text](thumbnail.jpg?raw=true "Title")

#  Content
## sven2
The actual game we reverse (Sven Zwø XS)

## dumper
DLL file for dumping all loaded textures while the game is running. Just inject it with your favourite injector and play some levels!



# Research and notes


## Engine
The game ships with multiple dlls from the mudGE / WitanEngine runtime. The descriptions below are based on reverse-engineering, exports and embedded strings.
### mudGE.dll
The main engine/runtime library of the game. It provides core engine functions like initialization, update handling, bitmap objects, serialization, basic math/utility classes and device abstractions used by the game and its plugins.
### pluginpack.dll
Plugin library for loading and saving media formats used by the engine. Based on embedded symbols and strings, it contains image/resource handling code and is basically an extension for mudGE's asset system. It's probably responsible to load the levels and textures.
### wtnlib.dll
Support library which contains lower-level utility and framework code used by the other 2 dlls and the game. It includes file helpers, UI classes, string utilities and a general runtime.


## Important functions
Here are a few important/interesting functions.
### LoadSprite
```bool LoadSprite(CApplication* app, void* filePath, void* sprite, int width, int height, void* spriteMirrored) //mudGe.dll+0x1D440``` \
This function is responsible for loading any sprites into the game, including characters, level elements, GUI controls but strangely not the level backgrounds. The sprite dumper hooks this to signal that we just loaded a sprite and to get its original name.
- app: Owner object of the function
- filePath: Encoded ("virtual") filepath of the sprite
- sprite: Sprite object output
- width: Width of the sprite
- height: Height of the sprite
- spriteMirrored: Optional parameter, if specified outputs a mirrored version of the sprite object
- returns: True on success, otherwise false

### GetFile
```CString GetFile(void* filePath) //wtnlib.dll+0xA5A0``` \
This function is responsible for turning a filepath object (for example from LoadSprite parameter 2) into a human-readable string. The sprite dumper uses this one to get the original sprite names.
- filePath: Encoded ("virtual") filepath
- returns: String struct

### DecodeBmpFile
```void DecodeBmpFile(void* codec, iIOStream* stream, void* texture) //pluginpack.dll+0x14360``` \
This function gets called somewhere later by LoadSprite to decode a bmp file into an ingame texture. It uses a custom IOStream class for decoding the file. The sprite dumper hooks this to actually dump the texture as a bmp file after the LoadSprite hook got called.
- codec: Owner object of the function
- stream: Custom bytestream class used for decoding
- texture: Decoded texture output
- returns: Nothing


## Important structures
Here are a few important/interesting structures.
### CString
```cpp
struct CString
{
    char pad[4]; //0x00
    char* data; //0x04
    int size; //0x08
};
```
Simple string structure that's used almost everywhere in the game. Contains a pointer to the string and its size.
### BmpFileHeader
```cpp
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
```
BMP file format header that I use for decoding the texture bitmaps (https://en.wikipedia.org/wiki/BMP_file_format).
### iIOStream
```cpp
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
```
Bytestream helper that's used for file i/o operations. I use this to read the texture data from the DecodeBmpFile hook. For some strange reason the attributes of that class didn't behave as expected so I had to use the virtual functions to read any data.

