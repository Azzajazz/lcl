# TODO List

## Bug fixes
- Improvements on error messages (on hold until the language is useable)
- Type checking of equality operator

## Feature adds
- Multiple function definitions
- Function calls
- Data structure definitions

## API improvements
- Change `AST_Node_List` to a bucket array, so that we get pointer stability and can return pointers instead of indexes

## Optimizations
- Ring buffer in lexer so that peeked tokens aren't processed twice.
- Hashmap for scope variable lookup
- Reduce allocations on sanitizer
- Primitive type IDs to avoid string comparisons
