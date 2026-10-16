#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qr.h"
#include "TinyPngOut.h"

#define INPUT_CAPACITY 4096
#define QUIET_ZONE 4
#define MODULE_SCALE 8

/*
 * Read one line of text from stdin.
 * Returns false if input fails or is too long for the input buffer.
 */
static bool readText(char text[], size_t capacity)
{
    if (fgets(text, (int)capacity, stdin) == NULL) {
        return false;
    }

    size_t length = strlen(text);

    /* If the newline was read, remove it. */
    if (length > 0 && text[length - 1] == '\n') {
        text[--length] = '\0';
    } else if (!feof(stdin)) {
        /*
         * The input may have exceeded the buffer. Consume the rest of the
         * line and report an error instead of silently encoding truncated text.
         */
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {
            /* Discard remaining characters on this line. */
        }

        fprintf(stderr, "Input is too long (maximum %zu characters).\n",
                capacity - 1);
        return false;
    }

    /* Also handle CRLF input when the file/terminal uses Windows line endings. */
    length = strlen(text);
    if (length > 0 && text[length - 1] == '\r') {
        text[length - 1] = '\0';
    }

    return true;
}

/*
 * Write the QR matrix to a PNG file using TinyPngOut.
 * Returns true if the image was written successfully.
 */
static bool writeQrPng(const uint8_t qrcode[], const char *filename)
{
    const int qrSize = qr_getSize(qrcode);
    const int imageSize = (qrSize + QUIET_ZONE * 2) * MODULE_SCALE;
    const size_t pixelCount = (size_t)imageSize * (size_t)imageSize;

    /* TinyPngOut expects an RGB buffer: three bytes per pixel. */
    if (pixelCount > SIZE_MAX / 3) {
        fprintf(stderr, "Image dimensions are too large.\n");
        return false;
    }

    uint8_t *pixels = malloc(pixelCount * 3);
    if (pixels == NULL) {
        fprintf(stderr, "Out of memory while creating the PNG image.\n");
        return false;
    }

    /* Start with a white image. */
    memset(pixels, 255, pixelCount * 3);

    /* Draw each dark QR module as a MODULE_SCALE x MODULE_SCALE square. */
    for (int y = 0; y < qrSize; y++) {
        for (int x = 0; x < qrSize; x++) {
            if (!qr_getModule(qrcode, x, y)) {
                continue;
            }

            const int startX = (x + QUIET_ZONE) * MODULE_SCALE;
            const int startY = (y + QUIET_ZONE) * MODULE_SCALE;

            for (int dy = 0; dy < MODULE_SCALE; dy++) {
                for (int dx = 0; dx < MODULE_SCALE; dx++) {
                    const int px = startX + dx;
                    const int py = startY + dy;
                    const size_t index =
                        ((size_t)py * (size_t)imageSize + (size_t)px) * 3;

                    pixels[index] = 0;
                    pixels[index + 1] = 0;
                    pixels[index + 2] = 0;
                }
            }
        }
    }

    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        perror("Could not open output PNG file");
        free(pixels);
        return false;
    }

    struct TinyPngOut png;
    enum TinyPngOut_Status status =
        TinyPngOut_init(&png, imageSize, imageSize, file);

    if (status != TINYPNGOUT_OK) {
        fprintf(stderr, "TinyPngOut_init failed (status %d).\n", (int)status);
        fclose(file);
        free(pixels);
        return false;
    }

    status = TinyPngOut_write(&png, pixels, pixelCount);

    free(pixels);

    if (fclose(file) != 0) {
        perror("Error closing output PNG file");
        return false;
    }

    if (status != TINYPNGOUT_OK) {
        fprintf(stderr, "TinyPngOut_write failed (status %d).\n", (int)status);
        return false;
    }

    return true;
}

int main(void)
{
    char text[INPUT_CAPACITY];

    /* Buffers sized for the largest QR version supported by qr.h. */
    uint8_t qrcode[QR_BUFFER_LEN_MAX] = {0};
    uint8_t tempBuffer[QR_BUFFER_LEN_MAX] = {0};

    printf("====================================\n");
    printf("          QR Code Generator\n");
    printf("====================================\n");
    printf("Enter the text or URL to encode:\n> ");
    fflush(stdout);

    if (!readText(text, sizeof(text))) {
        fprintf(stderr, "Failed to read input.\n");
        return EXIT_FAILURE;
    }

    if (text[0] == '\0') {
        fprintf(stderr, "Input cannot be empty. Please run the program again.\n");
        return EXIT_FAILURE;
    }

    printf("\nGenerating QR code...\n");

    const bool ok = qr_encodeText(
        text,
        tempBuffer,
        qrcode,
        QR_ECC_MEDIUM,
        QR_VERSION_MIN,
        QR_VERSION_MAX,
        QR_MASK_AUTO,
        true
    );

    if (!ok) {
        fprintf(stderr,
                "QR generation failed. The input may be too long to fit "
                "in a QR code.\n");
        return EXIT_FAILURE;
    }

    if (!writeQrPng(qrcode, "qrcode.png")) {
        fprintf(stderr, "Failed to write qrcode.png.\n");
        return EXIT_FAILURE;
    }

    const int qrSize = qr_getSize(qrcode);
    printf("\nQR code generated successfully.\n");
    printf("Output file: qrcode.png\n");
    printf("QR matrix: %d x %d modules\n", qrSize, qrSize);

    return EXIT_SUCCESS;
}