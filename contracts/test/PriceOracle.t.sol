// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

// Keep the minimal test-only cheatcode interface alongside its only consumer.
// forge-lint: disable-start(multi-contract-file)

import {IAccessControl} from "@openzeppelin/contracts/access/IAccessControl.sol";
import {PriceOracle} from "../src/PriceOracle.sol";
import {MockUSDC} from "../src/mocks/MockUSDC.sol";
import {MockWETH} from "../src/mocks/MockWETH.sol";

// Only the built-in Foundry cheatcodes needed by these tests; no forge-std dependency.
interface IPriceOracleVm {
    function warp(uint256 timestamp) external;
    function prank(address sender) external;
    function expectRevert(bytes calldata revertData) external;
    function expectEmit(bool checkTopic1, bool checkTopic2, bool checkTopic3, bool checkData, address emitter) external;
}

contract PriceOracleTest {
    IPriceOracleVm private constant VM = IPriceOracleVm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);

    uint256 private constant START_TIME = 1_000_000;
    uint256 private constant MAX_PRICE_AGE = 1 hours;
    uint256 private constant USDC_MAX_PRICE_AGE = 1 days;
    uint256 private constant WETH_PRICE = 3_000e8;
    uint256 private constant USDC_PRICE = 1e8;
    address private constant UNAUTHORIZED = address(0xBEEF);

    PriceOracle private oracle;
    MockWETH private weth;
    MockUSDC private usdc;

    function setUp() public {
        VM.warp(START_TIME);
        oracle = new PriceOracle();
        weth = new MockWETH();
        usdc = new MockUSDC();
        oracle.registerAsset(address(weth), MAX_PRICE_AGE);
    }

    function testDeployerIsAdmin() public view {
        assert(oracle.hasRole(oracle.DEFAULT_ADMIN_ROLE(), address(this)));
    }

    function testAdminCanRegisterAsset() public {
        VM.expectEmit(true, false, false, true, address(oracle));
        // Template event required by Foundry's expectEmit assertion.
        // forge-lint: disable-next-line(reentrancy-events)
        emit PriceOracle.AssetRegistered(address(usdc), USDC_MAX_PRICE_AGE);

        oracle.registerAsset(address(usdc), USDC_MAX_PRICE_AGE);

        (uint256 price, uint256 updatedAt, uint256 maxPriceAge, bool registered) = oracle.assetPrices(address(usdc));
        assert(registered);
        assert(price == 0);
        assert(updatedAt == 0);
        assert(maxPriceAge == USDC_MAX_PRICE_AGE);
        assert(!oracle.isPriceValid(address(usdc)));
    }

    function testUnauthorizedAccountCannotRegisterAsset() public {
        VM.expectRevert(
            abi.encodeWithSelector(IAccessControl.AccessControlUnauthorizedAccount.selector, UNAUTHORIZED, bytes32(0))
        );
        VM.prank(UNAUTHORIZED);
        oracle.registerAsset(address(usdc), MAX_PRICE_AGE);
    }

    function testZeroAddressRegistrationRejected() public {
        VM.expectRevert(abi.encodeWithSelector(PriceOracle.InvalidAsset.selector, address(0)));
        oracle.registerAsset(address(0), MAX_PRICE_AGE);
    }

    function testZeroMaxPriceAgeRejected() public {
        VM.expectRevert(abi.encodeWithSelector(PriceOracle.InvalidMaxPriceAge.selector, 0));
        oracle.registerAsset(address(usdc), 0);
    }

    function testDuplicateRegistrationPreservesExistingPrice() public {
        oracle.setPrice(address(weth), WETH_PRICE);

        VM.expectRevert(abi.encodeWithSelector(PriceOracle.AssetAlreadyRegistered.selector, address(weth)));
        oracle.registerAsset(address(weth), 2 hours);

        (uint256 price, uint256 updatedAt, uint256 maxPriceAge, bool registered) = oracle.assetPrices(address(weth));
        assert(registered);
        assert(price == WETH_PRICE);
        assert(updatedAt == START_TIME);
        assert(maxPriceAge == MAX_PRICE_AGE);
    }

    function testAdminCanSetPrice() public {
        VM.expectEmit(true, false, false, true, address(oracle));
        // Template event required by Foundry's expectEmit assertion.
        // forge-lint: disable-next-line(reentrancy-events)
        emit PriceOracle.PriceUpdated(address(weth), WETH_PRICE, START_TIME);

        oracle.setPrice(address(weth), WETH_PRICE);

        assert(oracle.getPrice(address(weth)) == WETH_PRICE);
        assert(oracle.isPriceValid(address(weth)));
    }

    function testUnauthorizedAccountCannotSetPrice() public {
        VM.expectRevert(
            abi.encodeWithSelector(IAccessControl.AccessControlUnauthorizedAccount.selector, UNAUTHORIZED, bytes32(0))
        );
        VM.prank(UNAUTHORIZED);
        oracle.setPrice(address(weth), WETH_PRICE);

        assert(!oracle.isPriceValid(address(weth)));
    }

    function testZeroPriceRejectedWithoutOverwritingExistingPrice() public {
        oracle.setPrice(address(weth), WETH_PRICE);
        VM.warp(START_TIME + 1);

        VM.expectRevert(abi.encodeWithSelector(PriceOracle.InvalidPrice.selector));
        oracle.setPrice(address(weth), 0);

        // Only the previously published price and timestamp are relevant here.
        // forge-lint: disable-next-line(unused-return)
        (uint256 price, uint256 updatedAt,,) = oracle.assetPrices(address(weth));
        assert(price == WETH_PRICE);
        assert(updatedAt == START_TIME);
    }

    function testUnsupportedAssetPriceUpdateRejected() public {
        VM.expectRevert(abi.encodeWithSelector(PriceOracle.UnsupportedAsset.selector, address(usdc)));
        oracle.setPrice(address(usdc), USDC_PRICE);
    }

    function testUnsupportedAssetReadRejected() public {
        VM.expectRevert(abi.encodeWithSelector(PriceOracle.UnsupportedAsset.selector, address(usdc)));
        // The expected revert is asserted above; no return value is expected.
        // forge-lint: disable-next-line(unused-return)
        oracle.getPrice(address(usdc));
    }

    function testUnsupportedAssetIsInvalid() public view {
        assert(!oracle.isPriceValid(address(usdc)));
        assert(!oracle.isPriceValid(address(0)));
    }

    function testRegisteredAssetWithoutPriceIsInvalid() public view {
        assert(!oracle.isPriceValid(address(weth)));
    }

    function testMissingPriceReadRejected() public {
        VM.expectRevert(abi.encodeWithSelector(PriceOracle.PriceNotSet.selector, address(weth)));
        // The expected revert is asserted above; no return value is expected.
        // forge-lint: disable-next-line(unused-return)
        oracle.getPrice(address(weth));
    }

    function testPriceUsesEightDecimals() public {
        oracle.registerAsset(address(usdc), MAX_PRICE_AGE);
        oracle.setPrice(address(weth), WETH_PRICE);
        oracle.setPrice(address(usdc), USDC_PRICE);

        assert(oracle.PRICE_DECIMALS() == 8);
        assert(oracle.getPrice(address(weth)) == 300_000_000_000);
        assert(oracle.getPrice(address(usdc)) == 100_000_000);
    }

    function testPriceUpdateRefreshesTimestamp() public {
        uint256 updateTime = START_TIME + 1 minutes;
        uint256 newPrice = 2_900e8;
        oracle.setPrice(address(weth), WETH_PRICE);
        VM.warp(updateTime);

        VM.expectEmit(true, false, false, true, address(oracle));
        // Template event required by Foundry's expectEmit assertion.
        // forge-lint: disable-next-line(reentrancy-events)
        emit PriceOracle.PriceUpdated(address(weth), newPrice, updateTime);

        oracle.setPrice(address(weth), newPrice);

        // Only the changed price and timestamp are relevant here.
        // forge-lint: disable-next-line(unused-return)
        (uint256 price, uint256 updatedAt,,) = oracle.assetPrices(address(weth));
        assert(price == newPrice);
        assert(updatedAt == updateTime);
    }

    function testPriceAtExactExpiryIsValid() public {
        oracle.setPrice(address(weth), WETH_PRICE);
        VM.warp(START_TIME + MAX_PRICE_AGE);

        assert(oracle.isPriceValid(address(weth)));
        assert(oracle.getPrice(address(weth)) == WETH_PRICE);
    }

    function testPriceOneSecondAfterExpiryIsInvalid() public {
        oracle.setPrice(address(weth), WETH_PRICE);
        VM.warp(START_TIME + MAX_PRICE_AGE + 1);

        assert(!oracle.isPriceValid(address(weth)));
        VM.expectRevert(abi.encodeWithSelector(PriceOracle.StalePrice.selector, address(weth)));
        // The expected revert is asserted above; no return value is expected.
        // forge-lint: disable-next-line(unused-return)
        oracle.getPrice(address(weth));
    }

    function testRepublishingSamePriceRestoresValidity() public {
        oracle.setPrice(address(weth), WETH_PRICE);
        VM.warp(START_TIME + MAX_PRICE_AGE + 1);
        assert(!oracle.isPriceValid(address(weth)));

        oracle.setPrice(address(weth), WETH_PRICE);

        assert(oracle.isPriceValid(address(weth)));
        assert(oracle.getPrice(address(weth)) == WETH_PRICE);
        // Only the refreshed timestamp is relevant here.
        // forge-lint: disable-next-line(unused-return)
        (, uint256 updatedAt,,) = oracle.assetPrices(address(weth));
        assert(updatedAt == START_TIME + MAX_PRICE_AGE + 1);
    }

    function testAssetPricesAndExpiryAreIndependent() public {
        uint256 newPrice = 2_800e8;
        oracle.registerAsset(address(usdc), USDC_MAX_PRICE_AGE);
        oracle.setPrice(address(weth), WETH_PRICE);
        oracle.setPrice(address(usdc), USDC_PRICE);
        VM.warp(START_TIME + MAX_PRICE_AGE + 1);

        assert(!oracle.isPriceValid(address(weth)));
        assert(oracle.isPriceValid(address(usdc)));
        assert(oracle.getPrice(address(usdc)) == USDC_PRICE);

        oracle.setPrice(address(weth), newPrice);

        assert(oracle.getPrice(address(weth)) == newPrice);
        assert(oracle.getPrice(address(usdc)) == USDC_PRICE);
        // Updating WETH must not refresh USDC's timestamp.
        // forge-lint: disable-next-line(unused-return)
        (, uint256 updatedAt,,) = oracle.assetPrices(address(usdc));
        assert(updatedAt == START_TIME);
    }
}

// forge-lint: disable-end(multi-contract-file)
