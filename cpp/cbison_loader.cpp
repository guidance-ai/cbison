#include "cbison.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <dlfcn.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

#ifdef _WIN32

// dl_* functions from
// https://github.com/ggml-org/llama.cpp/blob/ba76543/ggml/src/ggml-backend-reg.cpp

static void *dl_load_library(const fs::path &path) {
  // suppress error dialogs for missing DLLs
  DWORD old_mode = SetErrorMode(SEM_FAILCRITICALERRORS);
  SetErrorMode(old_mode | SEM_FAILCRITICALERRORS);
  HMODULE handle = LoadLibraryW(path.wstring().c_str());
  SetErrorMode(old_mode);
  return handle;
}

static void *dl_get_sym(void *handle, const char *name) {
  DWORD old_mode = SetErrorMode(SEM_FAILCRITICALERRORS);
  SetErrorMode(old_mode | SEM_FAILCRITICALERRORS);
  void *p = (void *)GetProcAddress((HMODULE)handle, name);
  SetErrorMode(old_mode);
  return p;
}

#else

static void *dl_load_library(const fs::path &path) {
  return dlopen(path.string().c_str(), RTLD_NOW | RTLD_LOCAL);
}

static void *dl_get_sym(void *handle, const char *name) {
  return dlsym(handle, name);
}

#endif

namespace cbison {

template <typename T>
T CbisonEngineDll::get_sym(const std::string &name) const {
  return reinterpret_cast<T>(dl_get_sym(handle_, name.c_str()));
}

bool CbisonEngineDll::load(const std::filesystem::path &path,
                           const std::string &prefix) {
  handle_ = dl_load_library(path);
  if (!handle_)
    return false;

  if (!prefix.empty()) {
    prefix_ = prefix;
  } else {
    prefix_ = path.stem().string();
  }
  return true;
}

cbison_tokenizer_t
CbisonEngineDll::new_hf_tokenizer(const std::string &tokenizer_json,
                                  const std::string &options_json,
                                  std::string &error_string) {
  using fn_t = cbison_new_hf_tokenizer_fn_t;
  std::string sym = prefix_ + "_cbison_new_hf_tokenizer";
  fn_t fn = get_sym<fn_t>(sym);
  if (!fn) {
    error_string = "Missing symbol: " + sym;
    return nullptr;
  }

  constexpr size_t buf_len = 1024;
  char err_buf[buf_len] = {};
  cbison_tokenizer_t tok =
      fn(tokenizer_json.c_str(), options_json.c_str(), err_buf, buf_len);
  error_string = err_buf;
  return tok;
}

cbison_factory_t CbisonEngineDll::new_factory(cbison_tokenizer_t tokenizer,
                                              const std::string &options_json,
                                              std::string &error_string) {
  using fn_t = cbison_new_factory_fn_t;
  std::string sym = prefix_ + "_cbison_new_factory";
  fn_t fn = get_sym<fn_t>(sym);
  if (!fn) {
    error_string = "Missing symbol: " + sym;
    return nullptr;
  }

  constexpr size_t buf_len = 1024;
  char err_buf[buf_len] = {};
  cbison_factory_t factory =
      fn(tokenizer, options_json.c_str(), err_buf, buf_len);
  error_string = err_buf;
  return factory;
}

cbison_tokenizer_t CbisonEngineDll::new_byte_tokenizer() {
  using fn_t = cbison_new_byte_tokenizer_fn_t;
  std::string sym = prefix_ + "_cbison_new_byte_tokenizer";
  fn_t fn = get_sym<fn_t>(sym);
  return fn ? fn() : nullptr;
}

} // namespace cbison