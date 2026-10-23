#include "freecam/freecam.h"

/* LZSS packer: 2048 byte window, 11 bit match position, 7 bit length, a
   binary search tree of window positions to find matches. A literal is a 1
   bit and 8 bits, a match a 0 bit, the position and length - 3; position 0
   ends the stream. */

#define LZSS_WINDOW 0x800
#define LZSS_LOOK_AHEAD 0x82
#define LZSS_ROOT LZSS_WINDOW

typedef struct LzssNode {
    int16_t parent;
    int16_t smaller;
    int16_t larger;
} LzssNode;

typedef struct BitStream {
    uint8_t *p;
    int mask;
    int rack;
} BitStream;

typedef struct Codec Codec;

ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, int align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));

#define gCodecs ((Codec **)0xA845D8)
#define gLzssCodec ((Codec *)0x54F778)

void Lzss_InitTree(LzssNode *tree, short r);
void Lzss_DeleteString(LzssNode *tree, short p);
void Lzss_ReplaceNode(LzssNode *tree, short old, short new);
int Lzss_AddString(LzssNode *tree, const uint8_t *window, short node, short *match);
void BitStream_WriteBit(BitStream *bs, int bit);
void BitStream_WriteBits(BitStream *bs, uint32_t value, int count);
int BitStream_ReadBit(BitStream *bs);
uint32_t BitStream_ReadBits(BitStream *bs, int count);
void Codec_Register(int index, Codec *codec);

void Lzss_Compress(void *unused, int len, uint8_t **src, BitStream *out)
{
    uint8_t *p = *src;
    LzssNode *tree = Mem_Alloc(0x3006, 4);
    uint8_t *window = Mem_Alloc(LZSS_WINDOW, 4);
    int pos = 1, ahead, matchLen, n;
    short match = 0;

    for (ahead = 0; ahead < LZSS_LOOK_AHEAD && len; ahead++) {
        len--;
        window[ahead + 1] = *p++;
    }
    Lzss_InitTree(tree, 1);
    matchLen = 0;
    while (ahead > 0) {
        if (matchLen > ahead)
            matchLen = ahead;
        if (matchLen <= 2) {
            n = 1;
            BitStream_WriteBit(out, 1);
            BitStream_WriteBits(out, window[(short)pos], 8);
        } else {
            BitStream_WriteBit(out, 0);
            BitStream_WriteBits(out, match, 11);
            BitStream_WriteBits(out, matchLen - 3, 7);
            n = matchLen;
        }
        while (n > 0) {
            Lzss_DeleteString(tree, (pos + LZSS_LOOK_AHEAD) & 0x7FF);
            if (len == 0) {
                ahead--;
            } else {
                window[(pos + LZSS_LOOK_AHEAD) & 0x7FF] = *p++;
                len--;
            }
            pos = (pos + 1) & 0x7FF;
            if (ahead)
                matchLen = Lzss_AddString(tree, window, pos, &match);
            n--;
        }
    }
    BitStream_WriteBit(out, 0);
    BitStream_WriteBits(out, 0, 11);
    Mem_Free(tree);
    Mem_Free(window);
}
HOOK(0x5302D0, Lzss_Compress);

void Lzss_InitTree(LzssNode *tree, short r)
{
    memset(tree, 0, 0x3006);
    tree[LZSS_ROOT].larger = r;
    tree[r].parent = LZSS_ROOT;
    tree[r].larger = 0;
    tree[r].smaller = 0;
}
HOOK(0x530470, Lzss_InitTree);

void Lzss_ContractNode(LzssNode *tree, short old, short new)
{
    LzssNode *par;

    tree[new].parent = tree[old].parent;
    par = &tree[tree[old].parent];
    if (par->larger == old)
        par->larger = new;
    else
        par->smaller = new;
    tree[old].parent = 0;
}
HOOK(0x530520, Lzss_ContractNode);

int Lzss_FindNextNode(LzssNode *tree, short node)
{
    short next = tree[node].smaller;

    while (tree[next].larger)
        next = tree[next].larger;
    return next;
}
HOOK(0x5305F0, Lzss_FindNextNode);

void Lzss_DeleteString(LzssNode *tree, short p)
{
    LzssNode *n = &tree[p];
    short r;

    if (!n->parent)
        return;
    if (!n->larger) {
        Lzss_ContractNode(tree, p, n->smaller);
    } else if (!n->smaller) {
        Lzss_ContractNode(tree, p, n->larger);
    } else {
        r = Lzss_FindNextNode(tree, p);
        Lzss_DeleteString(tree, r);
        Lzss_ReplaceNode(tree, p, r);
    }
}
HOOK(0x5304B0, Lzss_DeleteString);

void Lzss_ReplaceNode(LzssNode *tree, short old, short new)
{
    LzssNode *o = &tree[old];
    LzssNode *par = &tree[o->parent];
    LzssNode *n = &tree[new];

    if (par->smaller == old)
        par->smaller = new;
    else
        par->larger = new;
    *n = *o;
    tree[n->smaller].parent = new;
    tree[n->larger].parent = new;
    o->parent = 0;
}
HOOK(0x530580, Lzss_ReplaceNode);

/* Adds window position node to the tree; returns the longest match and
   stores where it is. */
int Lzss_AddString(LzssNode *tree, const uint8_t *window, short node, short *match)
{
    int i, delta, matchLen = 0;
    short test;
    int16_t *child;

    if (!node)
        return 0;
    test = tree[LZSS_ROOT].larger;
    for (;;) {
        for (i = 0; i < LZSS_LOOK_AHEAD; i++) {
            delta = window[(node + i) & 0x7FF] - window[(test + i) & 0x7FF];
            if (delta)
                break;
        }
        if (i >= matchLen) {
            matchLen = i;
            *match = test;
            if (i >= LZSS_LOOK_AHEAD) {
                Lzss_ReplaceNode(tree, test, node);
                return i;
            }
        }
        child = delta >= 0 ? &tree[test].larger : &tree[test].smaller;
        if (!*child) {
            *child = node;
            tree[node].parent = test;
            tree[node].larger = 0;
            tree[node].smaller = 0;
            return matchLen;
        }
        test = *child;
    }
}
HOOK(0x530630, Lzss_AddString);

void Lzss_Decompress(void *unused, BitStream *in, uint8_t **dst)
{
    uint8_t *out = *dst;
    uint8_t *window = Mem_Alloc(LZSS_WINDOW, 4);
    int pos = 1, match, len, i;
    uint8_t c;

    for (;;) {
        while (BitStream_ReadBit(in)) {
            c = BitStream_ReadBits(in, 8);
            *out++ = c;
            window[pos] = c;
            pos = (pos + 1) & 0x7FF;
        }
        match = BitStream_ReadBits(in, 11);
        if (!match)
            break;
        len = BitStream_ReadBits(in, 7) + 2;
        for (i = 0; i <= len; i++) {
            c = window[(match + i) & 0x7FF];
            *out++ = c;
            window[pos] = c;
            pos = (pos + 1) & 0x7FF;
        }
    }
    *dst = out;
    Mem_Free(window);
}
HOOK(0x530740, Lzss_Decompress);

void Lzss_Register(void)
{
    Codec_Register(0, gLzssCodec);
}
HOOK(0x530800, Lzss_Register);

void BitStream_WriteBit(BitStream *bs, int bit)
{
    if (bit)
        bs->rack |= bs->mask;
    bs->mask >>= 1;
    if (!bs->mask) {
        *bs->p = bs->rack;
        bs->rack = 0;
        bs->p++;
        bs->mask = 0x80;
    }
}
HOOK(0x530810, BitStream_WriteBit);

void BitStream_WriteBits(BitStream *bs, uint32_t value, int count)
{
    uint32_t m;

    for (m = 1u << ((count - 1) & 31); m; m >>= 1) {
        if (value & m)
            bs->rack |= bs->mask;
        bs->mask >>= 1;
        if (!bs->mask) {
            *bs->p = bs->rack;
            bs->rack = 0;
            bs->p++;
            bs->mask = 0x80;
        }
    }
}
HOOK(0x530850, BitStream_WriteBits);

int BitStream_ReadBit(BitStream *bs)
{
    int mask = bs->mask, bit;

    if (mask == 0x80)
        bs->rack = *bs->p++;
    bit = bs->rack & mask;
    bs->mask = mask >> 1;
    if (!bs->mask)
        bs->mask = 0x80;
    return bit != 0;
}
HOOK(0x5308B0, BitStream_ReadBit);

uint32_t BitStream_ReadBits(BitStream *bs, int count)
{
    uint32_t m, value = 0;

    for (m = 1u << ((count - 1) & 31); m; m >>= 1) {
        if (bs->mask == 0x80)
            bs->rack = *bs->p++;
        if (bs->rack & bs->mask)
            value |= m;
        bs->mask >>= 1;
        if (!bs->mask)
            bs->mask = 0x80;
    }
    return value;
}
HOOK(0x5308F0, BitStream_ReadBits);

void Codec_Register(int index, Codec *codec)
{
    gCodecs[index] = codec;
}
HOOK(0x530950, Codec_Register);
