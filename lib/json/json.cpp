#include "../filelib.h"

#include "json.h"

#include <filesystem>

const std::string ON_JLIST_WRITE = "on jlistWrite(): ";
bool jlistWrite(const std::string& filepath, const Json::Value& jv, bool addNewLine) {
    Json::FastWriter writer;

    FILE *fptr = fopen(filepath.c_str(), "a");
    if (fptr == nullptr) {
        printf("ERROR: %scan't open %s for appending", ON_JLIST_WRITE.c_str(), filepath.c_str());
        return false;
    }

    int cnt = fprintf(fptr, "%s%s", (addNewLine ? "\n" : ""), writer.write(jv).c_str());
    fclose(fptr);

    return cnt > 0 ;
}

const std::string ON_JLIST_WRITE_ALL = "on jlistWriteAll(): ";
bool jlistWriteAll(const std::string& filepath, const Json::Value& jvHeader, const Json::Value& jvList) {
    Json::FastWriter writer;

    FILE *fptr = fopen(filepath.c_str(), "w");
    if (fptr == nullptr) {
        printf("ERROR: %scan't open %s for writing", ON_JLIST_WRITE_ALL.c_str(), filepath.c_str());
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


const std::string ON_JLIST_READ_ALL = "on jlistReadAll(): ";
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
                printf("ERROR: %sheader line duplicate is omitted / %s", ON_JLIST_READ_ALL.c_str(), line.c_str());
            } else if (reader.parse(line.substr(pos + J_KEY_DELIMITER.length()), jvHeader)) {
                headerOk = true;
            } else {
                printf("ERROR: %sheader line is wrong / %s", ON_JLIST_READ_ALL.c_str(), line.c_str());
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
                printf("ERROR: %scan't parse jlist line #%d (of %lu total lines): '%s'\n\n", ON_JLIST_READ_ALL.c_str(), i, lines.size(), line.c_str());
                if (!ignoreErrors) {
                    return false;
                }

            }
        }
    }

    return true;
}


const std::string ON_JSON_READ = "on jsonRead(): ";
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

    printf("ERROR: %scan't read json (%s) from %s\n", ON_JSON_READ.c_str(), json.c_str(), filepath.c_str());
    return false;
}

const std::string ON_JSON_WRITE = "on jsonWrite(): ";
bool jsonWrite(const std::string& filepath, const Json::Value& jv) {
    Json::FastWriter writer;
    auto jsonStr = writer.write(jv);
    FILE *fptr = fopen(filepath.c_str(), "w");
    if (fptr == nullptr) {
        printf("ERROR: %scan't create file: %s\n", ON_JSON_WRITE.c_str(), filepath.c_str());
        return false;
    }

    if (fprintf(fptr, "%s", jsonStr.c_str()) < 1) {
        printf("ERROR: %scan't write (%s) into file: %s\n", ON_JSON_WRITE.c_str(), jsonStr.c_str(), filepath.c_str());
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
