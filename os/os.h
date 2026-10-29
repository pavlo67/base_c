#ifndef BASE_OS_H
#define BASE_OS_H

#include <filesystem>
#include <string>

bool executableDirectory(std::filesystem::path& directory, std::string& error);
bool rebootOS(std::string& error);
bool shutdownOS(std::string& error);

#endif // BASE_OS_H
