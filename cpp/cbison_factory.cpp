#include "cbison.hpp"

namespace cbison {

Matcher::Matcher(cbison_factory_t api, cbison_matcher_t m) noexcept
    : api_(api), m_(m) {}

Matcher::~Matcher() noexcept {
  if (m_)
    api_->free_matcher(m_);
}

Matcher::Matcher(Matcher &&o) noexcept : api_(o.api_), m_(o.m_) {
  o.m_ = nullptr;
}

Matcher &Matcher::operator=(Matcher &&o) noexcept {
  if (m_)
    api_->free_matcher(m_);
  api_ = o.api_;
  m_ = o.m_;
  o.m_ = nullptr;
  return *this;
}

Matcher Matcher::clone() const noexcept {
  auto c = api_->clone_matcher(m_);
  return Matcher(api_, c);
}

std::vector<uint32_t> Matcher::computeMask() const noexcept {
  size_t bytes = api_->mask_byte_len;
  size_t words = bytes / 4;
  std::vector<uint32_t> mask(words);
  api_->compute_mask(m_, mask.data(), bytes);
  return mask;
}

std::vector<uint32_t> Matcher::computeFFTokens(size_t max_tokens) const noexcept {
  std::vector<uint32_t> buf(max_tokens);
  int32_t n = api_->compute_ff_tokens(m_, buf.data(), max_tokens);
  if (n < 0)
    return {};
  buf.resize(static_cast<size_t>(n));
  return buf;
}

std::optional<std::string> Matcher::getError() const noexcept {
  auto e = api_->get_error(m_);
  if (!e)
    return std::nullopt;
  return std::string(e);
}

int Matcher::validateTokens(const std::vector<uint32_t> &tokens) const noexcept {
  return api_->validate_tokens(m_, tokens.data(), tokens.size());
}

int Matcher::consumeTokens(const std::vector<uint32_t> &tokens) const noexcept {
  return api_->consume_tokens(m_, tokens.data(), tokens.size());
}

int Matcher::rollback(size_t n) const noexcept {
  return api_->rollback ? api_->rollback(m_, n) : -1;
}

Factory::~Factory() noexcept {
  if (f_)
    f_->free_factory(f_);
}

Matcher Factory::newMatcher(const std::string &type, const std::string &grammar) const noexcept {
  auto m = f_->new_matcher(f_, type.c_str(), grammar.c_str());
  return Matcher(f_, m);
}

std::pair<bool, std::string> Factory::validateGrammar(const std::string &type, const std::string &grammar) const noexcept {
  char buf[16 * 1024];
  int32_t r = f_->validate_grammar(f_, type.c_str(), grammar.c_str(), buf, sizeof(buf));
  if (r == 0)
    return {true, ""};
  return {r >= 0, std::string(buf)};
}

int Factory::computeMasks(const std::vector<std::pair<Matcher *, uint32_t *>> &reqs) const noexcept {
  size_t n = reqs.size();
  std::vector<cbison_mask_req_t> c(n);
  for (size_t i = 0; i < n; ++i) {
    c[i].matcher = reqs[i].first->get();
    c[i].mask_dest = reqs[i].second;
  }
  return f_->compute_masks ? f_->compute_masks(f_, c.data(), n) : -1;
}

Tokenizer::Tokenizer(cbison_tokenizer_t t) noexcept : t_(t) {
  if (t_)
    t_->incr_ref_count(t_);
}

Tokenizer::~Tokenizer() noexcept {
  if (t_)
    t_->decr_ref_count(t_);
}

Tokenizer::Tokenizer(Tokenizer &&o) noexcept : t_(o.t_) {
  o.t_ = nullptr;
}

Tokenizer &Tokenizer::operator=(Tokenizer &&o) noexcept {
  if (t_)
    t_->decr_ref_count(t_);
  t_ = o.t_;
  o.t_ = nullptr;
  return *this;
}

cbison_tokenizer_t Tokenizer::get() const noexcept {
  return t_;
}

std::vector<uint8_t> Tokenizer::getToken(uint32_t token_id) const noexcept {
  size_t est_len = 32;
  std::vector<uint8_t> buf(est_len);
  int n = t_->get_token(t_, token_id, buf.data(), buf.size());
  if (n < 0)
    return {};
  if (static_cast<size_t>(n) > buf.size()) {
    buf.resize(n);
    n = t_->get_token(t_, token_id, buf.data(), buf.size());
    if (n < 0)
      return {};
  }
  buf.resize(n);
  return buf;
}

std::vector<uint32_t> Tokenizer::tokenizeBytes(const std::vector<uint8_t> &bytes) const noexcept {
  if (!t_->tokenize_bytes)
    return {};
  size_t est_tokens = bytes.size() + 1;
  std::vector<uint32_t> out(est_tokens);
  size_t n = t_->tokenize_bytes(t_, (const char*)bytes.data(), bytes.size(), out.data(), out.size());
  out.resize(n);
  return out;
}

std::vector<uint32_t> Tokenizer::tokenizeString(const std::string &s) const noexcept {
  return tokenizeBytes(std::vector<uint8_t>(s.begin(), s.end()));
}

} // namespace cbison
