#include <stdio.h>
#include <stdlib.h>

// Direct GMP test without lv00
#include <gmp.h>

int main() {
    printf("=== Direct GMP Test ===\n\n");
    printf("1. Initializing mpq...\n");
    mpq_t a;
    mpq_init(a);
    printf("   OK\n");
    
    printf("2. Setting value 1/2...\n");
    mpq_set_si(a, 1, 2);
    printf("   OK\n");
    
    printf("3. Canonicalizing...\n");
    mpq_canonicalize(a);
    printf("   OK\n");
    
    printf("4. Getting numerator and denominator...\n");
    mpz_t num, den;
    mpz_init(num);
    mpz_init(den);
    mpq_get_num(num, a);
    mpq_get_den(den, a);
    printf("   OK\n");
    
    printf("5. Converting to strings...\n");
    char* num_str = mpz_get_str(NULL, 10, num);
    char* den_str = mpz_get_str(NULL, 10, den);
    printf("   OK\n");
    
    printf("6. Printing: %s/%s\n", num_str, den_str);
    
    printf("\n7. Freeing strings...\n");
    free(num_str);
    free(den_str);
    printf("   OK\n");
    
    printf("8. Clearing mpz...\n");
    mpz_clear(num);
    mpz_clear(den);
    printf("   OK\n");
    
    printf("9. Clearing mpq...\n");
    mpq_clear(a);
    printf("   OK\n");
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}
