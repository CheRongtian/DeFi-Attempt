// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

import {AccessControl} from "@openzeppelin/contracts/access/AccessControl.sol";

/// @title Price Oracle
/// @notice Administrator-managed USD prices for the local and testnet lending protocol.
contract PriceOracle is AccessControl {
    uint8 public constant PRICE_DECIMALS = 8;
    bytes32 public constant PUBLISHER_ROLE = keccak256("PUBLISHER_ROLE");

    struct AssetPrice {
        uint256 price;
        uint256 updatedAt;
        uint256 maxPriceAge;
        bool registered;
    }

    /// @notice Raw stored data; use getPrice() when a fresh price is required.
    mapping(address asset => AssetPrice) public assetPrices;
    mapping(address asset => uint256 roundId) public latestRoundIds;

    error InvalidAsset(address asset);
    error InvalidMaxPriceAge(uint256 maxPriceAge);
    error AssetAlreadyRegistered(address asset);
    error UnsupportedAsset(address asset);
    error InvalidPrice();
    error InvalidReportTimestamp(uint256 reportedAt);
    error StaleReport(address asset, uint256 reportedAt);
    error InvalidRound(uint256 currentRoundId, uint256 submittedRoundId);
    error PriceNotSet(address asset);
    error StalePrice(address asset);

    event AssetRegistered(address indexed asset, uint256 maxPriceAge);
    event PriceUpdated(address indexed asset, uint256 price, uint256 updatedAt);
    event PricePublished(address indexed asset, uint256 price, uint256 reportedAt, uint256 roundId);

    constructor() {
        _grantRole(DEFAULT_ADMIN_ROLE, _msgSender());
    }

    /// @notice Register an asset once, with a positive price validity interval in seconds.
    /// @dev Registration alone does not provide a usable price; call setPrice() afterwards.
    function registerAsset(address asset, uint256 maxPriceAge) external onlyRole(DEFAULT_ADMIN_ROLE) {
        if (asset == address(0)) revert InvalidAsset(asset);
        if (maxPriceAge == 0) revert InvalidMaxPriceAge(maxPriceAge);
        if (assetPrices[asset].registered) revert AssetAlreadyRegistered(asset);

        assetPrices[asset] = AssetPrice({price: 0, updatedAt: 0, maxPriceAge: maxPriceAge, registered: true});

        emit AssetRegistered(asset, maxPriceAge);
    }

    /// @notice Publish a positive USD price with 8 decimals, e.g. 3_000e8 for $3,000.
    /// @dev The price is per whole token, independent of the token's own decimals.
    function setPrice(address asset, uint256 price) external onlyRole(DEFAULT_ADMIN_ROLE) {
        AssetPrice storage data = assetPrices[asset];
        if (!data.registered) revert UnsupportedAsset(asset);
        if (price == 0) revert InvalidPrice();

        data.price = price;
        data.updatedAt = block.timestamp;

        emit PriceUpdated(asset, price, block.timestamp);
    }

    /// @notice Publish a provider-aggregated price for a strictly newer oracle round.
    function publishPrice(address asset, uint256 price, uint256 reportedAt, uint256 roundId)
        external
        onlyRole(PUBLISHER_ROLE)
    {
        AssetPrice storage data = assetPrices[asset];
        if (!data.registered) revert UnsupportedAsset(asset);
        if (price == 0) revert InvalidPrice();
        if (reportedAt > block.timestamp) revert InvalidReportTimestamp(reportedAt);
        // Oracle freshness is intentionally measured using the chain timestamp.
        // forge-lint: disable-next-line(block-timestamp)
        if (block.timestamp - reportedAt > data.maxPriceAge) revert StaleReport(asset, reportedAt);

        uint256 currentRoundId = latestRoundIds[asset];
        if (roundId <= currentRoundId) revert InvalidRound(currentRoundId, roundId);

        latestRoundIds[asset] = roundId;
        data.price = price;
        data.updatedAt = reportedAt;

        emit PriceUpdated(asset, price, reportedAt);
        emit PricePublished(asset, price, reportedAt, roundId);
    }

    /// @notice Return the fresh USD price with 8 decimals.
    /// @dev Reverts for unsupported assets, missing prices or expired prices.
    function getPrice(address asset) external view returns (uint256) {
        AssetPrice storage data = assetPrices[asset];
        if (!data.registered) revert UnsupportedAsset(asset);
        if (data.price == 0) revert PriceNotSet(asset);
        // Oracle freshness is intentionally measured using the chain timestamp.
        // forge-lint: disable-next-line(block-timestamp)
        if (block.timestamp - data.updatedAt > data.maxPriceAge) revert StalePrice(asset);

        return data.price;
    }

    /// @notice Return false for unsupported assets, missing prices or expired prices.
    /// @dev A price is still valid when its age equals maxPriceAge.
    function isPriceValid(address asset) external view returns (bool) {
        AssetPrice storage data = assetPrices[asset];
        // Match getPrice(): the exact expiry boundary remains valid.
        // forge-lint: disable-next-line(block-timestamp)
        return data.registered && data.price > 0 && block.timestamp - data.updatedAt <= data.maxPriceAge;
    }
}
