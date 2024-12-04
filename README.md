## Distft - File storage and retrieval across multiple nodes

I've added a set of explainers below for the overall design of the Kademlia DHT underlying the Distft application. The application itself runs as a daemon and can be deployed (locally at least) with the provided Dockerfile - the executable may be built locally and copied into the docker image, or the application will build the executable within the docker container (either works)
- [x] ![Building Something Like BitTorrent Part 1: Introduction to (Modified) Kademlia](https://hackmd.io/XIFryxXUQWKk_EpLAWIhTQ)
- [x] ![Building Something Like BitTorrent Part 2: The Router](https://hackmd.io/JVOiM8K1Q7WC4mAAnZWJZQ)
- [x] ![Building Something Like BitTorrent Part 3: The RPC Interface](https://hackmd.io/uIFFoIzYS9qkBxsiYnVRSA)
- [x] ![Building Something Like BitTorrent Part 4: The Search Algorithm + The DHT Interface (Again)](https://hackmd.io/bF0SLYNxTui70ph98-0WNg)
- [x] ![Building Something Like BitTorrent Part 5: File Storage Application](https://hackmd.io/LIqI2OfrSXKA17QPw3CU3Q)


### Instructions for deploying

- Create the Docker network
```
> docker network create distft_net
```

- Deploy the founder container
```
> docker run -v <WORKSPACE DIR>:<WORKSPACE DIR>   -v <DISTFT DIR>:<DISTFT DIR> -p 8000:8000  -w <WORKSPACE DIR>  --user root --network distft_net --name founder_container -d <FOUNDER IMAGE ID>
```

- Create a shell in the container and run distft commands
```
> docker exec -it founder_container /bin/bash
> bazel-bin/src/client/distft_founder ...
```
