#include <stdio.h>
#include <string.h>

#include "lv.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

TEST_MAIN_BEGIN("Step 1: Creating package...")
    printf("Step 1: Creating package...\n");
    fflush(stdout);
    AxiomPackage *pkg = axiom_package_create("placeholder", "0.0.0");
    printf("  pkg = %p\n", (void *) pkg);
    fflush(stdout);
    printf("Step 2: Loading file...\n");
    fflush(stdout);
    AxiomLoadStatus status = axiom_package_load(pkg, "module/axiom_packages/group_theory.lvz");
    printf("  status = %d\n", (int) status);
    fflush(stdout);
    if (status != AXIOM_LOAD_OK) {
        return 1;
    }
    printf("  name='%s', templates=%d, unconstructibles=%d\n", pkg->name, axiom_package_get_template_count(pkg),
           axiom_package_get_unconstructible_count(pkg));
    fflush(stdout);
    printf("Step 3: Looking up unconstructibles...\n");
    fflush(stdout);
    KnownUnconstructible *uc = axiom_package_lookup_unconstructible(pkg, "word_problem");
    printf("  word_problem: %p\n", (void *) uc);
    fflush(stdout);
    if (uc) {
        printf("    reduces_to='%s', deps=%d\n", uc->reduces_to, uc->dependency_chain.count);
        fflush(stdout);
    }
    printf("Step 4: Content hash...\n");
    fflush(stdout);
    char *hash = axiom_package_compute_content_hash(pkg);
    printf("  hash = %p\n", (void *) hash);
    fflush(stdout);
    if (hash) {
        printf("  SHA-256: %.16s...\n", hash);
        fflush(stdout);
    }
    printf("Step 5: Save...\n");
    fflush(stdout);
    AxiomSaveStatus ss = axiom_package_save(pkg, "module/axiom_packages/group_theory_test_save.lvz");
    printf("  save_status = %d\n", (int) ss);
    fflush(stdout);
    printf("Step 6: Dependency validation...\n");
    fflush(stdout);
    bool valid = axiom_package_validate_dependencies(pkg, &pkg, 1);
    printf("  valid = %d\n", valid);
    fflush(stdout);
    printf("Step 7: Destroying...\n");
    fflush(stdout);
    axiom_package_destroy(pkg);
    printf("  done\n");
    fflush(stdout);
TEST_MAIN_END()
