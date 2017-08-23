# Produces html report file with description tables of all gtest tests
# found in specified folder.
# Usage: python _tools/gather_tests.py -i unittests/src -o tests.html

from __future__ import print_function  # Only needed for Python 2
import os
import re

re_test = re.compile("TEST\((.*), (.*)\)")
re_comment = re.compile("// (.*) ?")

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
for root, dirs, filenames in os.walk(args.test_src_dir):
    for f in filenames:
        with open(os.path.join(root, f),'r') as src:
            lines = src.readlines()
            for i in range(len(lines)):
                m = re_test.match(lines[i])
                if m:
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
                    tests.append((m.group(1), m.group(2), comment))

store_tests = FilterRows(tests, lambda row: row[0] == "MfxComponentStore")
decoder_tests = FilterRows(tests, lambda row: row[0] == "MfxDecoderComponent")
encoder_tests = FilterRows(tests, lambda row: row[0] == "MfxEncoderComponent")

columns = ('Testable', 'Test', 'Description')

with open(args.html_output, 'w') as dst:
    HtmlBegin(dst)
    HtmlHeading('C2 Store Tests')
    HtmlTable(columns, store_tests, dst)
    HtmlHeading('C2 Mock Component and Utility Library Tests')
    HtmlTable(columns, tests, dst)
    HtmlHeading('C2 Decoding Component Tests')
    HtmlTable(columns, decoder_tests, dst)
    HtmlHeading('C2 Encoding Component Tests')
    HtmlTable(columns, encoder_tests, dst)
    HtmlEnd(dst)
