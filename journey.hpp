// Journey is a lightweight append-only, header-less, journaling file format for backups (C++11)
// - rlyeh, zlib/libpng licensed.

// Features:
// - [x] Journaling support: data can be rolled back to an earlier state to retrieve older versions of files.
// - [x] Append-only format: create or update new entries just by appending stuff to the journal file.
// - [x] Compaction support: all duplicated names are removed. the one with the largest timestamp is kept.
// - [x] Always aligned: data is always aligned for safe memory accesses.
// - [x] Concat friendly: glue journals together and the result will still be a valid journey file.
// - [x] Foreign support: append data to a foreign file and result will still be a valid journey file.
// - [x] Simple, tiny, portable, cross-platform, header-only.
// - [x] ZLIB, LibPNG licensed.

// Extension:
// .joy

// File format:
// [random data]
// entry #1: [file][info]
// entry #2: [file][info]
// ...
// entry #N: [file][info]
// [EOF]
//
// Where, file block = {
//      [ 64-bit   padding ... ]
//      [ 08-bit file name ... ][\0]
//      [ 64-bit   padding ... ]
//      [ 08-bit file data ... ]
//      [ 64-bit   padding ... ] 
// }
// Where, info block = {
//      [ 64-bit stamp             ]
//      [ 64-bit name block length ]
//      [ 64-bit data block length ]
//      [ 64-bit file block length ]
//      [ 64-bit magic             ]
// }

#pragma once
#include <stdint.h>
#include <ctime>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#define JOURNEY_VERSION "2.0.1" /* (2015/12/08) Fix compilation warnings (un/signed warnings)
#define JOURNEY_VERSION "2.0.0" // (2015/12/07) More compact file format; fixes
#define JOURNEY_VERSION "1.0.0" // (2015/12/05) Initial commit */

class journey {
public:

    struct entry {
        uint64_t offset;
        uint64_t size;
        uint64_t stamp;
    };

    journey()
    {}

    journey( const std::string &file ) {
        init( file );
    }

    bool init( const std::string &file ) {
        *this = journey();
        return file.empty() ? false : (journal = file, true);
    }

    bool load( uint64_t beg_stamp = 0, uint64_t end_stamp = std::time(0), std::ostream *debugstream = 0 ) {
        init( std::string(journal) );
        if( beg_stamp > end_stamp ) {
            return false;
        }
        std::ifstream ifs( journal.c_str(), std::ios::binary | std::ios::ate );
        auto skip_padding = [&] {
            int pad = (((uint64_t(ifs.tellg()) + 8) & ~7) - uint64_t(ifs.tellg())) % 8;
            ifs.ignore( pad );
        };
        std::string fname;
        unsigned count = 0;
        while( ifs.good() && ifs.tellg() >= (8 * 5) ) {
            uint64_t stamp, namelen, datalen, filelen, magic;
            ifs.seekg( -8*5, ifs.cur );
            ifs.read( (char *)&stamp,   8 );
            ifs.read( (char *)&namelen, 8 );
            ifs.read( (char *)&datalen, 8 );
            ifs.read( (char *)&filelen, 8 );
            ifs.read( (char *)&magic,   8 );
            if( magic != magic_right_endian && magic != magic_wrong_endian ) {
                break;
            }

            ifs.seekg( -8*5-int64_t(filelen), ifs.cur );
            skip_padding();

            fname.resize( namelen );
            ifs.read( &fname[0], namelen );
            ifs.ignore(1);
            skip_padding();

            bool inscribed = ( stamp >= beg_stamp && stamp <= end_stamp && toc.find( fname ) == toc.end() ); 
            if( inscribed ) {
                toc[ fname ] = entry{ (uint64_t)ifs.tellg(), datalen, stamp };
            }
            if( debugstream ) {
                std::string brief( datalen > 16 ? 16 : datalen, '\0' ), action[2] = { "skipped", "inscribed" };
                ifs.read( &brief[0], datalen > 16 ? 16 : datalen );
                ifs.ignore( datalen > 16 ? datalen - 16 : 0 );
                *debugstream << "v1 - " << action[inscribed] << " '" << fname << "' " << datalen << " datalen; stamp=" << stamp << "; brief=" << brief << std::endl;
            } else {
                ifs.ignore( datalen );
            }
            skip_padding();

            ifs.seekg( -int64_t(filelen), ifs.cur );
            count ++;
        }
        if( debugstream ) {
            *debugstream << "---" << std::endl;
        }
        return ifs.good() && count > 0;
    }

    std::map<std::string, entry> get_toc() const {
        return toc;
    }

    template<typename T>
    bool read( T &data, const std::string &name ) const {
        auto found = toc.find(name);
        if( found != toc.end() ) {
            auto &entry = found->second;
            data.resize( entry.size );
            std::ifstream ifs( journal.c_str(), std::ios::binary );
            ifs.seekg( entry.offset );
            ifs.read( &data[0], entry.size );
            if( ifs.good() ) {
                return true;
            }
        }
        return (data = T(), false);
    }

    std::string read( const std::string &name ) const {
        std::string data;
        return read( data, name ) ? data : std::string();
    }

    bool append( const std::string &filename, const void *ptr, size_t len, uint64_t stamp = std::time(0) ) const {
        if( journal.size() && ptr && !filename.empty() ) {
            std::ofstream ofs( journal.c_str(), std::ios::binary | std::ios::app | std::ios::ate );
            auto write_padding = [&] {
                char buf[8] = {0};
                int pad = (((uint64_t(ofs.tellp()) + 8) & ~7) - uint64_t(ofs.tellp())) % 8;
                ofs.write( buf, pad );
            };
            if( ofs.good() ) {
                uint64_t namelen = filename.size(), datalen = len;
                uint64_t filelen = -int64_t( ofs.tellp() ), magic = magic_right_endian;
                write_padding();
                ofs.write( filename.c_str(), namelen );
                ofs.write( "\0", 1 );
                write_padding();
                ofs.write( (const char *)ptr, datalen );
                write_padding();
                filelen += int64_t(ofs.tellp());
                ofs.write( (const char *)&stamp,   8 );
                ofs.write( (const char *)&namelen, 8 );
                ofs.write( (const char *)&datalen, 8 );
                ofs.write( (const char *)&filelen, 8 );
                ofs.write( (const char *)&magic,   8 );
            }
            return ofs.good();
        }
        return false;
    }

    bool compact( const std::string &new_journal_file ) const {
        if( toc.empty() ) {
            return false;
        }
        std::string data;
        journey j2( new_journal_file );
        // preload everything (this can be memory hungry)
        for( auto &entry : toc ) {
            const char *name = entry.first.c_str();
            const auto &info = entry.second;
            if( !read( data, name ) ) {
                return false;
            }
            if( !j2.append( name, &data[0], data.size(), info.stamp ) ) {
                return false;
            }
        }
        return true;
    }

    protected:
    std::string journal;
    uint64_t magic_right_endian = 0x3179656E72756F6A; // 'journey1'
    uint64_t magic_wrong_endian = 0x6A6F75726E657931; // 'journey1' swapped
    std::map< std::string, entry > toc;
};


#ifdef JOURNEY_BUILD_TESTS

// tiny unittest suite { // usage: int main() { /* orphan test */ test(1<2); suite("grouped tests") { test(1<2); test(1<2); } }
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define suite(...) if(printf("------ %d ", __LINE__), printf(__VA_ARGS__), puts(""), true)
#define test(...)  (errno=0,++tst,err+=!(ok=!!(__VA_ARGS__))),printf("[%s] %d %s (%s)\n",ok?" OK ":"FAIL",__LINE__,#__VA_ARGS__,strerror(errno))
unsigned tst=0,err=0,ok=atexit([]{ suite("summary"){ printf("[%s] %d tests = %d passed + %d errors\n",err?"FAIL":" OK ",tst,tst-err,err); }});
// } rlyeh, public domain.

int main() {
    std::ostream *debugstream = 0; // &std::cout;
    uint64_t now = std::time(0), past = now / 2;

    journey j;
    suite( "initializing" ) {
        test( j.init( "journey.joy" ) );
    }

    suite( "creating or appending item, in the past timeline" ) {
        test( j.append( "hello.txt", "previous", strlen("previous"), past ) );
    }

    suite( "load toc from bigbang till now, and verify newly inserted item" ) {
        test( j.load(0, now, debugstream) );
        test( j.read( "hello.txt" ) == "previous" );
    }

    suite( "append new revision of file, with current timestamp" ) {
        test( j.append( "hello.txt", "latest", strlen("latest"), now ) );
    }

    suite( "refresh whole toc, and check latest revision is up to date" ) {
        test( j.load(0, now, debugstream) );
        test( j.read( "hello.txt" ) == "latest" );
    }

    suite( "refresh toc from bigbang till half time, and compact archive" ) {
        test( j.load(0, past, debugstream) );
        test( j.compact( "journey2.joy" ) );
    }
    suite( "ensure file is past revision") {
        journey j2;
        test( j2.init("journey2.joy") );
        test( j2.load(0, now, debugstream) );
        test( j2.read( "hello.txt" ) == "previous" );
    }

    suite( "refresh toc from half time till now, and compact archive" ) {
        test( j.load(past + 1, now, debugstream) );
        test( j.compact( "journey2.joy" ) );
    }

    suite( "ensure file is latest revision") {
        journey j2;
        test( j2.init("journey2.joy") );
        test( j2.load(0, now, debugstream) );
        test( j2.read( "hello.txt" ) == "latest" );
    }
}
#endif

#ifdef JOURNEY_BUILD_DEMO
#include <iostream>
#include <string>
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
#endif
