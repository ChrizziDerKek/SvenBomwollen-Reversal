#include "RawInspector.h"
#include "EditorGraphics.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace
{
constexpr cstr ObjectNames[] = { "X", "Y", "Base X", "Base Y", "Flags/species", "Width", "Height", "Speed X", "Speed Y", "Offset X", "Offset Y", "Kind (linked)", "Frame", "Idle", "Frame rate", "Frames", "Packed state", "Texture/permanent", "State", "Facing", "Tile (linked)", "Impatience", "Pulse", "Depth", "Hunting" };
constexpr cstr CellNames[] = { "Terrain", "Direction / bits", "Actor link", "Item code / bits", "Item link", "Spawner / help" };
i64 Number(const str& text)
{
	size_t used = 0;
	const bool hex = text.rfind("0x", 0) == 0 || text.rfind("0X", 0) == 0;
	const i64 number = std::stoll(text, &used, hex ? 16 : 10);
	if (used != text.size() || number < std::numeric_limits<i32>::min() || number > std::numeric_limits<u32>::max())
		throw std::runtime_error("Enter a signed 32-bit decimal or 0x-prefixed 32-bit hex value.");
	return number;
}
str Signed(u32 bits)
{
	i32 value;
	std::memcpy(&value, &bits, 4);
	return std::to_string(value);
}
} // namespace

RawInspector::RawInspector()
    : Row(EditorGraphics::Rectangle(312, RowHeight - 2, 0xFFD8CCA8, false)), Highlight(EditorGraphics::Rectangle(312, RowHeight - 2, 0xFFA8C880, false)), Tab(EditorGraphics::Rectangle(160, 24, 0xFFB8D498, false))
{}

void RawInspector::Sync(const LevelDocument& document, i32 object, i32 cell)
{
	if (Document != &document)
	{
		Document = &document;
		ObjectIndex = 0;
		CellIndex = 23;
		LastObject = LastCell = -2;
		Word = First = 0;
		Focus = Field::None;
	}
	if (object >= 0 && object != LastObject)
		ObjectIndex = object;
	if (cell >= 0 && cell != LastCell)
		CellIndex = cell;
	LastObject = object;
	LastCell = cell;
}

std::vector<u32> RawInspector::Words(const LevelDocument& document) const
{
	if (CurrentTarget == Target::Object)
	{
		const std::array<u32, 25> words = document.GetRawObject(ObjectIndex);
		return { words.begin(), words.end() };
	}
	const std::array<u32, 6> words = document.GetRawCell(CellIndex);
	return { words.begin(), words.end() };
}

void RawInspector::Edit(Field field, const LevelDocument& document)
{
	Focus = field;
	Text = field == Field::Index ? std::to_string(CurrentTarget == Target::Object ? ObjectIndex : CellIndex) : Signed(Words(document).at(Word));
	Replace = true;
}

void RawInspector::Commit(LevelDocument& document)
{
	if (Focus == Field::None)
		return;
	const i64 number = Number(Text);
	if (Focus == Field::Index)
	{
		if (number < 0 || number >= static_cast<i64>(CurrentTarget == Target::Object ? LevelFile::ObjectCount : LevelFile::CellCount))
			throw std::out_of_range("Object slots are 0..255; raw cells are 0..373.");
		(CurrentTarget == Target::Object ? ObjectIndex : CellIndex) = static_cast<i32>(number);
	}
	else if (CurrentTarget == Target::Object)
		document.SetRawObjectWord(ObjectIndex, Word, static_cast<u32>(number));
	else
		document.SetRawCellWord(CellIndex, Word, static_cast<u32>(number));
	Focus = Field::None;
}

bool RawInspector::Handle(const WindowInput::Result& input, LevelDocument& document, RECT bounds)
{
	if (input.Click)
		Commit(document);
	if (Focus != Field::None)
	{
		if (input.Back || input.Editor)
		{
			Focus = Field::None;
			return true;
		}
		if (input.Enter || input.SaveEditor || input.Playtest)
		{
			Commit(document);
			return !input.SaveEditor && !input.Playtest;
		}
		if (input.Control && input.BindingKey == 'a')
			Replace = true;
		if (input.Erase)
		{
			if (Replace)
				Text.clear();
			else if (!Text.empty())
				Text.pop_back();
			Replace = false;
		}
		if (!input.Control && !input.Text.empty())
		{
			if (Replace)
				Text.clear();
			if (Text.size() + input.Text.size() <= 32)
				Text += input.Text;
			Replace = false;
		}
		return true;
	}
	const bool inside = input.Mouse.x >= bounds.left && input.Mouse.x < bounds.right && input.Mouse.y >= bounds.top && input.Mouse.y < bounds.bottom;
	if (!inside)
		return false;
	const i32 y = input.Mouse.y - bounds.top, x = input.Mouse.x - bounds.left;
	if (input.Delete || input.Erase)
		return true;
	if (input.Left || input.Right)
	{
		First = std::clamp(First + (input.Right ? Rows : -Rows), 0, std::max(0, static_cast<i32>(Words(document).size()) - Rows));
		return true;
	}
	if (input.Up || input.Down)
	{
		Word = std::clamp(Word + (input.Down ? 1 : -1), 0, static_cast<i32>(Words(document).size()) - 1);
		First = std::clamp(First, std::max(0, Word - Rows + 1), Word);
		return true;
	}
	if (input.Wheel)
		First = std::clamp(First - input.Wheel * 3, 0, std::max(0, static_cast<i32>(Words(document).size()) - Rows));
	if (input.Click)
	{
		if (y < 24)
		{
			CurrentTarget = x < 160 ? Target::Object : Target::Cell;
			Word = First = 0;
		}
		else if (y < 48)
			Edit(Field::Index, document);
		else if (y >= 56 && y < 56 + Rows * RowHeight)
		{
			const i32 row = First + (y - 56) / RowHeight;
			if (row < static_cast<i32>(Words(document).size()))
			{
				Word = row;
				Edit(Field::Value, document);
			}
		}
		else if (y >= bounds.bottom - bounds.top - 96 && y < bounds.bottom - bounds.top - 72)
		{
			if (x < 80 || x >= 240)
				First = std::clamp(First + (x < 80 ? -Rows : Rows), 0, std::max(0, static_cast<i32>(Words(document).size()) - Rows));
		}
		else if (y >= bounds.bottom - bounds.top - 64 && y < bounds.bottom - bounds.top - 40)
			Edit(Field::Value, document);
	}
	return input.Click || input.RightClick || input.Wheel != 0;
}

void RawInspector::Append(std::vector<SpriteDraw>& commands, const LevelDocument& document, RECT bounds, const BitmapFont& font) const
{
	commands.push_back({ &Tab, bounds.left + (CurrentTarget == Target::Object ? 0 : 160), bounds.top, 0 });
	font.Append(commands, "Object words", bounds.left + 6, bounds.top + 4);
	font.Append(commands, "Cell words", bounds.left + 166, bounds.top + 4);
	const str index = Focus == Field::Index ? Text + "_" : std::to_string(CurrentTarget == Target::Object ? ObjectIndex : CellIndex);
	font.Append(commands, (CurrentTarget == Target::Object ? "Slot: " : "Raw cell: ") + index + " (click to edit)", bounds.left + 6, bounds.top + 28);
	const std::vector<u32> words = Words(document);
	for (i32 row = 0; row < Rows && First + row < static_cast<i32>(words.size()); ++row)
	{
		const i32 word = First + row, y = bounds.top + 56 + row * RowHeight;
		commands.push_back({ word == Word ? &Highlight : &Row, bounds.left + 4, y, 0 });
		const cstr name = CurrentTarget == Target::Object ? ObjectNames[word] : CellNames[word];
		font.Append(commands, EditorGraphics::FitText(font, std::to_string(word) + " " + name, 166), bounds.left + 8, y + 3);
		font.Append(commands, Signed(words[word]), bounds.left + 176, y + 3);
	}
	font.Append(commands, "< Prev", bounds.left + 6, bounds.bottom - 92);
	font.Append(commands, std::to_string(First) + "-" + std::to_string(std::min(First + Rows, static_cast<i32>(words.size())) - 1) + " / " + std::to_string(words.size()), bounds.left + 100, bounds.bottom - 92);
	font.Append(commands, "Next >", bounds.left + 246, bounds.bottom - 92);
	font.Append(commands, "Word " + std::to_string(Word) + ": " + (Focus == Field::Value ? Text + "_" : Signed(words[Word])), bounds.left + 6, bounds.bottom - 60);
	f32 real;
	const u32 bits = words[Word];
	std::memcpy(&real, &bits, 4);
	std::ostringstream aliases;
	aliases << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << bits << "  f32: " << std::dec << std::setprecision(6) << real;
	font.Append(commands, EditorGraphics::FitText(font, aliases.str(), 308), bounds.left + 6, bounds.bottom - 36);
	font.Append(commands, "Enter applies | Esc cancels | wheel scrolls", bounds.left + 6, bounds.bottom - 16);
}
