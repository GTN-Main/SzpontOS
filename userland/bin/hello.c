#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    printf("\033[1;32mHello from independent Ring 3 ELF binary in SzpontOS!\033[0m\n");
    printf("\033[1;36mPID:\033[0m \033[1;33m%d\033[0m, \033[1;36mPPID:\033[0m \033[1;33m%d\033[0m\n", getpid(), getppid());
    return 0;
}
