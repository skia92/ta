#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void wc(char*, int, char*);

int main(int argc, char* argv[]) {
    int file_size;
    char* text;
    FILE* fd;
    size_t result;

    printf("_main @ %p\n", main);

    if (!argv[1]) {
        fprintf(stderr, "You should specify an exact file name\n");
        exit(1);
    }

    fd = fopen(argv[1], "r");
    
    if (!fd) {
        fprintf(stderr, "No file in your directory.\n");
        exit(1);
    }

    fseek(fd, 0, SEEK_END);
    file_size = ftell(fd);
    rewind(fd);

    text = (char*) malloc(sizeof(char) * file_size);

    if (!text) {
        fprintf(stderr, "Memory error.\n");
        exit(1);
    }

    result = fread(text, 1, file_size, fd);

    if (result != file_size) {
        fprintf(stderr, "Read error.\n");
        exit(2);
    }

    wc(text, file_size, argv[1]);

    free(text);
    fclose(fd);


    return 0;
}

void wc(char* text, int file_size, char* file_name) {
    int nline = 0, nword = 0;
    int i;
    char* left;

    for(i = 0; i < file_size; i++) {
        if (text[i] == '\n') {
            nline++;
        }
    }

    left = strtok(text, " \n");
    while (left != NULL) {
        nword++;
        left = strtok(NULL, " \n");
    }
    

    fprintf(stdout, "\t%d\t%d\t%d %s\n", nline, nword, file_size, file_name);
}
