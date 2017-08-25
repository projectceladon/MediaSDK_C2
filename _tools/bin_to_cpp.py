# Converts all files from source folder to their c array representations.
# Output .h files and .cpp files are written to <dst_dir>.
# Usage: python _tools/bin_to_c.py -i ../streams -o unittests/streams

import os
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("-i", "--input", dest="binary_dir", required=True,
                  help="source directory with binary files")
parser.add_argument("-o", "--output", dest="c_dir", required=True,
                  help="destination directory with c files")
args = parser.parse_args()

for root, dirs, filenames in os.walk(args.binary_dir):
    for stream_file_name in filenames:

        binary_path = os.path.join(root, stream_file_name)

        with open(binary_path, 'rb') as src:
            c_file_name = stream_file_name + ".cpp"
            h_file_name = stream_file_name + ".h"
            c_id = stream_file_name.replace('.', '_')

            with open(os.path.join(args.c_dir, h_file_name), 'w') as dst:
                dst.write("#pragma once\n\n")
                dst.write("#include \"test_streams.h\"\n\n")
                dst.write("extern StreamDescription {};\n".format(c_id))

            with open(os.path.join(args.c_dir, c_file_name), 'w') as dst:
                dst.write('#include "{}"\n\n'.format(h_file_name))
                dst.write("StreamDescription {} = {{\n".format(c_id))
                dst.write("    .sps = { },\n");
                dst.write("    .pps = { },\n");
                dst.write("    .data = {\n");

                bytes_count = 0
                while True:
                    chunk = src.read(16)
                    if not chunk:
                        break
                    dst.write('        ')
                    dst.write(' '.join("'\\x{0:02X}',".format(ord(x)) for x in chunk))
                    dst.write('\n')
                dst.write("    }\n")
                dst.write("};\n")
