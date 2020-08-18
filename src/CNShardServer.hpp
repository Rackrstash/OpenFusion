#ifndef _CNSS_HPP
#define _CNSS_HPP

#include "CNProtocol.hpp"

#include <map>

enum SHARDPACKETID {
    // client 2 shard
    P_CL2FE_REQ_PC_ENTER = 318767105,
    P_CL2FE_REQ_PC_LOADING_COMPLETE = 318767245,
    P_CL2FE_REQ_PC_MOVE = 318767107,
    P_CL2FE_REQ_PC_STOP = 318767108,
    P_CL2FE_REQ_PC_JUMP = 318767109,
    P_CL2FE_REQ_PC_MOVEPLATFORM = 318767168,
    P_CL2FE_REQ_PC_GOTO = 318767124,
    P_CL2FE_GM_REQ_PC_SET_VALUE = 318767211,
    P_CL2FE_REQ_SEND_FREECHAT_MESSAGE = 318767111,
    P_CL2FE_REQ_PC_AVATAR_EMOTES_CHAT = 318767184,

    // shard 2 client
    P_FE2CL_REP_PC_ENTER_SUCC = 822083586,
    P_FE2CL_REP_PC_LOADING_COMPLETE_SUCC = 822083833,
    P_FE2CL_PC_NEW = 822083587,
    P_FE2CL_PC_MOVE = 822083592,
    P_FE2CL_PC_STOP = 822083593,
    P_FE2CL_PC_JUMP = 822083594,
    P_FE2CL_PC_EXIT = 822083590,
    P_FE2CL_PC_MOVEPLATFORM = 822083704,
    P_FE2CL_REP_PC_GOTO_SUCC = 822083633,
    P_FE2CL_GM_REP_PC_SET_VALUE = 822083781,
    P_FE2CL_REP_PC_AVATAR_EMOTES_CHAT = 822083730
};

#define REGISTER_SHARD_PACKET(pactype, handlr) CNShardServer::ShardPackets[pactype] = handlr;

// WARNING: THERE CAN ONLY BE ONE OF THESE SERVERS AT A TIME!!!!!! TODO: change players & packet handlers to be non-static
class CNShardServer : public CNServer {
private:
    static void handlePacket(CNSocket* sock, CNPacketData* data);
public:
    static std::map<uint32_t, PacketHandler> ShardPackets;

    CNShardServer(uint16_t p);

    void killConnection(CNSocket* cns);
};

#endif