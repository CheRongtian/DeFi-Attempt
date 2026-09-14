// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

// Keep the token harness and minimal cheatcode interface with their only consumer.
// forge-lint: disable-start(multi-contract-file)
// Exact equality is required for scaled balance and index conversion checks.
// forge-lint: disable-start(incorrect-strict-equality)
// Repeated amounts keep each token behavior test independently readable.
// forge-lint: disable-start(literal-instead-of-constant)
// ERC-20 calls intentionally assert that every transfer and approval path reverts.
// forge-lint: disable-start(erc20-unchecked-transfer)
// forge-lint: disable-start(arbitrary-send-erc20)
// forge-lint: disable-start(unused-return)

import {IERC20Errors} from "@openzeppelin/contracts/interfaces/draft-IERC6093.sol";
import {IIndexProvider} from "../src/interfaces/IIndexProvider.sol";
import {MathLib} from "../src/libraries/MathLib.sol";
import {DepositToken} from "../src/tokens/DepositToken.sol";

interface IDepositTokenVm {
    function expectRevert(bytes calldata revertData) external;
}

contract DepositIndexHarness is IIndexProvider {
    uint256 private liquidityIndex = MathLib.RAY;
    DepositToken public immutable TOKEN;

    constructor() {
        TOKEN = new DepositToken(address(this));
    }

    function normalizedLiquidityIndex() external view returns (uint256) {
        return liquidityIndex;
    }

    function normalizedBorrowIndex() external pure returns (uint256) {
        return MathLib.RAY;
    }

    function setLiquidityIndex(uint256 index) external {
        liquidityIndex = index;
    }

    function mint(address account, uint256 scaledAmount) external {
        TOKEN.mintScaled(account, scaledAmount);
    }

    function burn(address account, uint256 scaledAmount) external {
        TOKEN.burnScaled(account, scaledAmount);
    }
}

contract DepositTokenTest {
    IDepositTokenVm private constant VM = IDepositTokenVm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);
    address private constant ALICE = address(0xA11CE);
    address private constant BOB = address(0xB0B);

    DepositIndexHarness private harness;
    DepositToken private token;

    function setUp() public {
        harness = new DepositIndexHarness();
        token = harness.TOKEN();
    }

    function testStoresMetadataAndPool() public view {
        assert(keccak256(bytes(token.name())) == keccak256(bytes("USDC Deposit Token")));
        assert(keccak256(bytes(token.symbol())) == keccak256(bytes("dUSDC")));
        assert(token.decimals() == 6);
        assert(token.POOL() == address(harness));
    }

    function testMintCreatesScaledSupplierClaim() public {
        harness.mint(ALICE, 100e6);

        assert(token.scaledBalanceOf(ALICE) == 100e6);
        assert(token.scaledTotalSupply() == 100e6);
        assert(token.balanceOf(ALICE) == 100e6);
        assert(token.totalSupply() == 100e6);
    }

    function testBalanceGrowsWithLiquidityIndex() public {
        harness.mint(ALICE, 100e6);
        harness.setLiquidityIndex(11e26);

        assert(token.scaledBalanceOf(ALICE) == 100e6);
        assert(token.balanceOf(ALICE) == 110e6);
        assert(token.totalSupply() == 110e6);
    }

    function testSupplierClaimRoundsDown() public {
        harness.mint(ALICE, 3);
        harness.setLiquidityIndex(MathLib.RAY + 1);

        assert(token.balanceOf(ALICE) == 3);
    }

    function testBurnRemovesScaledBalance() public {
        harness.mint(ALICE, 100e6);
        harness.burn(ALICE, 40e6);

        assert(token.scaledBalanceOf(ALICE) == 60e6);
        assert(token.balanceOf(ALICE) == 60e6);
    }

    function testOnlyPoolCanMintOrBurn() public {
        VM.expectRevert(abi.encodeWithSelector(DepositToken.OnlyPool.selector, address(this)));
        token.mintScaled(ALICE, 1);

        VM.expectRevert(abi.encodeWithSelector(DepositToken.OnlyPool.selector, address(this)));
        token.burnScaled(ALICE, 1);
    }

    function testTransferAndApprovalAreDisabled() public {
        harness.mint(ALICE, 100e6);

        VM.expectRevert(abi.encodeWithSelector(DepositToken.NonTransferable.selector));
        token.transfer(BOB, 1);

        VM.expectRevert(abi.encodeWithSelector(DepositToken.NonTransferable.selector));
        token.approve(BOB, 1);

        VM.expectRevert(abi.encodeWithSelector(DepositToken.NonTransferable.selector));
        token.transferFrom(ALICE, BOB, 1);
    }

    function testInvalidPoolRejected() public {
        VM.expectRevert(abi.encodeWithSelector(DepositToken.InvalidAddress.selector));
        new DepositToken(address(0));
    }

    function testBurnAboveScaledBalanceRejected() public {
        harness.mint(ALICE, 1);

        VM.expectRevert(
            abi.encodeWithSelector(IERC20Errors.ERC20InsufficientBalance.selector, ALICE, uint256(1), uint256(2))
        );
        harness.burn(ALICE, 2);
    }
}

// forge-lint: disable-end(unused-return)
// forge-lint: disable-end(arbitrary-send-erc20)
// forge-lint: disable-end(erc20-unchecked-transfer)
// forge-lint: disable-end(literal-instead-of-constant)
// forge-lint: disable-end(incorrect-strict-equality)
// forge-lint: disable-end(multi-contract-file)
