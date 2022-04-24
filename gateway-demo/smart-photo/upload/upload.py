#!/usr/bin/env python3

import sys
import cgi, os
import cgitb; cgitb.enable()
form = cgi.FieldStorage()

message = 'undefined message'

print("""\
Content-Type: text/html\n
<html>
<body>""")

try:
    fileitem = form['file']
    if fileitem.filename:
        fn = os.path.basename(fileitem.filename)
        open('/smartphoto/photos/' + fn, 'wb').write(fileitem.file.read())
        message = 'The file "' + fn + '" was uploaded successfully'
    else:
        message = 'No file was uploaded'
except Exception as e:
    message = str(e)

print(message)

print("""\
</body>
</html>""")
