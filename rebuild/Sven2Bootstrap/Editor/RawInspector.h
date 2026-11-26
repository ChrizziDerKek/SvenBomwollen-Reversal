#pragma once
#include "LevelDocument.h"
#include "Engine/BitmapFont.h"
#include "Input/WindowInput.h"

class RawInspector
{
public:
	RawInspector();
	void Sync(const LevelDocument& document, i32 object, i32 cell);
	bool Handle(const WindowInput::Result& input, LevelDocument& document, RECT bounds);
	void Append(std::vector<SpriteDraw>& commands, const LevelDocument& document, RECT bounds, const BitmapFont& font) const;
	bool IsCell() const
	{
		return CurrentTarget == Target::Cell;
	}
	bool HasFocus() const
	{
		return Focus != Field::None;
	}
	void Commit(LevelDocument& document);

private:
	enum class Target
	{
		Object,
		Cell
	};
	enum class Field
	{
		None,
		Index,
		Value
	};
	std::vector<u32> Words(const LevelDocument& document) const;
	void Edit(Field field, const LevelDocument& document);
	Target CurrentTarget = Target::Object;
	Field Focus = Field::None;
	const LevelDocument* Document = nullptr;
	i32 ObjectIndex = 0, CellIndex = 23, LastObject = -2, LastCell = -2;
	i32 Word = 0, First = 0;
	str Text;
	bool Replace = false;
	Sprite Row, Highlight, Tab;
	static constexpr i32 RowHeight = 24, Rows = 10;
};
