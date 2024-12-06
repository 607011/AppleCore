#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>

int main(void)
{
    struct rlimit rl;
    int resources[] = {RLIMIT_CPU, RLIMIT_DATA, RLIMIT_AS, RLIMIT_RSS};
    char labels[][16] = {"CPU", "Data", "Virtual", "Resident"};
    int rlen = sizeof(resources) / sizeof(int);
    printf("RLIM_INFINITY is %lu\n", (uint64_t)RLIM_INFINITY);
    for (int i = 0; i < rlen; i++)
    {
        if (getrlimit(resources[i], &rl) != 0)
        {
            fprintf(stderr, "!!%s\n", strerror(errno));
            return -1;
        }
        printf("%8s soft: %lu hard: %lu\n", labels[i], (uint64_t)rl.rlim_cur, (uint64_t)rl.rlim_max);
    }
    return 0;
}
