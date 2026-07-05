#ifndef BASE_CPP_INFO_H
#define BASE_CPP_INFO_H

#include <string>

using namespace std;

struct Info {
    string error;

    [[nodiscard]] bool ok() const {
        return error.empty();
    }

    void setOk() {
        error.clear();
    }

    void setError(string_view msg) {
        error = msg;
    }
};

#endif //BASE_CPP_INFO_H
