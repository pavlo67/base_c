#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "lib/filelib.h"
#include "lib/parselib.h"

namespace fs = std::filesystem;

struct RenameEntry {
    fs::path source;
    fs::path temporary;
    fs::path target;
};

static bool findNextFileNumber(const fs::path& directory, uint64_t& firstNumber);
static bool prepareRenameEntries(const fs::path& directory, const std::vector<fs::path>& files, uint64_t firstNumber, std::vector<RenameEntry>& entries);
static bool renameEntries(std::vector<RenameEntry>& entries);

int main(int argc, char* argv[]) {
    if (argc > 2) {
        printf("ERROR: expected at most one parameter\n");
        return EXIT_FAILURE;
    }

    fs::path directory = fs::current_path();
    uint64_t firstNumber = 1;
    if (argc == 2) {
        if (!parseUnsigned(argv[1], firstNumber)) {
            directory = argv[1];
            if (!findNextFileNumber(directory, firstNumber)) {
                return EXIT_FAILURE;
            }
        }
    }
    printf("Parameters:\n\tDIRECTORY: %s\n\tFIRST_NUMBER: %llu\n\n", directory.c_str(),
           static_cast<unsigned long long>(firstNumber));

    std::vector<fs::path> files;
    if (!collectFiles(directory, files)) {
        return EXIT_FAILURE;
    }
    std::vector<RenameEntry> entries;
    if (!prepareRenameEntries(directory, files, firstNumber, entries)) {
        return EXIT_FAILURE;
    }
    if (!renameEntries(entries)) {
        return EXIT_FAILURE;
    }
    printf("Renamed files: %zu\n", entries.size());
    return EXIT_SUCCESS;
}

static bool findNextFileNumber(const fs::path& directory, uint64_t& firstNumber) {
    std::vector<fs::path> files;
    if (!collectFiles(directory, files)) {
        return false;
    }
    uint64_t maximumNumber = 0;
    bool numberFound = false;
    for (const fs::path& file : files) {
        const std::string name = file.filename().string();
        size_t digitsEnd = 0;
        while (digitsEnd < name.size() && name[digitsEnd] >= '0' && name[digitsEnd] <= '9') {
            ++digitsEnd;
        }
        if (digitsEnd == 0) {
            continue;
        }
        uint64_t number = 0;
        if (!parseUnsigned(name.substr(0, digitsEnd), number)) {
            continue;
        }
        maximumNumber = std::max(maximumNumber, number);
        numberFound = true;
    }
    if (!numberFound) {
        firstNumber = 1;
        return true;
    }
    if (maximumNumber == UINT64_MAX) {
        printf("[findNextFileNumber()] ERROR: maximum file number cannot be incremented in %s\n", directory.c_str());
        return false;
    }
    firstNumber = maximumNumber + 1;
    return true;
}

static bool prepareRenameEntries(const fs::path& directory, const std::vector<fs::path>& files, uint64_t firstNumber,
                                 std::vector<RenameEntry>& entries) {
    const uint64_t timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    uint64_t number = firstNumber;
    for (size_t index = 0; index < files.size(); ++index) {
        if (number == UINT64_MAX && index + 1 < files.size()) {
            printf("[prepareRenameEntries()] ERROR: file number overflow\n");
            return false;
        }
        RenameEntry entry;
        entry.source = files[index];
        entry.temporary = directory / (".num_" + std::to_string(timestamp) + "_" + std::to_string(index));
        entry.target = directory / (std::to_string(number) + files[index].extension().string());
        for (const RenameEntry& other : entries) {
            if (entry.target == other.target) {
                printf("[prepareRenameEntries()] ERROR: duplicate target %s\n", entry.target.c_str());
                return false;
            }
        }
        entries.push_back(std::move(entry));
        ++number;
    }
    return true;
}

static bool renameEntries(std::vector<RenameEntry>& entries) {
    std::error_code error;
    for (const RenameEntry& entry : entries) {
        const bool targetIsSource = std::any_of(entries.begin(), entries.end(), [&entry](const RenameEntry& other) {
            return entry.target == other.source;
        });
        if (!targetIsSource && fs::exists(entry.target, error)) {
            printf("[renameEntries()] ERROR: target already exists: %s\n", entry.target.c_str());
            return false;
        }
        if (error) {
            printf("[renameEntries()] ERROR: cannot inspect target %s: %s\n", entry.target.c_str(),
                   error.message().c_str());
            return false;
        }
    }
    for (const RenameEntry& entry : entries) {
        fs::rename(entry.source, entry.temporary, error);
        if (error) {
            printf("[renameEntries()] ERROR: cannot rename %s to %s: %s\n", entry.source.c_str(),
                   entry.temporary.c_str(), error.message().c_str());
            return false;
        }
    }
    for (const RenameEntry& entry : entries) {
        fs::rename(entry.temporary, entry.target, error);
        if (error) {
            printf("[renameEntries()] ERROR: cannot rename %s to %s: %s\n", entry.temporary.c_str(),
                   entry.target.c_str(), error.message().c_str());
            return false;
        }
    }
    return true;
}
