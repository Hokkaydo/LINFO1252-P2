#include "lib_tar.h"
#include <stdio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

/**
 * Computes checksum for a given header
 * @param header pointer to the header to compute checksum for
 * @return computed checksum
 *
 */
int checksum(tar_header_t *header)
{
    int sum = 0;
    for (int i = 0; i < 512; i++)
    {
        if (i >= 148 && i < 156)
            continue;
        sum += header->name[i];
    }
    sum += 256; // checksum initally 8 bytes of spaces (32 in ascii)
    return sum;
}

int validate_header(tar_header_t *header)
{
    if (strlen(header->magic) + 1 != TMAGLEN || strncmp(header->magic, TMAGIC, strlen(header->magic)) != 0)
    {
        return -1;
    }
    char version[2];
    version[0] = header->version[0];
    version[1] = header->version[1];
    if (strncmp(version, TVERSION, 2) != 0)
    {
        return -2;
    }
    if (TAR_INT(header->chksum) != checksum(header))
    {
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
int check_archive(int tar_fd)
{
    struct stat statbuf;
    if (fstat(tar_fd, &statbuf) == -1)
    {
        close(tar_fd);
        return -1;
    }
    if (statbuf.st_size <= 0)
    {
        close(tar_fd);
        return 0;
    }
    tar_header_t *fileptr = (tar_header_t *)mmap(NULL, statbuf.st_atime, PROT_READ, MAP_SHARED, tar_fd, 0);
    int header_amount = 0;
    int i = 0;
    while (i < statbuf.st_size / sizeof(tar_header_t))
    {
        tar_header_t header = fileptr[i];
        if (strlen(header.name) == 0)
        {
            i++;
            continue;
        }
        int ret = validate_header(&header);

        if (ret != 0)
        {
            munmap(fileptr, statbuf.st_size);
            return ret;
        }
        header_amount += 1;
        i += 1 + ceil(TAR_INT(header.size) / (float)sizeof(tar_header_t));
    }

    munmap(fileptr, statbuf.st_size);
    return header_amount;
}

int array_contains(char *array, int len, char item)
{
    if (len == 0)
        return 1;
    for (int i = 0; i < len; i++)
    {
        if (array[i] == item)
            return 1;
    }
    return 0;
}

int check_entry(int tar_fd, char *path, char *flags, int flags_len, tar_header_t* found_header)
{
    struct stat statbuf;
    if (fstat(tar_fd, &statbuf) == -1)
    {
        close(tar_fd);
        return -1;
    }
    if (statbuf.st_size <= 0)
    {
        close(tar_fd);
        return 0;
    }
    tar_header_t *fileptr = (tar_header_t *)mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, tar_fd, 0);

    int i = 0;
    while (i < statbuf.st_size / sizeof(tar_header_t))
    {
        tar_header_t header = fileptr[i];
        if (strlen(header.name) == 0)
        {
            i++;
            continue;
        }
        int ret = validate_header(&header);
        if (ret != 0)
        {
            i++;
            continue;
        }
        if (strncmp(header.name, path, fmax(strlen(header.name), strlen(path))) == 0 && array_contains(flags, flags_len, header.typeflag))
        {
            if(found_header != NULL)
                *found_header = header;
            munmap(fileptr, statbuf.st_size);
            return 1;
        }

        i += 1 + ceil(TAR_INT(header.size) / (float)sizeof(tar_header_t));
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
int exists(int tar_fd, char *path)
{
    return check_entry(tar_fd, path, NULL, 0, NULL);
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
int is_dir(int tar_fd, char *path)
{
    char flags[] = {DIRTYPE};
    return check_entry(tar_fd, path, flags, 1, NULL);
}

/**
 * Checks whether an entry exists in the archive and is a file.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive or the entry is not a file,
 *         any other value otherwise.
 */
int is_file(int tar_fd, char *path)
{
    char flags[] = {REGTYPE, AREGTYPE};
    return check_entry(tar_fd, path, flags, 2, NULL);
}

/**
 * Checks whether an entry exists in the archive and is a symlink.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 * @return zero if no entry at the given path exists in the archive or the entry is not symlink,
 *         any other value otherwise.
 */
int is_symlink(int tar_fd, char *path)
{
    char flags[] = {SYMTYPE};
    return check_entry(tar_fd, path, flags, 1, NULL);
}

int count_backslash(char *str)
{
    int count = 0;
    for (int i = 0; i < strlen(str); i++)
    {
        if (str[i] == '/')
        {
            count++;
        }
    }
    return count;
}

int get_symlink(int tar_fd, char *path, tar_header_t* found_header) {
    char flags[] = {SYMTYPE};
    return check_entry(tar_fd, path, flags, 1, found_header);
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
int list(int tar_fd, char *path, char **entries, size_t *no_entries)
{
    tar_header_t* symheader = (tar_header_t*)malloc(sizeof(tar_header_t));
    if(get_symlink(tar_fd, path, symheader)) {
        char name[100];
        strcpy(name, symheader->linkname);
        strcat(name, "/");
        free(symheader);
        return list(tar_fd, name, entries, no_entries);
    }

    if (is_dir(tar_fd, path) == 0 || path[strlen(path) - 1] != '/')
        return 0;

    struct stat statbuf;
    if (fstat(tar_fd, &statbuf) == -1)
    {
        close(tar_fd);
        return -1;
    }
    if (statbuf.st_size <= 0)
    {
        close(tar_fd);
        return 0;
    }
    tar_header_t *fileptr = (tar_header_t *)mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, tar_fd, 0);

    int i = 0;
    int entry = 0;
    while (i < statbuf.st_size / sizeof(tar_header_t))
    {
        if (entry >= *no_entries)
        {
            break;
        }
        tar_header_t header = fileptr[i];
        if (strlen(header.name) == 0)
        {
            i++;
            continue;
        }
        int ret = validate_header(&header);
        if (ret != 0)
        {
            i++;
            continue;
        }
        int backslash_amount = count_backslash(header.name);
        if (strncmp(header.name, path, strlen(path)) == 0 && strlen(header.name) != strlen(path) && backslash_amount <= 2 && (backslash_amount == 1 || header.name[strlen(header.name) - 1] == '/'))
        {
            strcpy(entries[entry++], header.name);
        }

        i += 1 + ceil(TAR_INT(header.size) / (float)sizeof(tar_header_t));
    }
    *no_entries = entry;
    munmap(fileptr, statbuf.st_size);
    return 1;
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
ssize_t read_file(int tar_fd, char *path, size_t offset, uint8_t *dest, size_t *len)
{
    if (is_file(tar_fd, path) == 0 && is_symlink(tar_fd, path) == 0)
        return -1;
    struct stat statbuf;
    if (fstat(tar_fd, &statbuf) == -1)
    {
        close(tar_fd);
        return -1;
    }
    if (statbuf.st_size <= 0)
    {
        close(tar_fd);
        return -1;
    }
    tar_header_t *fileptr = (tar_header_t *)mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, tar_fd, 0);

    int i = 0;

    while (i < statbuf.st_size / sizeof(tar_header_t))
    {
        tar_header_t header = fileptr[i];
        if (strlen(header.name) == 0)
        {
            i++;
            continue;
        }
        int ret = validate_header(&header);
        if (ret != 0)
        {
            i++;
            continue;
        }
        if ((strncmp(header.name, path, fmax(strlen(header.name), strlen(path))) == 0))
        {
            if (header.typeflag == SYMTYPE)
            {
                munmap(fileptr, statbuf.st_size);
                return read_file(tar_fd, header.linkname, offset, dest, len);
            }

            if (offset > TAR_INT(header.size) || offset > *len)
            {
                *len = 0;
                return -2;
            }
            int remainder = 0;
            if (*len < TAR_INT(header.size))
            {
                remainder = TAR_INT(header.size) - *len - offset;
            }

            char *src = ((char *)fileptr) + (i + 1) * 512 + offset;

            *len = fmin(*len, TAR_INT(header.size) - offset);

            memcpy(dest, src, *len);

            munmap(fileptr, statbuf.st_size);
            return remainder;
        }

        i += 1 + ceil(TAR_INT(header.size) / (float)sizeof(tar_header_t));
    }

    munmap(fileptr, statbuf.st_size);
    return -1;
}