#include "Highscores.h"
#include <windows.h>
#include <algorithm>
#include <charconv>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>

namespace sven2 {
namespace {
[[noreturn]] void ioError(const char* operation,DWORD error=GetLastError()) {
    throw std::system_error(static_cast<int>(error),std::system_category(),operation);
}
struct Handle {
    HANDLE value=INVALID_HANDLE_VALUE;
    ~Handle() { if(value!=INVALID_HANDLE_VALUE) CloseHandle(value); }
};
void validate(const HighscoreEntry& entry) {
    if(entry.level<0) throw std::invalid_argument("Highscore level must be nonnegative");
    if(entry.sheep<0) throw std::invalid_argument("Highscore animal count must be nonnegative");
    if(entry.score<0 || entry.name.empty() || entry.name.size()>80)
        throw std::invalid_argument("Highscore requires nonnegative score and a name of 1..20 Unicode characters");
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,entry.name.data(),static_cast<int>(entry.name.size()),nullptr,0);
    if(!n) throw std::invalid_argument("Highscore name is not valid UTF-8");
    std::wstring wide(n,L'\0');
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,entry.name.data(),static_cast<int>(entry.name.size()),&wide[0],n))
        throw std::invalid_argument("Highscore name conversion failed");
    int count=0;
    for(size_t i=0;i<wide.size();++i) {
        unsigned c=wide[i];
        if(c<32 || (c>=127 && c<=159) || c==0x2028 || c==0x2029)
            throw std::invalid_argument("Highscore names cannot contain control characters or line separators");
        if(c>=0xd800 && c<=0xdbff) ++i;
        ++count;
    }
    if(count>20) throw std::invalid_argument("Highscore name exceeds 20 Unicode characters");
}
void rank(std::vector<HighscoreEntry>& entries) {
    std::stable_sort(entries.begin(),entries.end(),[](const HighscoreEntry& a,const HighscoreEntry& b){return a.score>b.score;});
    if(entries.size()>10) entries.resize(10);
}
void saveTextFile(const std::filesystem::path& file,const std::string& data) {
    std::filesystem::create_directories(file.parent_path());
    // CREATE_NEW prevents collisions and never truncates another writer's temp.
    std::filesystem::path temp;
    Handle output;
    for(unsigned attempt=0;attempt<100;++attempt) {
        temp=file;
        temp+=L".tmp."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetTickCount64())+L"."+std::to_wstring(attempt);
        output.value=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(output.value!=INVALID_HANDLE_VALUE) break;
        DWORD error=GetLastError();
        if(error!=ERROR_FILE_EXISTS && error!=ERROR_ALREADY_EXISTS) ioError("Create highscore temporary file",error);
    }
    if(output.value==INVALID_HANDLE_VALUE) ioError("Highscore temporary file collisions",ERROR_FILE_EXISTS);
    try {
        DWORD written=0;
        if(!WriteFile(output.value,data.data(),static_cast<DWORD>(data.size()),&written,nullptr)) ioError("Write highscores");
        if(written!=data.size()) ioError("Short highscore write",ERROR_WRITE_FAULT);
        if(!FlushFileBuffers(output.value)) ioError("Flush highscores");
        HANDLE handle=output.value; output.value=INVALID_HANDLE_VALUE;
        if(!CloseHandle(handle)) ioError("Close highscore temporary file");
        // Same-directory rename is atomic; no COPY_ALLOWED fallback across volumes.
        if(!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            ioError("Replace highscores");
    } catch(...) {
        if(output.value!=INVALID_HANDLE_VALUE) { CloseHandle(output.value); output.value=INVALID_HANDLE_VALUE; }
        DeleteFileW(temp.c_str());
        throw;
    }
}

}
Highscores::Highscores(std::filesystem::path file,std::vector<HighscoreEntry> defaults)
    : file_(std::filesystem::absolute(std::move(file))) {
    Handle input;
    input.value=CreateFileW(file_.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(input.value==INVALID_HANDLE_VALUE) {
        DWORD error=GetLastError();
        if(error!=ERROR_FILE_NOT_FOUND && error!=ERROR_PATH_NOT_FOUND) ioError("Open highscores",error);
        for(const auto& entry:defaults) validate(entry);
        rank(defaults); entries_=std::move(defaults); return;
    }
    LARGE_INTEGER size{};
    if(!GetFileSizeEx(input.value,&size)) ioError("Read highscore size");
    if(size.QuadPart<0 || size.QuadPart>4096) throw std::runtime_error("Corrupt highscores: file exceeds 4096 bytes");
    std::string data(static_cast<size_t>(size.QuadPart),'\0');
    DWORD read=0;
    if(!ReadFile(input.value,data.data(),static_cast<DWORD>(data.size()),&read,nullptr)) ioError("Read highscores");
    if(read!=data.size()) throw std::runtime_error("Incomplete highscore read");
    try {
        size_t start=0;
        while(start<data.size()) {
            size_t end=data.find('\n',start);
            if(end==std::string::npos) end=data.size();
            std::string line=data.substr(start,end-start);
            if(!line.empty() && line.back()=='\r') line.pop_back();
            size_t tab=line.find('\t');
            if(tab==std::string::npos || tab==0) throw std::invalid_argument("Expected score TAB name");
            int score=0;
            const char* first=line.data(); const char* last=first+tab;
            auto parsed=std::from_chars(first,last,score);
            if(parsed.ec!=std::errc() || parsed.ptr!=last || line[0]<'0' || line[0]>'9')
                throw std::invalid_argument("Invalid decimal score");
            size_t levelTab=line.find('\t',tab+1);
            const size_t sheepTab=levelTab==std::string::npos?std::string::npos:line.find('\t',levelTab+1);
            int level=1;
            if(levelTab!=std::string::npos) {
                const char* levelFirst=line.data()+levelTab+1;
                const char* levelLast=line.data()+(sheepTab==std::string::npos?line.size():sheepTab);
                auto parsedLevel=std::from_chars(levelFirst,levelLast,level);
                if(parsedLevel.ec!=std::errc() || parsedLevel.ptr!=levelLast || level<0)
                    throw std::invalid_argument("Invalid level");
            }
            int sheep=0;
            if(sheepTab!=std::string::npos) {
                const char* firstSheep=line.data()+sheepTab+1;
                const char* lastSheep=line.data()+line.size();
                auto parsedSheep=std::from_chars(firstSheep,lastSheep,sheep);
                if(parsedSheep.ec!=std::errc() || parsedSheep.ptr!=lastSheep || sheep<0)
                    throw std::invalid_argument("Invalid animal count");
            }
            HighscoreEntry entry{line.substr(tab+1,levelTab==std::string::npos?std::string::npos:levelTab-tab-1),score,level,sheep}; validate(entry);
            entries_.push_back(std::move(entry));
            if(entries_.size()>10) throw std::invalid_argument("More than 10 highscore entries");
            start=end==data.size()?end:end+1;
        }
    } catch(const std::invalid_argument& error) {
        throw std::runtime_error(std::string("Corrupt highscore file (preserved): ")+error.what());
    }
    rank(entries_);
}
bool Highscores::qualifies(int score) const noexcept {
    return score>=0 && (entries_.size()<10 || score>entries_.back().score);
}
void Highscores::submit(const std::string& name,int score,int level,int sheep) {
    if(level<1) throw std::invalid_argument("Submitted highscore level must be at least 1");
    HighscoreEntry entry{name,score,level,sheep}; validate(entry);
    auto next=entries_; next.push_back(std::move(entry)); rank(next);
    save(next); entries_.swap(next);
}
void Highscores::save(const std::vector<HighscoreEntry>& entries) const {
    std::string data;
    for(const auto& entry:entries) data+=std::to_string(entry.score)+"\t"+entry.name+"\t"+std::to_string(entry.level)+"\t"+std::to_string(entry.sheep)+"\n";
    saveTextFile(file_,data);
}

}
