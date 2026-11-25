#pragma once
#include "LevelProject.h"
#include "Game/GameSession.h"
#include "Input/WindowInput.h"

class LevelEditor
{
public:
	LevelEditor(MudGE& engine, const LevelPackInfo& pack, const LevelInfo& level, size_t players, bool startBlank = false);
	~LevelEditor();
	bool Handle(const WindowInput::Result& input, f32 seconds, const std::array<PlayerInput, 2>& players = {});
	const std::vector<SpriteDraw>& Draws();
	std::vector<SoundRequest> TakeAudioEvents();
	bool UfoActive() const
	{
		return Preview && Preview->UfoActive() && !Preview->GetResults();
	}
	bool RequestWindowClose();
	bool HasSaved() const
	{
		return Saved;
	}
	bool NeedsCatalogRefresh() const
	{
		return Saved || CatalogChanged;
	}
	bool IsPlaytesting() const
	{
		return Preview != nullptr;
	}
	LevelDocument& GetDocument()
	{
		return Project.Current();
	}
	const LevelDocument& GetDocument() const
	{
		return Project.Current();
	}
	LevelProject& GetProject()
	{
		return Project;
	}
	RECT GetInspectorBounds() const
	{
		return InspectorBounds();
	}
	i32 GetSelectedObject() const
	{
		return Selected;
	}
	i32 GetSelectedCell() const
	{
		return SelectedCell;
	}
	size_t GetPlayerCount() const
	{
		return Players;
	}

private:
	enum class Tool
	{
		Select,
		Place,
		Collision
	};
	enum class Category
	{
		Animals,
		Enemies,
		Items,
		Scenery,
		Players,
		Special,
		Count
	};
	struct Choice
	{
		ObjectKind Kind;
		i32 Texture;
		str Name;
	};
	struct VisibleObject
	{
		size_t Slot;
		SpriteDraw Draw;
		f32 Depth;
		i32 Pass;
	};
	enum class InspectorPage
	{
		Level,
		Object,
		Cell,
		Project
	};
	enum class Property
	{
		Title,
		Time,
		TileSet,
		Music,
		Ufo,
		Weather,
		Bonus,
		Destinations,
		Rebuild,
		Slot,
		Kind,
		X,
		Y,
		Depth,
		Pass,
		Frame,
		Texture,
		Facing,
		Impatience,
		Permanent,
		Earlier,
		Later,
		CellIndex,
		Terrain,
		Spawner,
		Direction,
		Help,
		FindObject,
		PackName,
		Episode,
		NewPack,
		NewLevel,
		Duplicate,
		ImportLevel,
		ChooseLevel,
		OpenPack
	};
	enum class FieldMode
	{
		Text,
		Cycle,
		Action,
		ReadOnly
	};
	struct Field
	{
		Property Id;
		str Label, Value;
		FieldMode Mode = FieldMode::Text;
	};
	std::vector<Field> Fields() const;
	void ApplyField(Property property, const str& text, i32 step = 1);
	bool HandleInspector(const WindowInput::Result& input);
	void DrawInspector();
	RECT InspectorBounds() const;
	void ResetSelection();
	enum class PickerKind
	{
		None,
		Theme,
		Object,
		Level,
		Pack
	};
	struct PickerEntry
	{
		size_t Index;
		str Label;
	};
	std::vector<PickerEntry> PickerEntries() const;
	void ChoosePicker(size_t index);
	bool HandlePicker(const WindowInput::Result& input);
	void DrawPicker();
	bool CanvasAt(POINT point) const;
	void ChangeChoice(i32 step);
	void BuildChoices();
	void BuildObjects();
	i32 Pick(POINT point, bool cycle = false);
	void Playtest();
	void Save();
	MudGE& Engine;
	LevelProject Project;
	BitmapFont Font;
	Sprite Bar, Selection, Wall, GridCell, ActiveTool, WaterCell, PuddleCell, SpawnerCell, Panel, FieldBar, PanelHeader, PanelTab;
	Sprite ObjectOutline{};
	const Sprite* Cursor;
	std::vector<Choice> Choices;
	std::vector<VisibleObject> Objects;
	std::vector<SpriteDraw> Commands;
	Tool CurrentTool = Tool::Select;
	Category CurrentCategory = Category::Animals;
	i32 ChoiceIndex = 0, Selected = -1;
	POINT Mouse{}, DragOrigin{};
	bool Dragging = false, Toolbar = true, ShowGrid = true, DiscardArmed = false, Saved = false, SnapScenery = true, Inspector = false;
	InspectorPage InspectorTab = InspectorPage::Level;
	POINT PanelPosition{ 480, 60 }, PanelDrag{};
	bool MovingPanel = false, PanelCollapsed = false, SelectCells = false, ResetArmed = false;
	bool CatalogChanged = false;
	static constexpr i32 PanelWidth = 320, HeaderHeight = 24, TabsHeight = 24, FieldHeight = 28, PickerRows = 10;
	PickerKind Picker = PickerKind::None;
	str Search;
	size_t PickerFirst = 0, PickerSelected = 0;
	i32 PendingPack = -1;
	i32 SelectedCell = -1, TileChoice = 1;
	bool EditingField = false, ReplaceText = false;
	Property CurrentField = Property::Title;
	str FieldText;
	str Notice = "F3 close | F5 test | Ctrl+S save | I properties | N scenery snap | Tab tools";
	size_t Players;
	fpath PreviewFolder;
	str PreviewMap;
	LevelPackInfo PreviewPack;
	LevelSetInfo PreviewSet;
	std::unique_ptr<GameSession> Preview;
};
