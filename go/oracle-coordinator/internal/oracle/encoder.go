package oracle

import (
	"math/big"
	"strings"

	"github.com/ethereum/go-ethereum/accounts/abi"
	"github.com/ethereum/go-ethereum/common"
)

const priceOracleABI = `[
  {
    "type":"function",
    "name":"publishPrice",
    "stateMutability":"nonpayable",
    "inputs":[
      {"name":"asset","type":"address"},
      {"name":"price","type":"uint256"},
      {"name":"reportedAt","type":"uint256"},
      {"name":"roundId","type":"uint256"}
    ],
    "outputs":[]
  }
]`

type Encoder struct {
	contract abi.ABI
}

func NewEncoder() (Encoder, error) {
	contract, err := abi.JSON(strings.NewReader(priceOracleABI))
	if err != nil {
		return Encoder{}, err
	}
	return Encoder{contract: contract}, nil
}

func (encoder Encoder) PublishPrice(
	asset common.Address,
	price uint64,
	reportedAt int64,
	roundID int64,
) ([]byte, error) {
	return encoder.contract.Pack(
		"publishPrice",
		asset,
		new(big.Int).SetUint64(price),
		big.NewInt(reportedAt),
		big.NewInt(roundID),
	)
}
