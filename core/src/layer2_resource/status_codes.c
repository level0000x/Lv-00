#include "lv00/lv00.h"
#include "lv00/status_codes.h"

int lv00_status_is_success(int code)
{
    return code == 0 ? 1 : 0;
}

int lv00_status_is_error(int code)
{
    return code != 0 ? 1 : 0;
}

const char *lv00_status_message(int code)
{
    switch (code) {
        case 0:  return "OK";
        default: return "Error";
    }
}
