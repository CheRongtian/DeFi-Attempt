package oracle

import (
	"context"
	"errors"
	"fmt"
	"strconv"
	"strings"
	"time"

	"github.com/ethereum/go-ethereum/common"
	"github.com/ethereum/go-ethereum/common/hexutil"
	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"
)

var ErrLeaseHeld = errors.New("oracle database lease is held by another coordinator")

type Lease struct {
	Holder       string
	FencingToken int64
}

type PublicationRequest struct {
	ChainID    string
	Oracle     common.Address
	Asset      common.Address
	Wallet     common.Address
	Price      uint64
	ReportedAt int64
	Lease      Lease
}

type Publication struct {
	ID      string
	RoundID int64
	TxJobID string
}

type Store struct {
	pool    *pgxpool.Pool
	encoder Encoder
}

func NewStore(ctx context.Context, databaseURL string, encoder Encoder) (*Store, error) {
	pool, err := pgxpool.New(ctx, databaseURL)
	if err != nil {
		return nil, err
	}
	return &Store{pool: pool, encoder: encoder}, nil
}

func (store *Store) Close() {
	store.pool.Close()
}

func (store *Store) AcquireFence(
	ctx context.Context,
	chainID string,
	oracleAddress common.Address,
	holder string,
	duration time.Duration,
) (Lease, error) {
	var fencingToken int64
	err := store.pool.QueryRow(
		ctx,
		`INSERT INTO oracle_leases
		    (chain_id, oracle_address, holder, lease_until, fencing_token)
		 VALUES ($1, $2, $3, NOW() + make_interval(secs => $4), 1)
		 ON CONFLICT (chain_id, oracle_address) DO UPDATE SET
		    holder = EXCLUDED.holder,
		    lease_until = EXCLUDED.lease_until,
		    fencing_token = oracle_leases.fencing_token + 1,
		    updated_at = NOW()
		 WHERE oracle_leases.lease_until <= NOW()
		    OR oracle_leases.holder = EXCLUDED.holder
		 RETURNING fencing_token`,
		chainID,
		strings.ToLower(oracleAddress.Hex()),
		holder,
		int64(duration/time.Second),
	).Scan(&fencingToken)
	if errors.Is(err, pgx.ErrNoRows) {
		return Lease{}, ErrLeaseHeld
	}
	if err != nil {
		return Lease{}, err
	}
	return Lease{Holder: holder, FencingToken: fencingToken}, nil
}

func (store *Store) CreatePublication(
	ctx context.Context,
	request PublicationRequest,
) (Publication, bool, error) {
	transaction, err := store.pool.BeginTx(ctx, pgx.TxOptions{})
	if err != nil {
		return Publication{}, false, err
	}
	defer transaction.Rollback(ctx)

	var fencingToken int64
	err = transaction.QueryRow(
		ctx,
		`SELECT fencing_token
		 FROM oracle_leases
		 WHERE chain_id = $1
		   AND oracle_address = $2
		   AND holder = $3
		   AND fencing_token = $4
		   AND lease_until > NOW()
		 FOR UPDATE`,
		request.ChainID,
		strings.ToLower(request.Oracle.Hex()),
		request.Lease.Holder,
		request.Lease.FencingToken,
	).Scan(&fencingToken)
	if errors.Is(err, pgx.ErrNoRows) {
		return Publication{}, false, ErrLeaseHeld
	}
	if err != nil {
		return Publication{}, false, err
	}

	var publicationPending bool
	err = transaction.QueryRow(
		ctx,
		`SELECT EXISTS
		 (
		     SELECT 1
		     FROM oracle_publications publication
		     JOIN tx_jobs job ON job.job_id = publication.tx_job_id
		     WHERE publication.chain_id = $1
		       AND publication.oracle_address = $2
		       AND publication.asset_address = $3
		       AND job.status IN ('Pending', 'Submitted', 'Included')
		 )`,
		request.ChainID,
		strings.ToLower(request.Oracle.Hex()),
		strings.ToLower(request.Asset.Hex()),
	).Scan(&publicationPending)
	if err != nil {
		return Publication{}, false, err
	}
	if publicationPending {
		return Publication{}, false, nil
	}

	_, err = transaction.Exec(
		ctx,
		`INSERT INTO oracle_rounds (chain_id, oracle_address, asset_address)
		 VALUES ($1, $2, $3)
		 ON CONFLICT (chain_id, oracle_address, asset_address) DO NOTHING`,
		request.ChainID,
		strings.ToLower(request.Oracle.Hex()),
		strings.ToLower(request.Asset.Hex()),
	)
	if err != nil {
		return Publication{}, false, err
	}

	var roundID int64
	err = transaction.QueryRow(
		ctx,
		`UPDATE oracle_rounds
		 SET last_round_id = last_round_id + 1,
		     updated_at = NOW()
		 WHERE chain_id = $1
		   AND oracle_address = $2
		   AND asset_address = $3
		 RETURNING last_round_id`,
		request.ChainID,
		strings.ToLower(request.Oracle.Hex()),
		strings.ToLower(request.Asset.Hex()),
	).Scan(&roundID)
	if err != nil {
		return Publication{}, false, err
	}

	calldata, err := store.encoder.PublishPrice(
		request.Asset,
		request.Price,
		request.ReportedAt,
		roundID,
	)
	if err != nil {
		return Publication{}, false, err
	}

	publicationID := fmt.Sprintf(
		"oracle:%s:%s:%s:%d",
		request.ChainID,
		strings.ToLower(request.Oracle.Hex()),
		strings.ToLower(request.Asset.Hex()),
		roundID,
	)
	txJobID := "oracle-tx:" + publicationID
	_, err = transaction.Exec(
		ctx,
		`INSERT INTO tx_jobs
		    (job_id, chain_id, wallet_address, to_address, value, calldata, status)
		 VALUES ($1, $2, $3, $4, 0, $5, 'Pending')`,
		txJobID,
		request.ChainID,
		strings.ToLower(request.Wallet.Hex()),
		strings.ToLower(request.Oracle.Hex()),
		hexutil.Encode(calldata),
	)
	if err != nil {
		return Publication{}, false, err
	}

	_, err = transaction.Exec(
		ctx,
		`INSERT INTO oracle_publications
		    (publication_id, chain_id, oracle_address, asset_address, price,
		     reported_at, round_id, fencing_token, tx_job_id)
		 VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)`,
		publicationID,
		request.ChainID,
		strings.ToLower(request.Oracle.Hex()),
		strings.ToLower(request.Asset.Hex()),
		strconv.FormatUint(request.Price, 10),
		request.ReportedAt,
		roundID,
		request.Lease.FencingToken,
		txJobID,
	)
	if err != nil {
		return Publication{}, false, err
	}

	if err := transaction.Commit(ctx); err != nil {
		return Publication{}, false, err
	}
	return Publication{ID: publicationID, RoundID: roundID, TxJobID: txJobID}, true, nil
}
