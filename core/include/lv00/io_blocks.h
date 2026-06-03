#ifndef LV00_IO_BLOCKS_H
#define LV00_IO_BLOCKS_H

#include "lv00/func_block.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Effect types */
typedef enum {
    LV00_EFFECT_PURE,
    LV00_EFFECT_FILE_READ,
    LV00_EFFECT_FILE_WRITE,
    LV00_EFFECT_NETWORK,
    LV00_EFFECT_UI_RENDER,
    LV00_EFFECT_UI_INPUT,
    LV00_EFFECT_RANDOM,
    LV00_EFFECT_TIME
} Lv00EffectType;

/* IO status */
typedef enum {
    LV00_IO_SUCCESS,
    LV00_IO_ERROR_NOT_FOUND,
    LV00_IO_ERROR_PERMISSION,
    LV00_IO_ERROR_NETWORK,
    LV00_IO_ERROR_TIMEOUT,
    LV00_IO_ERROR_UNKNOWN
} Lv00IOStatus;

/* File block */
typedef struct Lv00FileBlock {
    void *base;
    Lv00EffectType effect;

    int path_port;
    int data_port;
    int result_port;
    int status_port;
} Lv00FileBlock;

/* Network block */
typedef struct Lv00NetworkBlock {
    void *base;
    Lv00EffectType effect;

    int url_port;
    int request_port;
    int response_port;
    int status_port;
} Lv00NetworkBlock;

/* UI event block */
typedef struct Lv00UIEventBlock {
    void *base;
    Lv00EffectType effect;

    int event_port;
    int action_port;
} Lv00UIEventBlock;

/* Factory */
Lv00FileBlock *lv00_file_block_create(Lv00EffectType effect);
void lv00_file_block_destroy(Lv00FileBlock *block);

Lv00NetworkBlock *lv00_network_block_create(void);
void lv00_network_block_destroy(Lv00NetworkBlock *block);

Lv00UIEventBlock *lv00_ui_event_block_create(Lv00EffectType effect);
void lv00_ui_event_block_destroy(Lv00UIEventBlock *block);

#ifdef __cplusplus
}
#endif

#endif /* LV00_IO_BLOCKS_H */
