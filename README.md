# Journey :camel: <a href="https://travis-ci.org/r-lyeh/journey"><img src="https://api.travis-ci.org/r-lyeh/journey.svg?branch=master" align="right" /></a>

Lightweight append-only, header-less, journaling file format (C++11)

### Features
- [x] Journaling support: data can be rolled back to an earlier state to retrieve older versions of files.
- [x] Append-only format: create or update new entries just by appending stuff to the journal file.
- [x] Compaction support: all duplicated names are removed and only the one with the largest timestamp is kept.
- [x] Always aligned: data is always aligned for safe accesses.
- [x] Concat friendly: journals can be glued together and the result will still be a valid journey file.
- [x] Foreign support: can append data to a foreign file and result will still be a valid journey file.
- [x] Simple, tiny, portable, cross-platform, header-only.
- [x] ZLIB, LibPNG licensed.

### File format:
```
          /--------------------------block---------------------------\ /-------------------------------------------info----------------------------------------------\
entry #1: [64-bit padding (\0)] [8-bit data ...] [64-bit padding (\0)] [1024-bytes utf-8 name ...] [64-bit datalen] [64-bit stamp] [64-bit block bytes] [64-bit magic] 
entry #2: [64-bit padding (\0)] [8-bit data ...] [64-bit padding (\0)] [1024-bytes utf-8 name ...] [64-bit datalen] [64-bit stamp] [64-bit block bytes] [64-bit magic] 
etc...
```

### Changelog
- v1.0.0 (2015/12/05): Initial commit
