#include "lv/interop.h"
#include "lv/engine.h"
#include "lv/error_codes.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 3) {
        printf("usage: %s <ggb> <svg>\n", argv[0]);
        return 1;
    }
    printf("[1] engine_create...\n");
    lvEngine *engine = engine_create();
    printf("[2] engine_create done: %p\n", (void *) engine);
    if (!engine) {
        printf("engine_create failed\n");
        return 2;
    }

    InteropImportConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.format = INTEROP_IMPORT_GEOGEBRA;
    snprintf(cfg.input_path, sizeof(cfg.input_path), "%s", argv[1]);
    printf("[3] interop_import_geogebra('%s')...\n", cfg.input_path);
    int r1 = interop_import_geogebra(engine, &cfg);
    printf("[4] geogebra import entities: %d\n", r1);

    memset(&cfg, 0, sizeof(cfg));
    cfg.format = INTEROP_IMPORT_SVG;
    snprintf(cfg.input_path, sizeof(cfg.input_path), "%s", argv[2]);
    printf("[5] interop_import_svg('%s')...\n", cfg.input_path);
    int r2 = interop_import_svg(engine, &cfg);
    printf("[6] svg import entities: %d\n", r2);

    printf("[7] engine_destroy...\n");
    engine_destroy(engine);
    printf("[8] done\n");
    return 0;
}
