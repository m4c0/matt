#define _CRT_SECURE_NO_WARNINGS
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ERR(...) fprintf(stderr, __VA_ARGS__)
#define ASSERT(x, ...) do { if (!(x)) { ERR(__VA_ARGS__); return 0; } } while (0)

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

static int read_str(FILE * f, char ** str, uint64_t len) {
  char * buf = *str = malloc(len);
  ASSERT(buf, "Out-of-memory");
  ASSERT(fread(buf, len, 1, f), "Error reading string");
  return 1;
}

static int check_u8(FILE * f, uint8_t v) {
  uint8_t val;
  ASSERT(read(f, &val), " reading value");
  ASSERT(val == v, "Unsupported value (expecting %d got %d)", v, val);
  return 1;
}

static int run_audio(FILE * f, uint64_t sz) {
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
      printf("sample rate: %lf\n", smp.f);
    } else if (elid == 0x9F) { // Channels
      ASSERT(hdr_sz == 1, "Invalid channels size (%lld)", hdr_sz);

      uint8_t val;
      ASSERT(read(f, &val), " reading channels");
      printf("channels: %d\n", val);
    } else ASSERT(0 <= fseek(f, hdr_sz, SEEK_CUR), " skipping unused audio element");
  }
  return 1;
}

static int run_track_entry(FILE * f, uint64_t trk_sz) {
  long trk_end = ftell(f) + trk_sz;
  while (ftell(f) < trk_end) {
    uint64_t elid, hdr_sz;
    ASSERT(element(f, &elid, &hdr_sz), " reading track entry element");
    if (elid == 0x83) { // Track Type
      ASSERT(hdr_sz == 1, "Invalid track type size (%lld)", hdr_sz);

      uint8_t val;
      ASSERT(read(f, &val), " reading value");
      if (val == 2) puts("this is audio");
      else ASSERT(0 <= fseek(f, trk_end, SEEK_SET), " skipping non-audio track");
    } else if (elid == 0x86) { // Codec ID
      char * buf;
      ASSERT(read_str(f, &buf, hdr_sz), " reading codec ID");
      printf("%.*s\n", (int)hdr_sz, buf);
    } else if (elid == 0xE1) { // Audio
      ASSERT(run_audio(f, hdr_sz), " reading audio data");
    } else ASSERT(0 <= fseek(f, hdr_sz, SEEK_CUR), " skipping unused track entry element");
  }
  return 1;
}

static int run_tracks(FILE * f, uint64_t trk_sz) {
  long trk_end = ftell(f) + trk_sz;
  while (ftell(f) < trk_end) {
    uint64_t elid, hdr_sz;
    ASSERT(element(f, &elid, &hdr_sz), " reading track element");
    if (elid == 0xAE) ASSERT(run_track_entry(f, hdr_sz), ""); // Track Entry
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

  while (ftell(f) < seg_end) {
    uint64_t elid;
    ASSERT(element(f, &elid, &hdr_sz), " reading segment element");
    if (elid == 0x1654ae6b) ASSERT(run_tracks(f, hdr_sz), ""); // Tracks
    else ASSERT(0 <= fseek(f, hdr_sz, SEEK_CUR), " skipping unused element");
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
