#!/usr/bin/env python3

import sys
from PIL import Image, ExifTags

img = Image.open(sys.argv[1])

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
    img.save(sys.argv[2], exif=exif)
else:
    img.save(sys.argv[2])
