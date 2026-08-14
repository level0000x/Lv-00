#ifndef INTEROP_COMMAND_INTERNAL_H
#define INTEROP_COMMAND_INTERNAL_H

#include "lv/engine.h"   /* lvEngine */
#include "lv/interop.h"  /* InteropCommand / InteropResponse */
#include "lv/lv_json.h"  /* lvJsonBuf */

/* 命令处理函数类型（VTable 分发与 export/stream 子模块共用） */
typedef int (*InteropCmdHandler)(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);

/* 定义在 interop_command_export.c（ExportGraph 命令族） */
int handle_cmd_export_graph(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);

/* 定义在 interop_command_stream.c（Stream 命令族） */
int handle_cmd_stream_start(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);
int handle_cmd_stream_stop(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);
int handle_cmd_stream_filter(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);
int handle_cmd_stream_stats(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);
int handle_cmd_stream_flush(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp);

/* 定义在 interop_command.c（核心文件）：展示型 JSON 响应初始化（stream 子模块复用） */
void interop_resp_json_init(lvJsonBuf *w, size_t cap);

#endif /* INTEROP_COMMAND_INTERNAL_H */
