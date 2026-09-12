// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

// Keep the minimal cheatcode interface and external-call harness with their only consumer.
// forge-lint: disable-start(multi-contract-file)
// Literal inputs and expected values make each arithmetic test independently readable.
// forge-lint: disable-start(literal-instead-of-constant)

import {Panic} from "@openzeppelin/contracts/utils/Panic.sol";
import {MathLib} from "../src/libraries/MathLib.sol";

interface IMathLibVm {
    function expectRevert(bytes calldata revertData) external;
}

// An external call lets expectRevert observe failures in internal library functions.
contract MathLibHarness {
    function mulDivDown(uint256 x, uint256 y, uint256 denominator) external pure returns (uint256) {
        return MathLib.mulDivDown(x, y, denominator);
    }

    function mulDivUp(uint256 x, uint256 y, uint256 denominator) external pure returns (uint256) {
        return MathLib.mulDivUp(x, y, denominator);
    }

    function toUsdWadDown(uint256 amount, uint256 oraclePrice, uint8 tokenDecimals) external pure returns (uint256) {
        return MathLib.toUsdWadDown(amount, oraclePrice, tokenDecimals);
    }

    function toUsdWadUp(uint256 amount, uint256 oraclePrice, uint8 tokenDecimals) external pure returns (uint256) {
        return MathLib.toUsdWadUp(amount, oraclePrice, tokenDecimals);
    }
}

contract MathLibTest {
    IMathLibVm private constant VM = IMathLibVm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);
    uint256 private constant MAX_UINT = type(uint256).max;
    bytes4 private constant PANIC_SELECTOR = 0x4e487b71;

    MathLibHarness private harness;

    function setUp() public {
        harness = new MathLibHarness();
    }

    function testScales() public pure {
        assert(MathLib.WAD == 1e18);
        assert(MathLib.BPS == 10_000);
        assert(MathLib.ORACLE_PRICE_SCALE == 1e8);
    }

    function testExactDivisionHasSameResultForBothDirections() public pure {
        assert(MathLib.mulDivDown(6, 7, 3) == 14);
        assert(MathLib.mulDivUp(6, 7, 3) == 14);
    }

    function testRemainderRoundsInRequestedDirection() public pure {
        assert(MathLib.mulDivDown(10, 10, 6) == 16);
        assert(MathLib.mulDivUp(10, 10, 6) == 17);
        assert(MathLib.mulDivDown(1, 1, 2) == 0);
        assert(MathLib.mulDivUp(1, 1, 2) == 1);
    }

    function testZeroProductRemainsZero() public pure {
        assert(MathLib.mulDivDown(0, MAX_UINT, 1) == 0);
        assert(MathLib.mulDivUp(0, MAX_UINT, 1) == 0);
        assert(MathLib.mulDivDown(MAX_UINT, 0, 1) == 0);
        assert(MathLib.mulDivUp(MAX_UINT, 0, 1) == 0);
    }

    function testBasisPointsUseExplicitRounding() public pure {
        assert(MathLib.mulDivDown(10_001, 7_500, MathLib.BPS) == 7_500);
        assert(MathLib.mulDivUp(10_001, 7_500, MathLib.BPS) == 7_501);
    }

    function testIntermediateProductCanExceedUint256() public pure {
        assert(MathLib.mulDivDown(1 << 200, 1 << 100, 1 << 100) == 1 << 200);
        assert(MathLib.mulDivUp(1 << 200, 1 << 100, 1 << 100) == 1 << 200);
        assert(MathLib.mulDivDown(MAX_UINT, MAX_UINT, MAX_UINT) == MAX_UINT);
        assert(MathLib.mulDivUp(MAX_UINT, MAX_UINT, MAX_UINT) == MAX_UINT);
    }

    function testZeroDenominatorRejected() public {
        VM.expectRevert(abi.encodeWithSelector(PANIC_SELECTOR, Panic.DIVISION_BY_ZERO));
        // The return value is intentionally unused because this call must revert.
        // forge-lint: disable-next-line(unused-return)
        harness.mulDivDown(1, 1, 0);

        VM.expectRevert(abi.encodeWithSelector(PANIC_SELECTOR, Panic.DIVISION_BY_ZERO));
        // forge-lint: disable-next-line(unused-return)
        harness.mulDivUp(1, 1, 0);
    }

    function testZeroProductDoesNotHideZeroDenominator() public {
        VM.expectRevert(abi.encodeWithSelector(PANIC_SELECTOR, Panic.DIVISION_BY_ZERO));
        // forge-lint: disable-next-line(unused-return)
        harness.mulDivDown(0, 1, 0);

        VM.expectRevert(abi.encodeWithSelector(PANIC_SELECTOR, Panic.DIVISION_BY_ZERO));
        // forge-lint: disable-next-line(unused-return)
        harness.mulDivUp(0, 1, 0);
    }

    function testOverflowingQuotientRejected() public {
        VM.expectRevert(abi.encodeWithSelector(PANIC_SELECTOR, Panic.UNDER_OVERFLOW));
        // forge-lint: disable-next-line(unused-return)
        harness.mulDivDown(MAX_UINT, MAX_UINT, 1);

        VM.expectRevert(abi.encodeWithSelector(PANIC_SELECTOR, Panic.UNDER_OVERFLOW));
        // forge-lint: disable-next-line(unused-return)
        harness.mulDivUp(MAX_UINT, MAX_UINT, 1);
    }

    function testRoundingUpOverflowRejected() public {
        // (MAX - 1)^2 = (MAX - 2) * MAX + 1: floor fits, ceil does not.
        assert(MathLib.mulDivDown(MAX_UINT - 1, MAX_UINT - 1, MAX_UINT - 2) == MAX_UINT);

        VM.expectRevert(abi.encodeWithSelector(PANIC_SELECTOR, Panic.UNDER_OVERFLOW));
        // forge-lint: disable-next-line(unused-return)
        harness.mulDivUp(MAX_UINT - 1, MAX_UINT - 1, MAX_UINT - 2);
    }

    function testTenWethAtThreeThousandDollars() public pure {
        assert(MathLib.toUsdWadDown(10e18, 3_000e8, 18) == 30_000e18);
        assert(MathLib.toUsdWadUp(10e18, 3_000e8, 18) == 30_000e18);
    }

    function testTenThousandUsdcAtOneDollar() public pure {
        assert(MathLib.toUsdWadDown(10_000e6, 1e8, 6) == 10_000e18);
        assert(MathLib.toUsdWadUp(10_000e6, 1e8, 6) == 10_000e18);
    }

    function testWethDustRounding() public pure {
        assert(MathLib.toUsdWadDown(1, 300_000_000_001, 18) == 3_000);
        assert(MathLib.toUsdWadUp(1, 300_000_000_001, 18) == 3_001);
        assert(MathLib.toUsdWadDown(1, 1, 18) == 0);
        assert(MathLib.toUsdWadUp(1, 1, 18) == 1);
    }

    function testSmallestUsdcUnitAndOraclePrice() public pure {
        assert(MathLib.toUsdWadDown(1, 1e8, 6) == 1e12);
        assert(MathLib.toUsdWadUp(1, 1e8, 6) == 1e12);
        assert(MathLib.toUsdWadDown(1, 1, 6) == 1e4);
        assert(MathLib.toUsdWadUp(1, 1, 6) == 1e4);
    }

    function testZeroAmountOrPriceHasZeroValue() public pure {
        assert(MathLib.toUsdWadDown(0, MAX_UINT, 18) == 0);
        assert(MathLib.toUsdWadUp(0, MAX_UINT, 18) == 0);
        assert(MathLib.toUsdWadDown(0, MAX_UINT, 6) == 0);
        assert(MathLib.toUsdWadUp(0, MAX_UINT, 6) == 0);
        assert(MathLib.toUsdWadDown(MAX_UINT, 0, 18) == 0);
        assert(MathLib.toUsdWadUp(MAX_UINT, 0, 18) == 0);
        assert(MathLib.toUsdWadDown(MAX_UINT, 0, 6) == 0);
        assert(MathLib.toUsdWadUp(MAX_UINT, 0, 6) == 0);
    }

    function testLargeWethValueAvoidsIntermediateOverflow() public pure {
        assert(MathLib.toUsdWadDown(MAX_UINT, 1e8, 18) == MAX_UINT);
        assert(MathLib.toUsdWadUp(MAX_UINT, 1e8, 18) == MAX_UINT);
    }

    function testUnsupportedDecimalsRejected() public {
        VM.expectRevert(abi.encodeWithSelector(MathLib.UnsupportedDecimals.selector, uint8(8)));
        // forge-lint: disable-next-line(unused-return)
        harness.toUsdWadDown(1e8, 1e8, 8);

        VM.expectRevert(abi.encodeWithSelector(MathLib.UnsupportedDecimals.selector, uint8(8)));
        // forge-lint: disable-next-line(unused-return)
        harness.toUsdWadUp(1e8, 1e8, 8);
    }

    function testWethValueOverflowRejected() public {
        VM.expectRevert(abi.encodeWithSelector(PANIC_SELECTOR, Panic.UNDER_OVERFLOW));
        // forge-lint: disable-next-line(unused-return)
        harness.toUsdWadDown(MAX_UINT, 1e8 + 1, 18);

        VM.expectRevert(abi.encodeWithSelector(PANIC_SELECTOR, Panic.UNDER_OVERFLOW));
        // forge-lint: disable-next-line(unused-return)
        harness.toUsdWadUp(MAX_UINT, 1e8 + 1, 18);
    }

    function testUsdcValueOverflowRejected() public {
        uint256 amount = MAX_UINT / 1e4 + 1;
        VM.expectRevert(abi.encodeWithSelector(PANIC_SELECTOR, Panic.UNDER_OVERFLOW));
        // forge-lint: disable-next-line(unused-return)
        harness.toUsdWadDown(amount, 1, 6);

        VM.expectRevert(abi.encodeWithSelector(PANIC_SELECTOR, Panic.UNDER_OVERFLOW));
        // forge-lint: disable-next-line(unused-return)
        harness.toUsdWadUp(amount, 1, 6);
    }
}

// forge-lint: disable-end(multi-contract-file)
// forge-lint: disable-end(literal-instead-of-constant)
