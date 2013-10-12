/*
 * Author: Jacky
*/

#ifndef _SOCKET_NAME_H
#define _SOCKET_NAME_H
#include <unistd.h>
#include "transport.h"
#include "dict.h"
int32_t 
client_bind (transport_t *this, struct sockaddr *sockaddr, 
             socklen_t *sockaddr_len, int sock, dict_t *options);

int32_t
socket_client_get_remote_sockaddr (transport_t *this, 
                                   struct sockaddr *sockaddr, 
                                   socklen_t *sockaddr_len,
                                   sa_family_t *sa_family, dict_t *options);

int32_t
socket_server_get_local_sockaddr (transport_t *this, struct sockaddr *addr, 
                                  socklen_t *addr_len, sa_family_t *sa_family, dict_t *options);

int32_t
get_transport_identifiers (transport_t *this);

#endif /* _SOCKET_NAME_H */
