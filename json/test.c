#include "cjson.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main()
{
    cJSON *root = cJSON_CreateObject();
    
    cJSON *pJsonArray = cJSON_CreateArray();

    cJSON *pJsonRoot = cJSON_CreateObject();
    cJSON_AddStringToObject(pJsonRoot, "name", "jack");
    cJSON_AddNumberToObject(pJsonRoot, "id", 12345);

    cJSON_AddItemToArray(pJsonArray, pJsonRoot);

    cJSON_AddItemToObject(root, "details", pJsonArray);

    char *out = cJSON_PrintUnformatted(root);

    printf("%s\n", out);

    cJSON_Delete(root);
    free(out);

    return 0;
}
