#pragma once

#include<stdbool.h>
#include<stdint.h>
#include<stddef.h>

/**
 * Error correction level.
 */

enum qr_ecc{
    QR_ECC_LOW = 0,  //UPTO 7% DAMAGE PROTECTION
    QR_ECC_MEDIUM,   // 15%
    QR_ECC_QUARTILE, // 25%
    QR_ECC_HIGH,     // 30%
};

/**
 * Mask pattern used.
 */
enum qr_mask
{
    QR_MASK_AUTO = -1,
    QR_MASK_0 = 0,
    QR_MASK_1 = 1,
    QR_MASK_2 = 2,
    QR_MASK_3 = 3,
    QR_MASK_4 = 4,
    QR_MASK_5 = 5,
    QR_MASK_6 = 6,
    QR_MASK_7 = 7,
};

/**
 * Describes which mode the data is encoded in.
 */

enum qr_mode{
    QR_MODE_NUMERIC = 0x1,
    QR_MODE_ALPHANUMERIC = 0x2,
    QR_MODE_BYTE = 0x4,
    QR_MODE_KANJI = 0x8,
    QR_MODE_ECI = 0x7,
};

/**
 * A segment of data in QR
 */
struct qr_segment{
    //Mode indicator for this segment
    enum qr_mode mode;

    //Length of this segment's unencoded data.
    //Number of characters for numeric/alphanumeric/kanji
    //Number of bytes for byte mode.
    int numChars;

    //Pointer to encoded bitstream, packed in bitwise big endian.
    uint8_t *data;

    //Number of valid data bits used in the buffer.
    int bitLength;
};


//Macro constans and functions

#define QR_VERSION_MIN 1  //Mininum QR version

#define QR_VERSION_MAX 40 //Maximum QR version

//Calculates the number of bytes needed to store any QR upto and including the given version number
#define QR_BUFFER_LEN_FOR_VERSION(n)  ((((n) * 4 + 17) * ((n) * 4 + 17) + 7) / 8 + 1)

//Worst case number of bytes needed to store on QR upto and including version 40
#define QR_BUFFER_LEN_MAX QR_BUFFER_LEN_FOR_VERSION(QR_VERSION_MAX)


/**
 * Encodes the given text string to a QR Code, returning true if successful.
 * If the data is too long to fit any version in the given range at the given ECC level, then false is returned.
 * The smallest possible QR Code version withing the given range is automatically chosen.
 * If boostEcl is true, then ECC level of the result may be higher than the ecl argument if it can be done without incresing the version.
 * Mask is either between 0 to 7 to force that mask or auto to automatically choose an appropriate mask.
 */
bool qr_encodeText(const char *text, uint8_t tempBuffer[], uint8_t qrcode[], enum qr_ecc ecl, int minVersion, int maxVersion, enum qr_mask mask, bool boostEcl);

/**
 * Encodes the given binary data to a QR Code, returning true if successful.
 * If the data is too long to fit any version in the given range at the given ECC level, then false is returned.
 * The smallest possible QR Code version withing the given range is automatically chosen.
 * If boostEcl is true, then ECC level of the result may be higher than the ecl argument if it can be done without incresing the version.
 * Mask is either between 0 to 7 to force that mask or auto to automatically choose an appropriate mask.
 */
bool qr_encodeBinary(uint8_t dataAndTemp[], size_t dataLen, uint8_t qrcode[], enum qr_ecc ecl, int minVersion, int maxVersion, enum qr_mask mask, bool boostEcl);

/**
 * Encodes the given segments to a QR Code, returning true if successful.
 * If the data is too long to fit in any version at the given ECC level, then false is returned.
 * The smallest possible QR Code version is automatically chosen for the output. The ECC level of the result may be higher than the ecl argument if it can be done without incresing the version.
 */
bool qr_encodeSegments(const struct qr_segment segs[], size_t len, enum qr_ecc ecl, uint8_t tempBuffer[], uint8_t qrcode[]);

/**
 * Encodes the given segments to a QR Code, returning true if successful.
 * If the data is too long to fit in any version at the given ECC level, then false is returned.
 * The smallest possible QR Code version is automatically chosen for the output. The ECC level of the result may be higher than the ecl argument if it can be done without incresing the version.
 */
bool qr_encodeSegmentsAdvanced(const struct qr_segment segs[], size_t len, enum qr_ecc ecl, int minVersion, int maxVersion, enum qr_mask mask, bool boostEcl, uint8_t tempBuffer[], uint8_t qrcode[]);

/**
 * Tests whether the given stirng can be encoded in numeric mode.
 */
bool qr_isNumeric(const char *text);

/**
 * Tests whether the given string can be encoded in alphanumeric mode.
 */

bool qr_isAlphanumeric(const char *text);

/**
 * Returns the number of bytes needed for the data buffer for a segment
 * containing the given number of characters using the given mode.
 */
size_t qr_calcSegmentBufferSize(enum qr_mode mode, size_t numChars);

/**
 * Returns a segment representing given text string encoded in numeric mode.
 */

struct qr_segment qr_makeNumeric(const char *digits, uint8_t buffer[]);

/**
 * Returns a segment representing given text string encoded in alphanumeric mode
 */

struct qr_segment qr_makeAlphanumeric(const char *text, uint8_t buffer[]);

/**
 * Returns the side length of the given QR code, assuming that encoding succeeded.
 * Result is in range [21, 177].
 */
int qr_getSize(const uint8_t qrcode[]);

/**
 * Returs the color of the module at the given coordinates, which is false for light or true for dark.
 * The top left corner has the coordinates (x = 0, y = 0).
 * If the given coordinates are out of bounds, then false (light) is returned.
 */
bool qr_getModule(const uint8_t qrcode[], int x, int y);
