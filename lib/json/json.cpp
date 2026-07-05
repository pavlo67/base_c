#include "../filelib.h"

#include "json.h"

#include <filesystem>

bool jlistWrite(const std::string& filepath, const Json::Value& jv, bool addNewLine) {
    Json::FastWriter writer;

    FILE *fptr = fopen(filepath.c_str(), "a");
    if (fptr == nullptr) {
        printf("jlistWrite(): can't open %s for appending", filepath.c_str());
        return false;
    }

    int cnt = fprintf(fptr, "%s%s", (addNewLine ? "\n" : ""), writer.write(jv).c_str());
    fclose(fptr);

    return cnt > 0 ;
}

bool jlistWriteAll(const std::string& filepath, const Json::Value& jvHeader, const Json::Value& jvList) {
    Json::FastWriter writer;

    FILE *fptr = fopen(filepath.c_str(), "w");
    if (fptr == nullptr) {
        printf("jlistWriteAll(): can't open %s for writing", filepath.c_str());
        return false;
    }


    if (!jvHeader.empty()) {
        auto headerStr = writer.write(jvHeader);
        if (!headerStr.empty() && fprintf(fptr, "%s",  (J_HEADER_KEY + J_KEY_DELIMITER + headerStr).c_str()) < 1) {
            return false;
        }
    }

    for (int i = 0; i < jvList.size(); i++) {
        if (fprintf(fptr, "%s", writer.write(jvList[i]).c_str()) < 1) {
            return false;
        }
    }
    fclose(fptr);

    return true;
}


bool jlistReadAll(const std::string& filepath, Json::Value& jvHeader, Json::Value& jvList, bool ignoreErrors) {
    if (!std::filesystem::exists(filepath)) {
        return false;
    }

    Json::Reader reader;
    jvHeader.clear();
    jvList.clear();

    bool headerOk = false;

    std::vector<std::string> lines;
    if (!readFileByLines(filepath, lines)) {
        return false;
    }

    for (int i = 0; i < lines.size(); i++) {
        auto& line = lines[i];
        auto pos = line.find(J_KEY_DELIMITER);
        if (pos == J_HEADER_KEY.length() && line.substr(0, pos) == J_HEADER_KEY) {
            if (headerOk) {
                printf("jlistRead(): header line duplicate is omitted / %s", line.c_str());
            } else if (reader.parse(line.substr(pos + J_KEY_DELIMITER.length()), jvHeader)) {
                headerOk = true;
            } else {
                printf("jlistRead(): header line is wrong / %s", line.c_str());
                if (!ignoreErrors) {
                    return false;
                }
            }
        } else {
            Json::Value jv;
            if (reader.parse(line, jv)) {
                jvList.append(jv);
                // printf("parsed: %s --> jv_list.size(): %d\n", line.c_str(), jv_list.size());

            } else if (!line.empty()) {
                printf("\ncan't parse jlist line #%d (of %lu total lines): '%s'\n\n", i, lines.size(), line.c_str());
                if (!ignoreErrors) {
                    return false;
                }

            }
        }
    }

    return true;
}


bool jsonRead(const std::string& filepath, Json::Value& jv) {
    if (!std::filesystem::exists(filepath)) {
        return false;
    }

    Json::Reader reader;
    jv.clear();

    std::string json; readFile(filepath,json);
    if (reader.parse(json, jv)) {
        return true;
    }

    printf("jsonRead(): can't read json (%s) from %s\n", json.c_str(), filepath.c_str());
    return false;
}

bool jsonWrite(const std::string& filepath, const Json::Value& jv) {
    Json::FastWriter writer;
    auto jsonStr = writer.write(jv);
    FILE *fptr = fopen(filepath.c_str(), "w");
    if (fptr == nullptr) {
        printf("jsonWrite(): can't create file: %s\n", filepath.c_str());
        return false;
    }

    if (fprintf(fptr, "%s", jsonStr.c_str()) < 1) {
        printf("jsonWrite(): can't write (%s) into file: %s\n", jsonStr.c_str(), filepath.c_str());
        fclose(fptr);
        return false;
    }
    fclose(fptr);

    return true;
}




// bool jdictReadKV(const std::string& filepath, const std::string& key, Json::Value& jv) {
//     if (!std::filesystem::exists(filepath)) {
//         return false;
//     }
//
//     Json::Reader reader;
//     jv.clear();
//
//     std::vector<std::string> lines;
//     readFileByLines(filepath, lines);
//
//     for (const auto& line : lines) {
//         auto pos = line.find(J_KEY_DELIMITER);
//         if (pos == key.length() && line.substr(0, pos) == key) {
//             if (reader.parse(line.substr(pos + J_KEY_DELIMITER.length()), jv)) {
//                 return true;
//             }
//             printf(("jdictReadKV() for key '%s' in %s: " + reader.getFormattedErrorMessages()).c_str(), key.c_str(), filepath.c_str());
//         }
//     }
//
//     printf("jdictReadKV(): no data for key '%s' in %s\n", key.c_str(), filepath.c_str());
//     return false;
// }
//
