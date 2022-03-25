#!/bin/bash

# Copyright (C) 2020 Intel Corporation

set -e

if [ ! -e "service_runtime.sh" ]; then
	echo "error: this script should run in the 'service_runtime' folder."
	exit 1
fi

mkdir -p ./rootfs/d/etc/runit/runsvdir/default
sudo chown www-data.www-data ./rootfs/d/etc/runit/runsvdir/default

./service_runtime.sh restart

sleep 2

docker exec -it service_runtime_container /bin/bash -c "\
cd /etc/runit/runsvdir/default; \
for s in /etc/sv/*; \
  do echo \$s; \
  ln -sf \$s; \
done \
"
