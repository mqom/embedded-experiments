FROM debian:bookworm-slim

ARG HOST_UID=1000
ARG HOST_GID=1000

RUN rm -f /etc/apt/sources.list.d/debian.sources && \
    echo "deb [check-valid-until=no] http://snapshot.debian.org/archive/debian/20250215T000000Z/ trixie main" > /etc/apt/sources.list

RUN apt-get -o Acquire::Check-Valid-Until=false -o Acquire::Retries=3 update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        wget \
        curl \
        cmake \
        gdb \
        gdbserver \
        valgrind \
        graphviz \
        libsdl2-dev \
        python3-venv \
        xz-utils \
        ca-certificates \
        stlink-tools \
        sudo \
        gcc-arm-none-eabi=15:14.2.rel1-1 \
        libnewlib-arm-none-eabi \
        libstdc++-arm-none-eabi-newlib \
        && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

RUN getent group ${HOST_GID} || groupadd -g ${HOST_GID} devgroup && \
    useradd -m -u ${HOST_UID} -g ${HOST_GID} -s /bin/bash devuser && \
    echo "devuser ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

WORKDIR /home/dev
RUN chown -R devuser:${HOST_GID} /home/dev

USER devuser

RUN echo 'if [ -x /usr/bin/dircolors ]; then' >> ~/.bashrc && \
    echo '    test -r ~/.dircolors && eval "$(dircolors -b ~/.dircolors)" || eval "$(dircolors -b)"' >> ~/.bashrc && \
    echo '    alias ls="ls --color=auto"' >> ~/.bashrc && \
    echo '    alias grep="grep --color=auto"' >> ~/.bashrc && \
    echo '    alias fgrep="fgrep --color=auto"' >> ~/.bashrc && \
    echo '    alias egrep="egrep --color=auto"' >> ~/.bashrc && \
    echo 'fi' >> ~/.bashrc

WORKDIR /home/app

CMD ["bash"]
