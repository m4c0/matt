#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define EXE(X) X ".exe"
#else
#define EXE(X) X
#endif

#ifdef __clang__
#  define CC "clang"
#elif _MSC_VER
#  define CC "cl"
#else
#  define CC "cc"
#endif

static void usage() {
  fprintf(stderr, "just call 'build' without arguments\n");
}

static int run(char ** args) {
  assert(args && args[0]);

#ifdef _WIN32
  if (0 == _spawnvp(_P_WAIT, args[0], (const char * const *)args)) {
    return 0;
  }
#else
  pid_t pid = fork();
  if (pid == 0) {
    execvp(args[0], args);
    abort();
  } else if (pid > 0) {
    int sl = 0;
    assert(0 <= waitpid(pid, &sl, 0));
    if (WIFEXITED(sl)) return WEXITSTATUS(sl);
  }
#endif

  fprintf(stderr, "failed to run child process: %s\n", args[0]);
  return 1;
}

int main(int argc, char ** argv) {
  if (argc != 1) return (usage(), 1);

  // TODO: support other compilers/platforms
  // TODO: support arbitrary path for libopus
  char * args[] = {
    EXE(CC), "-Wall",
    "-I", "opus-rfc6716/include", "opus-rfc6716/libopus.a",
    "-o", EXE("mkv2wav"), "mkv2wav.c",
    0
  };
  if (run(args)) return 1;

  return 0;
}
