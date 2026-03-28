#include <stdint.h>
#include <stdio.h>

#define ERR(...) fprintf(stderr, __VA_ARGS__)
#define ASSERT(x, ...) if (!(x)) { ERR(__VA_ARGS__); return 0; }

int read(FILE * f, void * res) {
  ASSERT(fread(res, 1, 1, f), "error reading byte");
  return 1;
}
int vint_raw(FILE * f, uint64_t * res, int id) {
  *res = 0;
  ASSERT(read(f, res), " 1 of vint");

  unsigned len = 0;
  if (*res & 0x80) {
    if (!id) *res &= 0x7f;
  } else if (*res & 0x40) {
    if (!id) *res &= 0x3f;
    len = 1;
  } else if (*res & 0x20) {
    if (!id) *res &= 0x1f;
    len = 2;
  } else if (*res & 0x10) {
    if (!id) *res &= 0x0f;
    len = 3;
  } else {
    ASSERT(*res & 0x08, "invalid byte 1 of vint");
    if (!id) *res &= 0x07;
    len = 4;
  }

  for (unsigned i = 0; i < len; i++) {
    *res <<= 8;
    ASSERT(read(f, res), " of vint");
  }

  return 1;
}
int vint(FILE * f, uint64_t * res) { return vint_raw(f, res, 0); }

int element(FILE * f, uint64_t exp_elid) {
  uint64_t elid, elsz;
  ASSERT(vint_raw(f, &elid, 1), " reading Element ID");
  ASSERT(elid && ~elid, "Element ID cannot have all zeroes or all ones");
  ASSERT(vint(f, &elsz), " reading Element Data Size");
  ASSERT(~elid, "Element Data Size cannot have all ones");
  ASSERT(elid == exp_elid, "Invalid Element ID");
  return 1;
}

int check_u8(FILE * f, uint8_t v) {
  uint8_t val;
  ASSERT(read(f, &val), " reading value");
  ASSERT(val == v, "Unsupported value (expecting %d got %d)", v, val);
  return 1;
}

int run(const char * name) {
  FILE * f = fopen(name, "rb");
  ASSERT(f, "file not found");

  ASSERT(element(f, 0x1A45DFA3), " reading EBML Element ID");
  ASSERT(element(f, 0x4286), " reading EBML Version ID");
  ASSERT(check_u8(f, 1), " of EBML Version ID");

  ASSERT(element(f, 0x42F7), " reading EBML Read Version ID");
  ASSERT(check_u8(f, 1), " of EBML Read Version ID");

  ASSERT(element(f, 0x42F2), " reading EBML Max ID Length");
  ASSERT(check_u8(f, 4), " of EBML Max ID Length");

  ASSERT(element(f, 0x42F3), " reading EBML Max Size Length");
  ASSERT(check_u8(f, 8), " of EBML Max Size Length");

  return 1;
}

int main(int argc, char ** argv) {
  for (int i = 1; i < argc; i++) {
    ASSERT(run(argv[i]), " while reading %s", argv[i]);
  }
  if (argc == 1) {
    fprintf(stderr, "usage: %s <file.mkv>...\n", argv[0]);
    return 1;
  }
}
