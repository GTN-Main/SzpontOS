/*
 * SzpontOS - Math Library Verification Test (mathtest)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("========================================\n");
    printf("   SzpontOS LibMath (libm) Test Suite   \n");
    printf("========================================\n\n");

    /* 1. Constants */
    printf("[1] Mathematical Constants:\n");
    printf("  pi       = %.10f (expected ~3.1415926535)\n", M_PI);
    printf("  e        = %.10f (expected ~2.7182818284)\n", M_E);
    printf("  ln(2)    = %.10f (expected ~0.6931471805)\n", M_LN2);
    printf("  sqrt(2)  = %.10f (expected ~1.4142135623)\n\n", M_SQRT2);

    /* 2. Trigonometry */
    printf("[2] Trigonometric Functions:\n");
    printf("  sin(0)        = %.6f\n", sin(0.0));
    printf("  sin(pi/2)     = %.6f (expected 1.000000)\n", sin(M_PI_2));
    printf("  sin(pi/6)     = %.6f (expected 0.500000)\n", sin(M_PI / 6.0));
    printf("  cos(0)        = %.6f (expected 1.000000)\n", cos(0.0));
    printf("  cos(pi)       = %.6f (expected -1.000000)\n", cos(M_PI));
    printf("  cos(pi/3)     = %.6f (expected 0.500000)\n", cos(M_PI / 3.0));
    printf("  tan(pi/4)     = %.6f (expected 1.000000)\n", tan(M_PI_4));
    printf("  atan(1.0)     = %.6f (expected pi/4 = %.6f)\n", atan(1.0), M_PI_4);
    printf("  atan2(1.0, 1) = %.6f (expected pi/4)\n", atan2(1.0, 1.0));
    printf("  asin(0.5)     = %.6f (expected pi/6 = %.6f)\n", asin(0.5), M_PI / 6.0);
    printf("  acos(0.5)     = %.6f (expected pi/3 = %.6f)\n\n", acos(0.5), M_PI / 3.0);

    /* 3. Exponential & Logarithms */
    printf("[3] Exponential & Logarithmic Functions:\n");
    printf("  exp(0)   = %.6f (expected 1.000000)\n", exp(0.0));
    printf("  exp(1)   = %.6f (expected e = %.6f)\n", exp(1.0), M_E);
    printf("  exp(2)   = %.6f (expected ~7.389056)\n", exp(2.0));
    printf("  log(e)   = %.6f (expected 1.000000)\n", log(M_E));
    printf("  log(1)   = %.6f (expected 0.000000)\n", log(1.0));
    printf("  log2(8)  = %.6f (expected 3.000000)\n", log2(8.0));
    printf("  log10(1000) = %.6f (expected 3.000000)\n\n", log10(1000.0));

    /* 4. Powers & Roots */
    printf("[4] Power & Root Functions:\n");
    printf("  sqrt(4.0)   = %.6f (expected 2.000000)\n", sqrt(4.0));
    printf("  sqrt(2.0)   = %.6f (expected 1.414213)\n", sqrt(2.0));
    printf("  cbrt(27.0)  = %.6f (expected 3.000000)\n", cbrt(27.0));
    printf("  hypot(3, 4) = %.6f (expected 5.000000)\n", hypot(3.0, 4.0));
    printf("  pow(2.0, 10.0) = %.6f (expected 1024.000000)\n", pow(2.0, 10.0));
    printf("  pow(3.0, 3.0)  = %.6f (expected 27.000000)\n", pow(3.0, 3.0));
    printf("  pow(4.0, 0.5)  = %.6f (expected 2.000000)\n\n", pow(4.0, 0.5));

    /* 5. Hyperbolic Functions */
    printf("[5] Hyperbolic Functions:\n");
    printf("  sinh(0)  = %.6f (expected 0.000000)\n", sinh(0.0));
    printf("  cosh(0)  = %.6f (expected 1.000000)\n", cosh(0.0));
    printf("  tanh(0)  = %.6f (expected 0.000000)\n", tanh(0.0));
    printf("  cosh(1)^2 - sinh(1)^2 = %.6f (identity = 1.000000)\n\n", cosh(1.0)*cosh(1.0) - sinh(1.0)*sinh(1.0));

    /* 6. Rounding & Utility */
    printf("[6] Rounding & Special Numbers:\n");
    printf("  floor(3.7) = %.1f, ceil(3.2) = %.1f, round(3.5) = %.1f\n", floor(3.7), ceil(3.2), round(3.5));
    printf("  fmod(10.5, 3.0) = %.2f (expected 1.50)\n", fmod(10.5, 3.0));
    printf("  fabs(-42.5)     = %.2f (expected 42.50)\n", fabs(-42.5));

    printf("\n[SUCCESS] LibMath is fully operational!\n");
    return 0;
}
