#include <stdint.h>
#include <stdio.h>

#define ERR(...) fprintf(stderr, __VA_ARGS__)
#define ASSERT(x, ...) if (!(x)) { ERR(__VA_ARGS__); return 0; }

int vint(FILE * f, uint64_t * res) {
  ASSERT(fread(res, 1, 1, f), "error reading byte 1 of vint");
  if (*res & 0x80) {
    *res &= 0x7f;
    return 1;
  }
  if (*res & 0x40) {
    *res &= 0x3f;
    *res <<= 8;
    ASSERT(fread(res, 1, 1, f), "error reading byte 2 of vint");
    return 1;
  }
  if (*res & 0x20) {
    *res &= 0x1f;
    *res <<= 16;
    ASSERT(fread(res, 2, 1, f), "error reading bytes 2 and 3 of vint");
    return 1;
  }
  if (*res & 0x10) {
    *res &= 0x0f;
    *res <<= 24;
    ASSERT(fread(res, 3, 1, f), "error reading bytes 2 thru 4 of vint");
    return 1;
  }

  ASSERT(*res & 0x08, "invalid byte 1 of vint");

  *res &= 0x07;
  *res <<= 32;
  ASSERT(fread(res, 4, 1, f), "error reading bytes 2 thru 5 of vint");
  return 1;
}

int run(const char * name) {
  FILE * f = fopen(name, "rb");
  ASSERT(f, "file not found");

  uint64_t elid;
  ASSERT(vint(f, &elid), " reading Element ID");

  printf("%llx\n", elid);
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
