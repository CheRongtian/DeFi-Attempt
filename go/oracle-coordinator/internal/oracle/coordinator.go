package oracle

import (
	"context"
	"errors"
	"log"
	"time"

	"github.com/ethereum/go-ethereum/common"
)

type CoordinatorConfig struct {
	ChainID         string
	Oracle          common.Address
	Asset           common.Address
	Wallet          common.Address
	Holder          string
	PublishInterval time.Duration
	LeaseDuration   time.Duration
}

type Coordinator struct {
	store     *Store
	providers []Provider
	config    CoordinatorConfig
}

func NewCoordinator(store *Store, providers []Provider, config CoordinatorConfig) Coordinator {
	return Coordinator{store: store, providers: providers, config: config}
}

func (coordinator Coordinator) Run(ctx context.Context) {
	coordinator.publish(ctx)
	ticker := time.NewTicker(coordinator.config.PublishInterval)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			coordinator.publish(ctx)
		}
	}
}

func (coordinator Coordinator) publish(ctx context.Context) {
	lease, err := coordinator.store.AcquireFence(
		ctx,
		coordinator.config.ChainID,
		coordinator.config.Oracle,
		coordinator.config.Holder,
		coordinator.config.LeaseDuration,
	)
	if errors.Is(err, ErrLeaseHeld) {
		return
	}
	if err != nil {
		log.Printf("oracle fencing failed: %v", err)
		return
	}

	price, quotes, err := Median(ctx, coordinator.providers, coordinator.config.Asset)
	if err != nil {
		log.Printf("oracle aggregation failed: %v", err)
		return
	}

	publication, created, err := coordinator.store.CreatePublication(ctx, PublicationRequest{
		ChainID:    coordinator.config.ChainID,
		Oracle:     coordinator.config.Oracle,
		Asset:      coordinator.config.Asset,
		Wallet:     coordinator.config.Wallet,
		Price:      price,
		ReportedAt: time.Now().Unix(),
		Lease:      lease,
	})
	if errors.Is(err, ErrLeaseHeld) {
		return
	}
	if err != nil {
		log.Printf("oracle publication failed: %v", err)
		return
	}
	if created {
		log.Printf(
			"queued oracle round %d at price %d from %d providers",
			publication.RoundID,
			price,
			len(quotes),
		)
	}
}
