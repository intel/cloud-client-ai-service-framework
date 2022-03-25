#!/usr/bin/env python3

import time
from ccai_stream import pipeline

p = pipeline('sample', '')

print(p)

p.start()

for i in range(15):
    time.sleep(1)
    print('.')

p.stop()

del p
