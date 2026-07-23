#ifndef lv_IO_BLOCKS_H
#define lv_IO_BLOCKS_H

#include "lv/func_block.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Effect types */
typedef enum {
    lv_EFFECT_PURE,
    lv_EFFECT_FILE_READ,
    lv_EFFECT_FILE_WRITE,
    lv_EFFECT_NETWORK,
    lv_EFFECT_UI_RENDER,
    lv_EFFECT_UI_INPUT,
    lv_EFFECT_RANDOM,
    lv_EFFECT_TIME
} lvEffectType;

/* IO status */
typedef enum {
    lv_IO_SUCCESS,
    lv_IO_ERROR_NOT_FOUND,
    lv_IO_ERROR_PERMISSION,
    lv_IO_ERROR_NETWORK,
    lv_IO_ERROR_TIMEOUT,
    lv_IO_ERROR_UNKNOWN
} lvIOStatus;

/* File block */
typedef struct lvFileBlock {
    void *base;
    lvEffectType effect;

    int path_port;
    int data_port;
    int result_port;
    int status_port;
} lvFileBlock;

/* Network block */
typedef struct lvNetworkBlock {
    void *base;
    lvEffectType effect;

    int url_port;
    int request_port;
    int response_port;
    int status_port;
} lvNetworkBlock;

/* UI event block */
typedef struct lvUIEventBlock {
    void *base;
    lvEffectType effect;

    int event_port;
    int action_port;
} lvUIEventBlock;

/* Factory */
lvFileBlock *lv_file_block_create(lvEffectType effect);
void lv_file_block_destroy(lvFileBlock *block);

lvNetworkBlock *lv_network_block_create(void);
void lv_network_block_destroy(lvNetworkBlock *block);

lvUIEventBlock *lv_ui_event_block_create(lvEffectType effect);
void lv_ui_event_block_destroy(lvUIEventBlock *block);

#ifdef __cplusplus
}
#endif

#endif /* lv_IO_BLOCKS_H */
