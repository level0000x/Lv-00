import zlib
import os

base = os.path.dirname(os.path.abspath(__file__))
GGB_EOCD_SIG = 0x06054b50
GGB_CENTRAL_DIR_SIG = 0x02014b50
GGB_LOCAL_FILE_SIG = 0x04034b50

def u16_le(data, off): return data[off] | (data[off + 1] << 8)
def u32_le(data, off): return data[off] | (data[off + 1] << 8) | (data[off + 2] << 16) | (data[off + 3] << 24)

def find_eocd(data, size):
    window = min(65557, size)
    min_i = size - window
    i = size - 22
    while i >= min_i:
        if u32_le(data, i) == GGB_EOCD_SIG:
            return i
        if i == 0:
            break
        i -= 1
    return None

def central_find(data, size, eocd, target):
    total = u16_le(data, eocd + 10)
    cd_size = u32_le(data, eocd + 12)
    cd_offset = u32_le(data, eocd + 16)
    if cd_offset > size or cd_size > size - cd_offset:
        return None
    pos = cd_offset
    tlen = len(target)
    for e in range(total):
        if pos + 46 > size:
            return None
        if u32_le(data, pos) != GGB_CENTRAL_DIR_SIG:
            return None
        method = u16_le(data, pos + 10)
        csize = u32_le(data, pos + 20)
        usize = u32_le(data, pos + 24)
        nlen = u16_le(data, pos + 28)
        elen = u16_le(data, pos + 30)
        clen = u16_le(data, pos + 32)
        local_off = u32_le(data, pos + 42)
        name = data[pos + 46:pos + 46 + nlen]
        if name == target:
            return (local_off, csize, usize, method)
        pos = pos + 46 + nlen + elen + clen
    return None

def local_data_offset(data, size, local_off):
    if local_off > size or 30 > size - local_off:
        return None
    if u32_le(data, local_off) != GGB_LOCAL_FILE_SIG:
        return None
    nlen = u16_le(data, local_off + 26)
    elen = u16_le(data, local_off + 28)
    off = local_off + 30 + nlen + elen
    if off > size:
        return None
    return off

class BitReader:
    def __init__(self, src):
        self.src = src
        self.src_len = len(src)
        self.bit_pos = 0
    def read(self, n):
        if n > 32:
            return None
        val = 0
        for i in range(n):
            byte_off = self.bit_pos >> 3
            if byte_off >= self.src_len:
                return None
            b = (self.src[byte_off] >> (self.bit_pos & 7)) & 1
            val |= b << i
            self.bit_pos += 1
        return val
    def align(self):
        self.bit_pos = (self.bit_pos + 7) & ~7

def huff_build(lengths, n):
    counts = [0] * 16
    for i in range(n):
        counts[lengths[i]] += 1
    counts[0] = 0
    offs = [0] * 16
    for l in range(1, 16):
        offs[l] = offs[l - 1] + counts[l - 1]
    symbols = [0] * (288 + 32)
    for i in range(n):
        if lengths[i] != 0:
            symbols[offs[lengths[i]]] = i
            offs[lengths[i]] += 1
    toffs = [0] * 16
    toffs[1] = 0
    for l in range(2, 16):
        toffs[l] = toffs[l - 1] + counts[l - 1]
    return (counts, symbols, toffs)

def huff_decode(br, t):
    counts, symbols, toffs = t
    code = 0
    first = 0
    index = 0
    for l in range(1, 16):
        bit = br.read(1)
        if bit is None:
            return None
        code = (code << 1) | bit
        count = counts[l]
        if code - first < count:
            return symbols[index + (code - first)]
        index += count
        first += count
        first <<= 1
        code <<= 1
    return None

LEN_BASE = [3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258]
LEN_EXTRA = [0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0]
DIST_BASE = [1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577]
DIST_EXTRA = [0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13]

def inflate_lz77(br, lit, dist, dst, dst_cap, out_pos):
    while True:
        sym = huff_decode(br, lit)
        if sym is None:
            print("LZ77-FAIL: lit huff decode failed at bit_pos", br.bit_pos)
            return None
        if sym < 256:
            if out_pos >= dst_cap:
                print("LZ77-FAIL: literal out of cap", out_pos, dst_cap)
                return None
            dst[out_pos] = sym
            out_pos += 1
        elif sym == 256:
            return out_pos
        else:
            len_idx = sym - 257
            if len_idx < 0 or len_idx >= 29:
                print("LZ77-FAIL: bad len_idx", len_idx)
                return None
            extra = br.read(LEN_EXTRA[len_idx])
            if extra is None:
                print("LZ77-FAIL: len extra bit read failed at bit_pos", br.bit_pos)
                return None
            length = LEN_BASE[len_idx] + extra
            d_sym = huff_decode(br, dist)
            if d_sym is None:
                print("LZ77-FAIL: dist huff decode failed at bit_pos", br.bit_pos)
                return None
            if d_sym < 0 or d_sym >= 30:
                print("LZ77-FAIL: bad d_sym", d_sym)
                return None
            extra = br.read(DIST_EXTRA[d_sym])
            if extra is None:
                print("LZ77-FAIL: dist extra bit read failed at bit_pos", br.bit_pos)
                return None
            distance = DIST_BASE[d_sym] + extra
            if distance == 0 or distance > out_pos:
                print("LZ77-FAIL: bad distance", distance, "out_pos", out_pos)
                return None
            if out_pos + length > dst_cap:
                print("LZ77-FAIL: len exceeds cap", out_pos, length, dst_cap)
                return None
            for i in range(length):
                dst[out_pos + i] = dst[out_pos - distance + i]
            out_pos += length

CODE_ORDER = [16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15]

def huff_dynamic(br):
    hlit = br.read(5); hdist = br.read(5); hclen = br.read(4)
    if hlit is None or hdist is None or hclen is None:
        print("DYN-FAIL: header bit read at bit_pos", br.bit_pos)
        return None
    nlit = hlit + 257
    ndist = hdist + 1
    nclen = hclen + 4
    print("  DYN header: hlit=%d hdist=%d hclen=%d -> nlit=%d ndist=%d nclen=%d bit_pos=%d" %
          (hlit, hdist, hclen, nlit, ndist, nclen, br.bit_pos))
    if nlit > 288 or ndist > 32 or nclen > 19:
        print("DYN-FAIL: header range", nlit, ndist, nclen)
        return None
    cl_lengths = [0] * 19
    for i in range(nclen):
        v = br.read(3)
        if v is None:
            print("DYN-FAIL: clen read at bit_pos", br.bit_pos)
            return None
        cl_lengths[CODE_ORDER[i]] = v
    print("  DYN cl_lengths:", cl_lengths, "bit_pos=%d" % br.bit_pos)
    bits_str = "".join(str((raw[br.bit_pos // 8] >> (br.bit_pos % 8 + j)) & 1) for j in range(0) ) if False else ""
    nxt = br.bit_pos
    bits24 = []
    for j in range(24):
        byte_off = nxt >> 3
        if byte_off < len(raw):
            bits24.append(str((raw[byte_off] >> (nxt & 7)) & 1))
        nxt += 1
    print("  DYN next 24 bits:", "".join(bits24))
    cl_tree = huff_build(cl_lengths, 19)
    lengths = []
    total = nlit + ndist
    n = 0
    while n < total:
        sym = huff_decode(br, cl_tree)
        if sym is None:
            print("DYN-FAIL: cl huff decode at bit_pos", br.bit_pos, "n", n, "total", total)
            return None
        if sym < 16:
            lengths.append(sym); n += 1
        elif sym == 16:
            rep = br.read(2)
            if rep is None:
                print("DYN-FAIL: rep16 read at bit_pos", br.bit_pos)
                return None
            if n == 0:
                print("DYN-FAIL: rep16 with n=0")
                return None
            prev = lengths[n - 1]
            for i in range(rep + 3):
                if n >= total:
                    print("DYN-FAIL: rep16 overflow n", n, "total", total)
                    return None
                lengths.append(prev); n += 1
        elif sym == 17:
            rep = br.read(3)
            if rep is None:
                print("DYN-FAIL: rep17 read at bit_pos", br.bit_pos)
                return None
            for i in range(rep + 3):
                if n >= total:
                    print("DYN-FAIL: rep17 overflow n", n, "total", total)
                    return None
                lengths.append(0); n += 1
        else:
            rep = br.read(7)
            if rep is None:
                print("DYN-FAIL: rep18 read at bit_pos", br.bit_pos)
                return None
            for i in range(rep + 11):
                if n >= total:
                    print("DYN-FAIL: rep18 overflow n", n, "total", total)
                    return None
                lengths.append(0); n += 1
    if n != total:
        print("DYN-FAIL: n!=total", n, total)
        return None
    lit = huff_build(lengths, nlit)
    dist = huff_build(lengths[nlit:], ndist)
    return (lit, dist)

def inflate_blocks(br, dst_cap):
    dst = bytearray(dst_cap)
    out_pos = 0
    while True:
        bfinal = br.read(1)
        btype = br.read(2)
        if bfinal is None or btype is None:
            print("BLOCK-FAIL: header read at bit_pos", br.bit_pos)
            return None
        print("BLOCK: bfinal", bfinal, "btype", btype, "at bit_pos", br.bit_pos - 3)
        if btype == 0:
            br.align()
            length = br.read(16); nlen = br.read(16)
            if length is None or nlen is None:
                return None
            if (length ^ 0xFFFF) != nlen:
                return None
            if out_pos + length > dst_cap:
                return None
            byte_start = br.bit_pos >> 3
            if byte_start > br.src_len or length > br.src_len - byte_start:
                return None
            dst[out_pos:out_pos + length] = br.src[byte_start:byte_start + length]
            out_pos += length
            br.bit_pos += length * 8
        elif btype == 1:
            fixed_lit = [8] * 144 + [9] * 112 + [7] * 24 + [8] * 8
            fixed_dist = [5] * 30
            lit = huff_build(fixed_lit, 288)
            dist = huff_build(fixed_dist, 30)
            res = inflate_lz77(br, lit, dist, dst, dst_cap, out_pos)
            if res is None:
                return None
            out_pos = res
        else:
            dyn = huff_dynamic(br)
            if dyn is None:
                return None
            lit, dist = dyn
            res = inflate_lz77(br, lit, dist, dst, dst_cap, out_pos)
            if res is None:
                return None
            out_pos = res
        if bfinal:
            break
    return bytes(dst[:out_pos])

def main():
    global raw
    for fname in ("test.ggb", "test_store.ggb"):
        path = os.path.join(base, fname)
        with open(path, "rb") as f:
            data = f.read()
        size = len(data)
        eocd = find_eocd(data, size)
        if eocd is None:
            print(fname, ": EOCD not found")
            continue
        ent = central_find(data, size, eocd, b"geogebra.xml")
        if ent is None:
            print(fname, ": entry not found")
            continue
        local_off, csize, usize, method = ent
        data_off = local_data_offset(data, size, local_off)
        print(f"== {fname}: eocd={eocd} local_off={local_off} data_off={data_off} "
              f"comp={csize} uncomp={usize} method={method}")
        if method == 8:
            raw = data[data_off:data_off + csize]
            with open(os.path.join(base, "geogebra_raw_deflate.bin"), "wb") as f:
                f.write(raw)
            br = BitReader(raw)
            out = inflate_blocks(br, usize)
            z = zlib.decompressobj(-zlib.MAX_WBITS)
            truth = z.decompress(raw) + z.flush()
            print(f"  C-algo produced={len(out) if out is not None else 'FAIL'}, "
                  f"zlib truth len={len(truth)} (uncomp={usize})")
            if out is None:
                print("  C-algo: inflate failed")
            else:
                n = min(len(out), len(truth))
                first_diff = next((i for i in range(n) if out[i] != truth[i]), None)
                print(f"  prefix match={n} bytes, first_diff at {first_diff}")
                if first_diff is not None:
                    lo = max(0, first_diff - 20); hi = min(len(truth), first_diff + 40)
                    print("  context truth:", truth[lo:hi])
                    lo2 = max(0, first_diff - 20); hi2 = min(len(out), first_diff + 40)
                    print("  context C-out:", out[lo2:hi2])
                if len(out) != len(truth):
                    print(f"  LEN MISMATCH: produced={len(out)} truth={len(truth)}")
            with open(os.path.join(base, "geogebra_expected.xml"), "wb") as f:
                f.write(truth)

if __name__ == "__main__":
    main()
