# bin2coff
Tool that creates Microsoft COFF object files out of binary files. Runs on Cygwin (may run on Linux - untested).

## Build
Code has a GNU-ism, `__attribute__((packed))`. Builds on Cygwin with GCC. Program is single-file (`bin2coff.c`).

## Usage
```
bin2coff FILENAME [ SYMBOL_PREFIX [ END_SYMBOL_PREFIX ] ]
```

Default value for `SYMBOL_PREFIX` is `File_`; default value for `END_SYMBOL_PREFIX` is `FileEnd_`.

The tool creates a Microsoft COFF object file with a single section consisting of full input file data. The section is non-executable and read-only. The tool adds two symbols pointing to beginning and end of data. It appends input file basename to `SYMBOL_PREFIX` and `END_SYMBOL_PREFIX` to make names for symbols pointing to beginning and end of data, respectively. The section is named after the input file basename.

The object file has undefined machine type field (so e. g. `objdump` won't recognize it).

## License
0-clause-BSD-like; see `LICENSE`.

## Author

[**chromoblob**](https://github.com/chromoblob)

© 2026
