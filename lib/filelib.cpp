#include <cstring>
#include <fstream>
#include <iostream>
#include <dirent.h>
#include <filesystem>

namespace fs = std::filesystem;

#include "filelib.h"

std::string nameOfFile(const std::string& filepath) {

    // TODO!!! backslash delimiter in windows

    int pos = filepath.find_last_of("/");
    if (pos >= 0) {
        return filepath.substr(pos + 1);
    }

    return filepath;
}

std::string baseOfFile(const std::string& filepath) {
    std::string filename = nameOfFile(filepath);

    int pos = filename.find_last_of(".");
    if (pos >= 0) {
        return filename.substr(0, pos);
    }

    return filename;
}

std::string extOfFile(const std::string& filepath) {
    std::string filename = nameOfFile(filepath);

    auto pos = filename.find_last_of('.');
    if (pos != std::string::npos) {
        return filename.substr(pos + 1);
    }

    return "";
}

std::string extPartOfFile(const std::string& filepath) {
    std::string filename = nameOfFile(filepath);

    auto pos = filename.find_last_of('.');
    if (pos != std::string::npos) {
        return filename.substr(pos);
    }

    return "";
}


std::string pathOfFile(const std::string& filepath) {

    // TODO!!! backslash delimiter in windows

    int pos = filepath.find_last_of("/");
    if (pos >= 0) {
        return filepath.substr(0, pos + 1);
    }

    return "";
}


FILE* newFile(const std::string& filepath) {
    const char* filepathC = filepath.c_str();
    FILE* fLogPtr = fopen(filepathC, "w");
    if (!fLogPtr) {
        printf("newFile(): error opening %s (errno = %d / %s)\n", filepathC, errno, std::strerror(errno));
    } else {
        printf("newFile() -->  %s\n", filepathC);
    }

    return fLogPtr;
}

bool clearFile(const std::string& filepath) {
    FILE* fLogPtr = fopen(filepath.c_str(), "w");
    if (!fLogPtr) {
        printf("clearFile(): error opening %s (errno = %d / %s)\n", filepath.c_str(), errno, std::strerror(errno));
        return false;
    }

    fflush(fLogPtr);
    fclose(fLogPtr);
    return true;
}

std::string newPath(const std::string& path) {
    if (path.empty()) {
        return "";
    }

    try {
        std::filesystem::create_directories(path);
    } catch (const std::exception& e) {
        printf("newPath(): error creating log path %s: %s\n", path.c_str(), e.what());
        return "";
    }

    return path.back() == '/' ? path : path + "/";;
}

bool readFileByLines(const std::string& filepath, std::vector<std::string>& lines) {
    std::ifstream inputFile(filepath);
    if (!inputFile.is_open()) {
        printf("readFileByLines(): error opening file: %s\n", filepath.c_str());
        return false;
    }

    std::string line;
    while (getline(inputFile, line)) {
        lines.push_back(line);
    }

    inputFile.close();
    return true;

}

bool readFile(const std::string& filepath, std::string& text) {
    std::ifstream inputFile(filepath);
    if (!inputFile.is_open()) {
        printf("readFile(): error opening file: %s\n", filepath.c_str());
        return false;
    }

    text.clear();
    std::string line;
    while (getline(inputFile, line)) {
        text += line + "\n";
    }

    inputFile.close();
    return true;
}

bool writeFile(const std::string& filepath, const char* modes, const char* content) {
    FILE *fptr = fopen(filepath.c_str(), modes);  // "a": append
    if (fptr == nullptr) {
        return false;
    } else if (!content) {
        return true;
    }

    int cnt = fprintf(fptr, "%s", content);
    fclose(fptr);

    return (size_t)cnt == strlen(content);
}

int findExtension(std::string path, const std::string* exts, int extsCnt) {
    if (extsCnt <= 0) {
        return -1;
    } else if (path.empty()) {
        path = "./";
    }

    DIR *dir = opendir(path.c_str());
    if (!dir) {
        printf("findExtension(): can't open directory '%s'\n", path.c_str());
        return -1;
    }

    dirent *ent;
    while ((ent = readdir(dir))) {
        std::string name(ent->d_name);
        for (int i = 0; i < extsCnt; i++) {
            uint32_t extLength = exts[i].length();
            if (name.length() > extLength && name.substr(name.length() - extLength) == exts[i]) {
                closedir(dir);
                return i;
            }
        }
    }

    closedir(dir);
    return -1;
}

bool hasSubdirs(std::filesystem::path path) {
    if (!std::filesystem::is_directory(path)) {
        return false;
    }

    auto pathStr = path.string();
    if (pathStr.empty()) {
        pathStr = "./";
    } else if (pathStr.back() != '/') {
        pathStr += "/";
    }

    dirent *ent;
    DIR *dir = opendir(pathStr.c_str());
    if (!dir) {
        printf("hasSubdirs(): can't open directory '%s'\n", pathStr.c_str());
        return false;
    }

    while ((ent = readdir(dir))) {
        std::string name(ent->d_name);
        if (name == "." || name == "..") {
             continue;
        } else if (std::filesystem::is_directory(pathStr + name)) {
            // printf("std::filesystem::is_directory(%s)\n", (pathStr + name).c_str());
            closedir(dir);
            return true;
        }
    }

    closedir(dir);
    return false;
}

std::list<std::string> listOfFiles(const std::string& path, const std::string* exts, int extsCnt, bool dirs, bool files, const std::string& prefix) {
    DIR *dir; dirent *ent; std::list<std::string> fileNames;
    if ((dir = opendir(path.c_str()))) {
        while ((ent = readdir(dir))) {
            if (ent->d_type == DT_REG && files) {
            } else if (ent->d_type == DT_DIR && dirs) {
            } else {
                continue;
            }

            std::string fileName(ent->d_name);
            if (!prefix.empty() && (fileName.length() < prefix.length() || fileName.substr(0, prefix.length()) != prefix)) {
                continue;
            }

            if (extsCnt <= 0) {
                fileNames.emplace_back(fileName);
            } else {
                for (int i = 0; i < extsCnt; i++) {
                    std::string ext = exts[i];
                    if (fileName.length() > ext.length() && fileName.substr(fileName.length() - ext.length()) == ext) {
                        fileNames.emplace_back(fileName);
                    }
                }
            }
        }
        closedir(dir);
        fileNames.sort();
    } else {
        perror(("listOfFiles(): could not open directory " + path).c_str());
    }

    return fileNames;
}

const std::string ON_RENAME_PATH = "on renamePath(): ";

bool renamePath(const std::string& oldPath, const std::string& newPath) {
    try {
        fs::rename(oldPath, newPath); // Renames and moves the file
        return true;
    } catch (const fs::filesystem_error& ex) {
        printf("%scan't rename %s to %s: %s\n", ON_RENAME_PATH.c_str(), oldPath.c_str(), newPath.c_str(), ex.what());
        return false;
    }
}

const std::string ON_ENSURE_DIRECTORY = "on ensureDirectory(): ";

bool ensureDirectory(const std::filesystem::path& path, const std::string& label, bool createIfMissing) {
    std::error_code ec;

    if (!fs::exists(path, ec)) {
        if (ec) {
            std::cerr << ON_ENSURE_DIRECTORY << "failed to check " << label << ": " << path << ": " << ec.message() << "\n";
            return false;
        }

        if (!createIfMissing) {
            std::cerr << ON_ENSURE_DIRECTORY << label << " does not exist: " << path << "\n";
            return false;
        }

        fs::create_directories(path, ec);
        if (ec) {
            std::cerr << ON_ENSURE_DIRECTORY << "failed to create " << label << ": " << path << ": " << ec.message() << "\n";
            return false;
        }
    }

    if (!fs::is_directory(path, ec) || ec) {
        std::cerr << ON_ENSURE_DIRECTORY << label << " is not a directory: " << path;
        if (ec) std::cerr << ": " << ec.message();
        std::cerr << "\n";
        return false;
    }

    return true;
}

const std::string ON_ENSURE_NOT_REGULAR_FILE = "on ensureNotRegularFile(): ";

bool ensureNotRegularFile(const std::filesystem::path& path, const std::string& label) {
    std::error_code ec;
    if (fs::exists(path, ec) && fs::is_regular_file(path, ec)) {
        std::cerr << ON_ENSURE_NOT_REGULAR_FILE << label << " must be a directory, but regular file exists: " << path << "\n";
        return false;
    }
    if (ec) {
        std::cerr << ON_ENSURE_NOT_REGULAR_FILE << "failed to inspect " << label << ": " << path << ": " << ec.message() << "\n";
        return false;
    }
    return true;
}

std::string safePathPart(std::string s) {
    for (char& c : s) {
        const bool ok = (c >= 'a' && c <= 'z') ||
                        (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') ||
                        c == '.' || c == '_' || c == '-';
        if (!ok) c = '_';
    }
    if (s.empty()) return "file";
    return s;
}

const std::string ON_SAME_FILE_SIZE = "on sameFileSize(): ";

bool sameFileSize(const std::filesystem::path& a, const std::filesystem::path& b, bool& same) {
    same = false;
    std::error_code ecA;
    std::error_code ecB;
    const auto sizeA = fs::file_size(a, ecA);
    const auto sizeB = fs::file_size(b, ecB);

    if (ecA) {
        std::cerr << ON_SAME_FILE_SIZE << "failed to read file size: " << a << ": " << ecA.message() << "\n";
        return false;
    }
    if (ecB) {
        std::cerr << ON_SAME_FILE_SIZE << "failed to read file size: " << b << ": " << ecB.message() << "\n";
        return false;
    }

    same = sizeA == sizeB;
    return true;
}

const std::string ON_REMOVE_FS_PATH = "on removeFsPath(): ";

bool removeFsPath(const std::filesystem::path& path, const std::string& reason) {
    std::error_code ec;
    if (!fs::remove(path, ec) || ec) {
        std::cerr << ON_REMOVE_FS_PATH << "failed to remove " << reason << ": " << path;
        if (ec) std::cerr << ": " << ec.message();
        std::cerr << "\n";
        return false;
    }
    return true;
}

const std::string ON_MOVE_FILE_REPLACING = "on moveFileReplacing(): ";

bool moveFileReplacing(const std::filesystem::path& srcPath, const std::filesystem::path& dstPath) {
    std::error_code ec;

    fs::rename(srcPath, dstPath, ec);
    if (!ec) return true;

    const std::string renameError = ec.message();
    ec.clear();

    fs::copy_file(srcPath, dstPath, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << ON_MOVE_FILE_REPLACING << "failed to move generated file: " << srcPath << " -> " << dstPath
                  << "; rename: " << renameError
                  << "; copy: " << ec.message() << "\n";
        return false;
    }

    fs::remove(srcPath, ec);
    if (ec) {
        std::cerr << ON_MOVE_FILE_REPLACING << "failed to remove source after copy: " << srcPath
                  << ": " << ec.message() << "\n";
        return false;
    }

    return true;
}

const std::string ON_CLEANUP_DIRECTORY = "on cleanupDirectory(): ";

bool cleanupDirectory(const std::filesystem::path& dirPath) {
    std::error_code ec;
    fs::remove_all(dirPath, ec);
    if (ec) {
        std::cerr << ON_CLEANUP_DIRECTORY << "failed to remove directory: " << dirPath
                  << ": " << ec.message() << "\n";
        return false;
    }
    return true;
}
