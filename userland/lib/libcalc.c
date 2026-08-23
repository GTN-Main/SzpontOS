#include <stdint.h>

int add(int a, int b) {
    return a + b;
}

int multiply(int a, int b) {
    return a * b;
}

const char *get_version(void) {
    return "libcalc.so v1.0.0 (SzpontOS Dynamic Shared Library)";
}
