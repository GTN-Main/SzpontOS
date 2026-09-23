// (C) Copyright by Szpont Industries. All rights reserved.
// SzpontUI DSO Handle definition for C++ dynamic shared object exit handling

extern "C" {
    __attribute__((visibility("hidden"))) void *__dso_handle = (void *)&__dso_handle;
}
