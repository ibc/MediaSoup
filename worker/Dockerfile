FROM ubuntu:22.04

# Install dependencies.
RUN set -x \
	&& apt-get update \
	&& apt-get install --yes \
		gcc g++ clang pkg-config bash-completion wget curl \
		screen python3-pip python3-yaml pkg-config zlib1g-dev \
		libgss-dev libssl-dev libxml2-dev gdb

# Install node 20.
RUN set -x \
	&& apt-get update \
	&& apt-get install --yes ca-certificates curl gnupg \
	&& mkdir -p /etc/apt/keyrings \
	&& curl -fsSL https://deb.nodesource.com/gpgkey/nodesource-repo.gpg.key \
	| gpg --dearmor -o /etc/apt/keyrings/nodesource.gpg \
	&& NODE_MAJOR=20 \
	&& echo "deb [signed-by=/etc/apt/keyrings/nodesource.gpg] https://deb.nodesource.com/node_$NODE_MAJOR.x nodistro main" \
	> /etc/apt/sources.list.d/nodesource.list \
	&& apt-get update \
	&& apt-get install nodejs --yes

# Enable core dumps.
RUN set -x \
	&& echo "mkdir -p /tmp/cores && chmod 777 /tmp/cores && echo \"/tmp/cores/core.%e.sig%s.%p\" > /proc/sys/kernel/core_pattern && ulimit -c unlimited" >> ~/.bashrc

# Make CC and CXX point to clang/clang++ installed above.
ENV LANG="C.UTF-8"
ENV CC="clang"
ENV CXX="clang++"

ENV MEDIASOUP_LOCAL_DEV="true"
ENV KEEP_BUILD_ARTIFACTS="1"

WORKDIR /mediasoup

CMD ["bash"]
