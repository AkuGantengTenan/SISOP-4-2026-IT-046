#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>

char source_dir[PATH_MAX];

static void get_full_path(char fpath[PATH_MAX], const char *path) {
    strcpy(fpath, source_dir);
    strncat(fpath, path, PATH_MAX - strlen(source_dir) - 1);
}

static int xmp_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    int res = 0;

    if (strcmp(path, "/tujuan.txt") == 0) {
        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_mode = S_IFREG | 0444; 
        stbuf->st_nlink = 1;
        stbuf->st_size = 66; 
        return 0;
    }

    char fpath[PATH_MAX];
    get_full_path(fpath, path);
    res = lstat(fpath, stbuf);
    if (res == -1) return -errno;

    return 0;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void) offset; (void) fi; (void) flags;
    char fpath[PATH_MAX];
    get_full_path(fpath, path);

    DIR *dp = opendir(fpath);
    if (dp == NULL) return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0, 0)) break;
    }
    closedir(dp);

    if (strcmp(path, "/") == 0) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_mode = S_IFREG | 0444;
        filler(buf, "tujuan.txt", &st, 0, 0);
    }

    return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path, "/tujuan.txt") == 0) {
        if ((fi->flags & O_ACCMODE) != O_RDONLY) return -EACCES;
        return 0;
    }

    char fpath[PATH_MAX];
    get_full_path(fpath, path);
    int res = open(fpath, fi->flags);
    if (res == -1) return -errno;
    close(res);

    return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    if (strcmp(path, "/tujuan.txt") == 0) {
        char gabungan[1024] = "";

        for (int i = 1; i <= 7; i++) {
            char filepath[PATH_MAX];
            snprintf(filepath, sizeof(filepath), "%s/%d.txt", source_dir, i);
            FILE *f = fopen(filepath, "r");
            if (f) {
                char line[256];
                while (fgets(line, sizeof(line), f)) {
                    if (strncmp(line, "KOORD: ", 7) == 0) {
                        char *val = line + 7;
                        val[strcspn(val, "\r\n")] = 0; 
                        strcat(gabungan, val);
                        break;
                    }
                }
                fclose(f);
            }
        }

        char final_text[1024];
        snprintf(final_text, sizeof(final_text), "Tujuan Mas Amba: %s\n", gabungan);
        
        size_t len = strlen(final_text);
        if (offset < len) {
            if (offset + size > len) size = len - offset;
            memcpy(buf, final_text + offset, size);
        } else {
            size = 0;
        }
        return size;
    }

    char fpath[PATH_MAX];
    get_full_path(fpath, path);
    int fd = open(fpath, O_RDONLY);
    if (fd == -1) return -errno;
    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;
    close(fd);
    
    return res;
}

static struct fuse_operations xmp_oper = {
    .getattr    = xmp_getattr,
    .readdir    = xmp_readdir,
    .open       = xmp_open,
    .read       = xmp_read,
};

int main(int argc, char *argv[]) {
    
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_dir>\n", argv[0]);
        return 1;
    }

    realpath(argv[1], source_dir);

    argv[1] = argv[2];
    argc--;

    return fuse_main(argc, argv, &xmp_oper, NULL);
}
