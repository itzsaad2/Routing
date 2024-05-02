#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"
using namespace std;


// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";

}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  const uint32_t next_hop_ip = next_hop.ipv4_numeric();
  
  if (arp_table.contains(next_hop_ip)) {
    EthernetFrame frame = EthernetFrame();
    frame.header.type = EthernetHeader::TYPE_IPv4;
    frame.header.src = ethernet_address_;
    frame.header.dst = arp_table[next_hop_ip].first;
    Serializer serializer;
    dgram.serialize(serializer);
    frame.payload = move(serializer.output());
    ready_to_be_sent.push(frame);

  } else {
    arp_queue.push(next_hop_ip);
    if (arp_requests.find(next_hop_ip) == arp_requests.end() || (timer - arp_requests[next_hop_ip] > 5000) ){
      send_arp_request();
      arp_requests[next_hop_ip] = timer;
    }
    packet_queue.push(dgram);    
  }
}

void NetworkInterface::send_arp_request() {
  ARPMessage arp_msg;
  arp_msg.opcode = ARPMessage::OPCODE_REQUEST;
  arp_msg.sender_ethernet_address = ethernet_address_;
  arp_msg.sender_ip_address = ip_address_.ipv4_numeric();
  arp_msg.target_ethernet_address = {0, 0, 0, 0, 0, 0};
  uint32_t target_ip_addr = arp_queue.front();
  arp_queue.pop(); 
  arp_msg.target_ip_address = target_ip_addr; 

  // Send ARP request
  EthernetFrame frame;
  frame.header.type = EthernetHeader::TYPE_ARP;
  frame.header.src = ethernet_address_;
  frame.header.dst = ETHERNET_BROADCAST;
  Serializer serializer;
  arp_msg.serialize(serializer);
  frame.payload = move(serializer.output());
  ready_to_be_sent.push(frame);
}



// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if (!ethernet_address_equal(frame.header.dst, ETHERNET_BROADCAST) && !ethernet_address_equal(frame.header.dst, ethernet_address_)) {
    return {};
  
  // IPv4
  } else if (frame.header.type == EthernetHeader::TYPE_IPv4){
    InternetDatagram datagram;
    Parser parser = Parser(frame.payload);
    datagram.parse(parser);
    return datagram;
    

  // ARP
  } else if (frame.header.type == EthernetHeader::TYPE_ARP){
    ARPMessage msg;
    Parser parser = Parser(frame.payload);

    if (!parser.has_error()){
      msg.parse(parser);

    }
    if (msg.supported()) { 
      arp_table[msg.sender_ip_address] = pair(msg.sender_ethernet_address, timer);
      // arp reply
      if (msg.opcode == ARPMessage::OPCODE_REQUEST && msg.target_ip_address == ip_address_.ipv4_numeric()){
        ARPMessage reply_msg;
        reply_msg.opcode = ARPMessage::OPCODE_REPLY;
        reply_msg.sender_ethernet_address = ethernet_address_;
        reply_msg.sender_ip_address = ip_address_.ipv4_numeric();
        reply_msg.target_ethernet_address = msg.sender_ethernet_address;
        reply_msg.target_ip_address = msg.sender_ip_address;

        EthernetFrame eth_frame;
        eth_frame.header.type = EthernetHeader::TYPE_ARP;
        eth_frame.header.src = ethernet_address_;
        eth_frame.header.dst = frame.header.src;
        Serializer serializer;
        reply_msg.serialize(serializer);
        eth_frame.payload = move(serializer.output());
        ready_to_be_sent.push(eth_frame);
        }

        while(!packet_queue.empty()){
          InternetDatagram packet = packet_queue.front();
          packet_queue.pop();
          EthernetFrame eth_frame = EthernetFrame();
          eth_frame.header.type = EthernetHeader::TYPE_IPv4;
          eth_frame.header.src = ethernet_address_;
          eth_frame.header.dst = msg.sender_ethernet_address;
          Serializer serializer;
          packet.serialize(serializer);
          eth_frame.payload = move(serializer.output());
          ready_to_be_sent.push(eth_frame);    
        
        }
      }
  }

  return {};
}

bool NetworkInterface::ethernet_address_equal(EthernetAddress addr1, EthernetAddress addr2) {
  for (int i = 0; i < 6; i++) {
    if (addr1[i] != addr2[i]) {
      return false;
    }
  }
  return true;
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  timer += ms_since_last_tick;

  // Expire any entry in ARP cache table that was learnt more than 30 seconds ago.
  list<uint32_t> arp_expired;

  for(auto elem = arp_table.begin(); elem != arp_table.end(); ++elem) {
    if (timer - elem->second.second > 30000){
        arp_expired.push_back(elem->first); 
    }
  }
  for (auto key : arp_expired) {
      arp_table.erase(key);
  }

  // Remove the pending ARP reply wait for any next hop IP that was sent more than 5 seconds ago. Furthermore, you should also empty any packets waiting for that IP address from the queue.
  for (auto elem = arp_requests.begin(); elem != arp_requests.end(); ++elem){
    if (timer - elem->second > 5000) {
      // while()
      InternetDatagram packet = packet_queue.front();
      if (packet.header.dst == elem->first){
        packet_queue.pop();
      }
    }
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if (!ready_to_be_sent.empty()){
    EthernetFrame frame = ready_to_be_sent.front();
    ready_to_be_sent.pop();
    return frame;
  }
  return {};
}
