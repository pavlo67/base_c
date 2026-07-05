#include <cstdio>
#include <cmath>

#include <json/json.h>
// https://en.wikibooks.org/wiki/JsonCpp

int main() {

    Json::Value root;
    root["subtree"] = 1;
    root["NaN"] = NAN;

    printf("root: '%s'\n", root.toStyledString().c_str());

    const char* root_str1 = root.toStyledString().c_str();
    printf("root string: '%s'\n", root_str1);

    // Json::FastWriter fast;
    // std::string sFast = fast.write(root);
    // std::cout << "Fast:\n" << sFast << "Styled:\n" << sStyled;

    Json::StyledWriter styled;
    std::string sStyled = styled.write(root);

    printf("root styled: '%s'\n", sStyled.c_str());

    const char* root_str2 = sStyled.c_str();
    printf("root styled string: '%s'\n", root_str2);

    Json::Value root_readed;
    Json::Reader reader;
    bool b = reader.parse(root_str2, root_readed);
    if (!b) {
        printf(reader.getFormattedErrorMessages().c_str());
    } else {
        printf("root readed: %s\n", root_readed.toStyledString().c_str());
    }

    return EXIT_SUCCESS;
}
