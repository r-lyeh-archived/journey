# Journey :camel: <a href="https://travis-ci.org/r-lyeh/journey"><img src="https://api.travis-ci.org/r-lyeh/journey.svg?branch=master" align="right" /></a>

Lightweight append-only, header-less, journaling file format for backups (C++11)

### Features
- [x] Journaling support: data can be rolled back to an earlier state to retrieve older versions of files.
- [x] Append-only format: create or update new entries just by appending stuff to the journal file.
- [x] Compaction support: all duplicated names are removed. the one with the largest timestamp is kept.
- [x] Always aligned: data is always aligned for safe memory accesses.
- [x] Concat friendly: glue journals together and the result will still be a valid journey file.
- [x] Foreign support: append data to a foreign file and result will still be a valid journey file.
- [x] Simple, tiny, portable, cross-platform, header-only.
- [x] ZLIB, LibPNG licensed.

### Extension
`.joy`

### File format:
```
[random data]
entry #1: [file][info]
entry #2: [file][info]
...
entry #N: [file][info]
[EOF]
//
Where, file block = {
     [ 64-bit   padding ... ]
     [ 08-bit file name ... ][\0]
     [ 64-bit   padding ... ]
     [ 08-bit file data ... ]
     [ 64-bit   padding ... ] 
}
Where, info block = {
     [ 64-bit stamp             ]
     [ 64-bit name block length ]
     [ 64-bit data block length ]
     [ 64-bit file block length ]
     [ 64-bit magic             ]
}
```

### Showcase
```c++
#include <iostream>
#include <string>
#include "journey.hpp"
int main( int argc, const char **argv ) {
    journey j;
    if( argc > 2 ) {
        j.init( argv[2] );
        if( std::string(argv[1]) == "append" ) {
            std::cout << j.load() << std::endl;
            std::string prev = j.read( "hello.txt" ); prev += '.';
            std::cout << j.append( "hello.txt", prev.c_str(), prev.size() ) << std::endl;
            std::cout << j.append( "empty", "", 0 ) << std::endl;
        }
        if( std::string(argv[1]) == "read" ) {
            std::cout << j.load() << std::endl;
            std::cout << j.read( "hello.txt" ) << std::endl;
        }
        if( std::string(argv[1]) == "list" ) {
            std::cout << j.load(0, std::time(0), &std::cout) << std::endl;
        }
        if( std::string(argv[1]) == "compact" && argc > 3 ) {
            std::cout << j.load() << std::endl;
            std::cout << j.compact(argv[3]) << std::endl;
        }
    } else {
        std::cout << argv[0] << " list    src_file.joy" << std::endl;
        std::cout << argv[0] << " read    src_file.joy" << std::endl;
        std::cout << argv[0] << " append  dst_file.joy" << std::endl;
        std::cout << argv[0] << " compact src_file.joy dst_file.joy" << std::endl;
    }
}
```

### Changelog
- v2.0.1 (2015/12/08): Fix compilation warnings (un/signed warnings)
- v2.0.0 (2015/12/07): More compact file format; fixes
- v1.0.0 (2015/12/05): Initial commit
