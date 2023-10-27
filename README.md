DOCKER CMDS:

build:

docker build -t distft_docker .

run (locally on a single network):

docker network create distft_network
docker run -d -v ~/.cache:/root/.cache -p 8000:8000 --name distft_founder1 --network distft_network -e "ADDRESS=localhost" -e "PORTS=8000 8001" distft_docker
docker run -d -v ~/.cache:/root/.cache -p 8002:8002 --name distft_founder2 --network distft_network -e "ADDRESS=localhost" -e "PORTS=8002 8003 8005" distft_docker
etc.