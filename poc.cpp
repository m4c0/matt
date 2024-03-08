#pragma leco tool
import silog;
import yoyo;

void fail(const char *err) {
  silog::log(silog::error, "Error: %s", err);
  throw 0;
}

int main() {
  yoyo::file_reader in{"example.mkv"};

  auto id = in.read_u32().take(fail);
  auto ver = in.read_u32().take(fail);
  auto size = in.read_u32_be().take(fail);
  in.seekg(size, yoyo::seek_mode::current).take(fail);
  silog::log(silog::info, "Header: %08x %d (size = %d)", id, ver, size);

  id = in.read_u32().take(fail);
  ver = in.read_u32().take(fail);
  size = in.read_u32_be().take(fail);
  in.seekg(size, yoyo::seek_mode::current).take(fail);
  silog::log(silog::info, "Segment: %08x %d (size = %d)", id, ver, size);
}
