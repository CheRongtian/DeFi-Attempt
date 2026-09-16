package oracle

import (
	"context"

	"github.com/ethereum/go-ethereum/common"
)

type Quote struct {
	Provider string
	Price    uint64
}

type Provider interface {
	Quote(context.Context, common.Address) (Quote, error)
}

type StaticProvider struct {
	name  string
	price uint64
}

func NewStaticProvider(name string, price uint64) StaticProvider {
	return StaticProvider{name: name, price: price}
}

func (provider StaticProvider) Quote(context.Context, common.Address) (Quote, error) {
	return Quote{Provider: provider.name, Price: provider.price}, nil
}
