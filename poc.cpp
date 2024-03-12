#pragma leco tool
import hai;
import jute;
import missingno;
import silog;
import traits;
import yoyo;

// {{{1 data types
using vint = unsigned long long;
static_assert(sizeof(vint) == 8);
using uint = unsigned long long;
static_assert(sizeof(uint) == 8);

struct element {
  vint id;
  yoyo::subreader data;
};

struct ebml_header {
  hai::cstr doctype{};
  uint doctype_read_version{1};
  uint doctype_version{1};
  uint ebml_max_id_length{4};
  uint ebml_max_size_length{8};
  uint ebml_read_version{1};
  uint ebml_version{1};
};

struct segment {};

struct document {
  ebml_header header;
  segment body;
};
// }}}1 Data Types

// {{{1 read element
constexpr unsigned octet_count(unsigned char w) {
  unsigned octets{1};
  while ((w & 0x80) == 0 && w != 0) {
    w <<= 1;
    octets++;
  }
  return octets;
}
constexpr bool le_8(unsigned val) { return val <= 8; }
constexpr mno::req<vint> read_vint(yoyo::reader &in, bool keep_mask = false) {
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
          if (!keep_mask) {
            unsigned mask = 0x80 >> (octets - 1);
            buf[0] ^= mask;
          }

          vint res{};
          for (auto i = 0; i < octets; i++) {
            res <<= 8;
            res |= buf[i];
          }
          return res;
        });
      });
}
constexpr auto read_element(yoyo::reader &in) {
  element res{};
  return read_vint(in, true)
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
// }}}1

constexpr mno::req<yoyo::subreader> reset(element &e) {
  return e.data.seekg(0, yoyo::seek_mode::set).map([&] { return e.data; });
}

template <typename Tp> struct element_reader;
template <> struct element_reader<uint> {
  static constexpr mno::req<void> read(yoyo::subreader &in, uint *res) {
    auto octets = in.raw_size();
    unsigned char buf[8];
    return in.read(buf, octets).map([&] {
      *res = 0;
      for (auto i = 0; i < octets; i++) {
        *res <<= 8;
        *res |= buf[i];
      }
    });
  }
};
template <> struct element_reader<hai::cstr> {
  static constexpr mno::req<void> read(yoyo::subreader &in, hai::cstr *str) {
    auto octets = in.raw_size();
    if (octets > 65536)
      return mno::req<void>::failed("Unsupported string bigger than 64kb");

    *str = hai::cstr{static_cast<unsigned>(octets)};
    return in.read(str->data(), octets);
  }
};

template <typename Tp> constexpr mno::req<void> read_attr(Tp *v, element e) {
  return reset(e).fmap([&](auto in) {
    if (in.raw_size() == 0)
      return mno::req<void>{};

    return element_reader<Tp>::read(in, v);
  });
}

constexpr mno::req<void> read_ebml_header(ebml_header *res, yoyo::reader &in) {
  return read_element(in)
      .fmap([&](element &e) {
        switch (e.id) {
        case 0x4282:
          return read_attr(&res->doctype, e);
        case 0x4285:
          return read_attr(&res->doctype_read_version, e);
        case 0x4286:
          return read_attr(&res->ebml_version, e);
        case 0x4287:
          return read_attr(&res->doctype_version, e);
        case 0x42F2:
          return read_attr(&res->ebml_max_id_length, e);
        case 0x42F3:
          return read_attr(&res->ebml_max_size_length, e);
        case 0x42F7:
          return read_attr(&res->ebml_read_version, e);
        default:
          return mno::req<void>{};
        }
      })
      .fmap([&] { return read_ebml_header(res, in); })
      .if_failed([&](auto msg) {
        return in.eof().assert([](auto v) { return v; }, msg).map([](auto) {});
      });
}

constexpr mno::req<void> read_segment(segment *res, yoyo::subreader &in) {
  return read_element(in)
      .fmap([&](element &e) {
        switch (e.id) {
        default:
          return mno::req<void>{};
        }
      })
      .fmap([&] { return read_segment(res, in); })
      .if_failed([&](auto msg) {
        return in.eof().assert([](auto v) { return v; }, msg).map([](auto) {});
      });
}

constexpr bool ebml_element_id(const element &e) { return e.id == 0x1A45DFA3; }
constexpr bool ebml_segment_id(const element &e) { return e.id == 0x18538067; }
constexpr auto read_document(yoyo::reader &in) {
  document res{};
  return read_element(in)
      .assert(ebml_element_id, "Invalid header")
      .fmap([&](auto e) {
        ebml_header hdr{};
        return reset(e)
            .fmap([&](auto d) { return read_ebml_header(&hdr, d); })
            .map([&] { res.header = traits::move(hdr); });
      })
      .fmap([&] { return read_element(in); })
      .assert(ebml_segment_id, "Invalid segment")
      .fmap([&](auto e) {
        segment seg{};
        return reset(e)
            .fmap([&](auto d) { return read_segment(&seg, d); })
            .map([&] { res.body = seg; });
      })
      .map([&] { return traits::move(res); });
}

[[nodiscard]] auto dump_doc(yoyo::reader &in) {
  return read_document(in)
      .map([](auto &&doc) {
        silog::log(silog::info, "EBMLVersion: %lld", doc.header.ebml_version);
        silog::log(silog::info, "EBMLReadVersion: %lld",
                   doc.header.ebml_read_version);
        silog::log(silog::info, "EBMLMaxIDLength: %lld",
                   doc.header.ebml_max_id_length);
        silog::log(silog::info, "EBMLMaxSizeLength: %lld",
                   doc.header.ebml_max_size_length);
        silog::log(silog::info, "DocType: [%s]", doc.header.doctype.data());
        silog::log(silog::info, "DocTypeVersion: %lld",
                   doc.header.doctype_version);
        silog::log(silog::info, "DocTypeReadVersion: %lld",
                   doc.header.doctype_read_version);
      })
      .map([] { return false; })
      .if_failed([&](auto msg) {
        return in.eof().assert([](auto v) { return v; }, msg);
      });
}

void fail(const char *err) {
  silog::log(silog::error, "Error: %s", err);
  throw 0;
}
int main() {
  yoyo::file_reader in{"example.mkv"};

  while (!dump_doc(in).take(fail)) {
    silog::log(silog::info, "------------------------------------");
  }
}
