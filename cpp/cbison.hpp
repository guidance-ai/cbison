#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <optional>
#include <filesystem>
#include "cbison_api.h"

namespace cbison {

/// C++ wrapper for a CBISON matcher instance.
class Matcher {
  cbison_factory_t api_;
  cbison_matcher_t m_;

public:
  /// Wrap existing matcher pointer.
  /// @param api  Factory pointer used to free/clone.
  /// @param m    Raw matcher pointer.
  Matcher(cbison_factory_t api, cbison_matcher_t m) noexcept;

  /// Frees the matcher.
  ~Matcher() noexcept;

  Matcher(const Matcher &) = delete;
  Matcher &operator=(const Matcher &) = delete;

  Matcher(Matcher &&o) noexcept;
  Matcher &operator=(Matcher &&o) noexcept;

  cbison_matcher_t get() const noexcept { return m_; }

  /// Clone the matcher.
  /// @return New Matcher.
  Matcher clone() const noexcept;

  /// Compute token mask for current state.
  /// @return Vector of representing bitmask for the entire tokenizer
  std::vector<uint32_t> computeMask() const noexcept;

  /// Compute fast-forward (forced) tokens.
  /// @param max_tokens  Maximum buffer size.
  /// @return Vector of token IDs, can be empty.
  std::vector<uint32_t> computeFFTokens(size_t max_tokens = 100) const noexcept;

  /// Get last error message from matcher.
  /// @return Optional string; std::nullopt if no error.
  std::optional<std::string> getError() const noexcept;

  /// Check if EOS token is allowed now.
  bool isAccepting() const noexcept { return api_->is_accepting(m_); }

  /// Check if matcher is forced-stopped (error or stop).
  bool isStopped() const noexcept { return api_->is_stopped(m_); }

  /// Validate how many tokens can be consumed.
  /// @param tokens  List of token IDs.
  /// @return Number of tokens consumable, or -1 on error.
  int validateTokens(const std::vector<uint32_t> &tokens) const noexcept;

  /// Consume tokens.
  /// @param tokens  List of token IDs.
  /// @return 0 on success, -1 on error.
  int consumeTokens(const std::vector<uint32_t> &tokens) const noexcept;

  /// Reset matcher to initial state.
  /// @return 0 on success, -1 on error.
  int reset() const noexcept { return api_->reset ? api_->reset(m_) : -1; }

  /// Backtrack matcher by n tokens.
  /// @param n  Number of tokens to rollback.
  /// @return 0 on success, -1 on error.
  int rollback(size_t n) const noexcept;
};

/// C++ wrapper for a CBISON factory.
class Factory {
  cbison_factory_t f_;

public:
  /// Wrap existing factory address.
  /// @param addr  Pointer value returned from loader.
  Factory(const void *addr) noexcept
      : f_(reinterpret_cast<cbison_factory_t>((void *)addr)) {}

  /// Frees the factory.
  ~Factory() noexcept;

  /// Vocabulary size.
  size_t nVocab() const noexcept { return f_->n_vocab; }

  /// Mask byte length: ceil(n_vocab/32)*4.
  size_t maskByteLen() const noexcept { return f_->mask_byte_len; }

  /// Create new matcher.
  /// @param type     Grammar type ("regex", "json", etc.).
  /// @param grammar  Grammar string.
  /// @return Matcher; m_.getError() yields error if any.
  Matcher newMatcher(const std::string &type,
                     const std::string &grammar) const noexcept;

  /// Validate grammar without creating matcher.
  /// @param type     Grammar type.
  /// @param grammar  Grammar string.
  /// @return pair(ok, message): ok==true on success or warning.
  std::pair<bool, std::string>
  validateGrammar(const std::string &type,
                  const std::string &grammar) const noexcept;

  /// Batch compute masks.
  /// @param reqs     Vector of (Matcher*, dest_pointer) pairs.
  /// @return 0 on success, -1 on error.
  int computeMasks(
      const std::vector<std::pair<Matcher *, uint32_t *>> &reqs) const noexcept;
};

/// C++ wrapper for a CBISON tokenizer instance.
class Tokenizer {
  cbison_tokenizer_t t_;

public:
  /// Wrap existing tokenizer.
  Tokenizer(cbison_tokenizer_t t) noexcept;

  /// Frees the tokenizer.
  ~Tokenizer() noexcept;

  Tokenizer(const Tokenizer &) = delete;
  Tokenizer &operator=(const Tokenizer &) = delete;

  Tokenizer(Tokenizer &&o) noexcept;
  Tokenizer &operator=(Tokenizer &&o) noexcept;

  cbison_tokenizer_t get() const noexcept;

  /// Return vector of bytes for given token.
  std::vector<uint8_t> getToken(uint32_t token_id) const noexcept;

  /// Tokenize bytes, return token ids.
  std::vector<uint32_t>
  tokenizeBytes(const std::vector<uint8_t> &bytes) const noexcept;

  /// Tokenize string (UTF-8), returns token ids.
  std::vector<uint32_t> tokenizeString(const std::string &s) const noexcept;

  /// Vocabulary size.
  size_t vocabSize() const noexcept { return t_->n_vocab; }

  /// EOS token ID.
  uint32_t eosTokenId() const noexcept { return t_->eos_token_id; }

  /// Whether input to tokenize_bytes must be UTF-8.
  bool requiresUtf8() const noexcept {
    return t_->tokenize_bytes_requires_utf8;
  }
};

class CbisonEngineDll {
  void *handle_ = nullptr;
  std::string prefix_;

  template <typename T> T get_sym(const std::string &name) const;

public:
  CbisonEngineDll() = default;

  /**
   * Loads the engine DLL from the given path and infers or sets the symbol
   * prefix. If prefix is empty, it's inferred from the filename stem.
   *
   * @param path Filesystem path to the shared library.
   * @param prefix Optional symbol prefix (e.g., "llg", "xgr").
   * @return true if successfully loaded, false otherwise.
   */
  bool load(const std::filesystem::path &path, const std::string &prefix = "");

  /**
   * Constructs a new HuggingFace tokenizer from a tokenizer.json string.
   *
   * @param tokenizer_json JSON content of tokenizer.json.
   * @param options_json Optional engine-specific options.
   * @param error_string Will be set to a diagnostic message, if any.
   * @return Tokenizer handle, or nullptr on error.
   */
  cbison_tokenizer_t new_hf_tokenizer(const std::string &tokenizer_json,
                                      const std::string &options_json,
                                      std::string &error_string);

  /**
   * Constructs a new CBISON factory for a given tokenizer and options.
   * Increments tokenizer's refcount. Returns nullptr on error.
   *
   * @param tokenizer Tokenizer handle.
   * @param options_json Optional engine-specific options.
   * @param error_string Will be set to a diagnostic message, if any.
   * @return Factory handle, or nullptr on error.
   */
  cbison_factory_t new_factory(cbison_tokenizer_t tokenizer,
                               const std::string &options_json,
                               std::string &error_string);

  /**
   * Constructs a minimal single-byte tokenizer (used for testing).
   *
   * @return Tokenizer handle, or nullptr on error.
   */
  cbison_tokenizer_t new_byte_tokenizer();
};

class CppTokenizer {
    cbison_tokenizer api_struct_;
    std::atomic<int> ref_count_{1};

    static CppTokenizer *fromC(cbison_tokenizer_t ptr);

    static int get_token_trampoline(cbison_tokenizer_t api, uint32_t token_id,
                                    uint8_t *bytes, size_t bytes_len);
    static int is_special_token_trampoline(cbison_tokenizer_t api, uint32_t token_id);
    static size_t tokenize_bytes_trampoline(cbison_tokenizer_t api,
                                            const char *bytes, size_t bytes_len,
                                            uint32_t *output_tokens,
                                            size_t output_tokens_len);
    static void incr_ref_trampoline(cbison_tokenizer_t api);
    static void decr_ref_trampoline(cbison_tokenizer_t api);
  
  public:
    CppTokenizer(size_t vocab, uint32_t eos, bool utf8_required);
    virtual ~CppTokenizer();
  
    cbison_tokenizer_t c_api() { return &api_struct_; }
  
    /// Get bytes for the given token.
    virtual std::vector<uint8_t> getToken(uint32_t token_id) const = 0;
  
    /// Returns true for non-plain-text tokens (like EOS).
    virtual bool isSpecialToken(uint32_t token_id) const = 0;
  
    /// Tokenize a string to token ids.
    virtual std::vector<uint32_t> tokenizeBytes(const std::string &input) const = 0;
  
  protected:
    size_t n_vocab;
    uint32_t eos_token_id;
    bool tokenize_bytes_requires_utf8;
  };

} // namespace cbison