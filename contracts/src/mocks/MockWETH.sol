// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

import {ERC20} from "@openzeppelin/contracts/token/ERC20/ERC20.sol";

/// @title Mock WETH
/// @notice Test-only WETH with unrestricted minting for local and testnet development.
contract MockWETH is ERC20 {
    constructor() ERC20("Mock WETH", "mWETH") {}

    function decimals() public pure override returns (uint8) {
        return 18;
    }

    function mint(address to, uint256 amount) external {
        _mint(to, amount);
    }
}
