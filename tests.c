#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "lib_tar.h"

/**
 * You are free to use this file to write tests for your implementation
 */

void debug_dump(const uint8_t *bytes, size_t len) {
    for (int i = 0; i < len;) {
        printf("%04x:  ", (int) i);

        for (int j = 0; j < 16 && i + j < len; j++) {
            printf("%02x ", bytes[i + j]);
        }
        printf("\t");
        for (int j = 0; j < 16 && i < len; j++, i++) {
            printf("%c ", bytes[i]);
        }
        printf("\n");
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s tar_file\n", argv[0]);
        return -1;
    }

    int fd = open(argv[1] , O_RDONLY);
    if (fd == -1) {
        perror("open(tar_file)");
        return -1;
    }

    //int ret = check_archive(fd);
    //printf("check_archive returned %d (valid if > 0)\n", ret);
    //ret = exists(fd, "truc/teaaaast.txt");
    //printf("exists returned %d (valid if != 0)\n", ret);
    //ret = is_dir(fd, "truc");
    //printf("is_dir returned %d (valid if != 0)\n", ret);
    //ret = is_file(fd, "truc/test.txt");
    //printf("is_file returned %d (valid if != 0)\n", ret);
    size_t len = 2048;
    uint8_t* buffer = (uint8_t*)malloc(len*sizeof(uint8_t));
    int ret = read_file(fd, "symlinkmachin.txt", 0, buffer, &len);
    printf("read returned %i (valid if >= 0)\n", ret);
    printf("%s\n", buffer);
    free(buffer);

    int init_no_entries = 20;
    size_t no_entries = init_no_entries;
    char **entries = (char**)malloc(no_entries * sizeof(char*));
    for(int i = 0; i < no_entries; i++) {
        entries[i] = (char*)malloc(100*sizeof(char));
    }
    ret = list(fd, "truc/", entries, &no_entries);
    printf("list returned %d (valid if > 0)\n", ret);
    printf("Discovered %ld entries :\n", no_entries);
    for(int i = 0; i < no_entries; i++) {
        printf("%s; ", entries[i]);
    }
    printf("\n");
    for(int i = 0; i < init_no_entries; i++) {
        free(entries[i]);
    }
    free(entries);
    
    close(fd);
    return 0;
}