#!/bin/bash
docker run -it --rm -v "$(pwd)":/app -p 8080:8080 evloop-dev bash
