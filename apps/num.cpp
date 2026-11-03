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

static bool findReferenceNumbering(const fs::path& directory, uint64_t& firstNumber, unsigned& numberWidth);
static bool prepareRenameEntries(const fs::path& directory, const std::vector<fs::path>& files, uint64_t firstNumber,
    unsigned numberWidth, std::vector<RenameEntry>& entries);
static bool renameEntries(std::vector<RenameEntry>& entries);

int main(int argc, char* argv[]) {
    if (argc > 2) {
        printf("ERROR: usage: %s [FIRST_NUMBER|SOURCE_DIRECTORY]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const fs::path directory = fs::current_path();
    fs::path sourceDirectory;
    uint64_t firstNumber = 1;
    unsigned numberWidth = 0;
    if (argc == 2) {
        if (!parseUnsigned(argv[1], firstNumber)) {
            sourceDirectory = argv[1];
            if (!findReferenceNumbering(sourceDirectory, firstNumber, numberWidth)) {
                return EXIT_FAILURE;
            }
        }
    }

    std::vector<fs::path> files;
    if (!collectFiles(directory, files)) {
        return EXIT_FAILURE;
    }
    if (!files.empty() && firstNumber > UINT64_MAX - (files.size() - 1)) {
        printf("ERROR: file number range overflows\n");
        return EXIT_FAILURE;
    }
    const uint64_t lastNumber = files.empty() ? firstNumber : firstNumber + files.size() - 1;
    numberWidth = numberWidth ? std::max(numberWidth, static_cast<unsigned>(std::to_string(lastNumber).size())) : static_cast<unsigned>(std::to_string(lastNumber).size()) + 2;
    printf("Parameters:\n\tTARGET_DIRECTORY: %s\n\tSOURCE_DIRECTORY: %s\n\tFIRST_NUMBER: %llu\n\tNUMBER_WIDTH: %u\n\n",
        directory.c_str(), sourceDirectory.empty() ? "(none)" : sourceDirectory.c_str(),
        static_cast<unsigned long long>(firstNumber), numberWidth);

    std::vector<RenameEntry> entries;
    if (!prepareRenameEntries(directory, files, firstNumber, numberWidth, entries)) {
        return EXIT_FAILURE;
    }
    if (!renameEntries(entries)) {
        return EXIT_FAILURE;
    }
    printf("Renamed files: %zu\n", entries.size());
    return EXIT_SUCCESS;
}

static bool findReferenceNumbering(const fs::path& directory, uint64_t& firstNumber, unsigned& numberWidth) {
    std::vector<fs::path> files;
    if (!collectFiles(directory, files)) {
        return false;
    }
    uint64_t maximumNumber = 0;
    bool numberFound = false;
    for (const fs::path& file : files) {
        uint64_t number = 0;
        unsigned width = 0;
        if (!parsePathNumberPrefix(file, number, width)) {
            continue;
        }
        maximumNumber = std::max(maximumNumber, number);
        numberWidth = std::max(numberWidth, width);
        numberFound = true;
    }
    if (!numberFound) {
        firstNumber = 1;
        return true;
    }
    if (maximumNumber == UINT64_MAX) {
        printf("[findReferenceNumbering()] ERROR: maximum file number cannot be incremented in %s\n", directory.c_str());
        return false;
    }
    firstNumber = maximumNumber + 1;
    return true;
}

static bool prepareRenameEntries(const fs::path& directory, const std::vector<fs::path>& files, uint64_t firstNumber,
                                 unsigned numberWidth, std::vector<RenameEntry>& entries) {
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
        entry.target = directory / makeNumberedName(number, files[index].extension().string(), numberWidth);
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
