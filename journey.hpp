// Journey is a header-less backup file format.
// - rlyeh, zlib/libpng licensed.

// Features:
// - [x] Journaling support: data can be rolled back to an earlier state to retrieve older versions of files.
// - [x] Append-only format: create or update new entries just by appending stuff to the journal file.
// - [x] Compaction support: all duplicated names are removed and only the one with the largest timestamp is kept.
// - [x] Always aligned: data is always aligned for safe accesses.
// - [x] Concat friendly: journals can be glued together and the result will still be a valid journey file.
// - [x] Foreign support: can append data to a foreign file and result will still be a valid journey file.
// - [x] Simple, tiny, portable, cross-platform, header-only.
// - [x] ZLIB, LibPNG licensed.

// File format:
//           /--------------------------block---------------------------\ /-------------------------------------------info----------------------------------------------\
// entry #1: [64-bit padding (\0)] [8-bit data ...] [64-bit padding (\0)] [1024-bytes utf-8 name ...] [64-bit datalen] [64-bit stamp] [64-bit block bytes] [64-bit magic] 
// entry #2: [64-bit padding (\0)] [8-bit data ...] [64-bit padding (\0)] [1024-bytes utf-8 name ...] [64-bit datalen] [64-bit stamp] [64-bit block bytes] [64-bit magic] 
// etc...

#pragma once
#include <stdint.h>
#include <ctime>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#define JOURNEY_VERSION "1.0.0" // (2015/12/05) Initial commit

class journey {
    std::string journal;
    uint64_t magic_right_endian = 0x3179656E72756F6A; // 'journey1'
    uint64_t magic_wrong_endian = 0x6A6F75726E657931; // 'journey1' swapped
    struct entry {
        uint64_t offset;
        uint64_t size;
        uint64_t stamp;
    };
    std::map< std::string, entry > toc;
public:

    journey()
    {}

    journey( const std::string &file ) {
        init( file );
    }

    bool init( const std::string &file ) {
        *this = journey();
        return file.empty() ? false : (journal = file, true);
    }

    bool load( uint64_t beg_stamp = 0, uint64_t end_stamp = std::time(0) ) {
        if( beg_stamp > end_stamp ) {
            return false;
        }
        std::ifstream ifs( journal.c_str(), std::ios::binary | std::ios::ate );
        auto skip_padding = [&] {
            int pad = (((uint64_t(ifs.tellg()) + 8) & ~7) - uint64_t(ifs.tellg())) % 8;
            ifs.ignore( pad );
        };
        unsigned count = 0;
        while( ifs.good() && ifs.tellg() >= (1024 + 8 + 8 + 8 + 8) ) {
            char fname[1024 + 1] = {};
            uint64_t bytes, stamp, block, magic;
            ifs.seekg( -1024-32, ifs.cur );
            ifs.read( (char *)&fname, 1024); fname[1024] = '\0';
            ifs.read( (char *)&bytes, 8 );
            ifs.read( (char *)&stamp, 8 );
            ifs.read( (char *)&block, 8 );
            ifs.read( (char *)&magic, 8 );
            if( magic != magic_right_endian && magic != magic_wrong_endian ) {
                break;
            }
            ifs.seekg( -1024-32-block, ifs.cur );
            skip_padding();
            bool inscribe = ( stamp >= beg_stamp && stamp <= end_stamp && toc.find( fname ) == toc.end() ); 
            if( inscribe ) {
                //std::cout << "v1 - inscribed '" << fname << "' " << bytes << " bytes - " << stamp << std::endl;
                toc[ fname ] = entry{ (uint64_t)ifs.tellg(), bytes, stamp };
            } else {
                //std::cout << "v1 - skipped '" << fname << "' " << bytes << " bytes - " << stamp << std::endl;
            }
            ifs.ignore( bytes );
            skip_padding();
            ifs.seekg( -block, ifs.cur );
            count ++;
        }
        return ifs.good() && count > 0;
    }

    template<typename T>
    bool read( T &data, const char *name ) const {
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

    std::string read( const char *name ) const {
        std::string data;
        return read( data, name ) ? data : std::string();
    }

    bool append( const std::string &filename, const void *ptr, size_t len, uint64_t stamp = std::time(0) ) const {
        if( journal.size() && ptr && stamp && !filename.empty() ) {
            std::ofstream ofs( journal.c_str(), std::ios::binary | std::ios::app | std::ios::ate );
            auto write_padding = [&] {
                char buf[8] = {0};
                int pad = (((uint64_t(ofs.tellp()) + 8) & ~7) - uint64_t(ofs.tellp())) % 8;
                ofs.write( buf, pad );
            };
            if( ofs.good() ) {
                uint64_t block = -uint64_t( ofs.tellp() ), magic = magic_right_endian, bytes = len;
                write_padding();
                ofs.write( (const char *)ptr, len );
                write_padding();
                block += uint64_t(ofs.tellp());
                char buffer[ 1024 + 1 ] = {};
                std::strcpy( buffer, filename.c_str() );
                ofs.write( buffer, 1024 );
                ofs.write( (const char *)&bytes, 8 );
                ofs.write( (const char *)&stamp, 8 );
                ofs.write( (const char *)&block, 8 );
                ofs.write( (const char *)&magic, 8 );
            }
            return ofs.good();
        }
        return false;
    }

    bool compact( const char *newfile ) const {
        if( toc.empty() ) {
            return false;
        }
        std::string data;
        journey j2( newfile );
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
};


#ifndef JOURNEY_BUILD_DEMO

#include <iostream>
#include <string>

int main( int argc, const char **argv ) {
    journey j;
    if( argc > 2 ) {
        j.init( argv[2] );
        if( std::string(argv[1]) == "append" ) {
            std::string prev = j.read( "hello.txt" ); prev += '.';
            std::cout << j.append( "hello.txt", prev.c_str(), prev.size() ) << std::endl;
            std::cout << j.append( "empty", "", 0 ) << std::endl;
        }
        if( std::string(argv[1]) == "read" ) {
            std::cout << j.load() << std::endl;
            std::cout << j.read( "hello.txt" ) << std::endl;
        }
        if( std::string(argv[1]) == "compact" && argc > 3 ) {
            std::cout << j.load() << std::endl;
            std::cout << j.compact(argv[3]) << std::endl;
        }
    } else {
        std::cout << argv[0] << " append  dst_file.jou" << std::endl;
        std::cout << argv[0] << " read    src_file.jou" << std::endl;
        std::cout << argv[0] << " compact src_file.jou dst_file.jou" << std::endl;
    }
}

#endif