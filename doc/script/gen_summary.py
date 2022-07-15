#!/usr/bin/env python3

import os
import os.path
import logging
import sys
from pathlib import Path

logging.basicConfig(
        level=logging.DEBUG,
        format='%(asctime)s.%(msecs)03d %(levelname)s %(module)s - %(funcName)s: %(message)s',
        datefmt='%Y-%m-%d %H:%M:%S')

def mk_dir(dist):
    if not os.path.isdir(dist):
        os.makedirs(dist)
    else:
        #logging.warning('dist dir exists!')
        pass

def head_level(l):
    level = 0
    for a in l:
        if a == '#':
            level += 1
        else:
            break
    return level

def number_list_to_anchor(number):
    s = ''
    for n in number:
        s += str(n)
        s += '_'
    if len(s) > 0:
        return s[:-1]

def head_line_to_summary(head, level, part, anchor):
    indent = '    ' * (level - 1)
    indent += '* '
    title = head.lstrip('#').strip()
    summary = '{}[{}](part{}/README.md#{})'.format(indent, title, part, anchor)
    return summary

def remove_head_line_tail_anchor(head):
    i = head.find('{')
    if i >= 0:
        ret = head[0:i]
    else:
        ret = head
    return ret.rstrip()

def build_summary(part, md, dist_dir, summary_file):
    dist_file = os.path.join(dist_dir, md)
    dist_file_dir = os.path.dirname(dist_file)
    mk_dir(dist_file_dir)

    with open(md, 'r') as f:
        inlines = f.readlines()
    logging.info('parse file: {}'.format(md))

    with open(dist_file, 'w') as f, open(summary_file, 'a+') as sf:
        number = [part - 1, ]
        for l in inlines:
            level = head_level(l)
            if level == 0:
                f.write(l)
                continue
            l = remove_head_line_tail_anchor(l)

            current_level = len(number)
            if level == current_level:
                number[level - 1] += 1
            if level > current_level:
                number.extend([0,] * (level - current_level))
                number[level - 1] += 1
            if level < current_level:
                number = number[0:level]
                number[level - 1] += 1
            anchor = number_list_to_anchor(number)
            summary = head_line_to_summary(l, level, part, anchor)
            logging.debug('summary: {}'.format(summary))

            # add anchor
            l += ' <div id={}></div>'.format(anchor)
            f.write(l + os.linesep)
            sf.write(summary + os.linesep)

def for_each_md(src_dir, dist_dir):
    summary_file = os.path.join(dist_dir, 'src/SUMMARY.md')
    summary_file_dir = os.path.dirname(summary_file)
    mk_dir(summary_file_dir)
    if os.path.exists(summary_file):
          os.remove(summary_file)

    i = 1
    while True:
        md = os.path.join(src_dir, 'part' + str(i), 'README.md')
        if not Path(md).exists():
            break
        build_summary(i, md, dist_dir, summary_file)
        i += 1

if __name__ == '__main__':
    dist_dir = './dist'
    mk_dir(dist_dir)
    for_each_md('./src', dist_dir)

