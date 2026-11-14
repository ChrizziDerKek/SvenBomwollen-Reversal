#include "GameData.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <xmllite.h>
#include <shlwapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

#pragma comment(lib, "xmllite.lib")
#pragma comment(lib, "shlwapi.lib")

namespace sven2 {
namespace {
namespace fs = std::filesystem;

void check(HRESULT result, const char* operation) {
    if (FAILED(result)) throw std::runtime_error(operation);
}

std::string utf8(const wchar_t* value, UINT length) {
    if (!length) return {};
    if (length > static_cast<UINT>((std::numeric_limits<int>::max)()))
        throw std::runtime_error("XML string is too long.");
    const int count = static_cast<int>(length);
    const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        value, count, nullptr, 0, nullptr, nullptr);
    if (!bytes) throw std::runtime_error("Invalid Unicode in XML.");
    std::string result(bytes, '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        value, count, result.data(), bytes, nullptr, nullptr))
        throw std::runtime_error("Cannot convert XML to UTF-8.");
    return result;
}

bool equalWindows(const std::wstring& a, const std::wstring& b) {
    return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
}

bool contained(const fs::path& root, const fs::path& candidate) {
    auto part = candidate.begin();
    for (const auto& base : root) {
        if (part == candidate.end() || !equalWindows(base.native(), part->native()))
            return false;
        ++part;
    }
    return true;
}

struct Node {
    std::string name;
    std::map<std::string, std::string> attributes;
    std::vector<Node> children;

    const std::string& attribute(const std::string& key) const {
        const auto found = attributes.find(key);
        if (found == attributes.end())
            throw std::runtime_error("Missing XML attribute: " + name + "/" + key);
        return found->second;
    }
};

std::vector<unsigned char> xmlBytes(const std::vector<unsigned char>& bytes) {
    // UTF-16 BOMs belong to XmlLite's encoding detection, not the legacy fallback.
    if (bytes.size() >= 2 &&
        ((bytes[0] == 0xff && bytes[1] == 0xfe) ||
         (bytes[0] == 0xfe && bytes[1] == 0xff))) return bytes;
    if (bytes.empty()) return bytes;
    if (bytes.size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
        throw std::runtime_error("XML file is too large for encoding validation.");
    const auto* input = reinterpret_cast<const char*>(bytes.data());
    const int length = static_cast<int>(bytes.size());
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, length, nullptr, 0))
        return bytes;
    if (GetLastError() != ERROR_NO_UNICODE_TRANSLATION)
        throw std::runtime_error("Cannot validate XML encoding.");

    // Original core files declare UTF-8 but contain Windows-1252 umlauts.
    // Transcode only invalid UTF-8; keep the UTF-8 declaration and any BOM intact.
    const int bom = bytes.size() >= 3 && bytes[0] == 0xef &&
        bytes[1] == 0xbb && bytes[2] == 0xbf ? 3 : 0;
    const int count = MultiByteToWideChar(1252, 0, input + bom, length - bom, nullptr, 0);
    if (!count) throw std::runtime_error("Cannot decode Windows-1252 XML.");
    std::wstring wide(count, L'\0');
    if (!MultiByteToWideChar(1252, 0, input + bom, length - bom, wide.data(), count))
        throw std::runtime_error("Cannot decode Windows-1252 XML.");
    auto converted = utf8(wide.data(), static_cast<UINT>(wide.size()));
    if (bom) converted.insert(0, "\xef\xbb\xbf");
    return {converted.begin(), converted.end()};
}

Node readXml(const std::vector<unsigned char>& source) {
    const auto bytes = xmlBytes(source);
    if (bytes.size() > (std::numeric_limits<UINT>::max)())
        throw std::runtime_error("XML file is too large.");
    Microsoft::WRL::ComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(bytes.data(), static_cast<UINT>(bytes.size())));
    if (!stream) throw std::runtime_error("Cannot create XML stream.");
    Microsoft::WRL::ComPtr<IXmlReader> reader;
    check(CreateXmlReader(__uuidof(IXmlReader),
        reinterpret_cast<void**>(reader.GetAddressOf()), nullptr), "Cannot create XML reader.");
    check(reader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit),
        "Cannot prohibit XML DTDs.");
    check(reader->SetProperty(XmlReaderProperty_MaxElementDepth, 64),
        "Cannot limit XML depth.");
    check(reader->SetInput(stream.Get()), "Cannot set XML input.");

    Node document;
    std::vector<Node*> stack{&document};
    XmlNodeType type;
    HRESULT result;
    while ((result = reader->Read(&type)) == S_OK) {
        if (type == XmlNodeType_Element) {
            const wchar_t* value = nullptr;
            UINT length = 0;
            check(reader->GetLocalName(&value, &length), "Cannot read XML element.");
            Node node;
            node.name = utf8(value, length);
            HRESULT attributeResult = reader->MoveToFirstAttribute();
            while (attributeResult == S_OK) {
                check(reader->GetLocalName(&value, &length), "Cannot read attribute name.");
                const auto name = utf8(value, length);
                check(reader->GetValue(&value, &length), "Cannot read attribute value.");
                node.attributes.emplace(name, utf8(value, length));
                attributeResult = reader->MoveToNextAttribute();
            }
            check(attributeResult, "Cannot enumerate XML attributes.");
            reader->MoveToElement();
            stack.back()->children.push_back(std::move(node));
            if (!reader->IsEmptyElement()) stack.push_back(&stack.back()->children.back());
        } else if (type == XmlNodeType_EndElement) {
            if (stack.size() <= 1) throw std::runtime_error("Unbalanced XML.");
            stack.pop_back();
        }
    }
    check(result, "Malformed XML or prohibited DTD.");
    if (stack.size() != 1 || document.children.size() != 1 ||
        document.children.front().name != "sven2")
        throw std::runtime_error("Expected a single sven2 XML root.");
    return std::move(document.children.front());
}

bool number(const std::string& value, double& result) {
    std::istringstream input(value);
    input.imbue(std::locale::classic());
    if (!(input >> result) || !std::isfinite(result)) return false;
    input >> std::ws;
    return input.eof();
}

LevelPackInfo readPack(const Node& package, std::string descriptor) {
    LevelPackInfo pack;
    pack.name = package.attribute("name");
    pack.descriptor = std::move(descriptor);
    for (const auto& section : package.children) {
        for (const auto& item : section.children) {
            if (section.name == "levelsets" && item.name == "levelset") {
                LevelSetInfo set;
                set.name = item.attribute("name");
                for (const auto& level : item.children) {
                    if (level.name != "level") continue;
                    set.levels.push_back({level.attribute("name"), level.attribute("map"),
                        level.attribute("tileset"), level.attribute("music")});
                }
                pack.levelsets.push_back(std::move(set));
            } else if (section.name == "tilesets" && item.name == "tileset") {
                TileSetInfo set;
                set.name = item.attribute("name");
                set.background = item.attribute("background");
                for (const auto& tile : item.children) {
                    if (tile.name != "tile") continue;
                    double frames = 0;
                    if (!number(tile.attribute("frames"), frames) || frames < 1 ||
                        frames > (std::numeric_limits<short>::max)() || std::floor(frames) != frames)
                        throw std::runtime_error("Invalid tile frame count in " + pack.descriptor);
                    set.tiles.push_back({tile.attribute("file"), static_cast<short>(frames)});
                }
                pack.tilesets.push_back(std::move(set));
            }
        }
    }
    return pack;
}
} // namespace

GameData::GameData(std::filesystem::path root) : root_(fs::canonical(root)) {
    if (!fs::is_directory(root_)) throw std::runtime_error("Data root is not a directory.");
    const auto core = readXml(readAsset("Sven2/Sven2.xml"));
    for (const auto& section : core.children) {
        if (section.name == "sounds") {
            for (const auto& item : section.children)
                if (item.name == "sound") sounds_[item.attribute("name")] = item.attribute("file");
        }
        if (section.name != "parameters") continue;
        for (const auto& item : section.children) {
            if (item.name != "parameter") continue;
            const auto& name = item.attribute("name");
            const auto& rawValue = item.attribute("value");
            parameterStrings_[name] = rawValue;
            double value = 0;
            // Core also contains nonnumeric parameters (e.g. highscore names).
            if (number(rawValue, value)) parameters_[name] = value;
        }
    }
    const auto table = readXml(readAsset("Sven2/StringTable.xml"));
    for (const auto& section : table.children) {
        if (section.name != "stringtable") continue;
        for (const auto& item : section.children) {
            if (item.name == "string") strings_[item.attribute("name")] = item.attribute("value");
        }
    }

    std::vector<fs::path> directories;
    for (const auto& entry : fs::directory_iterator(root_)) {
        if (entry.is_directory() && !equalWindows(entry.path().filename().native(), L"Sven2"))
            directories.push_back(entry.path().filename());
    }
    std::sort(directories.begin(), directories.end(), [](const fs::path& a, const fs::path& b) {
        const int order = CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE);
        return order == CSTR_LESS_THAN || (order == CSTR_EQUAL && a.native() < b.native());
    });
    for (const auto& directory : directories) {
        const auto expected = directory.native() + L".xml";
        bool found = false;
        for (const auto& entry : fs::directory_iterator(root_ / directory)) {
            if (entry.is_regular_file() && equalWindows(entry.path().filename().native(), expected)) {
                found = true;
                break;
            }
        }
        if (!found) continue;
        const auto descriptor = (directory / fs::path(expected)).generic_u8string();
        const auto document = readXml(readAsset(descriptor));
        for (const auto& package : document.children) {
            if (package.name == "package") packs_.push_back(readPack(package, descriptor));
        }
    }
}

const std::filesystem::path& GameData::root() const noexcept { return root_; }

std::filesystem::path GameData::assetPath(std::string name) const {
    if (name.empty() || name.find('\0') != std::string::npos)
        throw std::invalid_argument("Empty or invalid asset path.");
    const auto relative = fs::u8path(name);
    if (relative.has_root_path()) throw std::invalid_argument("Asset path must be relative.");
    auto resolved = root_;
    for (const auto& part : relative) {
        const auto component = part.native();
        if (component.empty() || component == L"." || component == L".." ||
            component.find_first_of(L"<>:\"|?*") != std::wstring::npos ||
            component.back() == L'.' || component.back() == L' ' ||
            std::any_of(component.begin(), component.end(), [](wchar_t c) { return c < 32; }))
            throw std::invalid_argument("Invalid relative asset component.");
        bool found = false;
        for (const auto& entry : fs::directory_iterator(resolved)) {
            if (!equalWindows(entry.path().filename().native(), component)) continue;
            if (found) throw std::runtime_error("Ambiguous asset path: " + name);
            resolved = entry.path();
            found = true;
        }
        if (!found) throw std::runtime_error("Asset not found: " + name);
        resolved = fs::canonical(resolved);
        if (!contained(root_, resolved)) throw std::invalid_argument("Asset escapes data root.");
    }
    if (!fs::is_regular_file(resolved)) throw std::runtime_error("Asset is not a file: " + name);
    return resolved;
}

std::vector<unsigned char> GameData::readAsset(const std::string& name) const {
    std::ifstream file(assetPath(name), std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("Cannot open asset: " + name);
    const auto length = file.tellg();
    if (length < 0 || static_cast<unsigned long long>(length) >
        static_cast<unsigned long long>((std::numeric_limits<std::streamsize>::max)()) ||
        static_cast<unsigned long long>(length) > (std::numeric_limits<size_t>::max)())
        throw std::runtime_error("Invalid asset size: " + name);
    std::vector<unsigned char> bytes(static_cast<size_t>(length));
    file.seekg(0);
    if (!bytes.empty() && !file.read(reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Cannot read asset: " + name);
    return bytes;
}

const std::vector<LevelPackInfo>& GameData::packs() const noexcept { return packs_; }

std::string GameData::text(const std::string& key) const {
    const auto found = strings_.find(key);
    return found == strings_.end() ? key : found->second;
}

std::string GameData::soundPath(const std::string& name) const {
    const auto found=sounds_.find(name);
    return found==sounds_.end()?std::string{}:found->second;
}

double GameData::parameter(const std::string& name, double defaultValue) const {
    const auto found = parameters_.find(name);
    return found == parameters_.end() ? defaultValue : found->second;
}

std::string GameData::parameterString(const std::string& name, const std::string& defaultValue) const {
    const auto found = parameterStrings_.find(name);
    return found == parameterStrings_.end() ? defaultValue : found->second;
}

const TileSetInfo& GameData::tileset(const LevelPackInfo& pack, const LevelInfo& level) const {
    for (const auto& set : pack.tilesets) {
        if (set.name == level.tileset) return set;
    }
    throw std::out_of_range("Unknown tileset '" + level.tileset + "' in " + pack.descriptor);
}

} // namespace sven2
