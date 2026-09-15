#include "dlp/tx/TxTypes.hpp"

#include <stdexcept>

namespace dlp::tx
{

std::string_view ToString(TxStatus status) noexcept
{
    switch(status)
    {
        case TxStatus::Pending:
            return "Pending";
        case TxStatus::Submitted:
            return "Submitted";
        case TxStatus::Included:
            return "Included";
        case TxStatus::Finalized:
            return "Finalized";
        case TxStatus::Failed:
            return "Failed";
        case TxStatus::Replaced:
            return "Replaced";
        case TxStatus::Reorged:
            return "Reorged";
    }
    return "Failed";
}

TxStatus TxStatusFromString(std::string_view value)
{
    if(value == "Pending") return TxStatus::Pending;
    if(value == "Submitted") return TxStatus::Submitted;
    if(value == "Included") return TxStatus::Included;
    if(value == "Finalized") return TxStatus::Finalized;
    if(value == "Failed") return TxStatus::Failed;
    if(value == "Replaced") return TxStatus::Replaced;
    if(value == "Reorged") return TxStatus::Reorged;
    throw std::invalid_argument("unknown transaction status");
}

}
