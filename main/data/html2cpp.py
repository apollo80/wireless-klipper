#!/usr/bin/python3
# -*- coding: utf-8 -*-

import os,sys
#print("PYTHONPATH={}".format(sys.path))
import rcssmin

out_file_prefix = '''/*
 * @file
 * @brief esp8266 tcp2serial bridge for klipper
 * @detauls html page template
 *
 * @author: apollo80
 * @email: apollo80@list.ru
 */


#undef  ICACHE_RODATA_ATTR
#define ICACHE_RODATA_ATTR  __attribute__((section(".irom.text")))

'''

def convert(in_filename, out_filename):
    py_cssmin = rcssmin._make_cssmin(python_only=True)
    outfile_cpp = open(out_filename + ".c", 'w')
    outfile_cpp.write(out_file_prefix)

    with open(in_filename, 'r') as infile:
        cppLine = str()
        for line in infile:
            if line.startswith("<!--") and line.endswith(" -->\n"):
                block_name = line[5:-5]
                if cppLine:
                    cppLine += "\";\n"
                    outfile_cpp.write(cppLine);

                cppLine = "const char html_" + block_name + "[] ICACHE_RODATA_ATTR = \""
            else:
                cppLine += py_cssmin(line).replace("\"", "\\\"");


if __name__ == '__main__':
    convert(sys.argv[1], sys.argv[2])
