#pragma leco tool
import missingno;
import silog;
import yoyo;

void fail(const char *err) {
  silog::log(silog::error, "Error: %s", err);
  throw 0;
}

constexpr bool ebml_element_id(unsigned id) { return id == 0x1A45DFA3; }
constexpr bool less_than_8(unsigned val) { return val < 8; }

constexpr unsigned octet_count(unsigned char w) {
  unsigned octets{1};
  while ((w & 0x80) == 0 && w != 0) {
    w <<= 1;
    octets++;
  }
  return octets;
}

using vint = unsigned long;
constexpr mno::req<vint> read_vint(auto &in) {
  unsigned char buf[8];
  return in.read_u8()
      .map(octet_count)
      .assert(less_than_8, "VINT with more than 8 octets")
      .fmap([&](auto o) {
        // take u16 back
        return in.seekg(-1, yoyo::seek_mode::current).map([&] { return o; });
      })
      .fmap([&](auto octets) {
        return in.read(buf, octets).map([&] {
          unsigned mask = 0x80 >> (octets - 1);
          buf[0] ^= mask;

          vint res{};
          for (auto i = 0; i < octets; i++) {
            res <<= 8;
            res |= buf[i];
          }
          return res;
        });
      });
}

int main() {
  yoyo::file_reader in{"example.mkv"};

  auto id = in.read_u32_be()
                .assert(ebml_element_id, "Expecting EBML element ID")
                .take(fail);
  auto size = read_vint(in).take(fail);

  silog::log(silog::info, "Header: ID=%x size=%ld", id, size);
}
