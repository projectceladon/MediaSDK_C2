# /********************************************************************************
#
# INTEL CORPORATION PROPRIETARY INFORMATION
# This software is supplied under the terms of a license agreement or nondisclosure
# agreement with Intel Corporation and may not be copied or disclosed except in
# accordance with the terms of that agreement
# Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.
#
# *********************************************************************************/

# Produces html report file with description tables of all gtest tests
# found in specified folder.
# Usage: python _tools/gather_tests.py -i unittests/src -o tests.html

from __future__ import print_function  # Only needed for Python 2
import os
import re

re_test = re.compile("TEST(?:_P)?\((.*), (.*)\)")
re_decode_conditions = re.compile('DecodingConditions\(\"(.*)\"\)')
re_comment = re.compile(" *// (.*) ?")

import argparse

parser = argparse.ArgumentParser()
parser.add_argument("-i", "--input", dest="test_src_dir", required=True,
                  help="directory with unittests sources")
parser.add_argument("-o", "--output", dest="html_output", required=True,
                  help="html report output file")
args = parser.parse_args()

def HtmlBegin(dst):
    print('<!DOCTYPE html><html><body>', file = dst)

def HtmlEnd(dst):
    print('</body></html>', file = dst)

def HtmlTable(columns, data, dst):
    print('<table>', file = dst)
    print('<tr><th>', file = dst)
    print('</th><th>'.join(columns), file = dst)
    print('</th></tr>', file = dst)
    for sublist in data:
        print('<tr><td>', file = dst)
        print('</td><td>'.join(sublist), file = dst)
        print('</td></tr>', file = dst)
    print('</table>', file = dst)

def HtmlHeading(text):
    print('<h3>', file = dst)
    print(text, file = dst)
    print('</h3>', file = dst)

def FilterRows(src, condition):
    dst = []
    for row in src[:]:
        if condition(row):
            dst.append(row)
            src.remove(row)
    return dst

tests = []
decode_conditions = []

for root, dirs, filenames in os.walk(args.test_src_dir):
    for f in filenames:
        with open(os.path.join(root, f),'r') as src:
            lines = src.readlines()
            for i in range(len(lines)):
                m = re_test.match(lines[i])
                if not m:
                    decode_condition = re_decode_conditions.search(lines[i])
                if m or decode_condition:
                    comment = ""
                    for j in range(i - 1, 1, -1):
                        comment_match = re_comment.match(lines[j])
                        if comment_match:
                            if len(comment) > 0:
                                comment = comment_match.group(1) + " " + comment
                            else:
                                comment = comment_match.group(1)
                        else:
                            break

                    if m:
                        tests.append((m.group(1), m.group(2), comment))
                    else:
                        decode_conditions.append((decode_condition.group(1), comment))

store_tests = FilterRows(tests, lambda row: row[0] in ["MfxComponentStore", "MfxC2Service"] )
decoder_tests = FilterRows(tests, lambda row: "Decoder" in row[0])
decoder_tests += [('DecoderDecode', 'Check/' + dc[0], dc[1]) for dc in decode_conditions]
encoder_tests = FilterRows(tests, lambda row: "Encoder" in row[0])

columns = ('Testable', 'Test', 'Description')

with open(args.html_output, 'w') as dst:
    HtmlBegin(dst)
    HtmlHeading('C2 Store and Service Tests')
    HtmlTable(columns, store_tests, dst)
    HtmlHeading('C2 Mock Component and Utility Library Tests')
    HtmlTable(columns, tests, dst)
    HtmlHeading('C2 Decoding Component Tests')
    HtmlTable(columns, decoder_tests, dst)
    HtmlHeading('C2 Encoding Component Tests')
    HtmlTable(columns, encoder_tests, dst)
    HtmlEnd(dst)
