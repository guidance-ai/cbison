#include <cassert>
#include <iostream>
#include <vector>
#include <algorithm>
#include <utility>
#include "cbison.hpp"

static void test_for_tokenizer(cbison::CbisonEngineDll &engine,
                               cbison_tokenizer_t t0) {
  cbison::Tokenizer t(t0);
  std::string err;
  auto fptr = engine.new_factory(t0, "{}", err);
  if (!fptr) {
    std::cerr << "Failed to create factory: " << err << '\n';
    abort();
  }
  cbison::Factory f(fptr);
  t0->decr_ref_count(t0);

  // validate grammar
  {
    auto [ok, msg] = f.validateGrammar("json", "{}");
    assert(ok && msg.empty());
  }
  {
    auto [ok, msg] = f.validateGrammar("json", "foobar");
    assert(!ok);
    assert(msg.find("expected ident") != std::string::npos);
  }

  // error on bad grammar
  {
    auto m_err = f.newMatcher("json", "foobar");
    auto err = m_err.getError();
    assert(err && err->find("expected ident") != std::string::npos);
  }

  // matcher on valid grammar
  auto m = f.newMatcher("json", "{}");
  assert(!m.getError());
  assert(!m.isAccepting());

  // validate_tokens for incomplete JSON
  auto tokens = t.tokenizeString("{\"a\":abc}");
  int n_valid = m.validateTokens(tokens);
  assert(n_valid < static_cast<int>(tokens.size()));

  // validate & consume for complete JSON
  tokens = t.tokenizeString("{\"a\":12}");
  n_valid = m.validateTokens(tokens);
  assert(n_valid == static_cast<int>(tokens.size()));
  assert(!m.isAccepting());
  m.consumeTokens(tokens);
  assert(m.isAccepting());
  assert(m.isStopped());

  // rollback and clone
  m.rollback(3);
  auto m2 = m.clone();
  assert(!m.isAccepting());
  assert(!m.isStopped());

  // consume last 3 tokens
  std::vector<uint32_t> last3(tokens.end() - 3, tokens.end());
  m.consumeTokens(last3);
  assert(m.isAccepting());
  assert(m.isStopped());

  // reset and re-consume full stream
  m.reset();
  assert(!m.isAccepting());
  assert(!m.isStopped());
  m.consumeTokens(tokens);
  assert(m.isAccepting());
  assert(m.isStopped());

  // m2 independent state
  assert(!m2.isAccepting());
  assert(!m2.isStopped());
  m2.consumeTokens(last3);
  assert(m2.isAccepting());
  assert(m2.isStopped());

  // compute mask and ff tokens
  m2.rollback(1);
  auto mask2 = m2.computeMask();
  for (auto v : mask2)
    std::cout << v << ' ';
  std::cout << '\n';
  auto ff = m2.computeFFTokens();
  assert(ff.empty());

  // batch compute masks
  m.rollback(1);
  size_t batch = 3;
  size_t words = f.maskByteLen() / 4;
  std::vector<uint32_t> mask(batch * words, 0);
  std::vector<std::pair<cbison::Matcher *, uint32_t *>> reqs = {
      {&m, mask.data()}, {&m2, mask.data() + 2 * words}};
  int rc = f.computeMasks(reqs);
  assert(rc == 0);

  // verify rows
  std::vector<uint32_t> row0(mask.begin(), mask.begin() + words);
  std::vector<uint32_t> row2(mask.begin() + 2 * words,
                             mask.begin() + 3 * words);
  assert(row0 == mask2);
  assert(row2 == mask2);
  for (size_t i = words; i < 2 * words; ++i)
    assert(mask[i] == 0);
}

class TrivialByteTokenizer : public cbison::CppTokenizer {
public:
  TrivialByteTokenizer()
      : CppTokenizer(257, 0x100, false) {} // 0x100 is EOS token ID

  std::vector<uint8_t> getToken(uint32_t token_id) const override {
    if (token_id < 0x100)
      return {static_cast<uint8_t>(token_id)};
    if (token_id == eos_token_id) {
      static constexpr char eos_str[] = "<|eos|>";
      return std::vector<uint8_t>(eos_str, eos_str + sizeof(eos_str) - 1);
    }
    return {};
  }

  bool isSpecialToken(uint32_t token_id) const override {
    return token_id == eos_token_id;
  }

  std::vector<uint32_t> tokenizeBytes(const std::string &input) const override {
    std::vector<uint32_t> result;
    result.reserve(input.size());
    for (unsigned char c : input)
      result.push_back(c);
    return result;
  }
};

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <path to engine library> [prefix]\n";
    return 1;
  }

  cbison::CbisonEngineDll engine;
  std::string prefix = "";
  if (argc >= 3) {
    prefix = argv[2];
  }
  if (!engine.load(argv[1], prefix)) {
    std::cerr << "Failed to load engine library: " << argv[1] << '\n';
    return 1;
  }

  test_for_tokenizer(engine, engine.new_byte_tokenizer());
  auto t = new TrivialByteTokenizer();
  test_for_tokenizer(engine, t->c_api());

  std::cout << "All tests passed\n";
  return 0;
}