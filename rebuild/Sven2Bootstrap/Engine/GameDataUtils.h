#pragma once
#include "Types.h"

namespace
{

void Check(HRESULT result, cstr operation)
{
	if (FAILED(result))
		throw std::runtime_error(operation);
}

str UTF8(cwstr value, u32 length)
{
	if (!length)
		return {};
	if (length > INT_MAX)
		throw std::runtime_error("XML string is too long.");
	const i32 count = static_cast<i32>(length);
	const i32 bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, count, nullptr, 0, nullptr, nullptr);
	if (!bytes)
		throw std::runtime_error("Invalid Unicode in XML.");
	str result(bytes, '\0');
	if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, count, result.data(), bytes, nullptr, nullptr))
		throw std::runtime_error("Cannot convert XML to UTF-8.");
	return result;
}

bool EqualWindows(const wstr& a, const wstr& b)
{
	return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
}

bool Contained(const fpath& root, const fpath& candidate)
{
	auto part = candidate.begin();
	for (const auto& base : root)
	{
		if (part == candidate.end() || !EqualWindows(base.native(), part->native()))
			return false;
		++part;
	}
	return true;
}

struct Node
{
	str Name;
	std::map<str, str> Attributes;
	std::vector<Node> Children;

	const str& Attribute(const str& key) const
	{
		const auto found = Attributes.find(key);
		if (found == Attributes.end())
			throw std::runtime_error("Missing XML attribute: " + Name + "/" + key);
		return found->second;
	}
};

bytestream XmlBytes(const bytestream& bytes)
{
	// UTF-16 BOMs belong to XmlLite's encoding detection, not the legacy fallback.
	if (bytes.size() >= 2 && ((bytes[0] == 0xff && bytes[1] == 0xfe) || (bytes[0] == 0xfe && bytes[1] == 0xff)))
		return bytes;
	if (bytes.empty())
		return bytes;
	if (bytes.size() > INT_MAX)
		throw std::runtime_error("XML file is too large for encoding validation.");
	const cstr input = reinterpret_cast<cstr>(bytes.data());
	const i32 length = static_cast<i32>(bytes.size());
	if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, length, nullptr, 0))
		return bytes;
	if (GetLastError() != ERROR_NO_UNICODE_TRANSLATION)
		throw std::runtime_error("Cannot validate XML encoding.");
	// Original core files declare UTF-8 but contain Windows-1252 umlauts.
	// Transcode only invalid UTF-8; keep the UTF-8 declaration and any BOM intact.
	const i32 bom = bytes.size() >= 3 && bytes[0] == 0xef && bytes[1] == 0xbb && bytes[2] == 0xbf ? 3 : 0;
	const i32 count = MultiByteToWideChar(1252, 0, input + bom, length - bom, nullptr, 0);
	if (!count)
		throw std::runtime_error("Cannot decode Windows-1252 XML.");
	wstr wide(count, L'\0');
	if (!MultiByteToWideChar(1252, 0, input + bom, length - bom, wide.data(), count))
		throw std::runtime_error("Cannot decode Windows-1252 XML.");
	str converted = UTF8(wide.data(), static_cast<u32>(wide.size()));
	if (bom)
		converted.insert(0, "\xef\xbb\xbf");
	return { converted.begin(), converted.end() };
}

bytestream LegacyAttributeMarkup(const bytestream& source)
{
	if (source.size() >= 2 && ((source[0] == 0xff && source[1] == 0xfe) || (source[0] == 0xfe && source[1] == 0xff)))
		return source;
	bytestream output;
	output.reserve(source.size());
	bool inTag = false;
	u8 quote = 0;
	for (size_t i = 0; i < source.size(); ++i)
	{
		const u8 c = source[i];
		if (!quote && c == '<' && i + 4 <= source.size() && str(source.begin() + i, source.begin() + i + 4) == "<!--")
		{
			const str remaining(source.begin() + i, source.end());
			const size_t end = remaining.find("-->");
			if (end == str::npos)
				throw std::runtime_error("Unterminated XML comment.");
			output.insert(output.end(), source.begin() + i, source.begin() + i + end + 3);
			i += end + 2;
			continue;
		}
		if (quote)
		{
			if (c == quote)
				quote = 0;
			else if (c == '<')
			{
				output.insert(output.end(), { '&', 'l', 't', ';' });
				continue;
			}
			else if (c == '&')
			{
				const str tail(source.begin() + i, source.begin() + std::min(source.size(), i + 16));
				const bool entity = tail.rfind("&amp;", 0) == 0 || tail.rfind("&lt;", 0) == 0 || tail.rfind("&gt;", 0) == 0 || tail.rfind("&quot;", 0) == 0 || tail.rfind("&apos;", 0) == 0 || (tail.rfind("&#", 0) == 0 && tail.find(';') != str::npos);
				if (!entity)
				{
					output.insert(output.end(), { '&', 'a', 'm', 'p', ';' });
					continue;
				}
			}
		}
		else if (c == '<')
			inTag = true;
		else if (c == '>')
			inTag = false;
		else if (inTag && (c == '\"' || c == '\''))
			quote = c;
		output.push_back(c);
	}
	return output;
}

Node ReadXml(const bytestream& source)
{
	const bytestream bytes = LegacyAttributeMarkup(XmlBytes(source));
	if (bytes.size() > UINT_MAX)
		throw std::runtime_error("XML file is too large.");
	Microsoft::WRL::ComPtr<IStream> stream;
	stream.Attach(SHCreateMemStream(bytes.data(), static_cast<u32>(bytes.size())));
	if (!stream)
		throw std::runtime_error("Cannot create XML stream.");
	Microsoft::WRL::ComPtr<IXmlReader> reader;
	Check(CreateXmlReader(IID_PPV_ARGS(reader.GetAddressOf()), nullptr), "Cannot create XML reader.");
	Check(reader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit), "Cannot prohibit XML DTDs.");
	Check(reader->SetProperty(XmlReaderProperty_MaxElementDepth, 64), "Cannot limit XML depth.");
	Check(reader->SetInput(stream.Get()), "Cannot set XML input.");
	Node document;
	std::vector<Node*> stack{ &document };
	XmlNodeType type;
	HRESULT result;
	while ((result = reader->Read(&type)) == S_OK)
	{
		if (type == XmlNodeType_Element)
		{
			cwstr value = nullptr;
			u32 length = 0;
			Check(reader->GetLocalName(&value, &length), "Cannot read XML element.");
			Node node;
			node.Name = UTF8(value, length);
			HRESULT attributeResult = reader->MoveToFirstAttribute();
			while (attributeResult == S_OK)
			{
				Check(reader->GetLocalName(&value, &length), "Cannot read attribute name.");
				const str name = UTF8(value, length);
				Check(reader->GetValue(&value, &length), "Cannot read attribute value.");
				node.Attributes.emplace(name, UTF8(value, length));
				attributeResult = reader->MoveToNextAttribute();
			}
			Check(attributeResult, "Cannot enumerate XML attributes.");
			reader->MoveToElement();
			stack.back()->Children.push_back(std::move(node));
			if (!reader->IsEmptyElement())
				stack.push_back(&stack.back()->Children.back());
		}
		else if (type == XmlNodeType_EndElement)
		{
			if (stack.size() <= 1)
				throw std::runtime_error("Unbalanced XML.");
			stack.pop_back();
		}
	}
	Check(result, "Malformed XML or prohibited DTD.");
	if (stack.size() != 1 || document.Children.size() != 1 || (document.Children.front().Name != "sven2" && document.Children.front().Name != "SvenXXX"))
		throw std::runtime_error("Expected a single Sven game XML root.");
	return std::move(document.Children.front());
}

bool Number(const str& value, f64& result)
{
	std::istringstream input(value);
	input.imbue(std::locale::classic());
	if (!(input >> result) || !std::isfinite(result))
		return false;
	input >> std::ws;
	return input.eof();
}

LevelPackInfo ReadPack(const Node& package, str descriptor)
{
	LevelPackInfo pack;
	pack.Name = package.Attribute("name");
	pack.Descriptor = std::move(descriptor);
	for (const Node& section : package.Children)
	{
		for (const Node& item : section.Children)
		{
			if (section.Name == "levelsets" && item.Name == "levelset")
			{
				LevelSetInfo set;
				set.Name = item.Attribute("name");
				for (const Node& level : item.Children)
				{
					if (level.Name != "level")
						continue;
					set.Levels.push_back({ level.Attribute("name"), level.Attribute("map"), level.Attribute("tileset"), level.Attribute("music") });
				}
				pack.LevelSets.push_back(std::move(set));
			}
			else if (section.Name == "tilesets" && item.Name == "tileset")
			{
				TileSetInfo set;
				set.Name = item.Attribute("name");
				set.Background = item.Attribute("background");
				for (const Node& tile : item.Children)
				{
					if (tile.Name != "tile")
						continue;
					f64 frames = 0;
					if (!Number(tile.Attribute("frames"), frames) || frames < 1 || frames > SHRT_MAX || std::floor(frames) != frames)
						throw std::runtime_error("Invalid tile frame count in " + pack.Descriptor);
					set.Tiles.push_back({ tile.Attribute("file"), static_cast<u16>(frames) });
				}
				pack.TileSets.push_back(std::move(set));
			}
		}
	}
	return pack;
}
} // namespace
