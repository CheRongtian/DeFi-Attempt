FROM ubuntu:24.04 AS builder

ARG DEBIAN_FRONTEND=noninteractive
ARG LIBPQXX_VERSION=8.0.2

RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        libboost-system-dev \
        libpq-dev \
        libsecp256k1-dev \
        nlohmann-json3-dev \
        pkg-config \
    && rm -rf /var/lib/apt/lists/*

RUN curl --fail --location --silent --show-error \
        "https://github.com/jtv/libpqxx/archive/refs/tags/${LIBPQXX_VERSION}.tar.gz" \
        --output /tmp/libpqxx.tar.gz \
    && mkdir /tmp/libpqxx-source \
    && tar --extract --gzip --file /tmp/libpqxx.tar.gz \
        --directory /tmp/libpqxx-source --strip-components=1 \
    && cmake -S /tmp/libpqxx-source -B /tmp/libpqxx-build \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON \
        -DSKIP_BUILD_TEST=ON \
    && cmake --build /tmp/libpqxx-build --parallel \
    && cmake --install /tmp/libpqxx-build

WORKDIR /source
COPY . .

RUN cmake -S . -B /tmp/dlp-build \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTING=OFF \
        -DDLP_BUILD_CPP_SMOKE=OFF \
    && cmake --build /tmp/dlp-build --parallel --target \
        dlp_indexer \
        dlp_outbox_publisher \
        dlp_risk_worker \
        dlp_tx_manager \
        dlp_liquidator \
        dlp_api_server

FROM ubuntu:24.04 AS runtime

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        ca-certificates \
        libboost-system1.83.0 \
        libpq5 \
        libsecp256k1-1 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /usr/local/lib/libpqxx* /usr/local/lib/
COPY --from=builder /tmp/dlp-build/cpp/indexer/dlp_indexer /usr/local/bin/
COPY --from=builder /tmp/dlp-build/cpp/messaging/dlp_outbox_publisher /usr/local/bin/
COPY --from=builder /tmp/dlp-build/cpp/risk-engine/dlp_risk_worker /usr/local/bin/
COPY --from=builder /tmp/dlp-build/cpp/tx-manager/dlp_tx_manager /usr/local/bin/
COPY --from=builder /tmp/dlp-build/cpp/liquidator/dlp_liquidator /usr/local/bin/
COPY --from=builder /tmp/dlp-build/cpp/api-server/dlp_api_server /usr/local/bin/
RUN ldconfig

STOPSIGNAL SIGTERM

FROM runtime AS indexer
ENTRYPOINT ["dlp_indexer"]

FROM runtime AS outbox-publisher
ENTRYPOINT ["dlp_outbox_publisher"]

FROM runtime AS risk-engine
ENTRYPOINT ["dlp_risk_worker"]

FROM runtime AS tx-manager
ENTRYPOINT ["dlp_tx_manager"]

FROM runtime AS liquidator
ENTRYPOINT ["dlp_liquidator"]

FROM runtime AS api-server
EXPOSE 8080
ENTRYPOINT ["dlp_api_server"]
