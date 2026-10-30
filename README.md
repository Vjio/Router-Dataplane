# Router Dataplane

A software IPv4 router data plane in C. It forwards packets using longest-prefix-match lookup over a sorted routing table (binary search), resolves next hops via ARP with a pending-packet queue, and generates ICMP replies and errors, covering the logic a real router's fast path handles.

Developed for a computer networking course; the virtual topology and checker are provided by the course, and the forwarding logic is my own implementation.

## Features

- Ethernet frame parsing and destination-MAC filtering (own interface or broadcast)
- IPv4 header checksum verification, TTL decrement and checksum update
- **Longest-prefix-match routing in O(log n)** using binary search
- ARP request/reply handling, ARP cache, and a queue for packets awaiting resolution
- ICMP echo reply, Time Exceeded, and Destination Unreachable

## Packet pipeline

1. **Ethernet:** accept frames addressed to the router or broadcast; drop the rest.
2. **Dispatch** by EtherType to the IPv4 or ARP handler (IPv6 stub present, not implemented).
3. **IPv4:**
   - Verify the header checksum; drop on mismatch.
   - If addressed to the router, reply to ICMP echo requests.
   - Otherwise check TTL; send ICMP Time Exceeded if it expired, else decrement it and recompute the checksum.
   - Look up the next hop (see below); send ICMP Destination Unreachable if there is no route.
   - Resolve the next-hop MAC via the ARP cache. On a miss, queue the packet and send an ARP request.
   - Rewrite the Ethernet header and forward out the correct interface.
4. **ARP:** update the cache on replies, answer requests for the router's own addresses, and flush queued packets once a reply arrives.

## Design notes

### Longest prefix match

The routing table is loaded once at startup and **sorted by [prefix, then mask length]**. Each lookup uses **binary search** to find the candidate entries for the destination address and picks the one with the longest matching mask.

| | Linear scan | This implementation |
|---|---|---|
| Lookup | O(n) | O(log n) [+ scan over equal-prefix entries] |
| Setup | none | O(n log n) sort at startup |

The trade-off is that the table is static. Supporting dynamic updates would need re-sorting or a different structure such as a binary trie.

### ARP and pending packets

Resolved next hops are cached in a flat array of (IP, MAC) entries, which is filled when ARP replies arrive.

When the next hop's MAC isn't cached, the packet is copied into a FIFO queue and a broadcast ARP request is sent out the egress interface. When a reply arrives, the router caches the sender's MAC, then drains the queue: each packet's next hop is recomputed with the LPM lookup, packets waiting on the newly resolved address get their destination MAC filled in and are forwarded, and the rest are put back in their original order.

### Checksum

The IPv4 header checksum is verified on every incoming packet. After the TTL is decremented, the checksum is zeroed and recomputed over the full 20-byte header.

## Project structure

| File | Responsibility |
|---|---|
| `router.c` | Main loop, Ethernet handling, dispatch |
| `ipv4.c` | IPv4 validation, TTL/checksum, forwarding, LPM lookup |
| `icmp.c` | ICMP echo reply, Time Exceeded, Destination Unreachable |
| `arp.c` | ARP cache, requests/replies, pending-packet queue |
| `lib/` | Course-provided support library (raw socket I/O, headers) |

## Requirements

- Linux, `gcc`, `make`
- Mininet and Python 3

## Building and running

```bash
make
./router <rtable_file> <interface_0> <interface_1> ...
```

Convenience targets for the two-router test topology:

```bash
make run_router0   # runs with rtable0.txt
make run_router1   # runs with rtable1.txt
```

These run inside the virtual topology set up by the checker. The interface names correspond to virtual links.

## Testing

```bash
./checker/checker.sh
```

The course provided checker covers [forwarding, ARP, ICMP echo, TTL expiry, unreachable destinations].
