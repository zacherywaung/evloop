#!/bin/bash
docker rm -f evloop 2>/dev/null
docker run -it --rm --name evloop \
    --ulimit nofile=65535:65535 \
    -v "$(pwd)":/app -p 8080:8080 evloop-dev bash
