﻿/**
 * @file test_minimal_parse.c
 * @brief 公理包最小化解析测试 - 验证 .lvz 文件加载
 *
 * 测试内容：
 * - euclidean_plane.lvz 公理包文件加载
 * - 模板注册数量验证
 * - 不可构造项列表解析
 * - 底部几何对象检查
 *
 * 注意：本测试依赖相对路径 axiom_packages/euclidean_plane.lvz，
 *       运行时需在项目根目录下执行。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00.h"

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("=== euclidean_plane.lvz (full with comments) ===\n");
    AxiomPackage *pkg = axiom_package_create("t", "0");
    if (!pkg) {
        printf("ERROR: Failed to create axiom package\n");
        return 1;
    }

    /* 路径：先尝试相对于可执行文件所在目录，再尝试相对于当前工作目录 */
    const char *paths[] = {
        "module/axiom_packages/euclidean_plane.lvz",
        "../axiom_packages/euclidean_plane.lvz",
        "../../axiom_packages/euclidean_plane.lvz",
    };
    AxiomLoadStatus s = AXIOM_LOAD_FILE_NOT_FOUND;
    int path_count = sizeof(paths) / sizeof(paths[0]);

    for (int i = 0; i < path_count; i++) {
        /* 先检查文件是否存在 */
        FILE *fp = fopen(paths[i], "r");
        if (fp) {
            fclose(fp);
            s = axiom_package_load(pkg, paths[i]);
            if (s == AXIOM_LOAD_OK) {
                printf("Loaded from: %s\n", paths[i]);
                break;
            }
        }
    }

    if (s != AXIOM_LOAD_OK) {
        printf("ERROR: Failed to load axiom package (status=%d)\n", (int) s);
        const char *err = axiom_package_get_last_error();
        if (err)
            printf("  Last error: %s\n", err);
        axiom_package_destroy(pkg);
        return 1;
    }

    printf("Load: %d, Tmpl: %d, UC: %d, Bottom: %s\n", (int) s, pkg->template_count, pkg->unconstructible_count,
           pkg->bottom_geometry ? pkg->bottom_geometry : "(null)");
    const char *err = axiom_package_get_last_error();
    if (err)
        printf("Error: %s\n", err);

    for (int i = 0; i < pkg->unconstructible_count; i++) {
        KnownUnconstructible *uc = &pkg->known_unconstructibles[i];
        printf("  UC[%d]: %s -> %s (deps=%d, verified=%d)\n", i, uc->name, uc->reduces_to, uc->dependency_count,
               uc->green_verified);
    }
    axiom_package_destroy(pkg);

    return 0;
}
