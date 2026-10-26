#include "config.h"

#include "lib/filelib.h"

#include <filesystem>
#include <sstream>
#include <utility>

namespace {
    std::string trimYamlToken(const std::string& value) {
        const size_t begin = value.find_first_not_of(" \t");
        if (begin == std::string::npos) { return ""; }
        const size_t end = value.find_last_not_of(" \t");
        return value.substr(begin, end - begin + 1);
    }
}

bool replaceYamlSectionScalar(const std::string& text, const std::string& section, const std::string& key,
        const std::string& value, std::string& updated, std::string& error) {
    error.clear();
    updated.clear();
    if (section.empty() || key.empty() || value.empty() ||
            section.find_first_of("\r\n:") != std::string::npos ||
            key.find_first_of("\r\n:") != std::string::npos ||
            value.find_first_of("\r\n") != std::string::npos) {
        error = "section, key or value is invalid";
        return false;
    }

    std::istringstream input(text);
    std::ostringstream output;
    std::string line;
    bool inSection = false;
    bool sectionFound = false;
    bool keyFound = false;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') { line.pop_back(); }
        const size_t first = line.find_first_not_of(" \t");
        if (first != std::string::npos && line[first] != '#') {
            const size_t colon = line.find(':', first);
            if (first == 0) {
                const std::string candidate = colon == std::string::npos ? "" :
                    trimYamlToken(line.substr(0, colon));
                inSection = candidate == section;
                if (inSection) {
                    if (sectionFound) {
                        error = "duplicate YAML section: " + section;
                        return false;
                    }
                    sectionFound = true;
                }
            } else if (inSection && colon != std::string::npos &&
                    trimYamlToken(line.substr(first, colon - first)) == key) {
                if (keyFound) {
                    error = "duplicate YAML key: " + section + "." + key;
                    return false;
                }
                size_t scalarBegin = colon + 1;
                while (scalarBegin < line.size() &&
                        (line[scalarBegin] == ' ' || line[scalarBegin] == '\t')) {
                    ++scalarBegin;
                }
                const size_t comment = line.find('#', scalarBegin);
                size_t scalarEnd = comment == std::string::npos ? line.size() : comment;
                while (scalarEnd > scalarBegin &&
                        (line[scalarEnd - 1] == ' ' || line[scalarEnd - 1] == '\t')) {
                    --scalarEnd;
                }
                if (scalarBegin == scalarEnd) {
                    error = "YAML scalar is empty: " + section + "." + key;
                    return false;
                }
                line.replace(scalarBegin, scalarEnd - scalarBegin, value);
                keyFound = true;
            }
        }
        output << line << '\n';
    }
    if (!sectionFound || !keyFound) {
        error = !sectionFound ? "YAML section is missing: " + section :
            "YAML key is missing: " + section + "." + key;
        return false;
    }
    updated = output.str();
    return true;
}

bool updateYamlScalarFiles(const std::string& configPath, const std::string& examplePath,
        const std::string& section, const std::string& key, const std::string& value, std::string& error) {
    error.clear();
    try {
        std::string configText;
        if (!readFile(configPath, configText)) {
            error = "cannot read configuration: " + configPath;
            return false;
        }
        std::string updatedConfig;
        if (!replaceYamlSectionScalar(configText, section, key, value, updatedConfig, error)) {
            return false;
        }
        const YAML::Node verified = YAML::Load(updatedConfig);
        if (!verified[section][key].IsScalar()) {
            error = "updated configuration scalar is unavailable: " + section + "." + key;
            return false;
        }

        std::string updatedExample;
        std::error_code fsError;
        const bool exampleExists = std::filesystem::exists(examplePath, fsError);
        if (fsError) {
            error = "cannot inspect example configuration: " + fsError.message();
            return false;
        }
        if (exampleExists) {
            std::string exampleText;
            if (!readFile(examplePath, exampleText) ||
                    !replaceYamlSectionScalar(exampleText, section, key, value, updatedExample, error)) {
                if (error.empty()) { error = "cannot update example configuration"; }
                return false;
            }
            const YAML::Node verifiedExample = YAML::Load(updatedExample);
            if (!verifiedExample[section][key].IsScalar()) {
                error = "updated example scalar is unavailable: " + section + "." + key;
                return false;
            }
        }

        std::filesystem::path configBackup;
        if (!writeFileWithTimestampBackup(configPath, updatedConfig, configBackup, error)) {
            return false;
        }
        if (exampleExists) {
            std::filesystem::path exampleBackup;
            if (!writeFileWithTimestampBackup(examplePath, updatedExample, exampleBackup, error)) {
                if (!copyFileReplacing(configBackup, configPath)) {
                    error += "; configuration rollback failed";
                }
                return false;
            }
        }
    } catch (const YAML::Exception& failure) {
        error = std::string("invalid YAML configuration: ") + failure.what();
        return false;
    } catch (const std::exception& failure) {
        error = failure.what();
        return false;
    }
    return true;
}

Config::Config(const std::string& filePath) {
    try {
        root_ = YAML::LoadFile(filePath);
        loaded_ = true;
    } catch (const std::exception& e) {
        printf("[Config::Config()] ERROR: %s: %s\n", filePath.c_str(), e.what());
        root_ = {};
        loaded_ = false;
    }
}

Config::Config(YAML::Node root) : root_(std::move(root)), loaded_(root_.IsDefined()) {}


