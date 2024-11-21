#! /bin/bash

endpoints=""
IFS=" " read -ra port_array <<< "$PORTS"
for port in "${port_array[@]}"; do
    endpoints+="$ADDRESS:$port "
done
build_dir="$BUILD_DIR"

bazel run --output_user_root="$build_dir" //src/client:distft_founder -- start $endpoints
sleep infinity
