// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

import {AccessControl} from "@openzeppelin/contracts/access/AccessControl.sol";

/// @title Price Oracle
/// @notice Administrator-managed USD prices for the local and testnet lending protocol.
contract PriceOracle is AccessControl {
    uint8 public constant PRICE_DECIMALS = 8;

    struct AssetPrice {
        uint256 price;
        uint256 updatedAt;
        uint256 maxPriceAge;
        bool registered;
    }

    /// @notice Raw stored data; use getPrice() when a fresh price is required.
    mapping(address asset => AssetPrice) public assetPrices;

    error InvalidAsset(address asset);
    error InvalidMaxPriceAge(uint256 maxPriceAge);
    error AssetAlreadyRegistered(address asset);
    error UnsupportedAsset(address asset);
    error InvalidPrice();
    error PriceNotSet(address asset);
    error StalePrice(address asset);

    event AssetRegistered(address indexed asset, uint256 maxPriceAge);
    event PriceUpdated(address indexed asset, uint256 price, uint256 updatedAt);

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
