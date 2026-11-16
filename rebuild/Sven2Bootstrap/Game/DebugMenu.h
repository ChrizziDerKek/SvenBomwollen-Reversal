#pragma once
#include "Types.h"
#include "Engine/BitmapFont.h"
#include "Input/WindowInput.h"
#include "Scene/Objects/ObjectKind.h"

class GameSession;

class DebugMenu
{
public:
	explicit DebugMenu(MudGE& engine);
	void Toggle();
	bool IsOpen() const
	{
		return Open;
	}
	bool Handle(const WindowInput::Result& input, GameSession* session);
	const std::vector<SpriteDraw>& Draws(const std::vector<SpriteDraw>& background);

private:
	enum class Row
	{
		NextLevel,
		Restart,
		AddLife,
		RemoveLife,
		Item,
		SpawnItem,
		Animal,
		SpawnAnimal,
		Count
	};
	void CycleSelection(i32 direction);
	BitmapFont Font;
	Sprite Panel, Highlight;
	const Sprite* Cursor;
	std::vector<SpriteDraw> Commands;
	std::vector<ObjectKind> Items;
	std::vector<str> ItemNames;
	std::vector<ObjectKind> Animals;
	std::vector<str> AnimalNames;
	bool Open = false;
	Row Selected = Row::NextLevel;
	i32 ItemIndex = 0, AnimalIndex = 0;
	POINT Mouse{ -1, -1 };
	str Notice;
};
