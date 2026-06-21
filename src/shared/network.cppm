module;

#include <cstdint>
#include <string>
#include <vector>

export module network;

// The message contract is defined once in network_messages.inc and shared with
// the #include-based services via network.hpp. Edit the .inc, not this list.
export namespace network
{
#include "network_messages.inc"
} // namespace network
