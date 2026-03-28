#include <stdint.h>
#include <stdio.h>

#define ERR(...) fprintf(stderr, __VA_ARGS__)
#define ASSERT(x, ...) if (!(x)) { ERR(__VA_ARGS__); return 0; }

int vint(FILE * f, uint64_t * res, int id) {
  *res = 0;
  ASSERT(fread(res, 1, 1, f), "error reading byte 1 of vint");

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
    ASSERT(fread(res, 1, 1, f), "error reading bytes 2 of vint");
  }

  return 1;
}

int run(const char * name) {
  FILE * f = fopen(name, "rb");
  ASSERT(f, "file not found");

  uint64_t elid;
  ASSERT(vint(f, &elid, 1), " reading Element ID");
  ASSERT(elid && ~elid, "Element ID cannot have all zeroes or all ones");
  uint64_t elsz;
  ASSERT(vint(f, &elsz, 0), " reading Element Data Size");
  ASSERT(~elid, "Element Data Size cannot have all ones");

  printf("%llx %lld\n", elid, elsz);
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
