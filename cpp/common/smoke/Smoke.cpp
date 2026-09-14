#include <iostream>

#include "dlp/ethereum/Address.hpp"
#include "dlp/ethereum/Uint256.hpp"

int main()
{
    const auto zeroAddress = dlp::ethereum::Address{};
    const auto chainId = dlp::ethereum::Uint256{1};

    std::cout << "DLP C++ common layer ready: " << zeroAddress.ToHex() << ", chain "
              << chainId.ToDecimal() << '\n';
    return 0;
}
