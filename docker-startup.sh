#! /bin/bash

bazel run //src/client:distft_founder -- start localhost:8000 localhost:8001
sleep infinity
