FROM gcr.io/bazel-public/bazel:latest

ARG BUILD_DIR=/home/vasanth/.distft/build

# dependencies
USER root
RUN apt update && apt install -y libssl-dev

# setup files
WORKDIR /distft
COPY . /distft

# run bash script
ENTRYPOINT ["/bin/sh", "-c"]
CMD ["bazel --output_user_root=${BUILD_DIR} run //src/client:distft_transient -- --interactive"]
