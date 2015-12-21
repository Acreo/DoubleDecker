# DoubleDecker messaging system

Dockerfile for building a docker image for the C version of the DoubleDecker broker.

### INSTALLATION

Generate the doubledecker-0.2.tar.gz file by running "make dist" in the c directory, move the file to the Dockerfile directory.
Run "docker built -t ddbroker ." to build the container.

A stripped version of the container is available on Docker Hub, retrieve it using docker pull acreo/ddbroker:latest.
The stripped image is  roughly 30 MB large, compared to the ~450MB for the raw ubuntu based container.
