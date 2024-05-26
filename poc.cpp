#pragma leco tool
import hai;
import hashley;
import jute;
import missingno;
import silog;
import traits;
import yoyo;

using namespace traits::ints;

// {{{1 data types
using vint = uint64_t;
static_assert(sizeof(vint) == 8);
using uint = uint64_t;
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

struct seek {
  uint id;
  uint position;
};

struct seek_head {
  hai::varray<unsigned> keys{16};
  hashley::siobhan map{17};
};

struct info {
  uint timestamp_scale;
};

struct audio {
  double freq{8000};
  uint channels{1};
  uint emphasis{0};
};

struct video {
  uint interlaced{0};
  uint field_order{2};
  uint pixel_width;
  uint pixel_height;
};

struct track {
  uint number;
  uint uid;
  uint type;
  uint enabled{1};
  uint def{1};
  uint lacing{1};
  uint max_block_add_id{0};
  hai::cstr codec_id;
  uint codec_delay{0};
  uint seek_pre_roll{0};
  video video;
  audio audio;
};

struct tracks {
  hai::varray<track> list{16};
};

struct cluster {
  uint timestamp;
  hai::varray<yoyo::subreader> blocks{8};
};

struct segment {
  seek_head seek;
  info info;
  tracks tracks;
  hai::varray<cluster> clusters{1024};
};

struct document {
  ebml_header header;
  segment body;
};

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
        return in.seekg(-1, yoyo::seek_mode::current)
            .trace("reading vint")
            .map([&] { return o; });
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
constexpr auto read_element(auto &in) { // reader or subreader
  element res{};
  return read_vint(in, true)
      .fmap([&](auto id) {
        res.id = id;
        return read_vint(in);
      })
      .fmap([&](auto size) { return yoyo::subreader::create(&in, size); })
      .fmap([&](auto sr) {
        res.data = sr;
        return in.seekg(sr.raw_size(), yoyo::seek_mode::current)
            .trace("jumping to end of element");
      })
      .map([&] { return res; });
}

// {{{1 read attribute
// {{{2 generic interface
template <typename Tp>
constexpr mno::req<void> read(yoyo::subreader &in, Tp *res);
template <typename Tp> constexpr mno::req<void> read_attr(Tp *v, element e) {
  return e.data.seekg(0, yoyo::seek_mode::set)
      .trace("reading attribute")
      .fmap([&] {
        if (e.data.raw_size() == 0)
          return mno::req<void>{};

        return read(e.data, v);
      });
}
constexpr mno::req<void> read_until_eof(yoyo::subreader &in, const auto &fn) {
  return mno::req<void>{}.until_failure(
      [&] {
        return read_element(in).trace("reading list of elements").fmap(fn);
      },
      [&](auto err) { return !in.eof().unwrap(false); });
}

// {{{2 read uint
template <> constexpr mno::req<void> read(yoyo::subreader &in, uint *res) {
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
// {{{2 read double
template <> constexpr mno::req<void> read(yoyo::subreader &in, double *res) {
  switch (in.raw_size()) {
  case 4: {
    return in.read_u32_be().map([&](auto n) {
      int64_t exp =
          static_cast<int64_t>((n >> 23) & 0xFF) - ((1 << (8 - 1)) - 1);
      uint64_t man = n & ((1 << 23) - 1);
      uint64_t sign = man | (1 << 23);
      float decs = sign / static_cast<float>(1 << 23);

      float d = (n & (1 << 31)) ? -1 : 1;
      d *= decs;
      if (exp > 0) {
        for (auto i = 0; i < exp; i++) {
          d *= 2.0;
        }
      } else {
        for (auto i = 0; i < -exp; i++) {
          d *= 0.5;
        }
      }
      *res = d;
    });
  }
  case 8: {
    return in.read_u64_be().map([&](auto n) {
      int64_t exp =
          static_cast<int64_t>((n >> 52) & 0x7FF) - ((1 << (11 - 1)) - 1);
      uint64_t man = n & ((1ULL << 52) - 1);
      uint64_t sign = man | (1ULL << 52);
      double decs = sign / static_cast<float>(1ULL << 52);

      double d = (n & (1ULL << 63)) ? -1 : 1;
      d *= decs;
      if (exp > 0) {
        for (auto i = 0; i < exp; i++) {
          d *= 2.0;
        }
      } else {
        for (auto i = 0; i < -exp; i++) {
          d *= 0.5;
        }
      }
      *res = d;
    });
  }
  default:
    return mno::req<void>::failed("Invalid float size");
  }
}
namespace {
template <auto N> static constexpr double test(const char (&buf)[N]) {
  yoyo::ce_reader in{buf};
  double res{};
  return yoyo::subreader::create(&in, N - 1)
      .fmap([&](auto sub) { return read(sub, &res); })
      .map([&] { return res; })
      .unwrap(0.0);
}
// Tests with weird exponents (this gets exp = 200, which is larger than any
// available int)
static_assert(0 < test("abcd"));
static_assert(0.15625 == test("\x3e\x20\0\0"));
static_assert(25 == test("\x41\xC8\0\0"));
static_assert(0.01171875 == test("\x3F\x88\0\0\0\0\0\0"));
} // namespace

// {{{2 read cstr
template <> constexpr mno::req<void> read(yoyo::subreader &in, hai::cstr *str) {
  auto octets = in.raw_size();
  if (octets > 65536)
    return mno::req<void>::failed("Unsupported string bigger than 64kb");

  *str = hai::cstr{static_cast<unsigned>(octets)};
  return in.read(str->data(), octets);
}
// {{{2 read ebml_header
template <>
constexpr mno::req<void> read(yoyo::subreader &in, ebml_header *res) {
  return read_until_eof(in, [&](element &e) {
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
  });
}
// {{{2 read seek
template <> constexpr mno::req<void> read(yoyo::subreader &in, seek *res) {
  return read_until_eof(in, [&](element &e) {
    switch (e.id) {
    case 0x53AB:
      return read_attr(&res->id, e);
    case 0x53AC:
      return read_attr(&res->position, e);
    default:
      return mno::req<void>::failed("Invalid element inside seek");
    }
  });
}

// {{{2 read seek_list
template <> constexpr mno::req<void> read(yoyo::subreader &in, seek_head *res) {
  return read_until_eof(in, [&](element &e) {
    switch (e.id) {
    case 0xBF: // CRC-32
      return mno::req<void>{};
    case 0x4DBB: {
      seek s{};
      return read_attr(&s, e).map([&] {
        res->map[s.id] = s.position;
        res->keys.push_back_doubling(s.id);
      });
    }
    default:
      return mno::req<void>::failed("Non-seek element inside seek head");
    }
  });
}

// {{{2 read info
template <> constexpr mno::req<void> read(yoyo::subreader &in, info *res) {
  return read_until_eof(in, [&](element &e) {
    switch (e.id) {
    case 0x2AD7B1:
      return read_attr(&res->timestamp_scale, e);
    default:
      return mno::req<void>{};
    }
  });
}

// {{{3 read video
template <> constexpr mno::req<void> read(yoyo::subreader &in, video *res) {
  return read_until_eof(in, [&](element &e) {
    switch (e.id) {
    case 0x9A:
      return read_attr(&res->interlaced, e);
    case 0x9D:
      return read_attr(&res->field_order, e);
    case 0xB0:
      return read_attr(&res->pixel_width, e);
    case 0xBA:
      return read_attr(&res->pixel_height, e);
    default:
      return mno::req<void>{};
    }
  });
}

// {{{3 read audio
template <> constexpr mno::req<void> read(yoyo::subreader &in, audio *res) {
  return read_until_eof(in, [&](element &e) {
    switch (e.id) {
    case 0xB5:
      return read_attr(&res->freq, e);
    case 0x9F:
      return read_attr(&res->channels, e);
    case 0x52F1:
      return read_attr(&res->emphasis, e);
    default:
      return mno::req<void>{};
    }
  });
}

// {{{2 read tracks
template <> constexpr mno::req<void> read(yoyo::subreader &in, track *res) {
  return read_until_eof(in, [&](element &e) {
    switch (e.id) {
    case 0xD7:
      return read_attr(&res->number, e);
    case 0x73C5:
      return read_attr(&res->uid, e);
    case 0x83:
      return read_attr(&res->type, e);
    case 0xB9:
      return read_attr(&res->enabled, e);
    case 0x88:
      return read_attr(&res->def, e);
    case 0x9C:
      return read_attr(&res->lacing, e);
    case 0x55EE:
      return read_attr(&res->max_block_add_id, e);
    case 0x86:
      return read_attr(&res->codec_id, e);
    case 0x56AA:
      return read_attr(&res->codec_delay, e);
    case 0x56BB:
      return read_attr(&res->seek_pre_roll, e);
    case 0xE0:
      return read_attr(&res->video, e);
    case 0xE1:
      return read_attr(&res->audio, e);
    default:
      return mno::req<void>{};
    }
  });
}
template <> constexpr mno::req<void> read(yoyo::subreader &in, tracks *res) {
  return read_until_eof(in, [&](element &e) {
    switch (e.id) {
    case 0xBF: // CRC-32
      return mno::req<void>{};
    case 0xAE: {
      track t{};
      return read_attr(&t, e).map([&] { res->list.push_back(t); });
    }
    default:
      return mno::req<void>::failed("Non-track element inside tracks");
    }
  });
}

// {{{2 read cluster
template <> constexpr mno::req<void> read(yoyo::subreader &in, cluster *res) {
  return read_until_eof(in, [&](element &e) {
    switch (e.id) {
    case 0xE7:
      return read_attr(&res->timestamp, e);
    case 0xA3:
      res->blocks.push_back_doubling(e.data);
      return mno::req<void>{};
    case 0xA0:
      return mno::req<void>::failed("BlockGroup TBD");
    default:
      return mno::req<void>{};
    }
  });
}

// {{{2 read segment
template <> constexpr mno::req<void> read(yoyo::subreader &in, segment *res) {
  return read_until_eof(in, [&](element &e) {
    switch (e.id) {
    case 0x114D9B74:
      return read_attr(&res->seek, e);
    case 0x1549A966:
      return read_attr(&res->info, e);
    case 0x1654AE6B:
      return read_attr(&res->tracks, e);
    case 0x1F43B675: {
      cluster c{};
      return read_attr(&c, e).map([&] { res->clusters.push_back(c); });
    }
    default:
      return mno::req<void>{};
    }
  });
}
// }}}1

// {{{1 top-level stuff
constexpr bool ebml_element_id(const element &e) { return e.id == 0x1A45DFA3; }
constexpr bool ebml_segment_id(const element &e) { return e.id == 0x18538067; }
constexpr auto read_document(yoyo::reader &in) {
  document res{};
  return read_element(in)
      .assert(ebml_element_id, "invalid header element id")
      .fmap([&](auto e) { return read_attr(&res.header, e); })
      .trace("reading header")

      .fmap([&] { return read_element(in); })
      .assert(ebml_segment_id, "invalid segment element id")
      .fmap([&](auto e) { return read_attr(&res.body, e); })
      .trace("reading segment")

      .map([&] { return traits::move(res); });
}

[[nodiscard]] static constexpr auto translate_id(uint id) {
  switch (id) {
  case 0x1254c367:
    return "Tags";
  case 0x1549a966:
    return "Info";
  case 0x1654ae6b:
    return "Tracks";
  default:
    return "<unknown>";
  }
}
[[nodiscard]] static constexpr auto tr_track_type(uint t) {
  switch (t) {
  case 1:
    return "video";
  case 2:
    return "audio";
  default:
    return "<other>";
  }
}

constexpr static char hexdigit(unsigned n) {
  return n >= 10 ? ('A' + (n - 10)) : ('0' + n);
}
[[nodiscard]] static mno::req<void> hexdump_line(yoyo::subreader r) {
  char octets[16];
  return r.read(octets, sizeof(octets)).map([&] {
    jute::heap line{""};
    for (unsigned char c : octets) {
      char octet[]{"   "};
      octet[0] = hexdigit(c / 16);
      octet[1] = hexdigit(c % 16);
      line = line + jute::view{octet};
    }
    silog::log(silog::info, "%.*s", static_cast<unsigned>((*line).size()),
               (*line).data());
  });
}

[[nodiscard]] static mno::req<void> hexdump(yoyo::subreader r) {
  mno::req<void> res{};
  for (auto i = 0; i < 10 && res.is_valid(); i++) {
    res = hexdump_line(r);
  }
  return res;
}

[[nodiscard]] static mno::req<void> dump_doc(yoyo::reader &in) {
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
        silog::log(silog::info, "SeekHead:");
        for (auto k : doc.body.seek.keys) {
          silog::log(silog::info, "  %10s @%d", translate_id(k),
                     doc.body.seek.map[k]);
        }
        silog::log(silog::info, "Timestamp scale: %lld",
                   doc.body.info.timestamp_scale);
        for (const auto &t : doc.body.tracks.list) {
          silog::log(silog::info, "Track %lld", t.number);
          silog::log(silog::info, "           ID %lld", t.uid);
          silog::log(silog::info, "         Type %s", tr_track_type(t.type));
          silog::log(silog::info, "      Enabled %lld", t.enabled);
          silog::log(silog::info, "      Default %lld", t.def);
          silog::log(silog::info, "       Lacing %lld", t.lacing);
          silog::log(silog::info, "  MaxBlkAddID %lld", t.max_block_add_id);
          silog::log(silog::info, "        Codec %s", t.codec_id.data());
          silog::log(silog::info, "   CodecDelay %lld", t.codec_delay);
          silog::log(silog::info, "  SeekPreRoll %lld", t.seek_pre_roll);
          if (t.type == 1) {
            silog::log(silog::info, "   Interlaced %lld", t.video.interlaced);
            silog::log(silog::info, "   FieldOrder %lld", t.video.field_order);
            silog::log(silog::info, "   PixelWidth %lld", t.video.pixel_width);
            silog::log(silog::info, "  PixelHeight %lld", t.video.pixel_height);
          } else if (t.type == 2) {
            silog::log(silog::info, "    Frequency %f", t.audio.freq);
            silog::log(silog::info, "     Channels %lld", t.audio.channels);
            silog::log(silog::info, "     Emphasis %lld", t.audio.emphasis);
          }
        }
        for (const auto &c : doc.body.clusters) {
          silog::log(silog::info, "Cluster @%ld (%d blocks)", c.timestamp,
                     c.blocks.size());
          auto blk = c.blocks[0];
          blk.seekg(0, yoyo::seek_mode::set)
              .fmap([&] { return hexdump(blk); })
              .fmap([&] { return blk.seekg(0, yoyo::seek_mode::end); })
              .log_error();
        }
      })
      .map([] { return false; })
      .if_failed([&](auto msg) {
        return in.eof().assert([](auto v) { return v; }, msg);
      })
      .fmap([&](bool eof) {
        if (eof)
          return mno::req<void>{};

        silog::log(silog::info, "------------------------------------");
        return dump_doc(in);
      });
}

int main() {
  yoyo::file_reader::open("example.mkv")
      .fmap(dump_doc)
      .trace("reading movie")
      .log_error();
}
// }}}1
