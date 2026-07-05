#include <cstdio>
#include <json/json.h>

#include "../json.h"

int main() {

    std::string filename1 = "1.jlist";
    std::string filename2 = "2.jlist";
    bool ok;

    Json::Value item1;
    item1["subtree"] = 1;
    item1["aaa"] = "qwer";
    ok = jlistWrite(filename1, item1);
    printf("jlistWrite(filename1, item1): %d\n", ok);

    Json::Value item2;
    item2["subtree"] = 2;
    item2["aaa"] = "werr";
    ok = jlistWrite(filename1, item2);
    printf("jlistWrite(filename1, item2): %d\n", ok);

    Json::Value jvList, jvHeader;
    ok = jlistReadAll(filename1, jvHeader, jvList);
    printf("jlistReadAll(filename1, jvHeader, jvList): %d\n", ok);

    ok = jlistWriteAll(filename2, jvHeader, jvList);
    printf("jlistWriteAll(filename2, jvHeader, jvList): %d\n", ok);

    return EXIT_SUCCESS;
}
