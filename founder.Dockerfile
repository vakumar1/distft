FROM gcr.io/bazel-public/bazel:latest

ARG BUILD_DIR=/home/vasanth/.distft/build
ARG FOUNDER_CONTAINER=founder_container

# dependencies
USER root
RUN apt update && apt install -y libssl-dev

# setup files
WORKDIR /distft
COPY . /distft

# setup network
ENV ADDRESS=${FOUNDER_CONTAINER}
ENV PORTS="8000 8001"
ENV BUILD_DIR=${BUILD_DIR}

# run bash script
RUN chmod +x ./founder-startup.sh
ENTRYPOINT ["/bin/sh", "-c"]
CMD ["./founder-startup.sh && tail -f /dev/null"]
