#include "lv00/representation_converter.h"
#include "lv00/func_block.h"
#include <stdlib.h>
#include <string.h>

/* 节点图内部结构：将 FuncBlock 映射为节点和边 */
typedef struct {
    int id;              /* 节点ID（对应 FuncBlock id） */
    char *name;          /* 节点名称 */
    int *input_ports;    /* 输入端口ID数组 */
    int input_count;     /* 输入端口数量 */
    int *output_ports;   /* 输出端口ID数组 */
    int output_count;    /* 输出端口数量 */
} NodeGraphNode;

typedef struct {
    NodeGraphNode *nodes;  /* 节点数组 */
    int node_count;         /* 节点数量 */
    int node_cap;           /* 节点数组容量 */

    /* 边：src_node.output_port -> dst_node.input_port */
    struct {
        int src_node;
        int src_port;
        int dst_node;
        int dst_port;
    } *edges;
    int edge_count;
    int edge_cap;
} NodeGraph;

/* 将函数块转换为节点图表示 */
/* 每个 FuncBlock → 一个 NodeGraphNode */
/* 块的输入/输出端口 → 节点的输入/输出端口 */
/* 块间的连接 → 节点间的边 */
Lv00ConvertResult lv00_convert_block_to_node(void *block) {
    Lv00ConvertResult result = {0};
    if (!block) {
        result.success = 0;
        strncpy(result.error_msg, "NULL block", sizeof(result.error_msg));
        return result;
    }

    /* block 指向 FuncBlock 数组结构 */
    typedef struct {
        FuncBlock **blocks;
        int count;
    } BlockGraphView;

    BlockGraphView *bg = (BlockGraphView *)block;

    /* 创建节点图 */
    NodeGraph *ng = calloc(1, sizeof(NodeGraph));
    if (!ng) {
        result.success = 0;
        strncpy(result.error_msg, "out of memory", sizeof(result.error_msg));
        return result;
    }
    ng->node_cap = bg->count > 0 ? bg->count : 8;
    ng->nodes = calloc(ng->node_cap, sizeof(NodeGraphNode));
    ng->edge_cap = 32;
    ng->edges = calloc(ng->edge_cap, sizeof(ng->edges[0]));

    /* 为每个 FuncBlock 创建对应节点 */
    for (int i = 0; i < bg->count; i++) {
        FuncBlock *fb = bg->blocks[i];
        if (!fb) continue;

        NodeGraphNode *node = &ng->nodes[ng->node_count++];
        node->id = func_block_get_id(fb);

        /* 复制名称 */
        const char *name = func_block_get_name(fb);
        if (name) {
            node->name = strdup(name);
        } else {
            node->name = strdup("unnamed");
        }

        /* 映射输入端口 */
        int in_count = func_block_get_input_count(fb);
        if (in_count > 0 && fb->input_port_ids) {
            node->input_ports = calloc(in_count, sizeof(int));
            memcpy(node->input_ports, fb->input_port_ids, in_count * sizeof(int));
            node->input_count = in_count;
        }

        /* 映射输出端口 */
        int out_count = func_block_get_output_count(fb);
        if (out_count > 0 && fb->output_port_ids) {
            node->output_ports = calloc(out_count, sizeof(int));
            memcpy(node->output_ports, fb->output_port_ids, out_count * sizeof(int));
            node->output_count = out_count;
        }
    }

    /* 根据端口依赖关系生成边 */
    /* 遍历所有块的端口依赖，建立节点间的连接 */
    for (int i = 0; i < bg->count; i++) {
        FuncBlock *fb = bg->blocks[i];
        if (!fb || fb->port_dep_count <= 0 || !fb->port_deps) continue;

        for (int j = 0; j < fb->port_dep_count; j++) {
            PortDependency *dep = &fb->port_deps[j];

            /* 查找源节点和目标节点在节点图中的索引 */
            int src_idx = -1, dst_idx = -1;
            for (int k = 0; k < ng->node_count; k++) {
                if (ng->nodes[k].id == dep->external_node_id) src_idx = k;
                if (ng->nodes[k].id == fb->id) dst_idx = k;
            }

            if (src_idx >= 0 && dst_idx >= 0) {
                /* 扩容检查 */
                if (ng->edge_count >= ng->edge_cap) {
                    ng->edge_cap *= 2;
                    ng->edges = realloc(ng->edges, ng->edge_cap * sizeof(ng->edges[0]));
                }
                ng->edges[ng->edge_count].src_node = src_idx;
                ng->edges[ng->edge_count].src_port = dep->port_id;
                ng->edges[ng->edge_count].dst_node = dst_idx;
                ng->edges[ng->edge_count].dst_port = dep->internal_node_id;
                ng->edge_count++;
            }
        }
    }

    result.output = ng;
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
    /* 反向转换暂未实现，保留接口 */
    result.success = 1;
    return result;
}
