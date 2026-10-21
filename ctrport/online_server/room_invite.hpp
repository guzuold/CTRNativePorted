// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "friend_identity.hpp"
#include <stdexcept>

namespace ctr_online {
// Copy/paste invitation, not an OS URL handler. The endpoint is only a hint:
// callers must verify its identity before sending the admission capability.
struct RoomInvite { std::string host, token, ip; };
inline RoomInvite parse_room_invite(std::string text) {
    const auto begin=text.find_first_not_of(" \t\r\n");
    if(begin==std::string::npos) throw std::runtime_error("Paste a room invitation");
    text=text.substr(begin,text.find_last_not_of(" \t\r\n")-begin+1);
    if(text.size()>512 || text.rfind("CTRROOM1|",0)!=0)
        throw std::runtime_error("Invalid room invitation format");
    const auto a=text.find('|',9), b=a==std::string::npos ? a : text.find('|',a+1);
    if(a==std::string::npos || b==std::string::npos || text.find('|',b+1)!=std::string::npos)
        throw std::runtime_error("Incomplete room invitation");
    RoomInvite result{text.substr(9,a-9),text.substr(a+1,b-a-1),text.substr(b+1)};
    FriendCodeInfo identity;
    if(parse_friend_code(result.host,identity)!=CodeParseError::Ok || result.token.size()!=64 ||
       result.token.find_first_not_of("0123456789abcdef")!=std::string::npos)
        throw std::runtime_error("Invalid room identity or authorization");
    result.host=identity.code;
    unsigned parts=0, value=0, digits=0;
    for(char ch:result.ip+".") {
        if(ch=='.') {
            if(!digits || value>255 || ++parts>4) throw std::runtime_error("Invalid invitation address");
            value=0;digits=0;
        } else {
            if(ch<'0' || ch>'9' || ++digits>3) throw std::runtime_error("Invalid invitation address");
            value=value*10+static_cast<unsigned>(ch-'0');
        }
    }
    if(parts!=4) throw std::runtime_error("Invalid invitation address");
    return result;
}
inline std::string format_room_invite(const RoomInvite& invite) {
    const auto text="CTRROOM1|"+invite.host+"|"+invite.token+"|"+invite.ip;
    parse_room_invite(text);
    return text;
}
} // namespace ctr_online
