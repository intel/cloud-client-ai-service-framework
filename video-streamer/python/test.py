#!/usr/bin/env python3

import time
from ccai_stream import pipeline

p = pipeline('launcher.object_detection',
'''{
"source":"device=/dev/video0",
"sink":"v4l2sink device=/dev/video2",
"resolution":"width=800,height=600"
}''')

print(p)

p.start()

for i in range(100):
    time.sleep(1)
    print('.')

p.stop()

del p
