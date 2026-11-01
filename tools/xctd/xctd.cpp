// XCTD (XCompress LZXTDECODE, magic 0x0FF512ED) offline decompressor.
// Format (cracked by the rexauto project; QuickBMS unxmemlzx / UniPyX prior art):
//   off0  u32 BE magic 0x0FF512ED
//   off4  u16 BE version 0x0100; off6 u16 reserved
//   off8  u32 crc/hash (ignored)
//   off12 u32 BE flags: bits0-3 window = 1<<(n+15); bits4-5 zbs = 0x8000<<n;
//         bits6-21 segment count (0 = raw payload at off16);
//         bits22-23 table width: 0 = 20-bit MSB-first packed, else BE32.
//   off16: table of segment UNCOMPRESSED sizes; body follows at
//         bdo = 16 + 4*ceil(bps*segs/32).
//   Chunk stream per segment: [BE16 size][payload]...; one chunk = one 32KB
//   LZX frame; segment k owns ceil(size_k/32768) chunks and is an independent
//   LZX stream. Layouts: CONTIGUOUS (one global stream; a BE16 0 is zero
//   padding of arbitrary byte length up to the next zbs boundary) or ZONED
//   (segment k alone in file zone [k*zbs,(k+1)*zbs), seg 0 at bdo; zone tail
//   is garbage). Output = segments concatenated.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// LZX decoding goes straight to libmspack's decoder core (lzxd_xmem.c, the
// SDK's lzxd.c with the XMemCompress pad-byte tweak). The SDK's lzx.cpp
// wrapper is deliberately not used: it is runtime C++ that has to be linked
// from an SDK build of the host platform, which made this tool a
// Linux/clang-only exercise. libmspack's public API needs no runtime at all,
// so this file builds unchanged on Linux and Windows (MSVC included) with
// nothing but the libmspack headers.
#include <system.h>  // mspack.h + macros.h
#include <lzx.h>

// lzxd_xmem.c is compiled as C, so the flag it defines carries C linkage. It
// has to be declared here, at namespace scope: a block-scope `extern int`
// gives it C++ linkage and the link fails on the mangled name (MSVC: LNK2019).
extern "C" int lzxd_xmem_no_uncompressed_realign;

namespace {

// libmspack only sees `struct mspack_file*`; these are the memory-backed
// files it reads the compressed chunk stream from and writes plaintext to.
struct MemFile {
  const uint8_t* read_base;
  uint8_t* write_base;
  size_t size;
  size_t pos;
};

struct MemSystem {
  mspack_system sys;  // first member: the callbacks receive this pointer
};

size_t remainingBytes(const MemFile* f) {
  return f->pos < f->size ? f->size - f->pos : 0;
}

int memRead(mspack_file* file, void* buffer, int bytes) {
  auto* f = reinterpret_cast<MemFile*>(file);
  if (bytes <= 0) return 0;
  size_t n = static_cast<size_t>(bytes);
  if (n > remainingBytes(f)) n = remainingBytes(f);
  if (n) std::memcpy(buffer, f->read_base + f->pos, n);
  f->pos += n;
  return static_cast<int>(n);
}

int memWrite(mspack_file* file, void* buffer, int bytes) {
  auto* f = reinterpret_cast<MemFile*>(file);
  if (bytes <= 0) return 0;
  size_t n = static_cast<size_t>(bytes);
  if (n > remainingBytes(f)) n = remainingBytes(f);
  if (n) std::memcpy(f->write_base + f->pos, buffer, n);
  f->pos += n;
  return static_cast<int>(n);
}

void* memAlloc(mspack_system*, size_t bytes) {
  return std::calloc(bytes ? bytes : 1, 1);
}

void memFree(void* ptr) { std::free(ptr); }

void memCopy(void* src, void* dest, size_t bytes) {
  std::memcpy(dest, src, bytes);
}

// One independent LZX stream: in -> out, window_size as a byte count (the
// format's window bits + 15). Same contract as the SDK's lzx_decompress()
// for the no-reference-window case this tool uses: 0 = success.
int lzxDecode(const void* in, size_t in_len, void* out, size_t out_len,
              uint32_t window_size) {
  if (!window_size || (window_size & (window_size - 1))) return 1;
  int window_bits = 0;
  for (uint32_t w = window_size; w > 1; w >>= 1) window_bits++;

  MemSystem mem{};
  mem.sys.read = memRead;
  mem.sys.write = memWrite;
  mem.sys.alloc = memAlloc;
  mem.sys.free = memFree;
  mem.sys.copy = memCopy;

  MemFile src{static_cast<const uint8_t*>(in), nullptr, in_len, 0};
  MemFile dst{nullptr, static_cast<uint8_t*>(out), out_len, 0};

  lzxd_stream* lzxd =
      lzxd_init(&mem.sys, reinterpret_cast<mspack_file*>(&src),
                reinterpret_cast<mspack_file*>(&dst), window_bits,
                0 /* reset_interval: none */, 0x8000 /* input buffer */,
                static_cast<off_t>(out_len), 0 /* is_delta */);
  if (!lzxd) return 1;
  const int rc = lzxd_decompress(lzxd, static_cast<off_t>(out_len));
  lzxd_free(lzxd);
  return rc;
}

}  // namespace

static uint32_t rd32(const uint8_t* b) {
  return (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) |
         b[3];
}
static uint32_t rd16(const uint8_t* b) { return (uint32_t(b[0]) << 8) | b[1]; }
static const uint32_t FRAME = 0x8000;

int main(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: %s <in> <out>\n", argv[0]);
    return 2;
  }
  FILE* fi = fopen(argv[1], "rb");
  if (!fi) { perror("in"); return 3; }
  fseek(fi, 0, SEEK_END);
  long fsz_l = ftell(fi);
  fseek(fi, 0, SEEK_SET);
  std::vector<uint8_t> data((size_t)fsz_l);
  if (fread(data.data(), 1, data.size(), fi) != data.size()) return 3;
  fclose(fi);
  const size_t fsz = data.size();

  if (fsz < 16 || rd32(&data[0]) != 0x0FF512ED || rd16(&data[4]) != 0x0100) {
    fprintf(stderr, "not XCTD (magic/version mismatch)\n");
    return 4;
  }
  uint32_t fl = rd32(&data[12]);
  uint32_t window = 1u << ((fl & 0xF) + 15);
  uint32_t zbs = 0x8000u << ((fl >> 4) & 3);
  uint32_t segs = (fl >> 6) & 0xFFFF;
  uint32_t bps = ((fl >> 22) & 3) ? 32 : 20;

  FILE* fo = fopen(argv[2], "wb");
  if (!fo) { perror("out"); return 3; }

  if (segs == 0) {
    fwrite(&data[16], 1, fsz - 16, fo);
    fclose(fo);
    fprintf(stderr, "raw: %zu -> %zu bytes (flags 0x%08x)\n", fsz, fsz - 16, fl);
    return 0;
  }

  std::vector<uint64_t> sizes(segs);
  size_t table_words = ((uint64_t)bps * segs + 31) / 32;
  size_t bdo = 16 + 4 * table_words;
  if (bdo > fsz) { fprintf(stderr, "table exceeds file\n"); return 4; }
  for (uint32_t k = 0; k < segs; k++) {
    if (bps == 32) {
      sizes[k] = rd32(&data[16 + 4 * (size_t)k]);
    } else {
      uint64_t bit = (uint64_t)k * 20, v = 0;
      for (int i = 0; i < 20; i++) {
        uint64_t b = bit + i;
        v = (v << 1) | ((data[16 + (b >> 3)] >> (7 - (b & 7))) & 1);
      }
      sizes[k] = v;
    }
  }
  auto frames_of = [&](uint32_t k) -> size_t {
    return (size_t)((sizes[k] + FRAME - 1) / FRAME);
  };
  uint64_t want = 0;
  for (uint32_t k = 0; k < segs; k++) want += frames_of(k);

  // Try CONTIGUOUS first: global walk, BE16 0 = zero pad to next zbs boundary.
  std::vector<std::pair<size_t, uint32_t>> chunks;
  bool contiguous_ok = true;
  {
    size_t p = bdo;
    while (p + 2 <= fsz) {
      if (data[p] == 0 && data[p + 1] == 0) {
        size_t np = ((p / zbs) + 1) * zbs;
        if (np > fsz) np = fsz;
        bool allz = true;
        for (size_t q = p; q < np; q++)
          if (data[q] != 0) { allz = false; break; }
        if (!allz) { contiguous_ok = false; break; }
        p = np;
        continue;
      }
      uint32_t sz = rd16(&data[p]);
      p += 2;
      if (p + sz > fsz) { contiguous_ok = false; break; }
      chunks.push_back({p, sz});
      p += sz;
    }
    if (contiguous_ok && p != fsz) {
      size_t q = p;
      while (q < fsz && data[q] == 0) q++;
      if (q != fsz) contiguous_ok = false;
    }
    if (contiguous_ok && want != chunks.size()) contiguous_ok = false;
  }

  // Fallback ZONED: segment k alone in zone k.
  std::vector<char> seg_stored(segs, 0);
  if (!contiguous_ok) {
    chunks.clear();
    for (uint32_t k = 0; k < segs; k++) {
      size_t zstart = (k == 0) ? bdo : (size_t)k * zbs;
      size_t zend = (size_t)(k + 1) * zbs;
      if (zend > fsz) zend = fsz;
      size_t p = zstart;
      // Stored segment (seen in the blitz.pak of Bejeweled): a leading
      // 0x0000 at the zone start marks sizes[k] raw bytes following the
      // marker; the zone is exactly marker + payload. Not LZX at all.
      if (p + 2 <= zend && rd16(&data[p]) == 0 &&
          p + 2 + (size_t)sizes[k] <= zend) {
        seg_stored[k] = 1;
        chunks.push_back({p + 2, (uint32_t)sizes[k]});
        continue;
      }
      for (size_t j = 0; j < frames_of(k); j++) {
        if (p + 2 > zend) {
          fprintf(stderr, "zoned: segment %u frame %zu runs past its zone\n", k, j);
          return 6;
        }
        uint32_t sz = rd16(&data[p]);
        p += 2;
        if (sz == 0 || p + sz > zend) {
          fprintf(stderr, "zoned: segment %u frame %zu bad chunk size %u at 0x%zx\n",
                  k, j, sz, p - 2);
          return 6;
        }
        chunks.push_back({p, sz});
        p += sz;
      }
    }
  }

  size_t ci = 0;
  uint64_t written = 0;
  std::vector<uint8_t> in, out;
  for (uint32_t k = 0; k < segs; k++) {
    if (!contiguous_ok && seg_stored[k]) {
      auto& c = chunks[ci++];
      fwrite(&data[c.first], 1, c.second, fo);
      written += c.second;
      continue;
    }
    size_t need = frames_of(k);
    in.clear();
    for (size_t j = 0; j < need; j++) {
      auto& c = chunks[ci + j];
      in.insert(in.end(), &data[c.first], &data[c.first] + c.second);
    }
    ci += need;
    out.assign((size_t)sizes[k], 0);
    int rc = lzxDecode(in.data(), in.size(), out.data(), out.size(), window);
    if (rc) {
      // XMemCompress dialect: no CAB realign pad after odd uncompressed
      // blocks. Segments are independent streams; retry this one flipped.
      lzxd_xmem_no_uncompressed_realign = 1;
      rc = lzxDecode(in.data(), in.size(), out.data(), out.size(), window);
      lzxd_xmem_no_uncompressed_realign = 0;
    }
    if (rc) {
      fprintf(stderr, "segment %u/%u decode failed rc=%d (size %llu, layout %s)\n",
              k, segs, rc, (unsigned long long)sizes[k],
              contiguous_ok ? "contiguous" : "zoned");
      return 5;
    }
    fwrite(out.data(), 1, (size_t)sizes[k], fo);
    written += sizes[k];
  }
  fclose(fo);
  fprintf(stderr, "%u segment(s), window 0x%x, zbs 0x%x, %s: %zu -> %llu bytes\n",
          segs, window, zbs, contiguous_ok ? "contiguous" : "zoned", fsz,
          (unsigned long long)written);
  return 0;
}
