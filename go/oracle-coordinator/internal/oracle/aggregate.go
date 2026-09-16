package oracle

import (
	"context"
	"errors"
	"sort"

	"github.com/ethereum/go-ethereum/common"
)

func Median(ctx context.Context, providers []Provider, asset common.Address) (uint64, []Quote, error) {
	if len(providers) == 0 {
		return 0, nil, errors.New("oracle requires at least one provider")
	}

	quotes := make([]Quote, 0, len(providers))
	prices := make([]uint64, 0, len(providers))
	for _, provider := range providers {
		quote, err := provider.Quote(ctx, asset)
		if err != nil {
			return 0, nil, err
		}
		if quote.Price == 0 {
			return 0, nil, errors.New("oracle provider returned a zero price")
		}
		quotes = append(quotes, quote)
		prices = append(prices, quote.Price)
	}

	sort.Slice(prices, func(left, right int) bool { return prices[left] < prices[right] })
	middle := len(prices) / 2
	if len(prices)%2 == 1 {
		return prices[middle], quotes, nil
	}
	return prices[middle-1] + (prices[middle]-prices[middle-1])/2, quotes, nil
}
