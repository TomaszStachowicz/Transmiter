#include "xBridgePlus_packets_def.h"

class xBridgePlus {
public:
//basic xBridge packets:
_beacon_packet beacon_packet;
_txid_packet txid_packet;
_data_packet data_packet;
_ack_packet ack_packet;

//xBridgePlus extension packets:
_data_request_packet data_request_packet;
_status_packet status_packet;
_quarter_packet quarter_packet;
_history_packet history_packet;

bool data_is_current=false;
bool received_ack_packet=false;
bool requested_data_packet=false;
bool requested_quarter_packet1=false;
bool requested_quarter_packet2=false;

};

