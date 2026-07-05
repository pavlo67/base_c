
#ifndef __FILELIB_H
#define __FILELIB_H

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
#include <list>

std::string nameOfFile(const std::string& filepath);
std::string baseOfFile(const std::string& filepath);
std::string pathOfFile(const std::string& filepath);
std::string extOfFile(const std::string& filepath);
std::string extPartOfFile(const std::string& filepath);

bool        clearFile(const std::string& filepath);
std::string newPath(const std::string& path);
FILE*       newFile(const std::string& filepath); // new_path doesn't add a time-part to the path

int         findExtension(std::string path, const std::string* exts, int extsCnt);
bool        hasSubdirs(std::filesystem::path path);

std::list<std::string> listOfFiles(const std::string& path, const std::string* exts, int extsCnt, bool dirs = true, bool files = true, const std::string& prefix = "");

bool        readFileByLines(const std::string& filepath, std::vector<std::string>& lines);
bool        readFile(const std::string& filepath, std::string& text);
bool        writeFile(const std::string& filepath, const char* modes, const char* content);
bool        renamePath(const std::string& oldPath, const std::string& newPath);

bool        ensureDirectory(const std::filesystem::path& path, const std::string& label, bool createIfMissing = true);
bool        ensureNotRegularFile(const std::filesystem::path& path, const std::string& label);
std::string safePathPart(std::string s);
bool        sameFileSize(const std::filesystem::path& a, const std::filesystem::path& b, bool& same);
bool        removeFsPath(const std::filesystem::path& path, const std::string& reason);
bool        moveFileReplacing(const std::filesystem::path& srcPath, const std::filesystem::path& dstPath);
bool        cleanupDirectory(const std::filesystem::path& dirPath);

#endif // __FILELIB_H
