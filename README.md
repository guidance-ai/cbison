# C Binary Interface for Structured Output Negotiation (CBISON)

This repo defines an interface for integration of structured output (constrained decoding)
engines into LLM inference systems.
It presents layout of an object that can be created by an engine,
and then passed to the inference system.

## cbison_factory

The core of the interface is a C struct `cbison_factory` that contains
(in addition to version and magic numbers, etc.) the following function pointers:

- `validate_grammar`, taking type and text of a grammar, and returning a boolean
  and diagnostics
- `new_matcher`, also taking type and text of a grammar, and returning a pointer to a matcher object

Functions on matcher objects are also defined in factory and include:

- state accessors: `get_error`, `is_accepting`, `is_stopped`
- `compute_mask`, which writes a bitmask corresponding to allowed tokens
  in the current state of the matcher
- `consume_tokens` advancing the state of the matcher

Following matcher functions are optional:

- `validate_tokens` checking if tokens would be accepted
- `compute_ff_tokens` returning any fast-forward tokens forced by the matcher
- `rollback` which is the inverse of `consume_tokens`
- `reset` which resets the matcher to the initial state

## cbison_tokenizer

A grammar engine constructs the factory interface given a `cbison_tokenizer`,
which defines the following functions:

- `get_token`, which given a token ID returns corresponding sequence of bytes
- `is_special_token`, which given a token ID returns true if the token is special
  (eg., `<|endoftext|>`, `<think>`, etc.)
- `tokenize_bytes`, which takes a sequence of bytes and returns a list of token IDs
  (this is required to correctly compute "fast-forward" tokens based on "fast-forward" bytes)
