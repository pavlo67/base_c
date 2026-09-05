#ifndef BASE_OS_H
#define BASE_OS_H

#include <string>

bool rebootOS(std::string& error);
bool shutdownOS(std::string& error);

#endif // BASE_OS_H
