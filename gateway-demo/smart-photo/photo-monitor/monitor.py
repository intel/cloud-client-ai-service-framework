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

def gen_thumbnail(inf):
    global _photo_dir
    global _thumbnail_dir
    try:
        thumbnail = os.path.realpath(
                os.path.join(
                    _thumbnail_dir, inf[len(_photo_dir) + 1:]))
        thumbnail_dir = os.path.dirname(thumbnail)
        if not os.path.exists(thumbnail_dir):
            os.makedirs(thumbnail_dir)

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
            img.save(thumbnail, exif=exif)
        else:
            img.save(thumbnail)
    except BaseException as e:
        print(str(e))
        print('cannot create thumbnail, ignore.')

def service_path(p):
    i = p.rfind('/smartphoto/')
    return p[i:]

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
    i = inotify.adapters.InotifyTree(os.path.realpath(monitor_dir))
    move_file = {}

    for event in i.event_gen(yield_nones=False):
        (_, type_names, path, fname) = event
        #print("PATH=[{}] FILENAME=[{}] EVENT_TYPES={}".format(
                   #path, fname, type_names))
        photo = os.path.join(path, fname)

        if 'IN_CLOSE_WRITE' in type_names:
            gen_thumbnail(photo)
            post_add_file(photo)
        if 'IN_DELETE' in type_names:
            post_delete_file(photo)
        if 'IN_MOVED_FROM' in type_names:
            move_file['from'] = photo
        if 'IN_MOVED_TO' in type_names:
            move_file['to'] = photo
            if move_file.get('from', None) != None:
                post_move_file(move_file)
                move_file['from'] = None

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='monitor file change')
    parser.add_argument('-d', type=str, help='directory', required=True)
    args = parser.parse_args()

    _photo_dir = os.path.realpath(args.d)
    _thumbnail_dir = os.path.realpath(os.path.join(_photo_dir, "../thumbnail"))

    main(_photo_dir)
