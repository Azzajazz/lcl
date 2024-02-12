# TODO List

## Bug fixes
- Improvements on error messages (on hold until the language is useable)
- Type checking of equality operator

## Feature adds
- Multiple function definitions
- Function calls
- Data structure definitions

## Optimizations
- Ring buffer in lexer so that peeked tokens aren't processed twice.
- Hashmap for scope variable lookup
- Reduce allocations on sanitizer
- Primitive type IDs to avoid string comparisons
