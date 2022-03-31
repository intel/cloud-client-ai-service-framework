#!/bin/bash

# Copyright (C) 2020 Intel Corporation

set -e

if [ ! -e "service_runtime.sh" ]; then
	echo "error: this script should run in the 'service_runtime' folder."
	exit 1
fi

mkdir -p ./rootfs/d/etc/lighttpd/conf-enabled/
touch ./rootfs/d/etc/lighttpd/conf-enabled/.ccai-volume
sudo chown www-data.www-data ./rootfs/d/etc/lighttpd/conf-enabled/

./service_runtime.sh restart

sleep 2

docker exec -it service_runtime_container /bin/bash -c "\
cd /etc/lighttpd/conf-enabled; \
for c in ../conf-available/16-*; \
  do echo \$c; \
  ln -sf \$c; \
done \
"

docker exec -it service_runtime_container /bin/bash -c "\
sv restart lighttpd \
"
