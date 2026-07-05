#include "execlib.h"

#include <iostream>
#include <memory>
#include <array>

#include "strlib.h"

bool exec(const std::string& cmd, std::string* result) {
    std::array<char, 128> buffer = {};

    // Use _popen on Windows, popen on Linux/Mac
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);

    if (!pipe) {
        fprintf(stderr, "execute(): can't open pipe for '%s'", cmd.c_str());
        return false;
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        if (result != nullptr) {
            *result += buffer.data();
        } else {
            auto b = std::string(buffer.data());
            trim(b);
            if (!b.empty()) {
                printf("%s\n", b.c_str());
            }
        }
    }

    return true;
}

// TODO: getting stderr in separate descriptor
// #include <unistd.h>
// #include <sys/wait.h>
// #include <iostream>
// #include <vector>
// #include <cstring>
//
// int main() {
//     int stdout_pipe[2];
//     int stderr_pipe[2];
//
//     pipe(stdout_pipe);
//     pipe(stderr_pipe);
//
//     pid_t pid = fork();
//
//     if (pid == 0) {
//         // Child process
//         dup2(stdout_pipe[1], STDOUT_FILENO);
//         dup2(stderr_pipe[1], STDERR_FILENO);
//
//         close(stdout_pipe[0]);
//         close(stdout_pipe[1]);
//         close(stderr_pipe[0]);
//         close(stderr_pipe[1]);
//
//         execl("/bin/sh", "sh", "-c", "ls -l /nonexistent", (char*)nullptr);
//         _exit(127); // Якщо exec не вдався
//     } else if (pid > 0) {
//         // Parent process
//         close(stdout_pipe[1]);
//         close(stderr_pipe[1]);
//
//         char buffer[256];
//         ssize_t count;
//
//         std::cout << "STDOUT:\n";
//         while ((count = read(stdout_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
//             buffer[count] = '\0';
//             std::cout << buffer;
//         }
//
//         std::cout << "STDERR:\n";
//         while ((count = read(stderr_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
//             buffer[count] = '\0';
//             std::cout << buffer;
//         }
//
//         close(stdout_pipe[0]);
//         close(stderr_pipe[0]);
//
//         int status;
//         waitpid(pid, &status, 0);
//         std::cout << "Exit code: " << WEXITSTATUS(status) << std::endl;
//     } else {
//         perror("fork");
//         return 1;
//     }
//
//     return 0;
// }