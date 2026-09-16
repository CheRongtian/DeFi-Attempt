package oracle

import (
	"bytes"
	"testing"

	"github.com/ethereum/go-ethereum/common"
	"github.com/ethereum/go-ethereum/crypto"
)

func TestPublishPriceUsesExpectedSelector(t *testing.T) {
	encoder, err := NewEncoder()
	if err != nil {
		t.Fatal(err)
	}

	calldata, err := encoder.PublishPrice(
		common.HexToAddress("0x0000000000000000000000000000000000000001"),
		300_100_000_000,
		1_700_000_000,
		7,
	)
	if err != nil {
		t.Fatal(err)
	}

	expected := crypto.Keccak256([]byte("publishPrice(address,uint256,uint256,uint256)"))[:4]
	if !bytes.Equal(calldata[:4], expected) {
		t.Fatalf("unexpected selector: %x", calldata[:4])
	}
}
