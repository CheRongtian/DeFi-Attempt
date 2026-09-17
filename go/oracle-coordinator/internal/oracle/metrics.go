package oracle

import (
	"context"
	"fmt"
	"log"
	"net"
	"net/http"
	"sync/atomic"
	"time"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promhttp"
)

type Metrics struct {
	registry            *prometheus.Registry
	ready               atomic.Bool
	leader              prometheus.Gauge
	fencingToken        prometheus.Gauge
	round               prometheus.Gauge
	lastUpdateTimestamp prometheus.Gauge
	publications        prometheus.Counter
	failures            prometheus.Counter
}

func NewMetrics() *Metrics {
	labels := prometheus.Labels{"service": "oracle-coordinator"}
	metrics := &Metrics{
		registry: prometheus.NewRegistry(),
		leader: prometheus.NewGauge(prometheus.GaugeOpts{
			Name:        "oracle_leader",
			Help:        "Whether this Oracle Coordinator replica is the elected leader",
			ConstLabels: labels,
		}),
		fencingToken: prometheus.NewGauge(prometheus.GaugeOpts{
			Name:        "oracle_fencing_token",
			Help:        "Latest database fencing token held by this replica",
			ConstLabels: labels,
		}),
		round: prometheus.NewGauge(prometheus.GaugeOpts{
			Name:        "oracle_round",
			Help:        "Latest Oracle publication round queued by this replica",
			ConstLabels: labels,
		}),
		lastUpdateTimestamp: prometheus.NewGauge(prometheus.GaugeOpts{
			Name:        "oracle_last_update_timestamp_seconds",
			Help:        "Unix timestamp of the latest Oracle publication queued by this replica",
			ConstLabels: labels,
		}),
		publications: prometheus.NewCounter(prometheus.CounterOpts{
			Name:        "oracle_publications_total",
			Help:        "Total Oracle publications queued by this replica",
			ConstLabels: labels,
		}),
		failures: prometheus.NewCounter(prometheus.CounterOpts{
			Name:        "oracle_publication_failures_total",
			Help:        "Total Oracle aggregation, fencing, or publication failures",
			ConstLabels: labels,
		}),
	}
	metrics.registry.MustRegister(
		metrics.leader,
		metrics.fencingToken,
		metrics.round,
		metrics.lastUpdateTimestamp,
		metrics.publications,
		metrics.failures,
		prometheus.NewGaugeFunc(prometheus.GaugeOpts{
			Name:        "service_health",
			Help:        "Whether the service process is healthy",
			ConstLabels: labels,
		}, func() float64 { return 1 }),
		prometheus.NewGaugeFunc(prometheus.GaugeOpts{
			Name:        "service_ready",
			Help:        "Whether the service has completed initialization",
			ConstLabels: labels,
		}, func() float64 {
			if metrics.ready.Load() {
				return 1
			}
			return 0
		}),
	)
	return metrics
}

func (metrics *Metrics) SetReady(ready bool) {
	metrics.ready.Store(ready)
}

func (metrics *Metrics) SetLeader(leader bool) {
	if leader {
		metrics.leader.Set(1)
		return
	}
	metrics.leader.Set(0)
}

func (metrics *Metrics) ObserveFence(token int64) {
	metrics.fencingToken.Set(float64(token))
}

func (metrics *Metrics) ObservePublication(round int64, reportedAt int64) {
	metrics.round.Set(float64(round))
	metrics.lastUpdateTimestamp.Set(float64(reportedAt))
	metrics.publications.Inc()
}

func (metrics *Metrics) ObserveFailure() {
	metrics.failures.Inc()
}

func StartMetricsServer(ctx context.Context, address string, port uint64, metrics *Metrics) error {
	listener, err := net.Listen("tcp", fmt.Sprintf("%s:%d", address, port))
	if err != nil {
		return err
	}

	mux := http.NewServeMux()
	mux.Handle("/metrics", promhttp.HandlerFor(metrics.registry, promhttp.HandlerOpts{}))
	mux.HandleFunc("/health", func(response http.ResponseWriter, _ *http.Request) {
		response.Header().Set("Content-Type", "application/json")
		response.Write([]byte("{\"status\":\"healthy\"}\n"))
	})
	mux.HandleFunc("/ready", func(response http.ResponseWriter, _ *http.Request) {
		response.Header().Set("Content-Type", "application/json")
		if !metrics.ready.Load() {
			response.WriteHeader(http.StatusServiceUnavailable)
			response.Write([]byte("{\"status\":\"starting\"}\n"))
			return
		}
		response.Write([]byte("{\"status\":\"ready\"}\n"))
	})

	server := &http.Server{Handler: mux, ReadHeaderTimeout: 5 * time.Second}
	go func() {
		<-ctx.Done()
		shutdownContext, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		server.Shutdown(shutdownContext)
	}()
	go func() {
		if err := server.Serve(listener); err != nil && err != http.ErrServerClosed {
			log.Printf("metrics server stopped: %v", err)
		}
	}()
	return nil
}
