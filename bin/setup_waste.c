#include <stdio.h>
#include <numa.h>
#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "usage: %s LOCAL_MB [SYNC_FILE] [NODE]\n", argv[0]);
        return 1;
    }

    int target_mb = atoi(argv[1]);
    long long target = 1024L*1024*target_mb;
    int node = 0;

    const char *sync_file = NULL;
    if (argc > 2)
        sync_file = argv[2];

    if (argc > 3) {
        node = atoi(argv[3]);
        fprintf(stderr, "setup_waste: Using node %d\n", node);
    }

    if (numa_run_on_node(node) < 0) {
        perror("numa_run_on_node");
        exit(1);
    }

#define DROP_FILE "/proc/sys/vm/drop_caches"
    FILE *fp = fopen(DROP_FILE, "w");
    if (!fp) {
        perror("fopen " DROP_FILE);
        return 1;
    }
    if (fprintf(fp, "3\n") < 0) {
        perror("write to " DROP_FILE);
        return 1;
    }
    if (fclose(fp) < 0) {
        perror("close " DROP_FILE);
        return 1;
    }

    long long free;
    if (numa_node_size(node, &free) < 0) {
        fprintf(stderr, "error: numa_node_size\n");
        return 1;
    }

    fprintf(stderr, "setup_wate: Local free mem: %lld bytes = %.2f MB = %.2f GB\n", free,
            free/1024.0f/1024.0f, free/1024.0f/1024.0f/1024.0f);

    long long waste = free - target;
    if (waste < 0) {
        fprintf(stderr, "error: not enough free memory\n");
        return 1;
    }

    numa_set_strict(1);

    fprintf(stderr, "setup_waste: Allocating %lld bytes = %.2f MB = %.2f GB\n", waste,
            waste/1024.0f/1024.0f, waste/1024.0f/1024.0f/1024.0f);

    void *ptr = numa_alloc_onnode(waste, node);
    if (!ptr) {
        fprintf(stderr, "error: numa_alloc_onnode\n");
        return 1;
    }

    if (mlock(ptr, waste) < 0) {
        perror("error: mlock");
        return 1;
    }

    if (numa_node_size(node, &free) < 0) {
        fprintf(stderr, "error: numa_node_size\n");
        return 1;
    }

    fprintf(stderr, "setup_wate: Local free mem: %lld bytes = %.2f MB = %.2f GB\n", free,
            free/1024.0f/1024.0f, free/1024.0f/1024.0f/1024.0f);

    fprintf(stderr, "setup_wate: Setup complete, entering background.\n");

    if (sync_file) {
        FILE *sfp = fopen(sync_file, "w");
        if (!sfp) {
            perror("error opening sync file");
            exit(1);
        }
        if (fprintf(sfp, "ready\n") < 0) {
            perror("error writing to sync file");
            exit(1);
        }
        fclose(sfp);
    }

    for (;;)
        pause();
}
