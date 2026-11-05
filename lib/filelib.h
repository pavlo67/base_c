
#ifndef __FILELIB_H
#define __FILELIB_H

#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <list>

std::string nameOfFile(const std::string& filepath);
std::string baseOfFile(const std::string& filepath);
std::string pathOfFile(const std::string& filepath);
std::string extOfFile(const std::string& filepath);
std::string extPartOfFile(const std::string& filepath);
std::filesystem::path filePathWithSuffix(const std::filesystem::path& filepath, const std::string& suffix);
std::filesystem::path filePathWithStemSuffix(const std::filesystem::path& filepath, const std::string& stemSuffix);

bool        clearFile(const std::string& filepath);
std::string newPath(const std::string& path);
FILE*       newFile(const std::string& filepath); // new_path doesn't add a time-part to the path

int         findExtension(std::string path, const std::string* exts, int extsCnt);
bool        hasSubdirs(std::filesystem::path path);
bool        collectFiles(const std::filesystem::path& directory, std::vector<std::filesystem::path>& files);
bool        isImagePath(const std::filesystem::path& path);
bool        parseImageNumber(const std::filesystem::path& path, uint64_t& number);
bool        parsePathNumberPrefix(const std::filesystem::path& path, uint64_t& number, unsigned& width);
std::string makeTimestamp();
std::string makeNumberedName(uint64_t number, const std::string& extension, unsigned width);
bool        isNameUsed(const std::string& name, const std::vector<std::filesystem::path>& firstPaths,
            const std::vector<std::filesystem::path>& secondPaths);

std::list<std::string> listOfFiles(const std::string& path, const std::string* exts, int extsCnt, bool dirs = true, bool files = true, const std::string& prefix = "");

bool        readFileByLines(const std::string& filepath, std::vector<std::string>& lines);
bool        readFile(const std::string& filepath, std::string& text);
bool        writeFile(const std::string& filepath, const char* modes, const char* content);
bool        writeFileWithTimestampBackup(const std::filesystem::path& filepath, const std::string& content,
            std::filesystem::path& backupPath, std::string& error);
bool        renamePath(const std::string& oldPath, const std::string& newPath);

bool        ensureDirectory(const std::filesystem::path& path, const std::string& label, bool createIfMissing = true);
bool        ensureNotRegularFile(const std::filesystem::path& path, const std::string& label);
std::string safePathPart(std::string s);
bool        sameFileSize(const std::filesystem::path& a, const std::filesystem::path& b, bool& same);
bool        sameFileContent(const std::filesystem::path& a, const std::filesystem::path& b, bool& same);
bool        copyFileReplacing(const std::filesystem::path& srcPath, const std::filesystem::path& dstPath);
bool        removeFilesWithExtension(const std::filesystem::path& directory, const std::string& extension,
            std::string& error);
bool        removeFsPath(const std::filesystem::path& path, const std::string& reason);
bool        moveFileReplacing(const std::filesystem::path& srcPath, const std::filesystem::path& dstPath);
bool        cleanupDirectory(const std::filesystem::path& dirPath);

bool        createUniqueDirectory(const std::filesystem::path& parent, const std::string& prefix, std::filesystem::path& directory);
bool        createNextNumberedDirectory(const std::filesystem::path& parent, unsigned width,
            unsigned& number, std::filesystem::path& directory);

#endif // __FILELIB_H
