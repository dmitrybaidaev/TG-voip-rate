#include <string>
#include <iostream>
#include <fstream>
#include "opusfile_adapter.h"

namespace {
    void usage() {
        fprintf(stdout, "Usage:\n");
        fprintf(stdout, "TEST -i <in_filename> -o <out_filename>\n");
    }
}

int main(int argc, char **argv)
{
    if(argc < 3) {
        usage();
        return 1;
    }
    argc--; argv++;

    std::string input_file_name;
    std::string output_file_name;

#define TRACE_CMD_PARSER(success) if(!(success)) {printf("Error near argument '%s'\n", argv[-1]); return 1;}
    while (argc > 0) {
               if (!strcmp(*argv, "-h")) { usage(); return 1;
        } else if (!strcmp(*argv, "-i")) { argc--; argv++; TRACE_CMD_PARSER(argc > 0) input_file_name = std::string(*argv);
        } else if (!strcmp(*argv, "-o")) { argc--; argv++; TRACE_CMD_PARSER(argc > 0) output_file_name = std::string(*argv);
        } else {
            fprintf(stderr, "Unrecognized option %s\n", *argv);
            return 1;
        }
        argc--; argv++;
    }

    if(input_file_name.empty()) {
        usage();
        return 1;
    }

    if(output_file_name.empty()) {
        output_file_name = input_file_name + "_mono_16khz.wav";
    }

    if(!tg_rate::opus_decode_mono_16khz(input_file_name, output_file_name)) {
        fprintf(stderr, "failed to decode Opus file: %s\n", input_file_name.c_str());
        return 1;
    }

    return 0;
}


