#ifndef JDICT_H
#define JDICT_H

#include <json/json.h>

const std::string J_KEY_DELIMITER = "::";
const std::string J_HEADER_KEY = "header";

bool jsonRead(const std::string& filepath, Json::Value& jv);
bool jsonWrite(const std::string& filepath, const Json::Value& jv);

bool jlistReadAll(const std::string& filepath, Json::Value& jvHeader, Json::Value& jvList, bool ignoreErrors = false);
bool jlistWriteAll(const std::string& filepath, const Json::Value& jvHeader, const Json::Value& jvList);
bool jlistWrite(const std::string& filepath, const Json::Value& jv, bool addNewLine = false);

// bool jdictReadKV(const std::string& filepath, const std::string& key, Json::Value& jv);

#endif //JDICT_H
