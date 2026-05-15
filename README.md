# SISOP-4-2026

<details>
<summary>Soal 1</summary>

**Penjelasan**

Pertama kita install file `amba_files` , kemudian di unzip


Buat file bernama `kenz_rescue.c`

```c
#define FUSE_USE_VERSION 31
#define PATH_MAX 4096 // Pengganti limits.h

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>

char source_dir[PATH_MAX];

static void get_full_path(char fpath[PATH_MAX], const char *path) {
    strcpy(fpath, source_dir);
    strncat(fpath, path, PATH_MAX - strlen(source_dir) - 1);
}
```

Bagian ini mengatur versi FUSE yang digunakan dan memuat header standar bahasa C yang diperlukan untuk manipulasi string dan sistem file. Variabel global `source_dir` digunakan untuk menyimpan lokasi absolute dari folder sumber, sehingga program selalu tahu ke mana harus mencari file aslinya. Fungsi `get_full_path` adalah fungsi pembantu untuk menggabungkan nama file dari FUSE dengan direktori sumber tersebut.


```c
static int xmp_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    if (strcmp(path, "/tujuan.txt") == 0) {
        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_mode = S_IFREG | 0444; 
        stbuf->st_nlink = 1;
        stbuf->st_size = 66; 
        return 0;
    }

    char fpath[PATH_MAX];
    get_full_path(fpath, path);
    int res = lstat(fpath, stbuf);
    if (res == -1) return -errno;
    return 0;
}
```

Fungsi `getattr` bertugas merespons permintaan metadata, seperti saat Anda menjalankan perintah stat atau ls -l. Jika sistem mendeteksi bahwa user sedang menanyakan info tentang `/tujuan.txt`, program akan langsung mencegatnya dan memalsukan datanya dengan memberikan ukuran pasti 66 bytes dan hak akses read-only (0444). Untuk file selain itu, permintaan metadata hanya akan diteruskan ke file aslinya di folder sumber.

```c
static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
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
```

Fungsi `readdir` dipanggil saat perintah ls dijalankan untuk melihat isi folder mount. Program akan membaca seluruh isi folder asli dan menampilkannya ke layar. Bagian terpenting ada di kondisi if terakhir; jika user sedang melihat isi root directory dari sistem mount, program secara paksa menyisipkan nama tujuan.txt ke dalam output, sehingga file tersebut seolah-olah nyata dan ada di dalam folder.

```c
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
```

Fungsi `open` dipanggil sebelum sebuah file mulai dibaca atau ditulis, contohnya saat mengeksekusi `cat`. Fungsi ini berperan sebagai pos keamanan sederhana. Jika user mencoba mengakses `/tujuan.txt` selain untuk dibaca, program akan langsung menolak dan mengembalikan error Access Denied. Untuk file dokumen biasa, program sekadar mengecek apakah file aslinya bisa dibuka dengan izin yang sesuai.

```c
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

    // Passthrough file normal disembunyikan untuk menyingkat snippet
}
```

Fungsi `read` merupakan inti pengerjaan logika on-the-fly. Saat user mencoba membaca isi `/tujuan.txt`, program tidak mencari file fisik. Sebagai gantinya, ia melakukan perulangan untuk membuka file 1.txt hingga 7.txt satu per satu. Di dalam setiap file, program mencari baris teks yang berawalan "KOORD: ", mengambil angkanya, dan merangkainya menjadi satu kalimat utuh berawalan "Tujuan Mas Amba: ". Kalimat inilah yang disuntikkan ke layar pengguna.

```c
static struct fuse_operations xmp_oper = {
    .getattr    = xmp_getattr,
    .readdir    = xmp_readdir,
    .open       = xmp_open,
    .read       = xmp_read,
};

int main(int argc, char *argv[]) {
    if (argc < 3) return 1;

    realpath(argv[1], source_dir);

    argv[1] = argv[2];
    argc--;

    return fuse_main(argc, argv, &xmp_oper, NULL);
}
```

Bagian penutup ini berfungsi sebagai "jembatan" antara program Anda dengan sistem operasi. Struct `xmp_oper` bertugas memetakan perintah standar Linux ke fungsi-fungsi kustom. Di dalam fungsi main, program mengamankan jalur folder sumber ke dalam variabel `source_dir` menggunakan fungsi realpath agar letaknya tidak hilang walau FUSE berjalan di latar . Terakhir, sistem FUSE resmi dijalankan melalui panggilan fungsi `fuse_main`.


**Output**


**Kendala**

Tidak ada kendala

</details>


