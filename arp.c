#include "arp.h"

void handle_arp(struct ether_hdr *ether_header, struct arp_hdr *arp_header, 
    char buf[MAX_PACKET_LEN], size_t len, size_t interface, 
    struct arp_table_entry *arp_table, int *arp_table_len,
    struct route_table_entry *routing_table, int routing_table_len,
    struct queue *q) {

    // check if this is an arp request
    if (ntohs(arp_header->opcode) == ARP_REQUEST) {
        // see if the router's ip is the one it is looking for
        if (arp_header->tprotoa == inet_addr(get_interface_ipv4(interface))) {
            // reply to request with router's mac
            // STEP 1: update ether header
            memcpy(ether_header->ethr_dhost, ether_header->ethr_shost, 6);
            get_interface_mac(interface, ether_header->ethr_shost);
    
            // STEP 2: update ARP header
            // update message code
            arp_header->opcode = htons(ARP_REPLY);

            // swap send and destination IP addr
            uint32_t ip_temp = arp_header->sprotoa;
            arp_header->sprotoa = arp_header->tprotoa;
            arp_header->tprotoa = ip_temp;

            // change D MAC to S MAC
            memcpy(arp_header->thwa, arp_header->shwa, 6);
            get_interface_mac(interface, arp_header->shwa);

            // send back the reply
            send_to_link(len, buf, interface);
        } else {
            // router is not the device ARP is looking for, drop packet
            return;
        }
    } else if (ntohs(arp_header->opcode) == ARP_REPLY) {
        // check if ARP repply is for this router
        if (arp_header->tprotoa == inet_addr(get_interface_ipv4(interface))) {
            // good, now the router knows the sender's MAC
            // cache it
            int index = (*arp_table_len)++;
            arp_table[index].ip = arp_header->sprotoa;
            memcpy(arp_table[index].mac, arp_header->shwa, 6);

            // make a new queue to make parsing the original one easier
            struct queue *temp_q = create_queue();

            // parse the queeu for any packets that were waiting for this MAC address
            while (!queue_empty(q)) {
                char *q_buf = (char *)queue_deq(q);
                struct ether_hdr *eth_hdr = (struct ether_hdr *)q_buf;
                struct ip_hdr *ip_hdr = (struct ip_hdr *)(q_buf + sizeof(struct ether_hdr));

                // find the next hop for this dequeued packet
                struct route_table_entry *best_route = get_best_route(ip_hdr->dest_addr, 
                    routing_table, routing_table_len);

                // check if this packet was waiting for the MAC the router just learned
                if (best_route->next_hop == arp_header->sprotoa) {
                    
                    // update the Ethernet header
                    memcpy(eth_hdr->ethr_dhost, arp_header->shwa, 6);

                    // find packet length
                    size_t len = sizeof(struct ether_hdr) + ntohs(ip_hdr->tot_len);

                    send_to_link(len, q_buf, best_route->interface);
                    
                    free(q_buf);
                } else {
                    // packet was not waiting for this MAC, put it in the new queue
                    queue_enq(temp_q, q_buf);
                }
            }

            // move all remaining packets back to the main queue
            while (!queue_empty(temp_q)) {
                queue_enq(q, queue_deq(temp_q));
            }
            free(temp_q);
        } else
            // ARP repply is not for this router, drop
            return;
    } else
        // router can't do anything with other kind of ARP messages drop packet
        return;
}

void send_arp_request(struct route_table_entry *best_route, int interface,
    uint16_t proto_type) {
    // make a new packet from scratch
    char *buf = calloc(1, sizeof(struct ether_hdr) + sizeof(struct arp_hdr));
    struct ether_hdr *ether_header = (struct ether_hdr *)buf;
    struct arp_hdr *arp_header = (struct arp_hdr *)(buf + sizeof(struct ether_hdr));
    
    // make ethernet header
    get_interface_mac(interface, ether_header->ethr_shost);
    memcpy(ether_header->ethr_dhost, BROADCAST_MAC, 6);
    ether_header->ethr_type = htons(ETHR_TYPE_ARP);
    
    // make ARP header
    arp_header->hw_len = ARP_HARDWARE_LEN_ETH;
    arp_header->hw_type = htons(ARP_HARDWARE_TYPE_ETH);
    arp_header->opcode = htons(ARP_REQUEST);
    arp_header->proto_len = ARP_PROTOCL_LEN_IP;
    arp_header->proto_type = htons(proto_type);

    get_interface_mac(interface, arp_header->shwa);
    arp_header->sprotoa = inet_addr(get_interface_ipv4(interface));
    memcpy(arp_header->thwa, BROADCAST_MAC, 6);
    arp_header->tprotoa = best_route->next_hop;

    send_to_link(sizeof(struct ether_hdr) + sizeof(struct arp_hdr), buf, best_route->interface);

    free(buf);
}
