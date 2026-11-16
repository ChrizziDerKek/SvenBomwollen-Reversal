#include "DebugMenu.h"
#include "GameSession.h"
#include "Scene/Objects/ObjectCatalog.h"
#include <array>

DebugMenu::DebugMenu(MudGE& engine)
    : Font(engine, true), Panel{ std::vector<u32>(640 * 510, 0xFFF0E8D0), 640, 640, 510, 1, 1, false }, Highlight{ std::vector<u32>(600 * 34, 0xFFD0DDC0), 600, 600, 34, 1, 1, false }, Cursor(engine.LoadSprite("Sven2/gfx/mousecursor.bmp", 5, 1))
{
	for (const ItemDefinition& item : GetItemDefinitions())
	{
		Items.push_back(item.Kind);
		ItemNames.push_back(item.Name);
	}
	for (const CharacterDefinition& character : GetCharacterDefinitions())
		if (character.Category == CharacterCategory::Animal)
		{
			Animals.push_back(character.Kind);
			AnimalNames.push_back(character.Name);
		}
}

void DebugMenu::Toggle()
{
	Open = !Open;
	Notice.clear();
}

void DebugMenu::CycleSelection(i32 direction)
{
	if (Selected == Row::Item)
		ItemIndex = (ItemIndex + direction + static_cast<i32>(Items.size())) % static_cast<i32>(Items.size());
	else if (Selected == Row::Animal)
		AnimalIndex = (AnimalIndex + direction + static_cast<i32>(Animals.size())) % static_cast<i32>(Animals.size());
}

bool DebugMenu::Handle(const WindowInput::Result& input, GameSession* session)
{
	if (!Open)
		return false;
	Mouse = input.Mouse;
	if (input.Back)
	{
		Open = false;
		return false;
	}
	const i32 count = static_cast<i32>(Row::Count);
	if (input.Up || input.Down)
		Selected = static_cast<Row>((static_cast<i32>(Selected) + (input.Down ? 1 : count - 1)) % count);
	const i32 hovered = (Mouse.y - 108) / 40;
	if ((input.MouseMoved || input.Click) && Mouse.x >= 100 && Mouse.x < 700 && Mouse.y >= 108 && hovered >= 0 && hovered < count)
		Selected = static_cast<Row>(hovered);
	if (input.Left || input.Right)
		CycleSelection(input.Right ? 1 : -1);
	if (!input.Confirm && !(input.Click && Mouse.x >= 100 && Mouse.x < 700 && Mouse.y >= 108 && Mouse.y < 108 + count * 40))
		return false;
	if (Selected == Row::Item || Selected == Row::Animal)
	{
		CycleSelection(1);
		return false;
	}
	if (!session)
	{
		Notice = "Start or resume a level to use these commands.";
		return false;
	}
	try
	{
		switch (Selected)
		{
		case Row::NextLevel:
			if (session->GameOver())
			{
				Notice = "Add a life before skipping to another level.";
				return false;
			}
			session->DebugNextLevel();
			Notice = "Moved to the next level.";
			return true;
		case Row::Restart:
			session->DebugRestartLevel();
			Notice = "Level restarted.";
			return true;
		case Row::AddLife:
			session->DebugChangeLives(1);
			Notice = "Lives: " + std::to_string(session->GetLives());
			break;
		case Row::RemoveLife:
			session->DebugChangeLives(-1);
			Notice = "Lives: " + std::to_string(session->GetLives());
			break;
		case Row::SpawnItem:
		case Row::SpawnAnimal:
		{
			const ObjectKind kind = Selected == Row::SpawnItem ? Items[ItemIndex] : Animals[AnimalIndex];
			Notice = session->DebugSpawn(kind) ? "Spawned nearby." : "No free nearby tile or object slot.";
			break;
		}
		default:
			break;
		}
	}
	catch (const std::exception& error)
	{
		Notice = error.what();
		Log(Notice);
	}
	return false;
}

const std::vector<SpriteDraw>& DebugMenu::Draws(const std::vector<SpriteDraw>& background)
{
	Commands = background;
	if (!Open)
		return Commands;
	Commands.push_back({ &Panel, 80, 45, 0 });
	Font.Append(Commands, "Debug / cheats - game paused", 110, 65);
	const std::array<str, 8> rows = { "Next level", "Restart level", "Add a life", "Remove a life", "Item: " + ItemNames[ItemIndex], "Spawn selected item", "Animal: " + AnimalNames[AnimalIndex], "Spawn selected animal" };
	for (size_t i = 0; i < rows.size(); ++i)
	{
		const i32 y = 108 + static_cast<i32>(i) * 40;
		if (i == static_cast<size_t>(Selected))
			Commands.push_back({ &Highlight, 100, y, 0 });
		Font.Append(Commands, rows[i], 116, y + 6);
	}
	Font.AppendWrapped(Commands, Notice, 110, 440, 580, 20);
	Font.Append(Commands, "Arrows / Enter or mouse. Left / Right select a type.", 110, 500);
	Font.Append(Commands, "F2 or Escape closes this menu.", 110, 524);
	if (Mouse.x >= 0 && Mouse.y >= 0)
		Commands.push_back({ Cursor, Mouse.x, Mouse.y, 0 });
	return Commands;
}
