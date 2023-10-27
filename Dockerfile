FROM gcr.io/bazel-public/bazel:latest

# dependencies
USER root
RUN apt update && apt install -y libssl-dev

# setup files
WORKDIR /distft
COPY . /distft

# setup network
ENV ADDRESS "localhost"
ENV PORTS "8000 8001"

# run bash script
RUN chmod +x ./docker-startup.sh
ENTRYPOINT ["/bin/sh", "-c"]
CMD ["./docker-startup.sh"]
