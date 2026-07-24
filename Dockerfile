# base image 
FROM ubuntu:latest

# Update the package list and install necessary packages
RUN apt-get update && apt-get install -y \
    gcc \
    make \
    vim \
    net-tools \
    iputils-ping \
    iproute2 \
    iptables \
    sudo \
    git \
    curl \
    iperf3 \
    tcpdump  # Example: Install some network tools

# Set the working directory 
WORKDIR /root

# Set a default command 
CMD ["/bin/bash"]