FROM ubuntu:24.04 AS cpp-dependencies

ARG DEBIAN_FRONTEND=noninteractive
ARG LIBPQXX_VERSION=8.0.2
ARG ETHASH_VERSION=1.1.0
ARG PROMETHEUS_CPP_VERSION=1.3.0
ARG NATS_C_VERSION=3.11.0

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

RUN mkdir -p /opt/dlp-dependencies/ethash \
    && curl --fail --location --silent --show-error \
        "https://github.com/chfast/ethash/archive/refs/tags/v${ETHASH_VERSION}.tar.gz" \
        --output /tmp/ethash.tar.gz \
    && tar --extract --gzip --file /tmp/ethash.tar.gz \
        --directory /opt/dlp-dependencies/ethash --strip-components=1 \
    && rm /tmp/ethash.tar.gz

RUN mkdir -p /opt/dlp-dependencies/prometheus-cpp \
    && curl --fail --location --silent --show-error \
        "https://github.com/jupp0r/prometheus-cpp/archive/refs/tags/v${PROMETHEUS_CPP_VERSION}.tar.gz" \
        --output /tmp/prometheus-cpp.tar.gz \
    && tar --extract --gzip --file /tmp/prometheus-cpp.tar.gz \
        --directory /opt/dlp-dependencies/prometheus-cpp --strip-components=1 \
    && rm /tmp/prometheus-cpp.tar.gz

RUN mkdir -p /opt/dlp-dependencies/nats-c \
    && curl --fail --location --silent --show-error \
        "https://github.com/nats-io/nats.c/archive/refs/tags/v${NATS_C_VERSION}.tar.gz" \
        --output /tmp/nats-c.tar.gz \
    && tar --extract --gzip --file /tmp/nats-c.tar.gz \
        --directory /opt/dlp-dependencies/nats-c --strip-components=1 \
    && rm /tmp/nats-c.tar.gz

FROM cpp-dependencies AS builder

WORKDIR /source
COPY . .

RUN cmake -S . -B /tmp/dlp-build \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTING=OFF \
        -DDLP_BUILD_CPP_SMOKE=OFF \
        -DFETCHCONTENT_SOURCE_DIR_ETHASH=/opt/dlp-dependencies/ethash \
        -DFETCHCONTENT_SOURCE_DIR_PROMETHEUS_CPP=/opt/dlp-dependencies/prometheus-cpp \
        -DFETCHCONTENT_SOURCE_DIR_NATS_C=/opt/dlp-dependencies/nats-c \
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

FROM golang:1.25-bookworm AS oracle-builder

WORKDIR /source
COPY go/oracle-coordinator/go.mod go/oracle-coordinator/go.sum ./
RUN go mod download
COPY go/oracle-coordinator/ ./
RUN CGO_ENABLED=0 go build -o /oracle-coordinator ./cmd/oracle-coordinator

FROM gcr.io/distroless/static-debian12:nonroot AS oracle-coordinator
COPY --from=oracle-builder /oracle-coordinator /usr/local/bin/oracle-coordinator
ENTRYPOINT ["/usr/local/bin/oracle-coordinator"]
