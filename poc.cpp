#pragma leco tool
import missingno;
import silog;
import yoyo;

void fail(const char *err) {
  silog::log(silog::error, "Error: %s", err);
  throw 0;
}

constexpr bool ebml_element_id(unsigned id) { return id == 0x0A45DFA3; }
constexpr bool le_8(unsigned val) { return val <= 8; }

constexpr unsigned octet_count(unsigned char w) {
  unsigned octets{1};
  while ((w & 0x80) == 0 && w != 0) {
    w <<= 1;
    octets++;
  }
  return octets;
}

using vint = unsigned long;
constexpr mno::req<vint> read_vint(yoyo::reader &in) {
  unsigned char buf[8];
  return in.read_u8()
      .map(octet_count)
      .assert(le_8, "VINT with more than 8 octets")
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

struct element {
  vint id;
  yoyo::subreader data;
};
constexpr auto read_element(yoyo::reader &in) {
  element res{};
  return read_vint(in)
      .fmap([&](auto id) {
        res.id = id;
        return read_vint(in);
      })
      .fmap([&](auto size) { return yoyo::subreader::create(&in, size); })
      .fmap([&](auto sr) {
        res.data = sr;
        return in.seekg(sr.raw_size(), yoyo::seek_mode::current);
      })
      .map([&] { return res; });
}

struct document {
  element header;
  element body;
};
constexpr auto read_document(yoyo::reader &in) {
  document res{};
  return read_element(in)
      .map([&](auto e) { res.header = e; })
      .fmap([&] { return read_element(in); })
      .map([&](auto e) {
        res.body = e;
        return res;
      });
}

[[nodiscard]] mno::req<void> dump_sequence(element &e) {
  return read_element(e.data)
      .fmap([&](auto ee) {
        silog::log(silog::info, "- ID=%lx size=%d", ee.id, ee.data.raw_size());
        return e.data.eof();
      })
      .fmap([&](auto eof) {
        if (eof)
          return mno::req<void>{};

        return dump_sequence(e);
      });
}
[[nodiscard]] auto dump_root(const char *name, element &e) {
  silog::log(silog::info, "%s: ID=%lx size=%d", name, e.id, e.data.raw_size());
  return e.data.seekg(0, yoyo::seek_mode::set).fmap([&] {
    return dump_sequence(e);
  });
}

[[nodiscard]] auto dump_doc(yoyo::reader &in) {
  return read_document(in)
      .fmap([](auto doc) {
        return dump_root("Header", doc.header).map([&] { return doc; });
      })
      .fmap([](auto doc) {
        return dump_root("Body", doc.body).map([&] { return doc; });
      })
      .fmap(
          [](auto doc) { return doc.body.data.seekg(0, yoyo::seek_mode::end); })
      .map([] { return false; })
      .if_failed([&](auto msg) {
        return in.eof().assert([](auto v) { return v; }, msg);
      });
}

int main() {
  yoyo::file_reader in{"example.mkv"};

  while (!dump_doc(in).take(fail)) {
    silog::log(silog::info, "------------------------------------");
  }
}
