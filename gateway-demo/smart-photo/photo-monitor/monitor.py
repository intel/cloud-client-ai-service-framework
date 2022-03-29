#!/usr/bin/env python3

import inotify.adapters
import argparse
import os
import requests
import json
import sys
from PIL import Image, ExifTags

_photo_dir = ''
_thumbnail_dir = ''

def gen_thumbnail(inf, outf):
    img = Image.open(inf)
    exif = img.info.get('exif', None)
    width, height = img.size
    if (width < height):
        l = width
    else:
        l = height

    left = (width - l)/2
    top = (height - l)/2
    right = (width + l)/2
    bottom = (height + l)/2

    img = img.crop((left, top, right, bottom))
    img = img.resize((200, 200))

    if exif:
        img.save(outf, exif=exif)
    else:
        img.save(outf)

def service_path(p):
    return os.path.join('/smartphoto/photos/', p)

def post_add_file(pp):
    pp = service_path(pp)
    print('NEW FILE:', pp)
    resp = requests.post('http://localhost:8080/cgi-bin/smartphoto',
            json={'method': 'add_file', 'param': pp})
    j = (resp.json())
    print(j)

def post_delete_file(pp):
    pp = service_path(pp)
    print('DEL FILE:', pp)
    resp = requests.post('http://localhost:8080/cgi-bin/smartphoto',
            json={'method': 'del_file', 'param': pp})
    j = (resp.json())
    print(j)

def post_move_file(mf):
    mf['from'] = service_path(mf['from'])
    mf['to'] = service_path(mf['to'])
    print('MOVE FILE:', mf)
    resp = requests.post('http://localhost:8080/cgi-bin/smartphoto',
            json={'method': 'mv_file', 'param': mf})
    j = (resp.json())
    print(j)

def main(monitor_dir):
    i = inotify.adapters.Inotify()
    i.add_watch(monitor_dir)

    move_file = {}

    for event in i.event_gen(yield_nones=False):
        (_, type_names, path, fname) = event
        print("PATH=[{}] FILENAME=[{}] EVENT_TYPES={}".format(
                   path, fname, type_names))
        if 'IN_CLOSE_WRITE' in type_names:
            gen_thumbnail(_photo_dir + fname, _thumbnail_dir + "/crop_" + fname)

            post_add_file(fname)
        if 'IN_DELETE' in type_names:
            post_delete_file(fname)
        if 'IN_MOVED_FROM' in type_names:
            move_file['from'] = fname
        if 'IN_MOVED_TO' in type_names:
            move_file['to'] = fname
            if move_file.get('from', None) != None:
                post_move_file(move_file)
                move_file['from'] = None

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='monitor file change')
    parser.add_argument('-d', type=str, help='directory', required=True)
    args = parser.parse_args()

    _photo_dir = args.d + "/"
    _thumbnail_dir = _photo_dir + "/../thumbnail/"

    main(_photo_dir)
