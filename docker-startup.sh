#! /bin/bash

endpoints=""
IFS=" " read -ra port_array <<< "$PORTS"
for port in "${port_array[@]}"; do
    endpoints+="$ADDRESS:$port "
done

bazel run //src/client:distft_founder -- start $endpoints
sleep infinity
