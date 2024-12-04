#! /bin/bash

endpoints=""
IFS=" " read -ra port_array <<< "$PORTS"
for port in "${port_array[@]}"; do
    endpoints+="$ADDRESS:$port "
done
build_dir="$BUILD_DIR"

bazel --output_user_root="$build_dir" run //src/client:distft_founder -- start $endpoints > "$build_dir/founder_stdout.txt"
