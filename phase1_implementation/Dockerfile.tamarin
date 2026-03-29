FROM ubuntu:22.04

ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
ENV PATH="/root/.local/bin:${PATH}"

# Install dependencies
RUN apt-get update && apt-get install -y \
    opam m4 pkg-config libssl-dev git curl ca-certificates \
    ocaml ocaml-findlib maude \
    && rm -rf /var/lib/apt/lists/*

# Install modern Node.js (required by Tamarin frontend build)
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y nodejs \
    && node --version \
    && npm --version \
    && rm -rf /var/lib/apt/lists/*

# Install recent Stack (APT version is too old on ARM)
RUN apt-get update && apt-get install -y libffi-dev libgmp-dev zlib1g-dev \
    && curl -sSL https://get.haskellstack.org/ | sh \
    && stack --version \
    && rm -rf /var/lib/apt/lists/*

# Clone and build Tamarin Prover
WORKDIR /opt
RUN git clone https://github.com/tamarin-prover/tamarin-prover.git \
    && cd tamarin-prover \
    && opam init --disable-sandboxing --auto-setup --yes \
    && eval $(opam env) \
    && opam install -y ocamlfind batteries \
    && make \
    && tamarin-prover --version

ENV PATH="/opt/tamarin-prover/dist/bin:$PATH"

WORKDIR /workspace
CMD ["bash"]
