DOCKER CMDS:

build:

docker build -t distft_docker .

run:

docker run -d -v ~/.cache:/root/.cache distft_docker
