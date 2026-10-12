#include <cstdint>
#include <cstdio>
#include <string>

#include <json/json.h>



int main(int argc, char *argv[]) {

    Json::Value jsonArray;

    jsonArray.append(1);
    jsonArray.append(2);

    Json::FastWriter writer;
    printf("%s\n", writer.write(jsonArray).c_str()); ;

    return 0;

}
