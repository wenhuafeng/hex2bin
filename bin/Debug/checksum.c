#include <stdio.h>
#include <stdint.h>

uint32_t calculate_checksum(FILE *file)
{
    uint32_t checksum = 0x00;
    int byte;

    while ((byte = fgetc(file)) != EOF) {
        checksum += byte;
    }

    return checksum;
}

int main(int argc, char *argv[])
{
    uint32_t checksum;
    FILE *file = NULL;

    if (argc != 2) {
        return -1;
    }

    file = fopen(argv[1], "rb");
    if (file == NULL) {
        printf("Error opening file\n");
        return -1;
    }

    checksum = calculate_checksum(file);

    printf("Checksum: 0x%x\n", checksum);
    fclose(file);

    return 0;
}
