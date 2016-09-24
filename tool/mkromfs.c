#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <dirent.h>
#include <string.h>

#define hash_init 5381

uint32_t hash_djb2(const uint8_t * str, uint32_t hash) {
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) ^ c;

    return hash;
}

void usage(const char * binname) {
    printf("Usage: %s [-d <dir>] [outfile]\n", binname);
    exit(-1);
}

void processdir(DIR * dirp, const char * curpath, FILE * outfile, const char * prefix) {
    char fullpath[1024];
    char buf[16 * 1024];
    struct dirent * ent;
    DIR * rec_dirp;
    uint32_t cur_hash = hash_djb2((const uint8_t *) curpath, hash_init);
    uint32_t size, w, hash,hash_path;
    uint8_t b;
    FILE * infile;

    printf("--> c:[%s], p:[%s] <--\r\n", curpath, prefix);

    while ((ent = readdir(dirp))) {
        strcpy(fullpath, prefix);
        strcat(fullpath, "/");
        strcat(fullpath, curpath);
        strcat(fullpath, ent->d_name);

        printf(" --> d:[%s] T:[%d] \r\n", ent->d_name, ent->d_type);
    #ifdef _WIN32
        if (GetFileAttributes(fullpath) & FILE_ATTRIBUTE_DIRECTORY) {
    #else
        if (ent->d_type == DT_DIR) {
    #endif
            // recursively process files unde sub-directories
            if (strcmp(ent->d_name, ".") == 0)
                continue;
            if (strcmp(ent->d_name, "..") == 0)
                continue;
            strcat(fullpath, "/");
            rec_dirp = opendir(fullpath);
            processdir(rec_dirp, fullpath + strlen(prefix) + 1, outfile, prefix);
            closedir(rec_dirp);
        } else {
            // save file into romfs
            hash = hash_djb2((const uint8_t *) ent->d_name, cur_hash);
            hash_path = hash_djb2((const uint8_t *) curpath, cur_hash);
            infile = fopen(fullpath, "rb");
            if (!infile) {
                perror("opening input file");
                exit(-1);
            }
            /*
             * saved info:
             *   file name hash : 4B
             *   size : 4B
             *   file path hash : 4B
             *   file name : N
             *   0 : 1B
             *   file content : N
             */

            // save file name hash in outfile as  little endian
            b = (hash >>  0) & 0xff; fwrite(&b, 1, 1, outfile);
            b = (hash >>  8) & 0xff; fwrite(&b, 1, 1, outfile);
            b = (hash >> 16) & 0xff; fwrite(&b, 1, 1, outfile);
            b = (hash >> 24) & 0xff; fwrite(&b, 1, 1, outfile);
            // move to end of infile
            fseek(infile, 0, SEEK_END);
            // size = file content len + file name len + 1
            size = ftell(infile) + strlen(ent->d_name) + 1;
            // move to start of infile
            fseek(infile, 0, SEEK_SET);
            // save total size
            b = (size >>  0) & 0xff; fwrite(&b, 1, 1, outfile);
            b = (size >>  8) & 0xff; fwrite(&b, 1, 1, outfile);
            b = (size >> 16) & 0xff; fwrite(&b, 1, 1, outfile);
            b = (size >> 24) & 0xff; fwrite(&b, 1, 1, outfile);
            // save file path hash
            b = (hash_path >>  0) & 0xff; fwrite(&b, 1, 1, outfile);
            b = (hash_path >>  8) & 0xff; fwrite(&b, 1, 1, outfile);
            b = (hash_path >> 16) & 0xff; fwrite(&b, 1, 1, outfile);
            b = (hash_path >> 24) & 0xff; fwrite(&b, 1, 1, outfile);
            // save file name
            fwrite(ent->d_name,strlen(ent->d_name),1,outfile);
            // save a single char '0'
            b = 0;fwrite(&b,1,1,outfile);
            // calc file content size
            size = size - strlen(ent->d_name) - 1;
            while (size) {
                // buffer size is limited at 16K
                w = size > 16 * 1024 ? 16 * 1024 : size;
                // copy file content from infile to outfile
                fread(buf, 1, w, infile);
                fwrite(buf, 1, w, outfile);
                size -= w;
            }
            fclose(infile);
        }
    }
}

int main(int argc, char ** argv) {
    char * binname = *argv++;
    char * o;
    char * outname = NULL;
    char * dirname = ".";
    uint64_t z = 0;
    FILE * outfile;
    DIR * dirp;

    while ((o = *argv++)) {
        if (*o == '-') {
            o++;
            switch (*o) {
            case 'd':
                dirname = *argv++;
                break;
            default:
                usage(binname);
                break;
            }
        } else {
            if (outname)
                usage(binname);
            outname = o;
        }
    }

    if (!outname)
        outfile = stdout;
    else
        outfile = fopen(outname, "wb");

    if (!outfile) {
        perror("opening output file");
        exit(-1);
    }

    dirp = opendir(dirname);
    if (!dirp) {
        perror("opening directory");
        exit(-1);
    }

    processdir(dirp, "", outfile, dirname);
    fwrite(&z, 1, 8, outfile);
    if (outname)
        fclose(outfile);
    closedir(dirp);

    return 0;
}
