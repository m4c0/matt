#define _CRT_SECURE_NO_WARNINGS
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opus.h"

#define ERR(...) fprintf(stderr, __VA_ARGS__)
#define ASSERT(x, ...) do { if (!(x)) { ERR(__VA_ARGS__); return 0; } } while (0)

#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct track {
  uint8_t id;
  uint8_t type;
  char * codec;

  double sample_rate;
  uint8_t channels;
  uint8_t bit_depth;

  OpusDecoder * dec;
  FILE * wav;

  struct track * next;
} track_t;

static int read(FILE * f, void * res) {
  ASSERT(fread(res, 1, 1, f), "error reading byte");
  return 1;
}
static int octet_count(uint64_t n) {
  int res = 1;
  while ((n & 0x80) == 0 && n != 0) {
    n <<= 1;
    res++;
  }
  return res;
}
static int vint_raw(FILE * f, uint64_t * res, int id) {
  *res = 0;
  ASSERT(read(f, res), " 1 of vint");

  unsigned len = octet_count(*res);
  ASSERT(len > 0 && len <= 8, "invalid octec count (%d)", len);

  unsigned mask = 0x80 >> (len - 1);
  if (!id) *res ^= mask;

  for (unsigned i = 1; i < len; i++) {
    *res <<= 8;
    ASSERT(read(f, res), " of vint");
  }

  return 1;
}
static int vint(FILE * f, uint64_t * res) { return vint_raw(f, res, 0); }

static int element(FILE * f, uint64_t * elid, uint64_t * elsz) {
  ASSERT(vint_raw(f, elid, 1), " reading Element ID");
  ASSERT(*elid && ~*elid, "Element ID cannot have all zeroes or all ones");
  ASSERT(vint(f, elsz), " reading Element Data Size");
  ASSERT(~*elsz, "Element Data Size cannot have all ones");
  return 1;
}
static int check_element(FILE * f, uint64_t exp_elid, uint64_t * elsz) {
  uint64_t elid;
  ASSERT(element(f, &elid, elsz), "");
  ASSERT(elid == exp_elid, "Invalid element ID (exp %llx got %llx)", exp_elid, elid);
  return 1;
}
static int element_sized(FILE * f, uint64_t exp_elid, uint64_t exp_elsz) {
  uint64_t elsz;
  ASSERT(check_element(f, exp_elid, &elsz), "");
  ASSERT(elsz == exp_elsz, "Invalid element data size (exp %lld got %lld)", exp_elsz, elsz);
  return 1;
}

// TODO: return char *
static int read_str(FILE * f, char ** str, uint64_t len) {
  char * buf = *str = malloc(len + 1);
  ASSERT(buf, "Out-of-memory");
  ASSERT(fread(buf, len, 1, f), "Error reading string");
  buf[len] = 0;
  return 1;
}

static int check_u8(FILE * f, uint8_t v) {
  uint8_t val;
  ASSERT(read(f, &val), " reading value");
  ASSERT(val == v, "Unsupported value (expecting %d got %d)", v, val);
  return 1;
}

static int run_audio(FILE * f, uint64_t sz, track_t * t) {
  long end = ftell(f) + sz;
  while (ftell(f) < end) {
    uint64_t elid, hdr_sz;
    ASSERT(element(f, &elid, &hdr_sz), " reading audio element");
    if (elid == 0xB5) { // Sampling Frequency
      ASSERT(hdr_sz == 8, "Invalid sample frequency size (%lld)", hdr_sz);

      union {
        uint8_t u[8];
        double f;
      } smp;
      for (int i = 0; i < 8; i++) ASSERT(fread(smp.u + 7 - i, 1, 1, f), "Error reading byte %d of sampling frequency", i + 1);
      t->sample_rate = smp.f;
    } else if (elid == 0x9F) { // Channels
      ASSERT(hdr_sz == 1, "Invalid channels size (%lld)", hdr_sz);
      ASSERT(read(f, &t->channels), " reading channels");
    } else if (elid == 0x6264) { // Bit Depth
      ASSERT(hdr_sz == 1, "Invalid bit depth size (%lld)", hdr_sz);
      ASSERT(read(f, &t->bit_depth), " reading bit depth");
    } else ASSERT(0 <= fseek(f, hdr_sz, SEEK_CUR), " skipping unused audio element");
  }
  return 1;
}

static track_t * run_track_entry(FILE * f, uint64_t trk_sz) {
  track_t * res = malloc(sizeof(track_t));
  *res = (track_t){0};

  long trk_end = ftell(f) + trk_sz;
  while (ftell(f) < trk_end) {
    uint64_t elid, hdr_sz;
    ASSERT(element(f, &elid, &hdr_sz), " reading track entry element");
    if (elid == 0x83) { // Track Type
      ASSERT(hdr_sz == 1, "Invalid track type size (%lld)", hdr_sz);
      ASSERT(read(f, &res->type), " reading track type");
      if (res->type != 2) {
        ASSERT(0 <= fseek(f, trk_end, SEEK_SET), " skipping non-audio track");
        *res = (track_t){0};
      }
    } else if (elid == 0x86) { // Codec ID
      ASSERT(read_str(f, &res->codec, hdr_sz), " reading codec ID");
    } else if (elid == 0xD7) { // Track ID
      ASSERT(hdr_sz == 1, "Invalid track type size (%lld)", hdr_sz);
      ASSERT(read(f, &res->id), " reading track ID");
    } else if (elid == 0xE1) { // Audio
      ASSERT(run_audio(f, hdr_sz, res), " reading audio data");
    } else ASSERT(0 <= fseek(f, hdr_sz, SEEK_CUR), " skipping unused track entry element");
  }
  return res;
}

static track_t * run_tracks(FILE * f, uint64_t trk_sz) {
  track_t * res = NULL;
  long trk_end = ftell(f) + trk_sz;
  while (ftell(f) < trk_end) {
    uint64_t elid, hdr_sz;
    ASSERT(element(f, &elid, &hdr_sz), " reading track element");
    if (elid == 0xAE) { // Track Entry
      track_t * t;
      ASSERT(t = run_track_entry(f, hdr_sz), "");
      t->next = res;
      res = t;
    }
    else ASSERT(0 <= fseek(f, hdr_sz, SEEK_CUR), " skipping unused track element");
  }
  return res;
}

#if 0
typedef struct opus {
  const uint8_t * blk;
  const uint8_t * end;
  unsigned bits_taken;
  unsigned rng;
  unsigned val;
  uint8_t leftover;
} opus_t;
static uint8_t opus_next(opus_t * o) {
  return o->blk >= o->end ? 0 : *o->blk++;
}
#define _2xx23 (1 << 23)
static void opus_dec_norm(opus_t * o) { // 4.1.2.1
  while (o->rng <= _2xx23) {
    o->rng <<= 8;

    uint8_t next = opus_next(o);
    uint8_t sym = (o->leftover << 7) | (next >> 1);
    o->leftover = next;

    o->val = ((o->val << 8) + (255 - sym)) & 0x7FFFFFFF;
  }
}
static void opus_decode_update(opus_t * o, unsigned fl_k, unsigned fh_k, unsigned ft) {
  o->val = o->val - (o->rng / ft) * (ft - fh_k);
  if (fl_k > 0) {
    o->rng = (o->rng / ft) * (fh_k - fl_k);
  } else {
    o->rng = o->rng - (o->rng / ft) * (ft - fh_k);
  }
  opus_dec_norm(o);
}
static uint16_t opus_decode_icdf_2(opus_t * o, unsigned f1, unsigned f2) {
  // (4.1.2)
  const unsigned ft = f1 + f2;
  unsigned fs = ft - MIN(1 + o->val / (o->rng / ft), ft);
  unsigned sym = fs < f1 ? 0 : 1;
  unsigned fl_k = fs < f1 ?  0 : f1;
  unsigned fh_k = fs < f1 ? f1 : ft;
  opus_decode_update(o, fl_k, fh_k, ft);
  return sym;
}
static uint16_t opus_decode_icdf_3(opus_t * o, unsigned f1, unsigned f2, unsigned f3) {
  // (4.1.2)
  const unsigned ft = f1 + f2 + f3;
  unsigned fs = ft - MIN(1 + o->val / (o->rng / ft), ft);
  unsigned sym  = fs < f1 ?  0 : (fs < (f1 + f2) ? 1 : 2);
  unsigned fl_k = fs < f1 ?  0 : (fs < (f1 + f2) ? f1 : f2);
  unsigned fh_k = fs < f1 ? f1 : (fs < (f1 + f2) ? (f1 + f2) : ft);
  opus_decode_update(o, fl_k, fh_k, ft);
  return sym;
}
static uint16_t opus_decode_uint8(opus_t * o, unsigned ft) {
  // (4.1.5)
  unsigned sym = ft - MIN(1 + o->val / (o->rng / ft), ft);
  unsigned fl_k = sym;
  unsigned fh_k = sym + 1;
  opus_decode_update(o, fl_k, fh_k, ft);
  return sym;
}
static uint16_t opus_decode_raw(opus_t * o, unsigned n) {
  unsigned all = ((unsigned *)o->end)[-1];
  uint16_t res = (uint16_t)(all >> o->bits_taken) & ((1 << (n + 1)) - 1);
  o->bits_taken += n;
  o->end -= o->bits_taken / 8;
  o->bits_taken %= 8;
  return res;
}
static int opus_decode(opus_t * o) {
  // TOC (3.1) CELT FB (48kHz) 20ms, Stereo, 1 frame per packet
  uint8_t toc = opus_next(o);
  ASSERT(toc == 0xFC, "Unsupported frame type (found %x)", toc);

  uint8_t b0 = opus_next(o);

  o->rng = 128; // 4.1.1
  o->val = 127 - (b0 >> 1);
  o->leftover = b0;

  opus_dec_norm(o);

  // (4.3) Decode table 56
  unsigned silence = opus_decode_icdf_2(o, 32767, 1);
  unsigned postfilter = opus_decode_icdf_2(o, 1, 1);
  unsigned octave = postfilter ? opus_decode_uint8(o, 6) : 0;
  unsigned period = postfilter ? opus_decode_raw(o, 4 + octave) : 0;
  unsigned gain = postfilter ? opus_decode_raw(o, 3) : 0;
  unsigned tapset = postfilter ? opus_decode_icdf_3(o, 2, 1, 1) : 0;
  unsigned transient = opus_decode_icdf_2(o, 7, 1);
  unsigned intra = opus_decode_icdf_2(o, 7, 1);

  printf("%8x %8x %d -- %x %x %d %3x - %x %d %d %d\n",
      o->rng, o->val, o->leftover & 1,
      silence, postfilter, octave, period, 
      gain, tapset, transient, intra);

  return 1;
}
#else
static int opus(track_t * trk, void * data, unsigned len) {
  if (!trk->dec) {
    int err;
    trk->dec = opus_decoder_create(trk->sample_rate, trk->channels, &err);
    ASSERT(err == OPUS_OK, "failed to create opus decoder");

    char name[256];
    sprintf(name, "track-%d.wav", trk->id);
    ASSERT(trk->wav = fopen(name, "wb"), "error opening wave file");
    
    const uint8_t wav[44] = {
      'R', 'I', 'F', 'F',
      0, 0, 0, 0, // file size - 8
      'W', 'A', 'V', 'E',
      'f', 'm', 't', ' ',
      16, 0, 0, 0, // "fmt " size - 8
      1, 0, // PCM
      2, 0, // Stereo
      0x80, 0xbb, 0, 0, // 48kHz
      0, 0x22, 2, 0, // Bytes to read per second (samples per block * 48k)
      4, 0, // Samples per block (2 of 16bit)
      16, 0, // Bits per sample
      'd', 'a', 't', 'a',
      0, 0, 0, 0, // data size
    };
    ASSERT(fwrite(wav, sizeof(wav), 1, trk->wav), "error writing wav header");
  }

  const int max_frame_size = 960 * 6;
  short out[max_frame_size];

  int smp = opus_decode(trk->dec, data, len, out, max_frame_size, 0);
  ASSERT(smp > 0, "failed to decode opus frame");

  ASSERT(fwrite(out, smp * 2, 1, trk->wav), "error writing wave data");
  
  return 1;
}
#endif

static int run_simple_block(FILE * f, uint64_t blk_sz, track_t * trks) {
  long blk_end = ftell(f) + blk_sz;

  uint64_t trk_no;
  ASSERT(vint(f, &trk_no), " reading track ID");

  uint16_t ts;
  ASSERT(fread(&ts, 2, 1, f), "Error reading timestamp"); 

  uint8_t flg;
  ASSERT(fread(&flg, 1, 1, f), "Error reading flags"); 
  ASSERT((flg | 0x80) == 0x80, "Unsupported flags");

  for (; trks && trks->id != trk_no; trks = trks->next) {}
  if (!trks) {
    ASSERT(0 <= fseek(f, blk_end, SEEK_SET), "");
    return 1;
  }

  long frm_sz = blk_end - ftell(f);
  uint8_t buf[frm_sz];
  ASSERT(fread(buf, frm_sz, 1, f), "Error reading OPUS frame data");

  //opus_t o = (opus_t) { buf, buf + frm_sz };
  //ASSERT(opus_decode(&o), " decoding OPUS");
  ASSERT(opus(trks, buf, frm_sz), " decoding block");

  return 1;
}

static int run_cluster(FILE * f, uint64_t cls_sz, track_t * trks) {
  long cls_end = ftell(f) + cls_sz;
  while (ftell(f) < cls_end) {
    uint64_t elid, hdr_sz;
    ASSERT(element(f, &elid, &hdr_sz), " reading cluster element");
    ASSERT(0xA0 != elid, "Block Group TODO");
    if (elid == 0xA3) ASSERT(run_simple_block(f, hdr_sz, trks), " reading block"); 
    else ASSERT(0 <= fseek(f, hdr_sz, SEEK_CUR), " skipping unused track element");
  }
  return 1;
}

static int run(const char * name) {
  FILE * f = fopen(name, "rb");
  ASSERT(f, "file not found");

  uint64_t hdr_sz;
  ASSERT(check_element(f, 0x1A45DFA3, &hdr_sz), " reading EBML Element ID");
  long l = ftell(f);

  ASSERT(element_sized(f, 0x4286, 1), " reading EBML Version ID");
  ASSERT(check_u8(f, 1), " of EBML Version ID");
  ASSERT(element_sized(f, 0x42F7, 1), " reading EBML Read Version ID");
  ASSERT(check_u8(f, 1), " of EBML Read Version ID");
  ASSERT(element_sized(f, 0x42F2, 1), " reading EBML Max ID Length");
  ASSERT(check_u8(f, 4), " of EBML Max ID Length");
  ASSERT(element_sized(f, 0x42F3, 1), " reading EBML Max Size Length");
  ASSERT(check_u8(f, 8), " of EBML Max Size Length");

  uint64_t len;
  ASSERT(check_element(f, 0x4282, &len), " reading DocType");
  char * buf;
  ASSERT(read_str(f, &buf, len), " for DocType");
  ASSERT(0 == strncmp(buf, "matroska", len), "Invalid DocType (exp 'matroska' got '%8s')", buf);

  ASSERT(element_sized(f, 0x4287, 1), " reading DocType Version ID");
  ASSERT(check_u8(f, 4), " of DocType Version ID");
  ASSERT(element_sized(f, 0x4285, 1), " reading DocType Read Version ID");
  ASSERT(check_u8(f, 2), " of DocType Read Version ID");

  ASSERT(hdr_sz == ftell(f) - l, "Unsupported elements in EBML Header");

  ASSERT(check_element(f, 0x18538067, &hdr_sz), " reading Segment");
  long seg_end = ftell(f) + hdr_sz;

  track_t * trks = NULL;
  while (ftell(f) < seg_end) {
    uint64_t elid;
    ASSERT(element(f, &elid, &hdr_sz), " reading segment element");
    if (elid == 0x1654ae6b) ASSERT(trks = run_tracks(f, hdr_sz), ""); // Tracks
    else if (elid == 0x1F43B675) ASSERT(run_cluster(f, hdr_sz, trks), ""); // Cluster
    else ASSERT(0 <= fseek(f, hdr_sz, SEEK_CUR), " skipping unused element");
  }

  for (; trks; trks = trks->next) {
    if (!trks->type) continue;
    printf("track: id=%d type=%d codec=%s smp=%.0f ch=%d bd=%d\n",
        trks->id,
        trks->type,
        trks->codec,
        trks->sample_rate,
        trks->channels,
        trks->bit_depth);

    unsigned sz = ftell(trks->wav) - 8;
    ASSERT(0 == fseek(trks->wav, 4, SEEK_SET), "error seeking to wave file size");
    ASSERT(fwrite(&sz, 4, 1, trks->wav), "error writing wave file size");
    sz -= 36;
    ASSERT(0 == fseek(trks->wav, 40, SEEK_SET), "error seeking to wave data size");
    ASSERT(fwrite(&sz, 4, 1, trks->wav), "error writing wave data size");
    fclose(trks->wav);
  }
  return 1;
}

int main(int argc, char ** argv) {
  for (int i = 1; i < argc; i++) {
    ASSERT(run(argv[i]), " of file %s", argv[i]);
  }
  if (argc == 1) {
    fprintf(stderr, "usage: %s <file.mkv>...\n", argv[0]);
    return 1;
  }
}
