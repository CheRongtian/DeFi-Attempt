package main

import (
	"context"
	"fmt"
	"log"
	"os"
	"os/signal"
	"strconv"
	"syscall"
	"time"

	"github.com/cherongtian/defi/oracle-coordinator/internal/oracle"
	"github.com/ethereum/go-ethereum/common"
)

func environment(name, fallback string) string {
	if value := os.Getenv(name); value != "" {
		return value
	}
	return fallback
}

func requiredEnvironment(name string) (string, error) {
	value := os.Getenv(name)
	if value == "" {
		return "", fmt.Errorf("missing environment variable: %s", name)
	}
	return value, nil
}

func unsignedEnvironment(name string, fallback uint64) (uint64, error) {
	value := environment(name, strconv.FormatUint(fallback, 10))
	return strconv.ParseUint(value, 10, 64)
}

func addressEnvironment(name string) (common.Address, error) {
	value, err := requiredEnvironment(name)
	if err != nil {
		return common.Address{}, err
	}
	if !common.IsHexAddress(value) {
		return common.Address{}, fmt.Errorf("%s must be an Ethereum address", name)
	}
	return common.HexToAddress(value), nil
}

func run() error {
	ctx, stop := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
	defer stop()

	chainID := environment("DLP_CHAIN_ID", "31337")
	oracleAddress, err := addressEnvironment("DLP_ORACLE_ADDRESS")
	if err != nil {
		return err
	}
	assetAddress, err := addressEnvironment("DLP_ORACLE_ASSET_ADDRESS")
	if err != nil {
		return err
	}
	walletAddress, err := addressEnvironment("DLP_OPERATOR_ADDRESS")
	if err != nil {
		return err
	}

	providerA, err := unsignedEnvironment("DLP_ORACLE_PROVIDER_A_PRICE", 299_500_000_000)
	if err != nil {
		return err
	}
	providerB, err := unsignedEnvironment("DLP_ORACLE_PROVIDER_B_PRICE", 300_700_000_000)
	if err != nil {
		return err
	}
	providerC, err := unsignedEnvironment("DLP_ORACLE_PROVIDER_C_PRICE", 300_100_000_000)
	if err != nil {
		return err
	}
	publishSeconds, err := unsignedEnvironment("DLP_ORACLE_PUBLISH_SECONDS", 30)
	if err != nil {
		return err
	}
	databaseLeaseSeconds, err := unsignedEnvironment("DLP_ORACLE_DATABASE_LEASE_SECONDS", 45)
	if err != nil {
		return err
	}

	holder := environment("POD_NAME", "oracle-local")
	encoder, err := oracle.NewEncoder()
	if err != nil {
		return err
	}
	store, err := oracle.NewStore(
		ctx,
		environment("DLP_DATABASE_URL", "postgresql://dlp:dlp@127.0.0.1:5432/dlp"),
		encoder,
	)
	if err != nil {
		return err
	}
	defer store.Close()

	metrics := oracle.NewMetrics()
	coordinator := oracle.NewCoordinator(
		store,
		[]oracle.Provider{
			oracle.NewStaticProvider("provider-a", providerA),
			oracle.NewStaticProvider("provider-b", providerB),
			oracle.NewStaticProvider("provider-c", providerC),
		},
		oracle.CoordinatorConfig{
			ChainID:         chainID,
			Oracle:          oracleAddress,
			Asset:           assetAddress,
			Wallet:          walletAddress,
			Holder:          holder,
			PublishInterval: time.Duration(publishSeconds) * time.Second,
			LeaseDuration:   time.Duration(databaseLeaseSeconds) * time.Second,
		},
		metrics,
	)
	metricsPort, err := unsignedEnvironment("DLP_METRICS_PORT", 9107)
	if err != nil {
		return err
	}
	if err := oracle.StartMetricsServer(
		ctx,
		environment("DLP_METRICS_ADDRESS", "0.0.0.0"),
		metricsPort,
		metrics,
	); err != nil {
		return err
	}
	metrics.SetReady(true)

	if environment("DLP_ORACLE_LEADER_ELECTION", "false") != "true" {
		metrics.SetLeader(true)
		coordinator.Run(ctx)
		metrics.SetLeader(false)
		return nil
	}
	return oracle.RunAsLeader(ctx, oracle.LeaderConfig{
		Namespace:     environment("POD_NAMESPACE", "dlp"),
		LeaseName:     environment("DLP_ORACLE_LEASE_NAME", "oracle-coordinator"),
		Identity:      holder,
		LeaseDuration: 15 * time.Second,
		RenewDeadline: 10 * time.Second,
		RetryPeriod:   2 * time.Second,
	}, func(leaderContext context.Context) {
		metrics.SetLeader(true)
		coordinator.Run(leaderContext)
		metrics.SetLeader(false)
	})
}

func main() {
	if err := run(); err != nil {
		log.Fatal(err)
	}
}
