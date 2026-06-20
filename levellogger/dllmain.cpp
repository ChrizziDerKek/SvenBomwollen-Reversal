#include <Windows.h>
#include <iostream>
#include <unordered_map>
#include <string>
#include <fstream>
#pragma comment(lib, "minhook.lib")
#include "minhook.h"

class CSprite;

#define ZWO

std::ofstream fstream;

//Converts the 2d screen coordinates to a percent value from 0.0f to 1.0f
#ifdef ZWO
void coords2float(int x, int y, float* fx, float* fy) {
#else
void coords2float(float x, float y, float* fx, float* fy) {
#endif
	//Nothing to do if there are no pointers
	if (!fx || !fy) return;
	//Divide the coordinates by their screen size
#ifdef ZWO
	*fx = (float)x / 800.0f;
	*fy = (float)y / 600.0f;
#else
	*fx = x / 800.0f;
	*fy = y / 600.0f;
#endif
}

//Gets a pointer of a specific type from the offset in the given module
template <class T>
T get_ptr(LPCSTR module, DWORD offset) {
	return (T)((DWORD)GetModuleHandleA(module) + offset);
}

//Truncates a filepath from a string to return the filename without the path
std::string truncate(LPCSTR str) {
	size_t slash = -1;
	std::string result = std::string(str);
	//Find the last slash or backslash
	for (size_t i = 0; i < result.size(); i++) {
		switch (result[i]) {
		case '/':
		case '\\':
			slash = i;
			break;
		}
	}
	//Return the original string if there isn't one
	if (slash == -1)
		return result;
	//Otherwise return everything after the last slash or backslash
	return result.substr(slash + 1);
}

//Stores the currently loaded textures
std::unordered_map<CSprite*, std::string> loaded_textures;
//True if we're loading a level
bool is_loading_level = false;
//State of the logger
enum eLevelLogState : uint8_t {
	Idle,
	Started,
	Logging,
};
eLevelLogState log_state = Idle;
//Game struct to store a string
struct CString {
	char pad[4];
	char* data;
	int size;
};
//Gets the filepath of a curl instance
using fpGetFile = CString(__thiscall*)(PVOID thisptr);
fpGetFile get_file;

//Loads a level with a tileset and its sprites by calling loadsprite
using fpLoadLevelSprites = bool(__thiscall*)(DWORD* thisptr, char* tileset, int a3, int a4);
fpLoadLevelSprites OG_LOAD_LEVEL_SPRITES;
bool __fastcall HK_LOAD_LEVEL_SPRITES(DWORD* thisptr, PVOID, char* tileset, int a3, int a4) {
	fstream.open("levels.txt", std::ios::app);
	//Print a message to know that we just loaded a level
	std::cout << "-------------LOADED LEVEL " << tileset << "-------------" << std::endl;
	fstream << "-------------LOADED LEVEL " << tileset << "-------------" << std::endl;
	//Set this to true to start loading the sprites into our map
	//We do this to avoid logging the base sprites that get loaded before
	//The game never loads them again afterwards, so we never have to reset it
	is_loading_level = true;
	//Clear the sprite map
	loaded_textures.clear();
	//Calls loadsprite for every sprite that's needed for the level
	bool retn = OG_LOAD_LEVEL_SPRITES(thisptr, tileset, a3, a4);
	//After loading all sprites, we want to start logging them in the drawcsprite function
	log_state = Started;
	return retn;
}

//Loads a sprite by its curl path
using fpLoadSprite = bool(__thiscall*)(PVOID thisptr, PVOID curl, CSprite* sprite1, int unk1, int unk2, CSprite* sprite2);
fpLoadSprite OG_LOAD_SPRITE;
bool __fastcall HK_LOAD_SPRITE(PVOID CApplication, PVOID, PVOID crl, CSprite* sprite1, int unk1, int unk2, CSprite* sprite2) {
	//Make sure we only load level sprites
	if (is_loading_level) {
		//Get the texture name without the filepath
		std::string texture = truncate(get_file(crl).data);
		//If the sprite exists, add it to our map
		if (sprite1 && loaded_textures.find(sprite2) == loaded_textures.end())
			loaded_textures[sprite1] = texture;
		//Also do this for the flipped version to be sure
		if (sprite2 && loaded_textures.find(sprite2) == loaded_textures.end())
			loaded_textures[sprite2] = texture;
	}
	//Do the actual sprite loading
	return OG_LOAD_SPRITE(CApplication, crl, sprite1, unk1, unk2, sprite2);
}

//Actually doesn't draw the sprite, but adds it to a drawing queue
#ifdef ZWO
using fpDrawCSprite = void(__thiscall*)(CSprite* thisptr, DWORD* idk, int x, int y, int spriteIndex);
#else
using fpDrawCSprite = int(__thiscall*)(CSprite* thisptr, int idk, float x, float y, int spriteIndex);
#endif
fpDrawCSprite OG_DRAW_CSPRITE;
#ifdef ZWO
void __fastcall HK_DRAW_CSPRITE(CSprite* thisptr, PVOID, DWORD* idk, int x, int y, int spriteIndex) {
#else
int __fastcall HK_DRAW_CSPRITE(CSprite* thisptr, PVOID, int idk, float x, float y, int spriteIndex) {
#endif
	//If x and y are zero, we know that we're drawing the ground
	//This is always the first sprite that gets drawn
	//so we can start and stop our logging when we reach here
#ifdef ZWO
	if (x == 0 && y == 0) {
#else
	if (x == 0.0f && y == 0.0f) {
#endif
		//If we want to start the logging, we update the state to log the textures
		if (log_state == Started)
			log_state = Logging;
		//If the state is already logging, we want to stop it by resetting the state
		else if (log_state == Logging) {
			fstream.close();
			log_state = Idle;
		}
	}
	//If the texture exists in our map and the state is logging, we print a message
	if (loaded_textures.find(thisptr) != loaded_textures.end() && log_state == Logging) {
		//Convert the coordinates to a percent value
		float actualX{ 0.0f }, actualY{ 0.0f };
		coords2float(x, y, &actualX, &actualY);
		//Log the sprite
		std::cout << loaded_textures[thisptr] << " (" << actualX << ", " << actualY << ")" << std::endl;
		fstream << loaded_textures[thisptr] << " (" << actualX << ", " << actualY << ")" << std::endl;
	}
	//Do the actual drawing
	return OG_DRAW_CSPRITE(thisptr, idk, x, y, spriteIndex);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		//Create a console
		if (!AllocConsole()) return FALSE;
		freopen_s(reinterpret_cast<FILE**>(stdout), ("CONOUT$"), ("w"), stdout);
		SetConsoleCP(CP_UTF8);
		SetConsoleOutputCP(CP_UTF8);
		MH_Initialize();
		//Get all pointers that we will need
#ifdef ZWO
		get_file = get_ptr<fpGetFile>("wtnlib.dll", 0xA5A0);
		OG_LOAD_LEVEL_SPRITES = get_ptr<fpLoadLevelSprites>(NULL, 0x33ED0);
		OG_DRAW_CSPRITE = get_ptr<fpDrawCSprite>("pluginpack.dll", 0xC960);
		OG_LOAD_SPRITE = get_ptr<fpLoadSprite>("mudGE.dll", 0x1D440);
#else
		get_file = get_ptr<fpGetFile>("wtnlib.dll", 0xA120);
		OG_LOAD_LEVEL_SPRITES = get_ptr<fpLoadLevelSprites>(NULL, 0x418C0);
		OG_DRAW_CSPRITE = get_ptr<fpDrawCSprite>("pluginpack.dll", 0xC8B0);
		OG_LOAD_SPRITE = get_ptr<fpLoadSprite>("mudGE.dll", 0x1CEF0);
#endif
		//Hook some functions
		MH_CreateHook(OG_LOAD_LEVEL_SPRITES, HK_LOAD_LEVEL_SPRITES, (PVOID*)&OG_LOAD_LEVEL_SPRITES);
		MH_CreateHook(OG_DRAW_CSPRITE, HK_DRAW_CSPRITE, (PVOID*)&OG_DRAW_CSPRITE);
		MH_CreateHook(OG_LOAD_SPRITE, HK_LOAD_SPRITE, (PVOID*)&OG_LOAD_SPRITE);
		//Enable the hooks
		MH_EnableHook(MH_ALL_HOOKS);
	}
	else if (reason == DLL_PROCESS_DETACH) {
		if (fstream.is_open())
			fstream.close();
		MH_DisableHook(MH_ALL_HOOKS);
		MH_RemoveHook(MH_ALL_HOOKS);
		MH_Uninitialize();
	}
    return TRUE;
}