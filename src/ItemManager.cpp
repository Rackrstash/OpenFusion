#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "ItemManager.hpp"
#include "PlayerManager.hpp"
#include "Player.hpp"

#include <string.h> // for memset() and memcmp()
#include <assert.h>

void ItemManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_ITEM_MOVE, itemMoveHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ITEM_DELETE, itemDeleteHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GIVE_ITEM, itemGMGiveHandler);
    //Trade handlers
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_OFFER, itemTradeOfferHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_OFFER_ACCEPT, itemTradeOfferAcceptHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_OFFER_REFUSAL, itemTradeOfferRefusalHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_CONFIRM, itemTradeConfirmHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_CONFIRM_CANCEL, itemTradeConfirmCancelHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_ITEM_REGISTER, itemTradeRegisterItemHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_ITEM_UNREGISTER, itemTradeUnregisterItemHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_CASH_REGISTER, itemTradeRegisterCashHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_EMOTES_CHAT, itemTradeChatHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_ITEM_CHEST_OPEN, chestOpenHandler);
}

void ItemManager::itemMoveHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_ITEM_MOVE))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_ITEM_MOVE* itemmove = (sP_CL2FE_REQ_ITEM_MOVE*)data->buf;
    INITSTRUCT(sP_FE2CL_PC_ITEM_MOVE_SUCC, resp);
    
    PlayerView& plr = PlayerManager::players[sock];
    
    if (plr.plr->Equip[itemmove->iFromSlotNum].iType != 0 && itemmove->eFrom == 0 && itemmove->eTo == 0) {
        // this packet should never happen unless it is a weapon, tell the client to do nothing and do nothing ourself
        resp.eTo = itemmove->eFrom;
        resp.iToSlotNum = itemmove->iFromSlotNum;
        resp.ToSlotItem = plr.plr->Equip[itemmove->iToSlotNum];
        resp.eFrom = itemmove->eTo;
        resp.iFromSlotNum = itemmove->iToSlotNum;
        resp.FromSlotItem = plr.plr->Equip[itemmove->iFromSlotNum];
        
        sock->sendPacket((void*)&resp, P_FE2CL_PC_ITEM_MOVE_SUCC, sizeof(sP_FE2CL_PC_ITEM_MOVE_SUCC));
        return;
    }
    
    if (itemmove->iToSlotNum > AINVEN_COUNT || itemmove->iToSlotNum < 0) 
        return; // sanity checks
    
    sItemBase fromItem;
    sItemBase toItem;

    // eFrom 0 means from equip
    if (itemmove->eFrom == 0) {
        // unequiping an item
        fromItem = plr.plr->Equip[itemmove->iFromSlotNum];
    } else {
        fromItem = plr.plr->Inven[itemmove->iFromSlotNum];
    }

    // eTo 0 means to equip
    if (itemmove->eTo == 0) {
        // equiping an item
        toItem = plr.plr->Equip[itemmove->iToSlotNum];
        plr.plr->Equip[itemmove->iToSlotNum] = fromItem;
    } else {
        toItem = plr.plr->Inven[itemmove->iToSlotNum];
        plr.plr->Inven[itemmove->iToSlotNum] = fromItem;
    }

    if (itemmove->eFrom == 0) {
        plr.plr->Equip[itemmove->iFromSlotNum] = toItem;
    } else {
        plr.plr->Inven[itemmove->iFromSlotNum] = toItem;
    }

    if (itemmove->eFrom == 0 || itemmove->eTo == 0) {
        INITSTRUCT(sP_FE2CL_PC_EQUIP_CHANGE, equipChange);

        equipChange.iPC_ID = plr.plr->iID;
        if (itemmove->eFrom == 0) {
            equipChange.iEquipSlotNum = itemmove->iFromSlotNum;
            equipChange.EquipSlotItem = toItem;
        } else {
            equipChange.iEquipSlotNum = itemmove->iToSlotNum;
            equipChange.EquipSlotItem = fromItem;
        }
        
        // unequip vehicle if equip slot 8 is 0
        if (plr.plr->Equip[8].iID == 0)
            plr.plr->iPCState = 0;

        // send equip event to other players
        for (CNSocket* otherSock : plr.viewable) {
            otherSock->sendPacket((void*)&equipChange, P_FE2CL_PC_EQUIP_CHANGE, sizeof(sP_FE2CL_PC_EQUIP_CHANGE));
        }
    }

    resp.eTo = itemmove->eFrom;
    resp.iToSlotNum = itemmove->iFromSlotNum;
    resp.ToSlotItem = toItem;
    resp.eFrom = itemmove->eTo;
    resp.iFromSlotNum = itemmove->iToSlotNum;
    resp.FromSlotItem = fromItem;

    sock->sendPacket((void*)&resp, P_FE2CL_PC_ITEM_MOVE_SUCC, sizeof(sP_FE2CL_PC_ITEM_MOVE_SUCC));
}

void ItemManager::itemDeleteHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_ITEM_DELETE))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_ITEM_DELETE* itemdel = (sP_CL2FE_REQ_PC_ITEM_DELETE*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_ITEM_DELETE_SUCC, resp);

    PlayerView& plr = PlayerManager::players[sock];

    resp.eIL = itemdel->eIL;
    resp.iSlotNum = itemdel->iSlotNum;

    // so, im not sure what this eIL thing does since you always delete items in inventory and not equips
    plr.plr->Inven[itemdel->iSlotNum].iID = 0;
    plr.plr->Inven[itemdel->iSlotNum].iType = 0;
    plr.plr->Inven[itemdel->iSlotNum].iOpt = 0;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_ITEM_DELETE_SUCC, sizeof(sP_FE2CL_REP_PC_ITEM_DELETE_SUCC));
}

void ItemManager::itemGMGiveHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_GIVE_ITEM))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_GIVE_ITEM* itemreq = (sP_CL2FE_REQ_PC_GIVE_ITEM*)data->buf;
    PlayerView& plr = PlayerManager::players[sock];

    // Commented and disabled for future use
    //if (!plr.plr->IsGM) {
    	// TODO: send fail packet
    //    return;
    //}

    if (itemreq->eIL == 2) {
        // Quest item, not a real item, handle this later, stubbed for now
        // sock->sendPacket(new CNPacketData((void*)resp, P_FE2CL_REP_PC_GIVE_ITEM_FAIL, sizeof(sP_FE2CL_REP_PC_GIVE_ITEM_FAIL), sock->getFEKey()));
    } else if (itemreq->eIL == 1 && itemreq->Item.iType >= 0 && itemreq->Item.iType <= 10) {
        
        INITSTRUCT(sP_FE2CL_REP_PC_GIVE_ITEM_SUCC, resp);

        resp.eIL = itemreq->eIL;
        resp.iSlotNum = itemreq->iSlotNum;
        resp.Item = itemreq->Item;

        plr.plr->Inven[itemreq->iSlotNum] = itemreq->Item;

        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_GIVE_ITEM_SUCC, sizeof(sP_FE2CL_REP_PC_GIVE_ITEM_SUCC));
    }
}

void ItemManager::itemTradeOfferHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_OFFER))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_OFFER* pacdat = (sP_CL2FE_REQ_PC_TRADE_OFFER*)data->buf;
    
    int iID_Check;
    
    if (pacdat->iID_Request == pacdat->iID_From) {
        iID_Check = pacdat->iID_To;
    } else {
        iID_Check = pacdat->iID_From;
    }
    
    CNSocket* otherSock = sock;
    
    for (auto pair : PlayerManager::players) {
        if (pair.second.plr->iID == iID_Check) { 
            otherSock = pair.first;
        }
    }
    
    PlayerView& plr = PlayerManager::players[otherSock];
    
    if (plr.plr->isTrading) {
        INITSTRUCT(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, resp);
       
        resp.iID_Request = pacdat->iID_To;
        resp.iID_From = pacdat->iID_From;
        resp.iID_To = pacdat->iID_To;
       
        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL));
        
        return; //prevent trading with a player already trading
    }
    
    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_OFFER, resp);

    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_OFFER, sizeof(sP_FE2CL_REP_PC_TRADE_OFFER));
}

void ItemManager::itemTradeOfferAcceptHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_OFFER_ACCEPT))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_OFFER_ACCEPT* pacdat = (sP_CL2FE_REQ_PC_TRADE_OFFER_ACCEPT*)data->buf;
    
    int iID_Check;
    
    if (pacdat->iID_Request == pacdat->iID_From) {
        iID_Check = pacdat->iID_To;
    } else {
        iID_Check = pacdat->iID_From;
    }
    
    CNSocket* otherSock = sock;
    
    for (auto pair : PlayerManager::players) {
        if (pair.second.plr->iID == iID_Check) { 
            otherSock = pair.first;
        }
    }
    
    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_OFFER, resp);

    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    
    // Clearing up trade slots
    
    PlayerView& plr = PlayerManager::players[sock];
    PlayerView& plr2 = PlayerManager::players[otherSock];
    
    if (plr2.plr->isTrading) {
        INITSTRUCT(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, resp);
       
        resp.iID_Request = pacdat->iID_To;
        resp.iID_From = pacdat->iID_From;
        resp.iID_To = pacdat->iID_To;
       
        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL));
        
        return; //prevent trading with a player already trading
    }
    
    plr.plr->isTrading = true;
    plr2.plr->isTrading = true;
    plr.plr->isTradeConfirm = false;
    plr2.plr->isTradeConfirm = false;
    
    for (int i = 0; i < 5; i++) {
        plr.plr->Trade[i].iID = 0;
        plr.plr->Trade[i].iType = 0;
        plr.plr->Trade[i].iOpt = 0;
        plr.plr->Trade[i].iInvenNum = 0;
        plr.plr->Trade[i].iSlotNum = 0;
    }
    
    for (int i = 0; i < 5; i++) {
        plr2.plr->Trade[i].iID = 0;
        plr2.plr->Trade[i].iType = 0;
        plr2.plr->Trade[i].iOpt = 0;
        plr2.plr->Trade[i].iInvenNum = 0;
        plr2.plr->Trade[i].iSlotNum = 0;
    }

    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_OFFER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_SUCC));
}

void ItemManager::itemTradeOfferRefusalHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_OFFER_REFUSAL))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_OFFER_REFUSAL* pacdat = (sP_CL2FE_REQ_PC_TRADE_OFFER_REFUSAL*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, resp);

    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    
    int iID_Check;
    
    if (pacdat->iID_Request == pacdat->iID_From) {
        iID_Check = pacdat->iID_To;
    } else {
        iID_Check = pacdat->iID_From;
    }
    
    CNSocket* otherSock = sock;
    
    for (auto pair : PlayerManager::players) {
        if (pair.second.plr->iID == iID_Check) { 
            otherSock = pair.first;
        }
    }

    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL));
}

void ItemManager::itemTradeConfirmHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_CONFIRM))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_CONFIRM* pacdat = (sP_CL2FE_REQ_PC_TRADE_CONFIRM*)data->buf;
    
    int iID_Check;
    
    if (pacdat->iID_Request == pacdat->iID_From) {
        iID_Check = pacdat->iID_To;
    } else {
        iID_Check = pacdat->iID_From;
    }
    
    CNSocket* otherSock = sock;
    
    for (auto pair : PlayerManager::players) {
        if (pair.second.plr->iID == iID_Check) { 
            otherSock = pair.first;
        }
    }
    
    PlayerView& plr = PlayerManager::players[sock];
    PlayerView& plr2 = PlayerManager::players[otherSock];
    
    if (plr2.plr->isTradeConfirm) {
        
        plr.plr->isTrading = false;
        plr2.plr->isTrading = false;
        plr.plr->isTradeConfirm = false;
        plr2.plr->isTradeConfirm = false;
        
        // Check if we have enough free slots
        int freeSlots = 0;
        int freeSlotsNeeded = 0;
        int freeSlots2 = 0;
        int freeSlotsNeeded2 = 0;
        
        for (int i = 0; i < AINVEN_COUNT; i++) {
            if (plr.plr->Inven[i].iID == 0)
                freeSlots++;
        }
        
        for (int i = 0; i < 5; i++) {
            if (plr.plr->Trade[i].iID != 0)
                freeSlotsNeeded++;
        }
        
        for (int i = 0; i < AINVEN_COUNT; i++) {
            if (plr2.plr->Inven[i].iID == 0)
                freeSlots2++;
        }
        
        for (int i = 0; i < 5; i++) {
            if (plr2.plr->Trade[i].iID != 0)
                freeSlotsNeeded2++;
        }
        
        if (freeSlotsNeeded2 - freeSlotsNeeded > freeSlots) {
            INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CONFIRM_ABORT, resp);
            
            resp.iID_Request = pacdat->iID_Request;
            resp.iID_From = pacdat->iID_From;
            resp.iID_To = pacdat->iID_To;
            
            sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM_ABORT, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_ABORT));
            otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM_ABORT, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_ABORT));
            return; // Fail trade because of the lack of slots
        }
        
        if (freeSlotsNeeded - freeSlotsNeeded2 > freeSlots2) {
            INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL, resp);
            
            resp.iID_Request = pacdat->iID_Request;
            resp.iID_From = pacdat->iID_From;
            resp.iID_To = pacdat->iID_To;
            
            sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL));
            otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL));
            return; // Fail trade because of the lack of slots
        }
        
        INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CONFIRM, resp);
        
        resp.iID_Request = pacdat->iID_Request;
        resp.iID_From = pacdat->iID_From;
        resp.iID_To = pacdat->iID_To;
        
        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM));
        // ^^ this is a must have or else the player won't accept a succ packet for some reason
        
        for (int i = 0; i < freeSlotsNeeded; i++) {
            plr.plr->Inven[plr.plr->Trade[i].iInvenNum].iID = 0;
            plr.plr->Inven[plr.plr->Trade[i].iInvenNum].iType = 0;
            plr.plr->Inven[plr.plr->Trade[i].iInvenNum].iOpt = 0;
        }
        
        for (int i = 0; i < freeSlotsNeeded2; i++) {
            plr2.plr->Inven[plr2.plr->Trade[i].iInvenNum].iID = 0;
            plr2.plr->Inven[plr2.plr->Trade[i].iInvenNum].iType = 0;
            plr2.plr->Inven[plr2.plr->Trade[i].iInvenNum].iOpt = 0;
        }
        
        for (int i = 0; i < AINVEN_COUNT; i++) {
            if (freeSlotsNeeded <= 0)
                    break;
                
            if (plr2.plr->Inven[i].iID == 0) {
                
                plr2.plr->Inven[i].iID = plr.plr->Trade[freeSlotsNeeded - 1].iID;
                plr2.plr->Inven[i].iType = plr.plr->Trade[freeSlotsNeeded - 1].iType;
                plr2.plr->Inven[i].iOpt = plr.plr->Trade[freeSlotsNeeded - 1].iOpt;
                plr.plr->Trade[freeSlotsNeeded - 1].iInvenNum = i;
                freeSlotsNeeded--;
            }
        }
        
        for (int i = 0; i < AINVEN_COUNT; i++) {                   
            if (freeSlotsNeeded2 <= 0)
                break;
            
            if (plr.plr->Inven[i].iID == 0) {
                
                plr.plr->Inven[i].iID = plr2.plr->Trade[freeSlotsNeeded2 - 1].iID;
                plr.plr->Inven[i].iType = plr2.plr->Trade[freeSlotsNeeded2 - 1].iType;
                plr.plr->Inven[i].iOpt = plr2.plr->Trade[freeSlotsNeeded2 - 1].iOpt;
                plr2.plr->Trade[freeSlotsNeeded2 - 1].iInvenNum = i;
                freeSlotsNeeded2--;
            }
        }
        
        INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CONFIRM_SUCC, resp2);
        
        resp2.iID_Request = pacdat->iID_Request;
        resp2.iID_From = pacdat->iID_From;
        resp2.iID_To = pacdat->iID_To;
        
        plr.plr->money = plr.plr->money + plr2.plr->moneyInTrade - plr.plr->moneyInTrade;
        resp2.iCandy = plr.plr->money;
        
        memcpy(resp2.Item, plr2.plr->Trade, sizeof(plr2.plr->Trade));
        memcpy(resp2.ItemStay, plr.plr->Trade, sizeof(plr.plr->Trade));
        
        sock->sendPacket((void*)&resp2, P_FE2CL_REP_PC_TRADE_CONFIRM_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_SUCC));
        
        plr2.plr->money = plr2.plr->money + plr.plr->moneyInTrade - plr2.plr->moneyInTrade;
        resp2.iCandy = plr2.plr->money;
        
        memcpy(resp2.Item, plr.plr->Trade, sizeof(plr.plr->Trade));
        memcpy(resp2.ItemStay, plr2.plr->Trade, sizeof(plr2.plr->Trade));
        
        otherSock->sendPacket((void*)&resp2, P_FE2CL_REP_PC_TRADE_CONFIRM_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_SUCC));
    } else {
        
        INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CONFIRM, resp);

        resp.iID_Request = pacdat->iID_Request;
        resp.iID_From = pacdat->iID_From;
        resp.iID_To = pacdat->iID_To;
        
        plr.plr->isTradeConfirm = true;
        
        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM));
        otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM));
    }
}

void ItemManager::itemTradeConfirmCancelHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_CONFIRM_CANCEL))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_CONFIRM_CANCEL* pacdat = (sP_CL2FE_REQ_PC_TRADE_CONFIRM_CANCEL*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL, resp);

    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    
    int iID_Check;
    
    if (pacdat->iID_Request == pacdat->iID_From) {
        iID_Check = pacdat->iID_To;
    } else {
        iID_Check = pacdat->iID_From;
    }
    
    CNSocket* otherSock = sock;
    
    for (auto pair : PlayerManager::players) {
        if (pair.second.plr->iID == iID_Check) { 
            otherSock = pair.first;
        }
    }
    
    PlayerView& plr = PlayerManager::players[sock];
    PlayerView& plr2 = PlayerManager::players[otherSock];
    
    plr.plr->isTrading = false;
    plr.plr->isTradeConfirm = false;
    plr2.plr->isTrading = false;
    plr2.plr->isTradeConfirm = false;

    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL));
}

void ItemManager::itemTradeRegisterItemHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_ITEM_REGISTER))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_ITEM_REGISTER* pacdat = (sP_CL2FE_REQ_PC_TRADE_ITEM_REGISTER*)data->buf;
    
    if (pacdat->Item.iSlotNum < 0 || pacdat->Item.iSlotNum > 4)
        return; // sanity check
    
    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC, resp);

    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    resp.TradeItem = pacdat->Item;
    resp.InvenItem = pacdat->Item;
    
    int iID_Check;
    
    if (pacdat->iID_Request == pacdat->iID_From) {
        iID_Check = pacdat->iID_To;
    } else {
        iID_Check = pacdat->iID_From;
    }
    
    CNSocket* otherSock = sock;
    
    for (auto pair : PlayerManager::players) {
        if (pair.second.plr->iID == iID_Check) { 
            otherSock = pair.first;
        }
    }
    
    PlayerView& plr = PlayerManager::players[sock];
    plr.plr->Trade[pacdat->Item.iSlotNum] = pacdat->Item;
    plr.plr->isTradeConfirm = false;
    
    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC));
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC));
}

void ItemManager::itemTradeUnregisterItemHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_ITEM_UNREGISTER))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_ITEM_UNREGISTER* pacdat = (sP_CL2FE_REQ_PC_TRADE_ITEM_UNREGISTER*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC, resp);

    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    resp.TradeItem = pacdat->Item;
    
    PlayerView& plr = PlayerManager::players[sock];
    resp.InvenItem = plr.plr->Trade[pacdat->Item.iSlotNum];
    plr.plr->isTradeConfirm = false;
    
    int iID_Check;
    
    if (pacdat->iID_Request == pacdat->iID_From) {
        iID_Check = pacdat->iID_To;
    } else {
        iID_Check = pacdat->iID_From;
    }
    
    CNSocket* otherSock = sock;
    
    for (auto pair : PlayerManager::players) {
        if (pair.second.plr->iID == iID_Check) { 
            otherSock = pair.first;
        }
    }
    
    int temp_num = pacdat->Item.iSlotNum;
    
    if (temp_num >= 0 && temp_num <= 4) {
        plr.plr->Trade[temp_num].iID = 0;
        plr.plr->Trade[temp_num].iType = 0;
        plr.plr->Trade[temp_num].iOpt = 0;
        plr.plr->Trade[temp_num].iInvenNum = 0;
        plr.plr->Trade[temp_num].iSlotNum = 0;
    }
    
    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC));
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC));
}

void ItemManager::itemTradeRegisterCashHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_CASH_REGISTER))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_CASH_REGISTER* pacdat = (sP_CL2FE_REQ_PC_TRADE_CASH_REGISTER*)data->buf;
    
    PlayerView& plr = PlayerManager::players[sock];
    
    if (pacdat->iCandy < 0 || pacdat->iCandy > plr.plr->money)
       return; // famous glitch, begone
    
    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC, resp);

    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    resp.iCandy = pacdat->iCandy;
    
    int iID_Check;
    
    if (pacdat->iID_Request == pacdat->iID_From) {
        iID_Check = pacdat->iID_To;
    } else {
        iID_Check = pacdat->iID_From;
    }
    
    CNSocket* otherSock = sock;
    
    for (auto pair : PlayerManager::players) {
        if (pair.second.plr->iID == iID_Check) { 
            otherSock = pair.first;
        }
    }
    
    plr.plr->moneyInTrade = pacdat->iCandy;
    plr.plr->isTradeConfirm = false;
    
    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC));
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC));
}

void ItemManager::itemTradeChatHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_EMOTES_CHAT))
        return; // malformed packet
    sP_CL2FE_REQ_PC_TRADE_EMOTES_CHAT* pacdat = (sP_CL2FE_REQ_PC_TRADE_EMOTES_CHAT*)data->buf;
    
    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_EMOTES_CHAT, resp);
    
    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    
    memcpy(resp.szFreeChat, pacdat->szFreeChat, sizeof(pacdat->szFreeChat));
    
    resp.iEmoteCode = pacdat->iEmoteCode;
    
    int iID_Check;
    
    if (pacdat->iID_Request == pacdat->iID_From) {
        iID_Check = pacdat->iID_To;
    } else {
        iID_Check = pacdat->iID_From;
    }
    
    CNSocket* otherSock = sock;
    
    for (auto pair : PlayerManager::players) {
        if (pair.second.plr->iID == iID_Check) { 
            otherSock = pair.first;
        }
    }
    
    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_EMOTES_CHAT, sizeof(sP_FE2CL_REP_PC_TRADE_EMOTES_CHAT));
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_EMOTES_CHAT, sizeof(sP_FE2CL_REP_PC_TRADE_EMOTES_CHAT));
}

void ItemManager::chestOpenHandler(CNSocket *sock, CNPacketData *data) {
    if (data->size != sizeof(sP_CL2FE_REQ_ITEM_CHEST_OPEN))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_ITEM_CHEST_OPEN *pkt = (sP_CL2FE_REQ_ITEM_CHEST_OPEN *)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

    // item giving packet
    const size_t resplen = sizeof(sP_FE2CL_REP_REWARD_ITEM) + sizeof(sItemReward);
    assert(resplen < CN_PACKET_BUFFER_SIZE);
    // we know it's only one trailing struct, so we can skip full validation

    uint8_t respbuf[resplen]; // not a variable length array, don't worry
    sP_FE2CL_REP_REWARD_ITEM *reward = (sP_FE2CL_REP_REWARD_ITEM *)respbuf;
    sItemReward *item = (sItemReward *)(respbuf + sizeof(sP_FE2CL_REP_REWARD_ITEM));

    // don't forget to zero the buffer!
    memset(respbuf, 0, resplen);

    // simple rewards
    reward->iFatigue = 100; // prevents warning message
    reward->iFatigue_Level = 1;
    reward->iItemCnt = 1; // remember to update resplen if you change this

    // item reward
    item->sItem.iType = 0;
    item->sItem.iID = 96;
    item->iSlotNum = pkt->iSlotNum;
    item->eIL = pkt->eIL;

    // update player
    plr->Inven[pkt->iSlotNum] = item->sItem;

    // transmit item
    sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, resplen);

    // chest opening acknowledgement packet
    INITSTRUCT(sP_FE2CL_REP_ITEM_CHEST_OPEN_SUCC, resp);

    resp.iSlotNum = pkt->iSlotNum;

    std::cout << "opening chest..." << std::endl;
    sock->sendPacket((void*)&resp, P_FE2CL_REP_ITEM_CHEST_OPEN_SUCC, sizeof(sP_FE2CL_REP_ITEM_CHEST_OPEN_SUCC));
}

// TODO: use this in cleaned up ItemManager
int ItemManager::findFreeSlot(Player *plr) {
    int i;
    sItemBase free;

    memset((void*)&free, 0, sizeof(sItemBase));

    for (i = 0; i < AINVEN_COUNT; i++)
        if (memcmp((void*)&plr->Inven[i], (void*)&free, sizeof(sItemBase)) == 0)
            return i;

    // not found
    return -1;
}
