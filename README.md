# Router Dataplane

A software implementation of an IPv4 router's forwarding (data) plane in C. The router reads a static routing table, performs longest-prefix-match lookups, handles Ethernet/ARP resolution, and correctly manages IPv4-layer details like checksums, TTL, and ICMP error/reply generation — the same logic a real router's fast path is responsible for.

Built as part of a computer networking / protocols coursework assignment.

## What it does

On every incoming packet, the router:

1. **Parses the Ethernet frame** and checks the destination MAC address (accepts frames addressed to the router's own interface or broadcast, drops everything else).
2. **Dispatches by EtherType** to the IPv4 or ARP handler (an IPv6 stub is present but not yet implemented).
3. **For IPv4 packets:**
   - Verifies the header checksum and drops the packet on mismatch.
   - If the router itself is the destination, replies to ICMP echo requests.
   - Otherwise, checks and decrements TTL, sending an ICMP Time Exceeded reply if TTL has expired.
   - Recomputes the header checksum after modifying TTL.
   - Looks up the destination in the routing table using **longest prefix match**; sends an ICMP Destination Unreachable reply if no route exists.
   - Resolves the next hop's MAC address via the ARP cache. On a cache miss, the packet is queued and an ARP request is sent; queued packets are flushed once the reply arrives.
   - Rewrites the Ethernet header (source/destination MAC) and forwards the packet out the correct interface.
4. **For ARP packets:** handles incoming ARP requests/replies, maintains the ARP table, and sends outgoing ARP requests for unresolved next hops.

## Building

```bash
make
```

This compiles `router.c`, `ipv4.c`, `icmp.c`, `arp.c`, and the support library in `lib/` into a single `router` binary.

## Running

The router binary expects a routing table file followed by a list of interface names:

```bash
./router <rtable_file> <interface_0> <interface_1> ...
```

Convenience targets are provided for the two-router topology used in testing:

```bash
make run_router0   # builds and runs with rtable0.txt
make run_router1   # builds and runs with rtable1.txt
```

These are meant to be run inside the virtual network topology set up by the checker (see below) — the interface names correspond to virtual links between routers/hosts, not physical NICs.

## Testing

A checker script (written by the PCOM team) is provided to validate the implementation against expected behavior:

```bash
./checker/checker.sh
```

## Status / roadmap

- [x] IPv4 forwarding with longest prefix match
- [x] TTL handling and checksum validation
- [x] ICMP echo reply, time exceeded, destination unreachable
- [x] ARP request/reply handling with a pending-packet queue
- [ ] IPv6 support
- [ ] VLAN tagging
- [ ] Dynamic routing (e.g. RIP/OSPF-style updates instead of a static table)
