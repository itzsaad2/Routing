#include "router.hh"

#include <iostream>
#include <limits>
#include <optional>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

//create RouteNode object
  RouteNode R;
  //assign values that were given to us to said object by calling our updateRouteNode function
  R.updateRouteNode(next_hop, route_prefix, interface_num, prefix_length);
  //add to the routing table
  routetable.push_back(R);
}
//This function sends the Datagram to the correct interface number outside of the network by sending to next hop.
void Router::SendOutsideNetwork(InternetDatagram &tosend, size_t inum, int nextID) 
{
    //double check nhop indeed has value and it is not null before doing value()
    //also double check tosend header ttl is not zero before sending(otherwise we would have to drop packet)
    if(routetable[nextID].nhop.has_value()){
        if(routetable[nextID].nhop != std::nullopt){
            if(tosend.header.ttl != 0){
                //get the address and make sure it is an adress type by using Address::from_ipv4_numeric
                const optional<Address> nexthop = routetable[nextID].nhop.value();
                Address addr = Address::from_ipv4_numeric(nexthop->ipv4_numeric());
                interface(inum).send_datagram(tosend,addr);   
            }
            
        }
    }
}

//This function sends the Datagram to the correct interface inside the network, hence we look up the dgram dst and send it there.
void Router::SendInsideNetwork(InternetDatagram &tosend, size_t inum) 
{
    //double check ttl is not zero before sending, no next hop to check here
    if(tosend.header.ttl != 0){
        //set address properly using the datagrams dst
        auto addr = Address::from_ipv4_numeric(tosend.header.dst);
        interface(inum).send_datagram(tosend, addr);
    } 
}

//this function returns the mask value and considers the special cases of 0 and 32 prefix length.
uint32_t Router::retmask(uint8_t plen){
    uint32_t mask;
    if(plen == 0){
        //cannot shift set to 0
            mask = 0;
        }
    else if(plen == 32){
        //bascially the same as shifitng by nothing. hex value is essentially 32 bits of all 1s meaning entire thing is address
        mask = 0xffffffff;
    }
    else{
        //we know we have to shift 0xfffffff by 32 - plen(where plen is 1 <= plen < 32) to create subnet mask
        uint32_t shiftthis = 0xffffffff;
        uint32_t shiftby = 32 - plen;
        mask = (shiftthis << shiftby);
    }
    return mask;
}

//This function checks to see if Subnet masks & addresses match, if so then we can return true as this is a possible route we can take.
bool Router::checkroute(uint32_t submask, uint32_t dst, uint32_t prefix ){
    bool retval;
    //do bitwise & with dst and prefix to get only thier network portion and compare to see if equal, assign boolean accordingly and return.
    auto bit1 = (submask & dst);
    auto bit2 = (submask & prefix);
    if(bit1 == bit2){
        retval = true;
    }
    else if(bit1 != bit2){
        retval = false;
    }

    return retval;
}

void Router::route() 
{
    //iterate through the interfaces
    //count is used to iterate.
    size_t count = 0;
    while(count != interfaces_.size()){
        //use maybe_receive to find out if there is a datagram in the current interface
        std::optional<InternetDatagram> hasdgram = interfaces_[count].maybe_receive();
        //check if empty, otherwise there is a dgram to send
        while (hasdgram.has_value())
            {
            //NextID: is the index of matching prefix route
            //longestmatch: simply keeps track of the current longest matching prefix for route
            int nextID = -1;
            int longestmatch = -1;
            //check if value it has is not null
            if(hasdgram != std::nullopt){
                //get the dgram for this interface
                InternetDatagram tosend = hasdgram.value();
                //find the longest matching prefix for the dgram's dst by iterating the route table
                //counter: is there to iterate through the routes
                size_t counter = 0;
                while(counter != routetable.size()){
                    uint8_t plen = routetable[counter].prefixlen;
                    //get the subnet mask using retmask function
                    uint32_t submask = retmask(plen);
                    uint32_t pfix = routetable[counter].prefix;
                    //then we check the routes to see if the subnet masks matches 
                    if(checkroute(submask, tosend.header.dst, pfix)){
                        //if they match check which is greater plen or longestmatch(make sure both types are the same)
                        int maxpfix = max(longestmatch, static_cast<int>(plen));
                        //if plen is greater then we have a new route to keep track of
                        if(maxpfix == static_cast<int>(plen)){
                            //keep track of new route that we can send to(cast right types as well)
                            longestmatch = static_cast<int>(plen);
                            nextID = static_cast<int>(counter);
                        }
                        else{
                            //go to next route if longest is greater
                            counter++;
                            continue;
                        }
                    }
                    //move to next route
                    counter++;
                }
                //all routes have been traversed and a route should have been found

                //made an if to check that NextId != -1 or greater than -1 as if these starements were false then no routes found, and an if for ttl not equal to 1 and not equal to zero as anything above 1 would allow us to send and not drop packet. 
                if(nextID != -1 || nextID > -1){
                    if(tosend.header.ttl != 1 && tosend.header.ttl != 0){
                        //decrease ttl since it will not go to 0
                        tosend.header.ttl -= 1;
                        //compute the checksum since we have modifed header by decreasing ttl(and to avoid bad datagram received)
                        tosend.header.compute_checksum();
                        //check if there is a next hop and it is not null, if so we have to send it outside of our network
                        if(routetable[nextID].nhop.has_value()){
                            if(routetable[nextID].nhop != std::nullopt){
                                //get interface number using route table and nextID for route to send to
                                size_t inum = routetable[nextID].interface_num;
                                SendOutsideNetwork(tosend, inum, nextID);
                            }
                        }
                        //otherwise if there is no nexthop, we have to send inside of our network(to datagrams dst)
                        else{
                            //get interface number using route table and nextID for route to send to
                            size_t inum = routetable[nextID].interface_num;
                            SendInsideNetwork(tosend, inum);
                        } 
                    }
                }
                //drop and go to next iteration/packet if no route found
                else{
                    hasdgram = interfaces_[count].maybe_receive();
                    continue;
                }
            }
            //otherwise drop
            //move to next dgram for current interface
            hasdgram = interfaces_[count].maybe_receive();
        }
        //move to next interface.
        count++;
    }
}