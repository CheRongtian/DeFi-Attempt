package oracle

import (
	"context"
	"errors"
	"os"
	"testing"
	"time"

	"github.com/ethereum/go-ethereum/common"
)

const testChainID = "3133745"

var (
	testOracle = common.HexToAddress("0x0000000000000000000000000000000000000045")
	testAsset  = common.HexToAddress("0x0000000000000000000000000000000000000046")
	testWallet = common.HexToAddress("0x0000000000000000000000000000000000000047")
)

func openTestStore(t *testing.T) *Store {
	t.Helper()
	databaseURL := os.Getenv("DLP_TEST_DATABASE_URL")
	if databaseURL == "" {
		t.Skip("set DLP_TEST_DATABASE_URL to run PostgreSQL integration tests")
	}

	encoder, err := NewEncoder()
	if err != nil {
		t.Fatal(err)
	}
	store, err := NewStore(context.Background(), databaseURL, encoder)
	if err != nil {
		t.Fatal(err)
	}
	cleanupOracleRows(t, store)
	t.Cleanup(func() {
		cleanupOracleRows(t, store)
		store.Close()
	})
	return store
}

func cleanupOracleRows(t *testing.T, store *Store) {
	t.Helper()
	ctx := context.Background()
	if _, err := store.pool.Exec(
		ctx,
		"DELETE FROM oracle_publications WHERE chain_id = $1",
		testChainID,
	); err != nil {
		t.Fatal(err)
	}
	if _, err := store.pool.Exec(
		ctx,
		"DELETE FROM tx_jobs WHERE chain_id = $1",
		testChainID,
	); err != nil {
		t.Fatal(err)
	}
	if _, err := store.pool.Exec(
		ctx,
		"DELETE FROM oracle_rounds WHERE chain_id = $1",
		testChainID,
	); err != nil {
		t.Fatal(err)
	}
	if _, err := store.pool.Exec(
		ctx,
		"DELETE FROM oracle_leases WHERE chain_id = $1",
		testChainID,
	); err != nil {
		t.Fatal(err)
	}
}

func publicationRequest(lease Lease) PublicationRequest {
	return PublicationRequest{
		ChainID:    testChainID,
		Oracle:     testOracle,
		Asset:      testAsset,
		Wallet:     testWallet,
		Price:      300_100_000_000,
		ReportedAt: 1_700_000_000,
		Lease:      lease,
	}
}

func TestPublicationIsIdempotentAndRoundsIncrease(t *testing.T) {
	store := openTestStore(t)
	ctx := context.Background()

	lease, err := store.AcquireFence(ctx, testChainID, testOracle, "oracle-a", time.Minute)
	if err != nil {
		t.Fatal(err)
	}
	first, created, err := store.CreatePublication(ctx, publicationRequest(lease))
	if err != nil {
		t.Fatal(err)
	}
	if !created || first.RoundID != 1 {
		t.Fatalf("expected the first publication at round 1, got %+v", first)
	}

	_, created, err = store.CreatePublication(ctx, publicationRequest(lease))
	if err != nil {
		t.Fatal(err)
	}
	if created {
		t.Fatal("created a duplicate publication while the first transaction was active")
	}

	if _, err := store.pool.Exec(
		ctx,
		"UPDATE tx_jobs SET status = 'Finalized' WHERE job_id = $1",
		first.TxJobID,
	); err != nil {
		t.Fatal(err)
	}
	lease, err = store.AcquireFence(ctx, testChainID, testOracle, "oracle-a", time.Minute)
	if err != nil {
		t.Fatal(err)
	}
	second, created, err := store.CreatePublication(ctx, publicationRequest(lease))
	if err != nil {
		t.Fatal(err)
	}
	if !created || second.RoundID != 2 {
		t.Fatalf("expected the next publication at round 2, got %+v", second)
	}
}

func TestPublicationRejectsSupersededFence(t *testing.T) {
	store := openTestStore(t)
	ctx := context.Background()

	oldLease, err := store.AcquireFence(ctx, testChainID, testOracle, "oracle-a", time.Minute)
	if err != nil {
		t.Fatal(err)
	}
	if _, err := store.pool.Exec(
		ctx,
		`UPDATE oracle_leases
		 SET lease_until = NOW() - INTERVAL '1 second'
		 WHERE chain_id = $1 AND oracle_address = $2`,
		testChainID,
		testOracle.Hex(),
	); err != nil {
		t.Fatal(err)
	}
	if _, err := store.AcquireFence(
		ctx,
		testChainID,
		testOracle,
		"oracle-b",
		time.Minute,
	); err != nil {
		t.Fatal(err)
	}

	_, _, err = store.CreatePublication(ctx, publicationRequest(oldLease))
	if !errors.Is(err, ErrLeaseHeld) {
		t.Fatalf("expected the superseded fence to be rejected, got %v", err)
	}
}
