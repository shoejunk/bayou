#pragma once

#include <cstdint>
#include <string>
#include <vector>

// The message contract is defined once in network_messages.inc and shared with
// the module-based client via network.cppm. Edit the .inc, not this list.
namespace network
{
#include "network_messages.inc"
} // namespace network
