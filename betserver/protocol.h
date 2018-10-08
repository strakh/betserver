//
//  protocol.h
//  betserver
//
//  Created by Stepan Rakhimov on 07/10/2018.
//  Copyright © 2018 Stepan Rakhimov. All rights reserved.
//

#ifndef protocol_h
#define protocol_h

#include <stdio.h>
#include <strings.h>

#define BETSERVER_OPEN 1
#define BETSERVER_ACCEPT 2
#define BETSERVER_BET 3
#define BETSERVER_RESULT 4

struct msg_header {
  unsigned int version;
  unsigned int size;
  unsigned int type;
  unsigned int id;
};

struct msg_header parse_message_header(unsigned int header)
{
  struct msg_header parsed_header;
  bzero(&parsed_header, sizeof(parsed_header));
  unsigned int version_mask = 0x7;
  unsigned int size_mask = 0xF8;
  unsigned int type_mask = 0xFF00;
  unsigned int id_mask = 0xFFFF0000;
  parsed_header.version = header & version_mask;
  parsed_header.size = (header & size_mask) >> 3;
  parsed_header.type = (header & type_mask) >> 8;
  parsed_header.id = (header & id_mask) >> 16;
  return parsed_header;
}

#endif /* protocol_h */
