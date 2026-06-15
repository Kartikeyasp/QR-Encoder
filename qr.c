#include "qr.h"
#include<stddef.h>
#include<assert.h>
#include<string.h>
#include<stdint.h>
#include<stdlib.h>

//Forward declarations for private functions
static void appendBitsToBuffer(unsigned int val, int numBits, uint8_t buffer[], int *bitLen);
static void addEccAndInterleave(uint8_t data[], int version, enum qr_ecc ecl, uint8_t result[]);
static int getNumDataCodewords(int version, enum qr_ecc ecl);
static int getNumRawDataModules(int version);
static void reedSolomonComputeDivisor(int degree, uint8_t result[]);
static void reedSolomonComputeRemainder(const uint8_t data[], int dataLen, const uint8_t generator[], int degree, uint8_t result[]);
static void initTables(void);
static uint8_t reedSolomonComputeMultiply(uint8_t x, uint8_t y);
static void initializeFunctionModules(int version, uint8_t qrcode[]);
static void drawLightFunctionModules(uint8_t qrcode[], int version);
static void drawFormatBits(enum qr_ecc ecl, enum qr_mask mask, uint8_t qrtcode[]);
static int getAlignmentPattenPositions(int version, uint8_t result[7]);
static void fillRectangle(int left, int top, int width, int height, uint8_t qrcode[]);
static void drawCodewords(const uint8_t data[], int dataLen, uint8_t qrcode[]);
static void applyMask(const uint8_t functionModules[], uint8_t qrcode[], enum qr_mask mask);
static long getPenaltyScore(const uint8_t qrcode[]);
static int finderPenaltyCountPatterns(const int runHistory[7], int qrsize);
static int finderPenaltyTerminateAndCount(bool cuurentRunColor, int currentRunLength, int runHistory[7], int qrsize);
static void finderPenaltyAddHistory(int currentRunLength, int runHistory[7], int qrsize);
static bool getModuleBounded(const uint8_t qrcode[], int x, int y);
static void setModuleBounded(uint8_t qrcode[], int x, int y, bool isDark);
static void setModuleUnbounded(uint8_t qrcode[], int x, int y, bool isDark);
static bool getBit(int x, int i);
static int calcSegmentBitLength(enum qr_mode mode, size_t numChars);
static int getTotalBits(const struct qr_segment segs[], size_t len, int version);
static int numCharCountBits(enum qr_mode, int version);

// For generating error correction codes.
static const int8_t ECC_CODEWORDS_PER_BLOCK[4][41] = {
	// Version: (note that index 0 is for padding, and is set to an illegal value)
	//0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40    Error correction level
	{-1,  7, 10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28, 30, 28, 28, 28, 28, 30, 30, 26, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},  // Low
	{-1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26, 26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28},  // Medium
	{-1, 13, 22, 18, 26, 18, 24, 18, 22, 20, 24, 28, 26, 24, 20, 30, 24, 28, 28, 26, 30, 28, 30, 30, 30, 30, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},  // Quartile
	{-1, 17, 28, 22, 16, 22, 28, 26, 26, 24, 28, 24, 28, 22, 24, 24, 30, 28, 28, 26, 28, 30, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},  // High
};

#define QR_REED_SOLOMON_DEGREE_MAX 30  // Based on the table above

// For generating error correction codes.
static const int8_t NUM_ERROR_CORRECTION_BLOCKS[4][41] = {
	// Version: (note that index 0 is for padding, and is set to an illegal value)
	//0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40    Error correction level
	{-1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4,  4,  4,  4,  4,  6,  6,  6,  6,  7,  8,  8,  9,  9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25},  // Low
	{-1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5,  5,  8,  9,  9, 10, 10, 11, 13, 14, 16, 17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49},  // Medium
	{-1, 1, 1, 2, 2, 4, 4, 6, 6, 8, 8,  8, 10, 12, 16, 12, 17, 16, 18, 21, 20, 23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68},  // Quartile
	{-1, 1, 1, 2, 4, 4, 4, 5, 6, 8, 8, 11, 11, 16, 16, 18, 16, 19, 21, 25, 25, 25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81},  // High
};

//Log Antilog tables for GF(265) multiplication
static uint8_t gfLogTable[256];
static uint8_t gfAntiLogTable[512];

//For automatic mask pattern selection.
static const int PENALTY_N1 = 3;
static const int PENALTY_N2 = 3;
static const int PENALTY_N3 = 40;
static const int PENALTY_N4 = 10;



/**
 * Encodes the given text string to a QR Code, returning true if successful.
 * If the data is too long to fit any version in the given range at the given ECC level, then false is returned.
 * The smallest possible QR Code version withing the given range is automatically chosen.
 * If boostEcl is true, then ECC level of the result may be higher than the ecl argument if it can be done without incresing the version.
 * Mask is either between 0 to 7 to force that mask or auto to automatically choose an appropriate mask.
 */
bool qr_encodeText(const char *text, uint8_t tempBuffer[], uint8_t qrcode[], enum qr_ecc ecl, int minVersion, int maxVersion, enum qr_mask mask, bool boostEcl)
{
    size_t textLen = strlen(text);
    if(textLen == 0)
    {
        return qr_encodeSegmentsAdvanced(NULL, 0, ecl, minVersion, maxVersion, mask, boostEcl, tempBuffer, qrcode);
    }
    size_t bufLen = (size_t)QR_BUFFER_LEN_FOR_VERSION(maxVersion);

    struct qr_segment seg;
    if(qr_isNumeric(text))
    {
        if(qr_calcSegmentBufferSize(QR_MODE_NUMERIC, textLen) > bufLen)
        {
            goto fail;
        }
        seg = qr_makeNumeric(text, tempBuffer);
    }
    else if(qr_isAlphanumeric(text))
    {
        if(qr_calcSegmentBufferSize(QR_MODE_ALPHANUMERIC, textLen) > bufLen)
        {
            goto fail;
        }
        seg = qr_makeAlphanumeric(text, tempBuffer);
    }
    else
    {
        if(textLen > bufLen)
        {
            goto fail;
        }
        for(size_t i = 0; i < textLen; i++)
        {
            tempBuffer[i] = (uint8_t)text[i];
        }
        seg.mode = QR_MODE_BYTE;
        seg.bitLength = calcSegmentBitLength(seg.mode, textLen);
        if(seg.bitLength == -1)
        {
            goto fail;
        }
        seg.numChars = (int)textLen;
        seg.data = tempBuffer;
    }
    return qr_encodeSegmentsAdvanced(&seg, 1, ecl, minVersion, maxVersion, mask, boostEcl, tempBuffer, qrcode);

fail:
    qrcode[0] = 0;
    return false;
}

/**
 * Encodes the given binary data to a QR Code, returning true if successful.
 * If the data is too long to fit any version in the given range at the given ECC level, then false is returned.
 * The smallest possible QR Code version withing the given range is automatically chosen.
 * If boostEcl is true, then ECC level of the result may be higher than the ecl argument if it can be done without incresing the version.
 * Mask is either between 0 to 7 to force that mask or auto to automatically choose an appropriate mask.
 */
bool qr_encodeBinary(uint8_t dataAndTemp[], size_t dataLen, uint8_t qrcode[], enum qr_ecc ecl, int minVersion, int maxVersion, enum qr_mask mask, bool boostEcl)
{
    struct qr_segment seg;
    seg.mode = QR_MODE_BYTE;
    seg.bitLength = calcSegmentBitLength(seg.mode, dataLen);
    if(seg.bitLength == -1)
    {
        qrcode[0] = 0;
        return false;
    }
    seg.numChars = (int)dataLen;
    seg.data = dataAndTemp;
    return qr_encodeSegmentsAdvanced(&seg, 1, ecl, minVersion, maxVersion, mask, boostEcl, dataAndTemp, qrcode);
}

/**
 * Encodes the given segments to a QR Code, returning true if successful.
 * If the data is too long to fit in any version at the given ECC level, then false is returned.
 * The smallest possible QR Code version is automatically chosen for the output. The ECC level of the result may be higher than the ecl argument if it can be done without incresing the version.
 */
bool qr_encodeSegments(const struct qr_segment segs[], size_t len, enum qr_ecc ecl, uint8_t tempBuffer[], uint8_t qrcode[])
{
    return qr_encodeSegmentsAdvanced(segs, len, ecl, QR_VERSION_MIN, QR_VERSION_MAX, QR_MASK_AUTO, true, tempBuffer, qrcode);
}

/**
 * Encodes the given segments to a QR Code, returning true if successful.
 * If the data is too long to fit in any version at the given ECC level, then false is returned.
 * The smallest possible QR Code version is automatically chosen for the output. The ECC level of the result may be higher than the ecl argument if it can be done without incresing the version.
 */
bool qr_encodeSegmentsAdvanced(const struct qr_segment segs[], size_t len, enum qr_ecc ecl, int minVersion, int maxVersion, enum qr_mask mask, bool boostEcl, uint8_t tempBuffer[], uint8_t qrcode[])
{
    assert(segs != NULL || len == 0);
    assert(QR_VERSION_MIN <= minVersion && minVersion <= maxVersion && maxVersion <= QR_VERSION_MAX);
    assert(0 <= (int)ecl && (int)ecl <= 3 && -1 <= (int)mask && (int)mask <= 7);

    int version, dataUsedBits;
    for(version = minVersion; ; version++)
    {
        int dataCapacityBits = getNumDataCodewords(version, ecl) * 8; //Number of data bits available
        dataUsedBits = getTotalBits(segs, len, version);
        if(dataUsedBits != -1 && dataUsedBits <= dataCapacityBits)
        {
            break;
        }
        if(version >= maxVersion)
        {
            qrcode[0] = 0;
            return false;
        }
    }
    assert(dataUsedBits != -1);

    //Increase the error coorection level while the data stil fits in the current version number
    for(int i = (int)QR_ECC_MEDIUM; i <= (int)QR_ECC_HIGH; i++)
    {
        if(boostEcl && dataUsedBits <= getNumDataCodewords(version, (enum qr_ecc)i) * 8)
        {
            ecl = (enum qr_ecc)i;
        }
    }

    //Concatenate all segments to create the data bit string
    memset(qrcode, 0, (size_t) QR_BUFFER_LEN_FOR_VERSION(version) * sizeof(qrcode[0]));
    int bitLen = 0;
    for(size_t i = 0; i < len; i++)
    {
        const struct qr_segment *seg = &segs[i];
        appendBitsToBuffer((unsigned int)seg->mode, 4, qrcode, &bitLen);
        appendBitsToBuffer((unsigned int)seg->numChars, numCharCountBits(seg->mode, version), qrcode, &bitLen);
        for(int j = 0; j < seg->bitLength; j++)
        {
            int bit = (seg->data[j >> 3] >> (7 - (j & 7))) & 1;
            appendBitsToBuffer((unsigned int) bit, 1, qrcode, &bitLen);
        }
    }

    assert(bitLen == dataUsedBits);

    //Add terminator and pad up to a byte if applicable
    int dataCapacityBits = getNumDataCodewords(version, ecl) * 8;
    assert(bitLen <= dataCapacityBits);
    int terminatorBits = dataCapacityBits - bitLen;
    if(terminatorBits > 4)
    {
        terminatorBits = 4;    
    }
    appendBitsToBuffer(0, terminatorBits, qrcode, &bitLen);
    appendBitsToBuffer(0, (8 - bitLen % 8) % 8, qrcode, &bitLen);
    assert(bitLen % 8 == 0);

    //Pad with alternating bytes until data capacity is reached
    for(uint8_t padByte = 0xEC; bitLen < dataCapacityBits; padByte ^= 0xEC ^ 0x11)
    {
        appendBitsToBuffer(padByte, 8, qrcode, &bitLen);
    }

    //Compute ECC, draw modules
    addEccAndInterleave(qrcode, version, ecl, tempBuffer);
    initializeFunctionModules(version, qrcode);
    drawCodewords(tempBuffer, getNumRawDataModules(version) / 8, qrcode);
    drawLightFunctionModules(qrcode, version);
    initializeFunctionModules(version, tempBuffer);

    //Do masking)
    if(mask == QR_MASK_AUTO)
    {
        long minPenalty = __LONG_MAX__;
        for(int i = 0; i < 8; i++)
        {
            enum qr_mask msk = (enum qr_mask)i;
            applyMask(tempBuffer, qrcode, msk);
            long penalty = getPenaltyScore(qrcode);
            if(penalty < minPenalty)
            {
                mask = msk;
                minPenalty = penalty;
            }
            applyMask(tempBuffer, qrcode, msk);
        }
    }
    assert(0<= (int)mask && (int)mask <= 7);
    applyMask(tempBuffer, qrcode, mask);
    drawFormatBits(ecl, mask, qrcode);

    return true;
}



static void appendBitsToBuffer(unsigned int val, int numBits, uint8_t buffer[], int *bitLen)
{
    assert(0<=numBits && numBits<=16 && (unsigned long)val >> numBits == 0);
    //select the bits from MSB to LSB
    for(int i = numBits - 1; i>=0; i--,(*bitLen)++)
    {
        //buffer[*bitLen>>3] to select byte
        //(val>>i) & 1 to select
        //(7 - (*bitLen & 7)) to select position in the byte
        buffer[*bitLen >> 3] |= ((val >> i) & 1) << (7 - (*bitLen & 7));
    }
}

/**
 * Append error correction bytes to each block of the given data array then interleave byes from the blocks and store them in result array.
 */

static void addEccAndInterleave(uint8_t data[], int version, enum qr_ecc ecl, uint8_t result[])
{
    //Calculate parameter numbers
    assert(0<=(int)ecl && (int)ecl < 4 && QR_VERSION_MIN <= version && version<=QR_VERSION_MAX);
    int numBlocks = NUM_ERROR_CORRECTION_BLOCKS[(int)ecl][version];
    int blockEccLen = ECC_CODEWORDS_PER_BLOCK[(int)ecl][version];
    int rawCodewords = getNumRawDataModules(version)/8;
    int dataLen = getNumDataCodewords(version,ecl);
    int numShortBlocks = numBlocks - rawCodewords % numBlocks;
    int shortBlockDataLen = rawCodewords / numBlocks - blockEccLen;

    //Inititalizing log and antilog tables for GF(256) multiplication
    initTables();

    //Split data into blocks, calculate ECC and interleave
    uint8_t rs_Generator[QR_REED_SOLOMON_DEGREE_MAX];
    reedSolomonComputeDivisor(blockEccLen, rs_Generator);

    const uint8_t *dat = data;
    //For every block
    for(int i = 0; i < numBlocks; i++)
    {
        //Current block length
        int blockDataLen = shortBlockDataLen + (i < numShortBlocks ? 0 : 1);
        uint8_t *ecc = &data[dataLen]; //Temp storage
        //Generate ecc
        reedSolomonComputeRemainder(dat, blockDataLen, rs_Generator, blockEccLen, ecc);
        //Copy data
        for(int j = 0, k = i; j < blockDataLen; j++, k+= numBlocks)
        {
            if(j == shortBlockDataLen)
            {
                k -= numShortBlocks;
            }
            result[k] = dat[j];
        }
        //Copy ECC
        for(int j = 0, k = dataLen + i; j < blockEccLen; j++, k += numBlocks)
        {
            result[k] = ecc[j];
        }
        dat += blockDataLen;
    }
}


/**
 * Returns number of 8-bit codewords that can be used for storing data (excluding ECC) for given version and error correction level
 */

static int getNumDataCodewords(int version, enum qr_ecc ecl)
{
    assert(0 <= ecl && ecl < 4);
    return getNumRawDataModules(version) / 8 - ECC_CODEWORDS_PER_BLOCK[ecl][version] * NUM_ERROR_CORRECTION_BLOCKS[ecl][version];
}


/**
 * Returns the number of data bits that can be stored in QR of given version, after all function modules are excluded.
 */

static int getNumRawDataModules(int version)
{
    assert(QR_VERSION_MIN <= version && version <= QR_VERSION_MAX);
    //Total matrix minus finder patterns, separators, timing patterns, format information 
    int result = (16 * version + 128) * version + 64;
    //For alignment patterns
    if(version>=2)
    {
        int numAlign = version / 7 + 2;
        result -= (25 * numAlign - 10) * numAlign - 55;
        //For version information
        if (version >= 7)
        {
            result -= 36;
        }
    }
    return result;
}

/**
 * Computes Reed-Solomon ECC generator polynomial for given degree
 */

static void reedSolomonComputeDivisor(int degree, uint8_t result[])
{
    assert(1<=degree && degree<=QR_REED_SOLOMON_DEGREE_MAX);

    //Polynomials coefficients are stored from highest to lowest power, excluding leading term which is always 1.
    memset(result, 0, (size_t)degree * sizeof(result[0]));
    result[degree - 1] = 1; //Start with monomial x^0

    //Compute the product polynomial (x - r^0) * (x - r^1) * (x - r^2) * ... * (x - r^{degree - 1}),
    //r = 0x02 which is generator element for GF(256) mod 0x11D

    uint8_t root = 1;
    for(int i = 0; i<degree; i++)
    {
        //Multiply current product by (x - r^i)
        for(int j = 0; j < degree; j++)
        {
            result[j] = reedSolomonComputeMultiply(result[j], root);
            if(j+1 < degree)
            {
                result[j] ^= result[j+1];
            }
        }
        root = reedSolomonComputeMultiply(root, 0x02);
    }
}


/**
 * Computes Reed-Solomon error correction codeword for the given data and divisor polynomials.
 * All polynomils are in big endian and has implicit leading term 1.
 */

static void reedSolomonComputeRemainder(const uint8_t data[], int dataLen, const uint8_t generator[], int degree, uint8_t result[])
{
    assert(1<=degree && degree<=QR_REED_SOLOMON_DEGREE_MAX);
    memset(result, 0, (size_t)degree * sizeof(result[0]));
    for(int i = 0; i < dataLen; i++)
    {
        uint8_t factor = data[i] ^ result[0];
        memmove(&result[0], &result[1], (size_t)(degree - 1) * sizeof(result[0]));
        result[degree - 1] = 0;
        for(int j = 0; j < degree; j++)
        {
            result[j] ^= reedSolomonComputeMultiply(generator[j], factor);
        }
    }

}


/**
 * Initialize log and antilog tables for GF(256) multiplication
 */

static void initTables(void)
{
    uint16_t x = 1;
    gfLogTable[0] = 0;
    for(int i = 0; i<255; i++)
    {
        gfAntiLogTable[i] = (uint8_t)x;
        gfAntiLogTable[i+255] = (uint8_t)x;
        gfLogTable[x] = i;

        //Multiply by x (polynomial variable)
        x = x << 1;

        //If top bit overflows, reduce modulo the irreducible polynomial 0x11D;
        if(x & 0x100)
        {
            x ^= 0x11D;
        }
    }
}


/**
 * Returns the product of two given field elements in GF(2^8) reduced modulo the irreducible polynomial 0x11D
 */

static uint8_t reedSolomonComputeMultiply(uint8_t x, uint8_t y)
{
    if(x == 0 || y == 0)
    {
        return 0;
    }

    int lx = gfLogTable[x];
    int ly = gfLogTable[y];

    return gfAntiLogTable[lx+ly];
}


/**
 * Clears the given QR with light (0) modules for the given size,
 * then marks every function module as dark (1) 
 */
static void initializeFunctionModules(int version, uint8_t qrcode[])
{
    //Initialize QR code
    int qrsize = version * 4 + 17;
    memset(qrcode, 0, (size_t)((qrsize * qrsize + 7)/8 + 1) * sizeof(qrcode[0]));
    qrcode[0] = (uint8_t)qrsize;

    //Fill horizontal and vertical timing patterns
    fillRectangle(6, 0, 1, qrsize, qrcode);
    fillRectangle(0, 6, qrsize, 1, qrcode);

    //Fill 3 finder patterns (all corners except bottom right) and format bits
    fillRectangle(0, 0, 9, 9, qrcode);
    fillRectangle(qrsize - 8, 0, 8, 9, qrcode);
    fillRectangle(0, qrsize - 8, 9, 8, qrcode);

    //Fill alignment patterns
    uint8_t alignPatternPos[7];
    int numAlign = getAlignmentPattenPositions(version, alignPatternPos);
    for(int i = 0; i < numAlign; i++)
    {
        for (int j = 0; j < numAlign; j++)
        {
            //Don't draw on the three finder corners
            if(!((i == 0 && j == 0) || (i == 0 && j == numAlign - 1 ) || (i == numAlign - 1 && j == 0)))
            {
                fillRectangle(alignPatternPos[i] - 2, alignPatternPos[j] - 2, 5, 5, qrcode);
            }
        }
    }

    //Fill version blocks
    if(version >= 7)
    {
        fillRectangle(qrsize - 11, 0, 3, 6, qrcode);
        fillRectangle(0, qrsize - 11, 6, 3, qrcode);
    }
}

/**
 * Draw light function modules onto the given QR code without changing non-functional modules.
 * Does not draw format bits. 
 * Requires all function modules to be previously marked dark.
 */
static void drawLightFunctionModules(uint8_t qrcode[], int version)
{
    //Draw horizontal and vertical timing patterns
    int qrsize = qr_getSize(qrcode);
    for(int i = 7; i < qrsize - 7; i += 2)
    {
        setModuleBounded(qrcode, 6, i, false);
        setModuleBounded(qrcode, i, 6, false);
    }

    //Draw 3 finder patterns (all corners except bottom right)
    //4 Module radius around the center of finder pattern
    for(int dy = -4; dy <= 4; dy++)
    {
        for(int dx = -4; dx <= 4; dx++)
        {
            int dist = abs(dx);
            if(abs(dy) > dist)
            {
                dist = abs(dy);
            }
            if(dist == 2 || dist == 4)
            {
                setModuleUnbounded(qrcode, 3 + dx, 3 + dy, false);
                setModuleUnbounded(qrcode, qrsize - 4 + dx, 3 + dy, false);
                setModuleUnbounded(qrcode, 3 + dx, qrsize - 4 + dy, false);
            }
        }
    }

    //Draw alignment patterns
    uint8_t alignPatternPos[7];
    int numAlign = getAlignmentPattenPositions(version, alignPatternPos);
    for(int i = 0; i < numAlign; i++)
    {
        for(int j = 0; j < numAlign; j++)
        {
            if((i == 0 && j == 0) || (i == 0 && j == numAlign - 1) || (i == numAlign - 1 && j == 0))
            {
                continue;
            }
            //1 module radius around alignment pattern center
            for(int dy = -1 ; dy <= 1; dy++)
            {
                for(int dx = -1; dx <= 1; dx ++)
                {
                    setModuleBounded(qrcode, alignPatternPos[i] + dx, alignPatternPos[j] + dy, dx == 0 && dy == 0);
                }
            }
        }
    }

    //Draw version blocks
    if(version >= 7)
    {
        //Calulate error correction code and pack bits
        int rem = version;

        for(int i = 0; i < 12; i++)
        {
            rem = (rem << 1) ^ ((rem >> 11) & 1) * 0x1F25;
        }
        long bits = (long)version << 12 | rem;
        assert(bits >> 18 == 0);

        //Draw two copies
        for(int i = 0; i < 6; i++)
        {
            for(int j = 0; j < 3; j++)
            {
                int k = qrsize - 11 + j;
                setModuleBounded(qrcode, k, i, (bits & 1) != 0);
                setModuleBounded(qrcode, i, k, (bits & 1) != 0);
                bits = bits >> 1;
            }
        }
    }
}

/**
 * Draws two copies of format bits based on the given mask and error correction level.
 */
static void drawFormatBits(enum qr_ecc ecl, enum qr_mask mask, uint8_t qrcode[])
{
    //Calculate error correction code and pack bits
    assert(0 <= (int)mask && (int)mask <= 7);
    static const int table[] = {1, 0, 3, 2};
    int data = table[(int) ecl] << 3 | (int)mask;
    int rem = data;
    for(int i = 0; i < 10; i++)
    {
        rem = (rem << 1) ^ (rem >> 9) * 0x537;
    }  

    int bits = (data << 10 | rem) ^ 0x5412;
    assert(bits >> 15 == 0);

    //Draw first copy
    for(int i = 0; i<= 5; i++)
    {
        setModuleBounded(qrcode, 8, i, getBit(bits , i));
    }
    setModuleBounded(qrcode, 8, 7, getBit(bits, 6));
    setModuleBounded(qrcode, 8, 8, getBit(bits, 7));
    setModuleBounded(qrcode, 7, 8, getBit(bits, 8));
    for(int i = 9; i < 15; i++)
    {
        setModuleBounded(qrcode, 14 - i, 8, getBit(bits, i));
    }

    //Draw second copy
    int qrsize = qr_getSize(qrcode);
    for(int i = 0; i < 8; i++)
    {
        setModuleBounded(qrcode, qrsize - 1 - i, 8, getBit(bits, i));
    }
    for(int i = 8; i < 15; i++)
    {
        setModuleBounded(qrcode, 8, qrsize - 15 + i, getBit(bits, i));
    }
    setModuleBounded(qrcode, 8, qrsize - 8, true); //Always dark
}

/**
 * Stores an ascending list of positions of alignment patterns for this version number,
 * returningn the length of tht list.
 */
static int getAlignmentPattenPositions(int version, uint8_t result[7])
{
    assert(1 <= version && version <= 40);

    static const uint8_t table[41][7] = {
        {0},                          // dummy
        {0},                          // v1
        {6,18},
        {6,22},
        {6,26},
        {6,30},
        {6,34},
        {6,22,38},
        {6,24,42},
        {6,26,46},
        {6,28,50},
        {6,30,54},
        {6,32,58},
        {6,34,62},
        {6,26,46,66},
        {6,26,48,70},
        {6,26,50,74},
        {6,30,54,78},
        {6,30,56,82},
        {6,30,58,86},
        {6,34,62,90},
        {6,28,50,72,94},
        {6,26,50,74,98},
        {6,30,54,78,102},
        {6,28,54,80,106},
        {6,32,58,84,110},
        {6,30,58,86,114},
        {6,34,62,90,118},
        {6,26,50,74,98,122},
        {6,30,54,78,102,126},
        {6,26,52,78,104,130},
        {6,30,56,82,108,134},
        {6,34,60,86,112,138},
        {6,30,58,86,114,142},
        {6,34,62,90,118,146},
        {6,30,54,78,102,126,150},
        {6,24,50,76,102,128,154},
        {6,28,54,80,106,132,158},
        {6,32,58,84,110,136,162},
        {6,26,54,82,110,138,166},
        {6,30,58,86,114,142,170}
    };

    int n = (version == 1 ? 0 : version / 7 + 2);

    for (int i = 0; i < n; i++)
    {
        result[i] = table[version][i];
    }

    return n;
}

/**
 * Sets every module in the range [left : left + width] * [top : top * height] to dark.
 */
static void fillRectangle(int left, int top, int width, int height, uint8_t qrcode[])
{
    for(int dy = 0; dy < height; dy++)
    {
        for(int dx = 0; dx < width; dx++)
        {
            setModuleBounded(qrcode, left + dx, top + dy, true);
        }
    }
}

/**
 * Draws the raw codewords (including data and ECC) onto the given QR code. This requires the initial state of the QR to be dark at function modules and light at codeword modues
 */
static void drawCodewords(const uint8_t data[], int dataLen, uint8_t qrcode[])
{
    int qrsize = qr_getSize(qrcode);
    int i = 0; //Bit index into the data

    for(int right = qrsize - 1; right >= 1; right -= 2)
    {
        if(right == 6) right = 5; //Skip vertical timing pattern

        for(int vert = 0; vert < qrsize; vert++)
        {
            for(int j = 0; j < 2; j++)
            {
                int x = right - j;
                bool upward = ((right + 1) & 2) == 0;
                int y = upward ? qrsize - 1 - vert : vert;
                if(!getModuleBounded(qrcode, x, y) && i < dataLen * 8)
                {
                    bool dark = getBit(data[i >> 3], 7 - (i & 7));
                    setModuleBounded(qrcode, x, y, dark);
                    i++;
                }
            }
        }
    }
    assert(i == dataLen * 8);
}



/**
 * XORs the codeword modules in this QR Code with the given mask pattern.
 * The codeword bits must be drawn before masking.
 */
static void applyMask(const uint8_t functionModules[], uint8_t qrcode[], enum qr_mask mask)
{
    assert(0 <= (int)mask && (int)mask <= 7);
    int qrsize = qr_getSize(qrcode);
    for(int y = 0; y < qrsize; y++)
    {
        for(int x = 0; x < qrsize; x++)
        {
            if(getModuleBounded(functionModules, x, y))
            {
                continue;
            }
            bool invert;
            switch ((int)mask)
            {
            case 0:
                invert = (x + y) % 2 == 0;
                break;

            case 1:
                invert = y % 2 == 0;
                break;

            case 2:
                invert = x % 3 == 0;
                break;
            
            case 3:
                invert = (x + y) % 3 == 0;
                break;

            case 4:
                invert = (x/3 + y/2) % 2 == 0;
                break;

            case 5:
                invert = x * y % 2 + x * y % 3 == 0;
                break;

            case 6:
                invert = (x * y % 2 + x * y % 3) % 2 == 0;
                break;

            case 7:
                invert = ((x + y) % 2 + x * y % 3) % 2 == 0;
                break;
            
            default:
                assert(false);
                return;
            }

            bool val = getModuleBounded(qrcode, x, y);
            setModuleBounded(qrcode, x, y, val ^ invert);
        }
    }
}

/**
 * Calculates and returns the penalty score based on state of the given QR Code's current modules.
 */
static long getPenaltyScore(const uint8_t qrcode[])
{
    int qrsize = qr_getSize(qrcode);
    long result = 0;

    //Rows having same color, and finder like patterns
    for(int y = 0; y < qrsize; y++)
    {
        bool runColor = false;
        int runX = 0;
        int runHistory[7] = {0};
        for(int x = 0; x < qrsize; x++)
        {
            if(getModuleBounded(qrcode, x, y) == runColor)
            {
                runX++;
                if(runX == 5)
                {
                    result += PENALTY_N1;
                }
                else if(runX > 5)
                {
                    result++;
                }
            }
            else
            {
                finderPenaltyAddHistory(runX, runHistory, qrsize);
                if(!runColor)
                {
                    result+=finderPenaltyCountPatterns(runHistory, qrsize) * PENALTY_N3;
                }
                runColor = getModuleBounded(qrcode, x, y);
                runX = 1;
            }
        }
        result += finderPenaltyTerminateAndCount(runColor, runX, runHistory, qrsize) * PENALTY_N3;
    }

    //Columns having same color, and finder-like patterns
    for(int x = 0; x < qrsize; x++)
    {
        bool runColor = false;
        int runY = 0;
        int runHistory[7] = {0};
        for(int y = 0; y < qrsize; y++)
        {
            if(getModuleBounded(qrcode, x, y) == runColor)
            {
                runY++;
                if(runY == 5)
                {
                    result += PENALTY_N1;
                }
                else if(runY > 5)
                {
                    result++;
                }
            }
            else
            {
                finderPenaltyAddHistory(runY, runHistory, qrsize);
                if(!runColor)
                {
                    result += finderPenaltyCountPatterns(runHistory, qrsize) * PENALTY_N3;
                }
                runColor = getModuleBounded(qrcode, x, y);
                runY = 1;
            }
        }
        result += finderPenaltyTerminateAndCount(runColor, runY, runHistory, qrsize) * PENALTY_N3;
    }

    //2*2 blocks of modules having same color
    for(int y = 0; y < qrsize - 1; y++)
    {
        for(int x = 0; x < qrsize - 1; x++)
        {
            bool color = getModuleBounded(qrcode, x, y);
            if(color == getModuleBounded(qrcode, x + 1, y) && color == getModuleBounded(qrcode, x, y + 1) && getModuleBounded(qrcode, x+1, y+1))
            {
                result += PENALTY_N2;
            } 
        }
    }

    //Balance of dark and light modules
    int dark = 0;
    for(int y = 0; y < qrsize; y++)
    {
        for(int x = 0; x < qrsize; x++)
        {
            if(getModuleBounded(qrcode, x, y))
            {
                dark++;
            }
        }
    }

    int total = qrsize * qrsize;

    int k = (int)((labs(dark * 20L - total * 10L) + total - 1) / total) - 1;
    assert(0 <= k && k<= 9);
    result += k * PENALTY_N4;
    return result;
}

/**
 * Can only be called immediately after a light run is added, and returns either 0, 1 or 2. A helper for getPenaltyScore()
 */
static int finderPenaltyCountPatterns(const int runHistory[7], int qrsize)
{
    int n = runHistory[1];
    assert(n <= qrsize * 3); (void)qrsize;
    bool core = n > 0 && runHistory[2] == n && runHistory[3] == n*3 && runHistory[4] == n && runHistory[5] == n;
    
    return (core && runHistory[0] >= n * 4 && runHistory[6] >= n ? 1 : 0) + (core && runHistory[6] >= n * 4 && runHistory[0] >= n ? 1 : 0);
}

/**
 * Must be called at the end of a line of modules. A helper function for getPenaltyScore()
 */
static int finderPenaltyTerminateAndCount(bool cuurentRunColor, int currentRunLength, int runHistory[7], int qrsize)
{
    if(cuurentRunColor) //Terminated dark run
    {
        finderPenaltyAddHistory(currentRunLength, runHistory, qrsize);
        currentRunLength = 0;
    }
    currentRunLength += qrsize; //Add light border
    finderPenaltyAddHistory(currentRunLength, runHistory, qrsize);
    return finderPenaltyCountPatterns(runHistory, qrsize);
}

/**
 * Pushes the given value to the front and drops the last value. A helper function for getPenaltyScore().
 */
static void finderPenaltyAddHistory(int currentRunLength, int runHistory[7], int qrsize)
{
    if(runHistory[0] == 0)
    {
        currentRunLength += qrsize; 
    }
    memmove(&runHistory[1], &runHistory[0], 6 * sizeof(runHistory[0]));
    runHistory[0] = currentRunLength;
}


/**
 * Returns the side length of the given QR code, assuming that encoding succeeded.
 * Result is in range [21, 177].
 */
int qr_getSize(const uint8_t qrcode[])
{
    assert(qrcode != NULL);
    int result = qrcode[0];
    assert((QR_VERSION_MIN * 4 + 17) <= result && result <=(QR_VERSION_MAX * 4 + 17));
    return result;
}

/**
 * Returs the color of the module at the given coordinates, which is false for light or true for dark.
 * The top left corner has the coordinates (x = 0, y = 0).
 * If the given coordinates are out of bounds, then false (light) is returned.
 */
bool qr_getModule(const uint8_t qrcode[], int x, int y)
{
    assert(qrcode != NULL);
    int qrsize = qrcode[0];
    return(0 <= x && x < qrsize && 0 <= y && y < qrsize) && getModuleBounded(qrcode, x, y);
}


/**
 * Return the color of the module at the given coordinates, which must be in bounds.
 */
static bool getModuleBounded(const uint8_t qrcode[], int x, int y)
{
    int qrsize = qr_getSize(qrcode);
    assert(0 <= x && x < qrsize && 0<=y && y < qrsize);
    //Bit index
    int index = y * qrsize + x;
    //(index>>3) + 1 gives byte
    // index & 7 gives bit in that byte
    return getBit(qrcode[(index >> 3)  + 1], index & 7);
}

/**
 * Sets the color of the module at the given coordinates, which must be in bounds.
 */
static void setModuleBounded(uint8_t qrcode[], int x, int y, bool isDark)
{
    int qrsize = qr_getSize(qrcode);
    assert(0 <= x && x < qrsize && 0 <= y && y < qrsize);
    int index = y * qrsize + x;
    int bitIndex = index & 7;
    int byteIndex = (index >> 3) + 1;
    if(isDark)
    {
        qrcode[byteIndex] |= (1 << bitIndex);
    }
    else
    {
        qrcode[byteIndex] &= (1 << bitIndex) ^ 0xFF;
    }
}

/**
 * Sets the color of the module at the given coordinates, does nothing if out of bounds.
 */
static void setModuleUnbounded(uint8_t qrcode[], int x, int y, bool isDark)
{
    int qrsize = qr_getSize(qrcode);
    if(0 <= x && x < qrsize && 0 <= y && y < qrsize)
    {
        setModuleBounded(qrcode, x, y, isDark);
    }
}

/**
 * Returns true iff the i'th bit of x is set to 1. 
 * Requires x>=0 and 0<=i<=14.
 */
static bool getBit(int x, int i)
{
    return ((x >> i) & 1) != 0;
}



/**
 * Tests whether the given stirng can be encoded in numeric mode.
 */
bool qr_isNumeric(const char *text)
{
    assert(text != NULL);
    for(; *text != '\0'; text++)
    {
        if(*text < '0' || *text > '9')
        {
            return false;
        }
    }
    return true;
}

/**
 * Tests whether the given string can be encoded in alphanumeric mode.
 */
bool qr_isAlphanumeric(const char *text)
{
    const char *ALPHANUMERIC_CHARACTERSET = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";
    assert(text != NULL);
    for( ; *text != '\0'; text++)
    {
        if(strchr(ALPHANUMERIC_CHARACTERSET, *text) == NULL)
        {
            return false;
        }
    }
    return true;
}

/**
 * Returns the number of data bits needed to represent a segment
 * containing the given number of characters using the given mode.
 * Returns -1 on failure.
 */
static int calcSegmentBitLength(enum qr_mode mode, size_t numChars)
{
    if(numChars > (unsigned int)INT16_MAX)
    {
        return -1;
    }

    long result = (long)numChars;

    if(mode == QR_MODE_NUMERIC)
    {
        result = (result * 10 + 2)/3;
        //10 bits per 3 digits
    }
    else if(mode == QR_MODE_ALPHANUMERIC)
    {
        result = (result * 11 + 1)/2;
        //11 bits per 2 characters
    }
    else if(mode == QR_MODE_BYTE)
    {
        result = result * 8;
    }
    else if(mode == QR_MODE_KANJI)
    {
        result = result * 13;
        //13 bits per JIS Kanji
    }
    else if(mode == QR_MODE_ECI && numChars == 0)
    {
        result = 3*8;
    }
    else
    {
        assert(false);
        return -1;
    }
    assert(result>=0);
    if(result > INT16_MAX)
    {
        return -1;
    }
    return (int)result;
}

/**
 * Returns the number of bytes needed for the data buffer for a segment
 * containing the given number of characters using the given mode.
 */
size_t qr_calcSegmentBufferSize(enum qr_mode mode, size_t numChars)
{
    int temp = calcSegmentBitLength(mode, numChars);
    if(temp == -1)
    {
        return SIZE_MAX;
    }
    assert(0 <= temp && temp <= INT16_MAX);
    return ((size_t)temp + 7) / 8;
}

/**
 * Returns a segment representing given text string encoded in numeric mode.
 */

struct qr_segment qr_makeNumeric(const char *digits, uint8_t buffer[])
{
    assert(digits!=NULL);
    struct qr_segment result;
    //Number of digits (unencoded)
    size_t len = strlen(digits);
    result.mode = QR_MODE_NUMERIC;
    int bitLen = calcSegmentBitLength(result.mode, len);
    assert(bitLen != -1);
    result.numChars = (int)len;
    if(bitLen > 0)
    {
        //Initialize to 0 enough bytes to hold bitLength number of bits
        memset(buffer, 0, ((size_t)bitLen + 7) / 8 * sizeof(buffer[0]));
    }
    result.bitLength = 0;

    unsigned int accumuData = 0;
    int accumCount = 0;
    //Take digits one by one
    for( ; *digits != '\0'; digits++)
    {
        char c = *digits;
        assert('0' <= c && c <= '9');
        accumuData = accumuData * 10 + (unsigned int)(c - '0');
        accumCount++;
        //Take in groups of 3 digits
        if(accumCount == 3)
        {
            //add to buffer
            appendBitsToBuffer(accumuData, 10, buffer, &result.bitLength);
            accumCount = 0;
            accumuData = 0;
        }
    }
    if(accumCount > 0) // 1 or 2 digits remaining
    {
        appendBitsToBuffer(accumuData, accumCount * 3 + 1, buffer,&result.bitLength);
    }
    assert(result.bitLength == bitLen);
    result.data = buffer;
    return result;
}

/* 
 * Returns a segment representing the given text string encoded in alphanumeric mode.
 * The characters allowed are: 0 to 9, A to Z (uppercase only), space,
 * dollar, percent, asterisk, plus, hyphen, period, slash, colon.
 */
struct qr_segment qr_makeAlphanumeric(const char *text, uint8_t buffer[])
{
    const char *ALPHANUMERIC_CHARSET = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";
    assert(text != NULL);
    struct qr_segment result;
    size_t len = strlen(text);
    result.mode = QR_MODE_ALPHANUMERIC;
    int bitLen = calcSegmentBitLength(result.mode, len);
    assert(bitLen != -1);
    result.numChars = (int)len;
    if(bitLen > 0)
    {
        memset(buffer, 0, ((size_t)bitLen + 7)/ 8 * sizeof(buffer[0]));
    }
    result.bitLength = 0;

    unsigned int accumData = 0;
    int accumCount = 0;
    for(; *text != '\0'; text++)
    {
        const char *temp = strchr(ALPHANUMERIC_CHARSET, *text);
        assert(temp != NULL);
        accumData = accumData * 45 + (unsigned int)(temp - ALPHANUMERIC_CHARSET);
        accumCount++;
        if(accumCount == 2)
        {
            appendBitsToBuffer(accumData, 11, buffer, &result.bitLength);
            accumData = 0;
            accumCount = 0;
        }
    }
    if(accumCount > 0) //1 character remaining
    {
        appendBitsToBuffer(accumData, 6, buffer, &result.bitLength);
    }
    assert(result.bitLength == bitLen);
    result.data = buffer;
    return result;
}

/**
 * Calculates the number of bits needed to encode the given segments at the given version.
 * Returns a non-negative number if successful. Otherwise returns -1 if a segment 
 * has too many characters to fit its length field, or the total bits exceeds INT16_MAX.
 */
static int getTotalBits(const struct qr_segment segs[], size_t len, int version)
{
    assert(segs != NULL || len == 0);
    long result = 0;
    for(size_t i = 0; i < len; i++)
    {
        int numChars = segs[i].numChars;
        int bitLength = segs[i].bitLength;
        assert(0 <= numChars && numChars <= INT16_MAX);
        assert(0 <= bitLength && bitLength <= INT16_MAX);
        int ccbits = numCharCountBits(segs[i].mode, version);
        assert(0 <= ccbits && ccbits <= 16);
        if(numChars >= (1L << ccbits))
        {
            return -1;
        }
        result += 4L + ccbits + bitLength;
        if(result > INT16_MAX)
        {
            return -1;
        }
    }

    assert(0 <= result && result <= INT16_MAX);
    return (int)result;
}

/**
 * Returns the bit width of the character count field for a segment in the given mode
 * ina QR code in the given version number
 */
static int numCharCountBits(enum qr_mode mode, int version)
{
    assert(QR_VERSION_MIN <= version && version <= QR_VERSION_MAX);
    int i = (version + 7) / 17;
    switch(mode)
    {
        case QR_MODE_NUMERIC:
        {
            static const int temp[] = {10, 12, 14}; 
            return temp[i];
        }    
        case QR_MODE_ALPHANUMERIC:
        {
            static const int temp[] = {9, 11, 13}; 
            return temp[i];
        }    
        case QR_MODE_BYTE:
        {
            static const int temp[] = {8, 16, 16}; 
            return temp[i];
        }    
        case QR_MODE_KANJI:
        {
            static const int temp[] = {8, 10, 12}; 
            return temp[i];
        }    
        case QR_MODE_ECI:
            return 0;
        default:
            assert(false); 
            return -1; //Dummy value
    }
}