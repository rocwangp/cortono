#include "../std.hpp"

namespace cortono::http
{
    class html_codec
    {
        public:
            static std::string decode(const std::string &uri)
            {
                //Note from RFC1630:  "Sequences which start with a percent sign
                //but are not followed by two hexadecimal characters (0-9,A-F) are reserved
                //for future extension"
                const unsigned char *ptr = (const unsigned char *)uri.c_str();
                std::string ret;
                ret.reserve(uri.length());
                for (; *ptr; ++ptr)
                {
                    if (*ptr == '%')
                    {
                        if (*(ptr + 1))
                        {
                            char a = *(ptr + 1);
                            char b = *(ptr + 2);
                            if (!((a >= 0x30 && a < 0x40) || (a >= 0x41 && a < 0x47))) continue;
                            if (!((b >= 0x30 && b < 0x40) || (b >= 0x41 && b < 0x47))) continue;
                            char buf[3];
                            buf[0] = a;
                            buf[1] = b;
                            buf[2] = 0;
                            ret += (char)strtoul(buf, NULL, 16);
                            ptr += 2;
                            continue;
                        }
                    }
                    /* if (*ptr=='+') */
                    /* { */
                    /*     ret+=' '; */
                    /*     continue; */
                    /* } */
                    ret += *ptr;
                }
                return ret;
            }
    };

    class gzip_codec
    {
        public:
            static std::string compress(std::string_view filename) {
                using namespace std::experimental;
                auto filesize = filesystem::file_size(filename);
                std::vector<char> buffer(filesize + 1, '0');
                std::ifstream fin("test.js");
                fin.readsome(&buffer[0], filesize);
                fin.close();
                std::string compressed_data;
                unsigned char out[1024 * 128] = "\0";
                z_stream strm;
                strm.zalloc = Z_NULL;
                strm.zfree = Z_NULL;
                strm.opaque = Z_NULL;
                if(deflateInit2(&strm, -1, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
                    return "";
                }
                strm.next_in = (unsigned char*)(&buffer[0]);
                strm.avail_in = (uInt)filesize;
                do {
                    int have;
                    strm.avail_out = 128 * 1024;
                    strm.next_out = out;
                    if(deflate(&strm, Z_FINISH) == Z_STREAM_ERROR) {
                        return "";
                    }
                    have = 128 * 1024 - strm.avail_out;
                    compressed_data.append((char*)out, have);
                }while(strm.avail_out == 0);
                if(deflateEnd(&strm) != Z_OK) {
                    return "";
                }
                return compressed_data;
            }
    };

}

