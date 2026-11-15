// png.cpp - minimal PNG encoder (no libraries). PNG stores the raster as
// filtered scanlines compressed with zlib/DEFLATE. We use DEFLATE's fixed
// Huffman tables plus LZ77 back-references, which compresses the large flat
// colour areas in our images very well.
#include "framebuffer.h"
#include <array>
#include <fstream>

namespace {

uint32_t crc32(const uint8_t* data, size_t n, uint32_t crc = 0) {
    static std::array<uint32_t, 256> table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    crc = ~crc;
    for (size_t i = 0; i < n; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

uint32_t adler32(const std::vector<uint8_t>& d) {
    uint32_t a = 1, b = 0;
    for (uint8_t v : d) {
        a = (a + v) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

struct BitWriter {
    std::vector<uint8_t>& out;
    uint32_t buf = 0;
    int n = 0;
    void put(uint32_t bits, int count) {  // DEFLATE packs bits least-significant first
        buf |= bits << n;
        n += count;
        while (n >= 8) {
            out.push_back(buf & 0xFF);
            buf >>= 8;
            n -= 8;
        }
    }
    void putHuffman(uint32_t code, int len) {  // Huffman codes are stored most-significant first
        uint32_t rev = 0;
        for (int i = 0; i < len; ++i) rev |= ((code >> i) & 1) << (len - 1 - i);
        put(rev, len);
    }
    void flush() {
        if (n > 0) out.push_back(buf & 0xFF);
        buf = 0;
        n = 0;
    }
};

void putLiteral(BitWriter& w, int sym) {  // fixed Huffman literal/length table (RFC 1951 3.2.6)
    if (sym < 144) w.putHuffman(0x30 + sym, 8);
    else if (sym < 256) w.putHuffman(0x190 + sym - 144, 9);
    else if (sym < 280) w.putHuffman(sym - 256, 7);
    else w.putHuffman(0xC0 + sym - 280, 8);
}

const int LEN_BASE[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                          31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const int LEN_EXTRA[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                           2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const int DIST_BASE[30] = {1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
                           193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
const int DIST_EXTRA[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                            6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

std::vector<uint8_t> zlibCompress(const std::vector<uint8_t>& in) {
    std::vector<uint8_t> out = {0x78, 0x01};  // zlib header: deflate, 32K window
    BitWriter w{out};
    w.put(1, 1);  // BFINAL: this is the only block
    w.put(1, 2);  // BTYPE = 01: fixed Huffman codes

    const int WINDOW = 32768, HASH = 1 << 15, MAX_CHAIN = 48;
    std::vector<int> head(HASH, -1), prev(in.size(), -1);
    auto hash3 = [&](size_t i) { return ((in[i] << 10) ^ (in[i + 1] << 5) ^ in[i + 2]) & (HASH - 1); };
    auto insert = [&](size_t i) {
        if (i + 2 >= in.size()) return;
        int h = hash3(i);
        prev[i] = head[h];
        head[h] = (int)i;
    };

    size_t i = 0;
    while (i < in.size()) {
        int bestLen = 0, bestDist = 0;
        if (i + 2 < in.size()) {  // LZ77: search earlier occurrences of the next 3 bytes
            int cand = head[hash3(i)];
            for (int chain = 0; cand >= 0 && chain < MAX_CHAIN; ++chain, cand = prev[cand]) {
                if ((int)i - cand > WINDOW) break;
                int len = 0;
                while (len < 258 && i + len < in.size() && in[cand + len] == in[i + len]) ++len;
                if (len > bestLen) {
                    bestLen = len;
                    bestDist = (int)i - cand;
                    if (len == 258) break;
                }
            }
        }
        if (bestLen >= 3) {  // emit <length, distance> pair
            int li = 28;
            while (LEN_BASE[li] > bestLen) --li;
            putLiteral(w, 257 + li);
            w.put(bestLen - LEN_BASE[li], LEN_EXTRA[li]);
            int di = 29;
            while (DIST_BASE[di] > bestDist) --di;
            w.putHuffman(di, 5);
            w.put(bestDist - DIST_BASE[di], DIST_EXTRA[di]);
            for (int k = 0; k < bestLen; ++k) insert(i + k);
            i += bestLen;
        } else {  // emit a literal byte
            putLiteral(w, in[i]);
            insert(i);
            ++i;
        }
    }
    putLiteral(w, 256);  // end of block
    w.flush();
    uint32_t a = adler32(in);
    for (int s = 24; s >= 0; s -= 8) out.push_back((a >> s) & 0xFF);
    return out;
}

void put32be(std::vector<uint8_t>& v, uint32_t x) {
    for (int s = 24; s >= 0; s -= 8) v.push_back((x >> s) & 0xFF);
}

void writeChunk(std::ofstream& f, const char* type, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> buf;
    put32be(buf, (uint32_t)data.size());
    buf.insert(buf.end(), type, type + 4);
    buf.insert(buf.end(), data.begin(), data.end());
    put32be(buf, crc32(buf.data() + 4, buf.size() - 4));  // CRC covers type + data
    f.write(reinterpret_cast<const char*>(buf.data()), (std::streamsize)buf.size());
}

}  // namespace

bool Framebuffer::savePNG(const std::string& path) const {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    f.write(reinterpret_cast<const char*>(sig), 8);

    std::vector<uint8_t> ihdr;
    put32be(ihdr, (uint32_t)w_);
    put32be(ihdr, (uint32_t)h_);
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});  // 8 bits/channel, RGB, deflate, no filter, no interlace
    writeChunk(f, "IHDR", ihdr);

    std::vector<uint8_t> raw;
    raw.reserve((size_t)h_ * (w_ * 3 + 1));
    for (int y = 0; y < h_; ++y) {
        raw.push_back(0);  // filter type 0 (None) for every scanline
        for (int x = 0; x < w_; ++x) {
            const Color& c = data_[(size_t)y * w_ + x];
            raw.insert(raw.end(), {c.r, c.g, c.b});
        }
    }
    writeChunk(f, "IDAT", zlibCompress(raw));
    writeChunk(f, "IEND", {});
    return (bool)f;
}
