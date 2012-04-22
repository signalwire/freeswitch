#include <cassert>
#include <string>
#include <vector>
#include <bitset>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
using girerr::throwf;
#include "xmlrpc-c/base64.hpp"

using namespace std;
using namespace xmlrpc_c;


namespace {

char const table_a2b_base64[] = {
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
    52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1, 0,-1,-1, /* Note PAD->0 */
    -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
    -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
};

char const base64Pad('=');
size_t const base64MaxChunkSize(57);
     // Max binary chunk size (76 character line)
#define BASE64_LINE_SZ 128      /* Buffer size for a single line. */    

unsigned char const table_b2a_base64[] =
   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

} // namespace



class bitBuffer {
public:
    bitBuffer() : bitsInBuffer(0) {};

    void
    shiftIn8Bits(unsigned char const newBits) {
        // Shift in 8 bits to the right end of the buffer

        this->buffer = (this->buffer << 8) | newBits;
        this->bitsInBuffer += 8;

        assert(this->bitsInBuffer <= 12);
    }

    void
    shiftIn6Bits(unsigned char const newBits) {
        // Shift in 6 bits to the right end of the buffer

        this->buffer = (this->buffer << 6) | newBits;
        this->bitsInBuffer += 6;

        assert(this->bitsInBuffer <= 12);
    }

    void
    shiftOut6Bits(unsigned char * const outputP) {
        // Shift out 6 bits from the left end of the buffer

        assert(bitsInBuffer >= 6);

        *outputP = (this->buffer >> (this->bitsInBuffer - 6)) & 0x3f;
        this->bitsInBuffer -= 6;
    }

    void
    shiftOut8Bits(unsigned char * const outputP) {
        // Shift out 8 bits from the left end of the buffer

        assert(bitsInBuffer >= 8);

        *outputP = (this->buffer >> (this->bitsInBuffer - 8)) & 0x3f;
        this->bitsInBuffer -= 8;
    }

    void
    shiftOutResidue(unsigned char * const outputP) {
        // Shift out the residual 2 or 4 bits, padded on the right with 0
        // to 6 bits.

        while (this->bitsInBuffer < 6) {
            this->buffer <<= 2;
            this->bitsInBuffer += 2;
        }
        
        this->shiftOut6Bits(outputP);
    }

    void
    discardResidue() {
        assert(bitsInBuffer < 8);
        
        this->bitsInBuffer = 0;
    }
    
    unsigned int
    bitCount() {
        return bitsInBuffer;
    }

private:
    unsigned int buffer;
    unsigned int bitsInBuffer;
};


namespace xmlrpc_c {


static void
encodeChunk(vector<unsigned char> const& bytes,
            size_t                const  lineStart,
            size_t                const  chunkSize,
            string *              const  outputP) {
    
    bitBuffer buffer;
        // A buffer in which we accumulate bits (up to 12 bits)
        // until we have enough (6) for a base64 character.

    // I suppose this would be more efficient with an iterator on
    // 'bytes' and/or *outputP.  I'd have to find out how to use one.

    for (size_t linePos = 0; linePos < chunkSize; ++linePos) {
        // Shift the data into our buffer
        buffer.shiftIn8Bits(bytes[lineStart + linePos]);
        
        // Encode any complete 6 bit groups
        while (buffer.bitCount() >= 6) {
            unsigned char theseBits;
            buffer.shiftOut6Bits(&theseBits);
            outputP->append(1, table_b2a_base64[theseBits]);
        }
    }
    if (buffer.bitCount() > 0) {
        // Handle residual bits in the buffer
        unsigned char theseBits;
        buffer.shiftOutResidue(&theseBits);
    
        outputP->append(1, table_b2a_base64[theseBits]);

        // Pad to a multiple of 4 characters (24 bits)
        assert(outputP->length() % 4 > 0);
        outputP->append(4-outputP->length() % 4, base64Pad);
    } else {
        assert(outputP->length() % 4 == 0);
    }
}



string
base64FromBytes(vector<unsigned char> const& bytes,
                newlineCtl            const  newlineCtl) {

    string retval;

    if (bytes.size() == 0) {
        if (newlineCtl == NEWLINE_YES)
            retval = "\r\n";
        else
            retval = "";
    } else {
        // It would be good to preallocate retval.  Need to look up
        // how to do that.
        for (size_t chunkStart = 0;
             chunkStart < bytes.size();
             chunkStart += base64MaxChunkSize) {

            size_t const chunkSize(
                min(base64MaxChunkSize, bytes.size() - chunkStart));
    
            encodeChunk(bytes, chunkStart, chunkSize, &retval);

            if (newlineCtl == NEWLINE_YES)
                // Append a courtesy crlf
                retval += "\r\n";
        }
    }
    return retval;
}



vector<unsigned char>
bytesFromBase64(string const& base64) {

    vector<unsigned char> retval;
    bitBuffer buffer;
    unsigned int npad;

    npad = 0;  // No pad characters seen yet

    for (unsigned int cursor = 0; cursor < base64.length(); ++cursor) {
        char const thisChar(base64[cursor] & 0x7f);

        if (thisChar == '\r' || thisChar == '\n' || thisChar == ' ') {
            // ignore this punctuation
        } else {
            if (thisChar == base64Pad) {
                // This pad character is here to synchronize a chunk to 
                // a multiple of 24 bits (4 base64 characters; 3 bytes).
                buffer.discardResidue();
            } else {
                unsigned int const tableIndex(thisChar);
                if (table_a2b_base64[tableIndex] == -1)
                    throwf("Contains non-base64 character "
                           "with ASCII code 0x%02x", thisChar);
                
                buffer.shiftIn6Bits(table_a2b_base64[tableIndex]);
            
                if (buffer.bitCount() >= 8) {
                    unsigned char thisByte;
                    buffer.shiftOut8Bits(&thisByte);
                    retval.push_back(thisByte);
                }
            }
        }
    }

    if (buffer.bitCount() > 0)
        throwf("Not a multiple of 4 characters");

    return retval;
}

} //namespace
