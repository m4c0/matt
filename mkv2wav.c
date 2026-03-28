#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ERR(...) fprintf(stderr, __VA_ARGS__)
#define ASSERT(x, ...) if (!(x)) { ERR(__VA_ARGS__); return 0; }

int read(FILE * f, void * res) {
  ASSERT(fread(res, 1, 1, f), "error reading byte");
  return 1;
}
int octet_count(uint64_t n) {
  int res = 1;
  while ((n & 0x80) == 0 && n != 0) {
    n <<= 1;
    res++;
  }
  return res;
}
int vint_raw(FILE * f, uint64_t * res, int id) {
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
int vint(FILE * f, uint64_t * res) { return vint_raw(f, res, 0); }

int element(FILE * f, uint64_t * elid, uint64_t * elsz) {
  ASSERT(vint_raw(f, elid, 1), " reading Element ID");
  ASSERT(*elid && ~*elid, "Element ID cannot have all zeroes or all ones");
  ASSERT(vint(f, elsz), " reading Element Data Size");
  ASSERT(~*elsz, "Element Data Size cannot have all ones");
  return 1;
}
int check_element(FILE * f, uint64_t exp_elid, uint64_t * elsz) {
  uint64_t elid;
  ASSERT(element(f, &elid, elsz), "");
  ASSERT(elid == exp_elid, "Invalid element ID (exp %llx got %llx)", exp_elid, elid);
  return 1;
}
int element_sized(FILE * f, uint64_t exp_elid, uint64_t exp_elsz) {
  uint64_t elsz;
  ASSERT(check_element(f, exp_elid, &elsz), "");
  ASSERT(elsz == exp_elsz, "Invalid element data size (exp %lld got %lld)", exp_elsz, elsz);
  return 1;
}

int check_u8(FILE * f, uint8_t v) {
  uint8_t val;
  ASSERT(read(f, &val), " reading value");
  ASSERT(val == v, "Unsupported value (expecting %d got %d)", v, val);
  return 1;
}

int run_track(FILE * f, uint64_t trk_sz) {
  long trk_end = ftell(f) + trk_sz;
  ASSERT(0 <= fseek(f, trk_sz, SEEK_CUR), " skipping unused element");
  return 1;
}

int run(const char * name) {
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
  ASSERT(len == 8, "Invalid DocType length (exp 8 got %lld)", len);
  char buf[8];
  ASSERT(fread(buf, 8, 1, f), "Error reading DocType");
  ASSERT(0 == strncmp(buf, "matroska", 8), "Invalid DocType (exp 'matroska' got '%8s')", buf);

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
    switch (elid) {
      case 0x1654ae6b: ASSERT(run_track(f, hdr_sz), ""); break; // Tracks
      default:
        ASSERT(0 <= fseek(f, hdr_sz, SEEK_CUR), " skipping unused element");
        break;
    }
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
