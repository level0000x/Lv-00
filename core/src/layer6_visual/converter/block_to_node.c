#include <string.h>

#include "lv/func_block.h"
#include "lv/lv_utils.h"
#include "lv/representation_converter.h"

/* 节点图内部结构：将 FuncBlock 映射为节点和边 */
typedef struct {
    int id;            /* 节点ID（对应 FuncBlock id） */
    char *name;        /* 节点名称 */
    int *input_ports;  /* 输入端口ID数组 */
    int input_count;   /* 输入端口数量 */
    int *output_ports; /* 输出端口ID数组 */
    int output_count;  /* 输出端口数量 */
} NodeGraphNode;

typedef struct {
    NodeGraphNode *nodes; /* 节点数组 */
    int node_count;       /* 节点数量 */
    int node_cap;         /* 节点数组容量 */

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

static void lv_convert_block_to_node_cleanup(NodeGraph *ng);

/* 将函数块转换为节点图表示 */
/* 每个 FuncBlock → 一个 NodeGraphNode */
/* 块的输入/输出端口 → 节点的输入/输出端口 */
/* 块间的连接 → 节点间的边 */
lvConvertResult lv_convert_block_to_node(void *block) {
    lvConvertResult result = {0};
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

    BlockGraphView *bg = (BlockGraphView *) block;

    /* 创建节点图 */
    NodeGraph *ng = lv_calloc(1, sizeof(NodeGraph));
    if (!ng) {
        result.success = 0;
        strncpy(result.error_msg, "out of memory", sizeof(result.error_msg));
        return result;
    }
    ng->node_cap = bg->count > 0 ? bg->count : 8;
    ng->nodes = lv_calloc(ng->node_cap, sizeof(NodeGraphNode));
    if (!ng->nodes) {
        lv_free((void **) &ng);
        result.success = 0;
        strncpy(result.error_msg, "out of memory", sizeof(result.error_msg));
        return result;
    }
    ng->edge_cap = 32;
    ng->edges = lv_calloc(ng->edge_cap, sizeof(ng->edges[0]));
    if (!ng->edges) {
        lv_free((void **) &ng->nodes);
        lv_free((void **) &ng);
        result.success = 0;
        strncpy(result.error_msg, "out of memory", sizeof(result.error_msg));
        return result;
    }

    /* 为每个 FuncBlock 创建对应节点 */
    for (int i = 0; i < bg->count; i++) {
        FuncBlock *fb = bg->blocks[i];
        if (!fb)
            continue;

        NodeGraphNode *node = &ng->nodes[ng->node_count++];
        node->id = func_block_get_id(fb);

        /* 复制名称 */
        const char *name = func_block_get_name(fb);
        if (name) {
            node->name = lv_strdup_safe(name);
        } else {
            node->name = lv_strdup_safe("unnamed");
        }

        /* 映射输入端口 */
        int in_count = func_block_get_input_count(fb);
        if (in_count > 0 && fb->input_port_ids) {
            node->input_ports = lv_calloc(in_count, sizeof(int));
            if (!node->input_ports) {
                lv_free((void **) &node->name);
                ng->node_count--;
                lv_convert_block_to_node_cleanup(ng);
                result.success = 0;
                strncpy(result.error_msg, "out of memory", sizeof(result.error_msg));
                return result;
            }
            memcpy(node->input_ports, fb->input_port_ids, in_count * sizeof(int));
            node->input_count = in_count;
        }

        /* 映射输出端口 */
        int out_count = func_block_get_output_count(fb);
        if (out_count > 0 && fb->output_port_ids) {
            node->output_ports = lv_calloc(out_count, sizeof(int));
            if (!node->output_ports) {
                lv_free((void **) &node->name);
                lv_free((void **) &node->input_ports);
                ng->node_count--;
                lv_convert_block_to_node_cleanup(ng);
                result.success = 0;
                strncpy(result.error_msg, "out of memory", sizeof(result.error_msg));
                return result;
            }
            memcpy(node->output_ports, fb->output_port_ids, out_count * sizeof(int));
            node->output_count = out_count;
        }
    }

    /* 根据端口依赖关系生成边 */
    /* 遍历所有块的端口依赖，建立节点间的连接 */
    for (int i = 0; i < bg->count; i++) {
        FuncBlock *fb = bg->blocks[i];
        if (!fb || fb->port_dep_count <= 0 || !fb->port_deps)
            continue;

        for (int j = 0; j < fb->port_dep_count; j++) {
            PortDependency *dep = &fb->port_deps[j];

            /* 查找源节点和目标节点在节点图中的索引 */
            int src_idx = -1, dst_idx = -1;
            for (int k = 0; k < ng->node_count; k++) {
                if (ng->nodes[k].id == dep->external_node_id)
                    src_idx = k;
                if (ng->nodes[k].id == fb->id)
                    dst_idx = k;
            }

            if (src_idx >= 0 && dst_idx >= 0) {
                /* 扩容检查 */
                if (ng->edge_count >= ng->edge_cap) {
                    ng->edge_cap *= 2;
                    void *_tmp = lv_realloc(ng->edges, ng->edge_cap * sizeof(ng->edges[0]));
                    if (!_tmp) {
                        lv_convert_block_to_node_cleanup(ng);
                        result.success = 0;
                        strncpy(result.error_msg, "out of memory", sizeof(result.error_msg));
                        return result;
                    }
                    ng->edges = _tmp;
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

/* 清理节点图中的动态内存（strdup分配的name等） */
static void lv_convert_block_to_node_cleanup(NodeGraph *ng) {
    if (!ng)
        return;
    for (int i = 0; i < ng->node_count; i++) {
        lv_free((void **) &ng->nodes[i].name);
        lv_free((void **) &ng->nodes[i].input_ports);
        lv_free((void **) &ng->nodes[i].output_ports);
    }
    lv_free((void **) &ng->edges);
    lv_free((void **) &ng->nodes);
    ng->node_count = 0;
    ng->edge_count = 0;
    lv_free((void **) &ng);
}

lvConvertResult lv_convert_node_to_block(void *node) {
    lvConvertResult result = {0};
    if (!node) {
        result.success = 0;
        strncpy(result.error_msg, "NULL node", sizeof(result.error_msg));
        return result;
    }

    /* node 指向 NodeGraph 结构（由 lv_convert_block_to_node 产生） */
    NodeGraph *ng = (NodeGraph *) node;

    /* 创建 BlockGraphView 结构作为输出 */
    typedef struct {
        FuncBlock **blocks;
        int count;
    } BlockGraphView;

    BlockGraphView *bg = lv_calloc(1, sizeof(BlockGraphView));
    if (!bg) {
        result.success = 0;
        strncpy(result.error_msg, "out of memory", sizeof(result.error_msg));
        return result;
    }

    if (ng->node_count <= 0) {
        /* 空节点图，返回空的 BlockGraphView */
        bg->blocks = NULL;
        bg->count = 0;
        result.output = bg;
        result.success = 1;
        return result;
    }

    /* 为每个节点创建对应的 FuncBlock */
    bg->count = ng->node_count;
    bg->blocks = lv_calloc(ng->node_count, sizeof(FuncBlock *));
    if (!bg->blocks) {
        lv_free((void **) &bg);
        result.success = 0;
        strncpy(result.error_msg, "out of memory", sizeof(result.error_msg));
        return result;
    }

    for (int i = 0; i < ng->node_count; i++) {
        NodeGraphNode *n = &ng->nodes[i];

        /* 创建函数块，使用节点 ID 作为块 ID */
        FuncBlock *fb = func_block_create(n->id);
        if (!fb) {
            /* 内存不足，清理已创建的块 */
            for (int j = 0; j < i; j++) {
                func_block_destroy(bg->blocks[j]);
            }
            lv_free((void **) &bg->blocks);
            lv_free((void **) &bg);
            result.success = 0;
            strncpy(result.error_msg, "out of memory creating FuncBlock", sizeof(result.error_msg));
            return result;
        }

        /* 设置函数块名称 */
        if (n->name) {
            func_block_set_name(fb, n->name);
        }

        /* 将节点的输入端口映射回块的输入端口 */
        if (n->input_count > 0 && n->input_ports) {
            func_block_set_input_ports(fb, n->input_ports, n->input_count);
        }

        /* 将节点的输出端口映射回块的输出端口 */
        if (n->output_count > 0 && n->output_ports) {
            func_block_set_output_ports(fb, n->output_ports, n->output_count);
        }

        bg->blocks[i] = fb;
    }

    /* 根据边信息重建端口依赖关系 */
    /* 边: src_node.output_port -> dst_node.input_port
     * 反向映射为 PortDependency:
     *   - external_node_id = 源节点 ID
     *   - port_id = 源端口
     *   - internal_node_id = 目标端口
     */
    for (int i = 0; i < ng->edge_count; i++) {
        int src_idx = ng->edges[i].src_node;
        int dst_idx = ng->edges[i].dst_node;
        int src_port = ng->edges[i].src_port;
        int dst_port = ng->edges[i].dst_port;

        if (src_idx < 0 || src_idx >= ng->node_count || dst_idx < 0 || dst_idx >= ng->node_count) {
            continue; /* 无效索引，跳过 */
        }

        FuncBlock *dst_fb = bg->blocks[dst_idx];
        if (!dst_fb)
            continue;

        /* 创建端口依赖：目标块依赖源节点的输出端口 */
        PortDependency dep;
        dep.type = PORT_DEP_INCIDENCE;                /* 默认关联约束类型 */
        dep.port_id = dst_port;                       /* 目标块的输入端口 */
        dep.external_node_id = ng->nodes[src_idx].id; /* 外部节点 ID（源节点） */
        dep.internal_node_id = src_port;              /* 内部节点 ID（源端口） */
        dep.constraint_data = NULL;

        func_block_add_port_dependency(dst_fb, &dep);
    }

    result.output = bg;
    result.success = 1;
    return result;
}
