// JavaScript bindings for OpenColorIO: load a config, list what it offers, bake 3D LUTs.
//
// Configs are read from the module's file system (Module.FS), so a page writes the .ocio
// and its LUT files there first. An .ocioz archive or an ocio:// built-in URI also works.
//
// A bake samples the source colour space's code values on an n x n x n grid from 0 to 1,
// red varying fastest (the .cube order), and returns n*n*n*3 floats.

#include <OpenColorIO/OpenColorIO.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <string>
#include <vector>

namespace OCIO = OCIO_NAMESPACE;
using emscripten::val;

namespace {

OCIO::ConstConfigRcPtr config;

[[noreturn]] void fail(const std::string &message) {
  val::global("Error").new_(message).throw_();
}

OCIO::ConstConfigRcPtr current() {
  if (!config) fail("No config loaded");
  return config;
}

val bake(const OCIO::ConstProcessorRcPtr &processor, int size) {
  if (size < 2 || size > 129) fail("Size must be between 2 and 129");
  const size_t count = size_t(size) * size * size;
  std::vector<float> rgb(count * 3);
  size_t i = 0;
  for (int b = 0; b < size; ++b)
    for (int g = 0; g < size; ++g)
      for (int r = 0; r < size; ++r) {
        rgb[i++] = float(r) / (size - 1);
        rgb[i++] = float(g) / (size - 1);
        rgb[i++] = float(b) / (size - 1);
      }
  OCIO::PackedImageDesc image(rgb.data(), long(count), 1, 3);
  processor->getDefaultCPUProcessor()->apply(image);
  // copy out of module memory so the result survives the next call
  return val::global("Float32Array").new_(val(emscripten::typed_memory_view(rgb.size(), rgb.data())));
}

}  // namespace

#ifndef HEAVISIDE_VERSION
#define HEAVISIDE_VERSION "dev"
#endif

// OCIO's version, and Heaviside's own (YY.MMDD.HHMM, or "dev" for a local build)
std::string version() { return OCIO::GetVersion(); }
std::string build() { return HEAVISIDE_VERSION; }

void load(const std::string &path) {
  try {
    auto loaded = OCIO::Config::CreateFromFile(path.c_str());
    loaded->validate();
    config = loaded;
  } catch (const OCIO::Exception &e) {
    fail(e.what());
  }
}

// { name, spaces: [{ name, family, encoding, display }], displays: [{ name, views }] }
val info() {
  auto cfg = current();
  val spaces = val::array();
  const int n = cfg->getNumColorSpaces(OCIO::SEARCH_REFERENCE_SPACE_ALL, OCIO::COLORSPACE_ACTIVE);
  for (int i = 0; i < n; ++i) {
    const char *name = cfg->getColorSpaceNameByIndex(OCIO::SEARCH_REFERENCE_SPACE_ALL, OCIO::COLORSPACE_ACTIVE, i);
    auto space = cfg->getColorSpace(name);
    val s = val::object();
    s.set("name", std::string(name));
    s.set("family", std::string(space->getFamily()));
    s.set("encoding", std::string(space->getEncoding()));
    s.set("display", space->getReferenceSpaceType() == OCIO::REFERENCE_SPACE_DISPLAY);
    spaces.call<void>("push", s);
  }
  val displays = val::array();
  for (int d = 0; d < cfg->getNumDisplays(); ++d) {
    const char *display = cfg->getDisplay(d);
    val views = val::array();
    for (int v = 0; v < cfg->getNumViews(display); ++v) views.call<void>("push", std::string(cfg->getView(display, v)));
    val entry = val::object();
    entry.set("name", std::string(display));
    entry.set("views", views);
    displays.call<void>("push", entry);
  }
  val out = val::object();
  out.set("name", std::string(cfg->getName()));
  out.set("spaces", spaces);
  out.set("displays", displays);
  return out;
}

val bakeView(const std::string &source, const std::string &display, const std::string &view, int size) {
  try {
    return bake(current()->getProcessor(source.c_str(), display.c_str(), view.c_str(), OCIO::TRANSFORM_DIR_FORWARD), size);
  } catch (const OCIO::Exception &e) {
    fail(e.what());
  }
}

val bakeSpace(const std::string &source, const std::string &destination, int size) {
  try {
    return bake(current()->getProcessor(source.c_str(), destination.c_str()), size);
  } catch (const OCIO::Exception &e) {
    fail(e.what());
  }
}

EMSCRIPTEN_BINDINGS(heaviside) {
  emscripten::function("version", &version);
  emscripten::function("build", &build);
  emscripten::function("load", &load);
  emscripten::function("info", &info);
  emscripten::function("bakeView", &bakeView);
  emscripten::function("bakeSpace", &bakeSpace);
}
