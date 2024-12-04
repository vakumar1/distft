FROM gcr.io/bazel-public/bazel:latest

ARG BUILD_DIR=/home/vasanth/.distft/build
ARG FOUNDER_CONTAINER=founder_container
ARG TRANSIENT_CONTAINER=transient_container

# dependencies
USER root
RUN apt update && apt install -y libssl-dev

# setup files
WORKDIR /distft
COPY . /distft

# setup network
ENV REMOTE_ADDRESS=${FOUNDER_CONTAINER}
ENV REMOTE_PORT="8000"
ENV LOCAL_ADDRESS=${TRANSIENT_CONTAINER}
ENV LOCAL_PORT="8002"
ENV BUILD_DIR=${BUILD_DIR}

# run bazel commands
ENTRYPOINT ["/bin/sh", "-c"]
CMD ["bazel --output_user_root=$BUILD_DIR run //src/client:distft_transient -- start $REMOTE_ADDRESS:$REMOTE_PORT $LOCAL_ADDRESS:$LOCAL_PORT > $BUILD_DIR/transient_stdout.txt && tail -f /dev/null"]
