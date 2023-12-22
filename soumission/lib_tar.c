#include "lib_tar.h"
#include <stdio.h>
#include <sys/stat.h> 
#include <sys/mman.h> 
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define min(a,b) a < b ? a : b

/**
 * Computes checksum for a given header
 * @param header pointer to the header to compute checksum for
 * @return computed checksum
 * 
*/
int checksum(tar_header_t* header) {
    int sum = 0;
    for(int i = 0; i < 512; i++) {
        if(i >= 148 && i <156) continue;
        sum += header->name[i];
    }
    sum+=256; //checksum initally 8 bytes of spaces (32 in ascii)
    return sum;
}

int validate_header(tar_header_t* header) {
    if(strlen(header->magic)+1 != TMAGLEN || strncmp(header->magic, TMAGIC, strlen(header->magic) != 0)) {
        return -1;
    }
    char version[2];
    version[0] = header->version[0];
    version[1] = header->version[1];
    if(strncmp(version, TVERSION, 2) != 0) {
        return -2;
    }
    if(TAR_INT(header->chksum) != checksum(header)) {
        return -3;
    }

    return 0;
}

/**
 * Checks whether the archive is valid.
 *
 * Each non-null header of a valid archive has:
 *  - a magic value of "ustar" and a null,
 *  - a version value of "00" and no null,
 *  - a correct checksum
 *
 * @param tar_fd A file descriptor pointing to the start of a file supposed to contain a tar archive.
 *
 * @return a zero or positive value if the archive is valid, representing the number of non-null headers in the archive,
 *         -1 if the archive contains a header with an invalid magic value,
 *         -2 if the archive contains a header with an invalid version value,
 *         -3 if the archive contains a header with an invalid checksum value
 */
int check_archive(int tar_fd) {
    struct stat statbuf;
    if(fstat(tar_fd, &statbuf) == -1) {close(tar_fd); return -1;}
    if(statbuf.st_size <= 0) {close (tar_fd); return 0;}
    tar_header_t* fileptr = (tar_header_t*)mmap(NULL, statbuf.st_atime, PROT_READ, MAP_SHARED, tar_fd, 0);
        
    int i = 0;
    while(i < statbuf.st_size/sizeof(tar_header_t)) {
        tar_header_t header = fileptr[i];
        if(strlen(header.name) == 0) {
            i++;
            continue;
        }
        int ret = validate_header(&header);

        if(ret != 0) {
            munmap(fileptr, statbuf.st_size);
            return ret;
        }
        i+= 1 + ceil(TAR_INT(header.size)/(float)sizeof(tar_header_t));
    }


    munmap(fileptr, statbuf.st_size);
    return 0;
}

/**
 * Checks whether an entry exists in the archive.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive,
 *         any other value otherwise.
 */
int exists(int tar_fd, char *path) {
    struct stat statbuf;
    if(fstat(tar_fd, &statbuf) == -1) {close(tar_fd); return -1;}
    if(statbuf.st_size <= 0) {close (tar_fd); return 0;}
    tar_header_t* fileptr = (tar_header_t*)mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, tar_fd, 0);
    
    
    int i = 0;
    while(i < statbuf.st_size/sizeof(tar_header_t)) {
        tar_header_t header = fileptr[i];
        if(strlen(header.name) == 0) {
            i++;
            continue;
        }
        int ret = validate_header(&header);
        if(ret != 0) {
            i++;
            continue;
        }
        if(strncmp(header.name, path, min(strlen(header.name), strlen(path))) == 0) {
            munmap(fileptr, statbuf.st_size);
            return 1;
        }

        i+= 1 + ceil(TAR_INT(header.size)/(float)sizeof(tar_header_t));
    }
    
    
    munmap(fileptr, statbuf.st_size);

    return 0;
}

/**
 * Checks whether an entry exists in the archive and is a directory.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive or the entry is not a directory,
 *         any other value otherwise.
 */
int is_dir(int tar_fd, char *path) {
    struct stat statbuf;
    if(fstat(tar_fd, &statbuf) == -1) {close(tar_fd); return -1;}
    if(statbuf.st_size <= 0) {close (tar_fd); return 0;}
    tar_header_t* fileptr = (tar_header_t*)mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, tar_fd, 0);
    
    
    int i = 0;
    while(i < statbuf.st_size/sizeof(tar_header_t)) {
        tar_header_t header = fileptr[i];
        if(strlen(header.name) == 0) {
            i++;
            continue;
        }
        int ret = validate_header(&header);
        if(ret != 0) {
            i++;
            continue;
        }
        printf("%s %s %c %li\n", header.name, path, header.typeflag, min(strlen(header.name), strlen(path)));
        if(strncmp(header.name, path, min(strlen(header.name), strlen(path))) == 0 && header.typeflag == DIRTYPE) {
            munmap(fileptr, statbuf.st_size);
            return 1;
        }

        i+= 1 + ceil(TAR_INT(header.size)/(float)sizeof(tar_header_t));
    }
    
    
    munmap(fileptr, statbuf.st_size);

    return 0;}

/**
 * Checks whether an entry exists in the archive and is a file.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive or the entry is not a file,
 *         any other value otherwise.
 */
int is_file(int tar_fd, char *path) {
    return 0;
}

/**
 * Checks whether an entry exists in the archive and is a symlink.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 * @return zero if no entry at the given path exists in the archive or the entry is not symlink,
 *         any other value otherwise.
 */
int is_symlink(int tar_fd, char *path) {
    return 0;
}


/**
 * Lists the entries at a given path in the archive.
 * list() does not recurse into the directories listed at the given path.
 *
 * Example:
 *  dir/          list(..., "dir/", ...) lists "dir/a", "dir/b", "dir/c/" and "dir/e/"
 *   ├── a
 *   ├── b
 *   ├── c/
 *   │   └── d
 *   └── e/
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive. If the entry is a symlink, it must be resolved to its linked-to entry.
 * @param entries An array of char arrays, each one is long enough to contain a tar entry path.
 * @param no_entries An in-out argument.
 *                   The caller set it to the number of entries in `entries`.
 *                   The callee set it to the number of entries listed.
 *
 * @return zero if no directory at the given path exists in the archive,
 *         any other value otherwise.
 */
int list(int tar_fd, char *path, char **entries, size_t *no_entries) {
    return 0;
}

/**
 * Reads a file at a given path in the archive.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive to read from.  If the entry is a symlink, it must be resolved to its linked-to entry.
 * @param offset An offset in the file from which to start reading from, zero indicates the start of the file.
 * @param dest A destination buffer to read the given file into.
 * @param len An in-out argument.
 *            The caller set it to the size of dest.
 *            The callee set it to the number of bytes written to dest.
 *
 * @return -1 if no entry at the given path exists in the archive or the entry is not a file,
 *         -2 if the offset is outside the file total length,
 *         zero if the file was read in its entirety into the destination buffer,
 *         a positive value if the file was partially read, representing the remaining bytes left to be read to reach
 *         the end of the file.
 *
 */
ssize_t read_file(int tar_fd, char *path, size_t offset, uint8_t *dest, size_t *len) {
    return 0;
}