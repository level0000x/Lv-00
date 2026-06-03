#include "lv00/representation_converter.h"
#include <stdlib.h>
#include <string.h>

Lv00ConvertResult lv00_convert_block_to_node(void *block) {
    Lv00ConvertResult result = {0};
    if (!block) {
        result.success = 0;
        strncpy(result.error_msg, "NULL block", sizeof(result.error_msg));
        return result;
    }
    /* TODO: create node graph representation */
    result.success = 1;
    return result;
}

Lv00ConvertResult lv00_convert_node_to_block(void *node) {
    Lv00ConvertResult result = {0};
    if (!node) {
        result.success = 0;
        strncpy(result.error_msg, "NULL node", sizeof(result.error_msg));
        return result;
    }
    result.success = 1;
    return result;
}
