#include "dot_runner.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

namespace cpp2model {
namespace {

std::string read_all(FILE *f) {
  std::string out;
  std::array<char, 4096> buf{};
  size_t n;
  while ((n = fread(buf.data(), 1, buf.size(), f)) > 0)
    out.append(buf.data(), n);
  return out;
}

} // namespace

std::string dot_to_svg(const std::string &dot_src, std::string &err) {
  err.clear();

  char tmpl[] = "/tmp/cpp2model_dot_XXXXXX";
  int fd = mkstemp(tmpl);
  if (fd < 0) {
    err = "mkstemp failed";
    return "";
  }
  {
    FILE *f = fdopen(fd, "w");
    if (!f) {
      close(fd);
      unlink(tmpl);
      err = "fdopen failed";
      return "";
    }
    fwrite(dot_src.data(), 1, dot_src.size(), f);
    fclose(f);
  }

  std::string cmd = "dot -Tsvg ";
  cmd += tmpl;
  cmd += " 2>&1";

  FILE *p = popen(cmd.c_str(), "r");
  if (!p) {
    unlink(tmpl);
    err = "popen failed";
    return "";
  }

  std::string output = read_all(p);
  int rc = pclose(p);
  unlink(tmpl);

  if (rc != 0) {
    err = "dot failed (exit " + std::to_string(rc) + "): " + output;
    return "";
  }
  return output;
}

} // namespace cpp2model