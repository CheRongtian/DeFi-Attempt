// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

import {MockUSDC} from "../src/mocks/MockUSDC.sol";

contract MockUSDCTest {
    MockUSDC private token;

    function setUp() public {
        token = new MockUSDC();
    }

    function testNameAndSymbol() public view {
        assert(keccak256(bytes(token.name())) == keccak256(bytes("Mock USDC")));
        assert(keccak256(bytes(token.symbol())) == keccak256(bytes("mUSDC")));
    }

    function testDecimals() public view {
        assert(token.decimals() == 6);
    }

    function testMint() public {
        uint256 amount = 1_000e6;

        token.mint(address(this), amount);

        // assert(token.balanceOf(address(this)) == amount);
        // forge-lint: disable-next-line(incorrect-strict-equality)
        assert(token.balanceOf(address(this)) == amount);
        assert(token.totalSupply() == amount);
    }

    function testTransfer() public {
        address recipient = address(0xBEEF);
        uint256 amount = 125e6;

        token.mint(address(this), amount);
        bool transferred = token.transfer(recipient, amount);

        assert(transferred);
        // assert(token.balanceOf(address(this)) == 0);
        // assert(token.balanceOf(recipient) == amount);
        // forge-lint: disable-next-line(incorrect-strict-equality)
        assert(token.balanceOf(address(this)) == 0);

        // forge-lint: disable-next-line(incorrect-strict-equality)
        assert(token.balanceOf(recipient) == amount);
    }
}
