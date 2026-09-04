// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

import {MockWETH} from "../src/mocks/MockWETH.sol";

contract MockWETHTest {
    MockWETH private token;

    function setUp() public {
        token = new MockWETH();
    }

    function testNameAndSymbol() public view {
        assert(keccak256(bytes(token.name())) == keccak256(bytes("Mock WETH")));
        assert(keccak256(bytes(token.symbol())) == keccak256(bytes("mWETH")));
    }

    function testDecimals() public view {
        assert(token.decimals() == 18);
    }

    function testMint() public {
        uint256 amount = 1_000e18;

        token.mint(address(this), amount);

        // forge-lint: disable-next-line(incorrect-strict-equality)
        assert(token.balanceOf(address(this)) == amount);
        assert(token.totalSupply() == amount);
    }

    function testTransfer() public {
        address recipient = address(0xBEEF);
        uint256 amount = 125e18;

        token.mint(address(this), amount);
        bool transferred = token.transfer(recipient, amount);

        assert(transferred);

        // forge-lint: disable-next-line(incorrect-strict-equality)
        assert(token.balanceOf(address(this)) == 0);

        // forge-lint: disable-next-line(incorrect-strict-equality)
        assert(token.balanceOf(recipient) == amount);
    }
}