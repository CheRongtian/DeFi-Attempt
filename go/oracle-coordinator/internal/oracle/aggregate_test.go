package oracle

import (
	"context"
	"testing"

	"github.com/ethereum/go-ethereum/common"
)

func TestMedianUsesMiddleProviderPrice(t *testing.T) {
	providers := []Provider{
		NewStaticProvider("provider-a", 2995),
		NewStaticProvider("provider-b", 3007),
		NewStaticProvider("provider-c", 3001),
	}

	price, quotes, err := Median(context.Background(), providers, common.Address{})
	if err != nil {
		t.Fatal(err)
	}
	if price != 3001 {
		t.Fatalf("expected median 3001, got %d", price)
	}
	if len(quotes) != 3 {
		t.Fatalf("expected 3 quotes, got %d", len(quotes))
	}
}

func TestMedianAveragesMiddlePricesForEvenProviderCount(t *testing.T) {
	providers := []Provider{
		NewStaticProvider("provider-a", 100),
		NewStaticProvider("provider-b", 200),
	}

	price, _, err := Median(context.Background(), providers, common.Address{})
	if err != nil {
		t.Fatal(err)
	}
	if price != 150 {
		t.Fatalf("expected median 150, got %d", price)
	}
}
