FROM gcr.io/bazel-public/bazel:latest

USER root

RUN apt update && apt install -y libssl-dev

WORKDIR /distft

COPY . /distft

EXPOSE 8000

ENTRYPOINT ["/bin/sh", "-c"]

RUN chmod +x ./docker-startup.sh

CMD ["./docker-startup.sh"]
