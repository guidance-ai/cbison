#include "cbison.hpp"
#include <cstring>
#include <algorithm>
#include <cassert>

#define CBISON_TOKENIZER_IMPL_MAGIC 0xc9f0b1a1

namespace cbison {

CppTokenizer *CppTokenizer::fromC(cbison_tokenizer_t ptr) {
  assert(ptr && ptr->magic == CBISON_TOKENIZER_MAGIC);
  assert(ptr->impl_magic == CBISON_TOKENIZER_IMPL_MAGIC);
  return reinterpret_cast<CppTokenizer *>(ptr->impl_data);
}

int CppTokenizer::get_token_trampoline(cbison_tokenizer_t api,
                                       uint32_t token_id, uint8_t *bytes,
                                       size_t bytes_len) {
  CppTokenizer *self = fromC(api);
  auto tok = self->getToken(token_id);
  size_t n = tok.size();
  if (bytes && bytes_len)
    std::memcpy(bytes, tok.data(), std::min(bytes_len, n));
  return int(n);
}

int CppTokenizer::is_special_token_trampoline(cbison_tokenizer_t api,
                                              uint32_t token_id) {
  CppTokenizer *self = fromC(api);
  if (token_id >= self->n_vocab)
    return -1;
  return self->isSpecialToken(token_id) ? 1 : 0;
}

size_t CppTokenizer::tokenize_bytes_trampoline(cbison_tokenizer_t api,
                                               const char *bytes,
                                               size_t bytes_len,
                                               uint32_t *output_tokens,
                                               size_t output_tokens_len) {
  auto toks = fromC(api)->tokenizeBytes(std::string(bytes, bytes_len));
  if (output_tokens && output_tokens_len)
    std::memcpy(output_tokens, toks.data(),
                std::min(output_tokens_len, toks.size()) * sizeof(uint32_t));
  return toks.size();
}

void CppTokenizer::incr_ref_trampoline(cbison_tokenizer_t api) {
  fromC(api)->ref_count_.fetch_add(1, std::memory_order_relaxed);
}

void CppTokenizer::decr_ref_trampoline(cbison_tokenizer_t api) {
  CppTokenizer *self = fromC(api);
  if (self->ref_count_.fetch_sub(1, std::memory_order_acq_rel) == 1)
    delete self;
}

CppTokenizer::CppTokenizer(size_t vocab, uint32_t eos, bool utf8_required)
    : n_vocab(vocab), eos_token_id(eos),
      tokenize_bytes_requires_utf8(utf8_required) {
  std::memset(&api_struct_, 0, sizeof(api_struct_));
  api_struct_.magic = CBISON_TOKENIZER_MAGIC;
  api_struct_.impl_magic = CBISON_TOKENIZER_IMPL_MAGIC;
  api_struct_.impl_data = this;
  api_struct_.version_major = CBISON_TOKENIZER_VERSION_MAJOR;
  api_struct_.version_minor = CBISON_TOKENIZER_VERSION_MINOR;
  api_struct_.n_vocab = vocab;
  api_struct_.eos_token_id = eos;
  api_struct_.tokenize_bytes_requires_utf8 = utf8_required;
  api_struct_.get_token = get_token_trampoline;
  api_struct_.is_special_token = is_special_token_trampoline;
  api_struct_.tokenize_bytes = tokenize_bytes_trampoline;
  api_struct_.incr_ref_count = incr_ref_trampoline;
  api_struct_.decr_ref_count = decr_ref_trampoline;
}

CppTokenizer::~CppTokenizer() = default;

} // namespace cbison