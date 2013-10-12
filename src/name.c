/*
 * Author: Jacky
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include "name.h"
#include "transport.h"
#include "socket.h"
#include "dict.h"

#ifdef CLIENT_PORT_CEILING
#undef CLIENT_PORT_CEILING
#endif

#define CLIENT_PORT_CEILING 1024

#ifndef AF_INET_SDP
#define AF_INET_SDP 27
#endif

int32_t
lz_resolve_ip6 (const char *hostname, 
                uint16_t port, 
                int family, 
                void **dnscache, 
                struct addrinfo **addr_info);

static int32_t
af_inet_bind_to_port_lt_ceiling (int fd, struct sockaddr *sockaddr, 
                                 socklen_t sockaddr_len, int ceiling)
{
        int32_t ret = -1;
        /*  struct sockaddr_in sin = {0, }; */
        uint16_t port = ceiling - 1;

        while (port)
        {
                switch (sockaddr->sa_family)
                {
                case AF_INET6:
                        ((struct sockaddr_in6 *)sockaddr)->sin6_port = htons (port);
                        break;

                case AF_INET_SDP:
                case AF_INET:
                        ((struct sockaddr_in *)sockaddr)->sin_port = htons (port);
                        break;
                }

                ret = bind (fd, sockaddr, sockaddr_len);

                if (ret == 0)
                        break;

                if (ret == -1 && errno == EACCES)
                        break;

                port--;
        }

        return ret;
}

static int32_t
af_unix_client_bind (transport_t *this, 
                     struct sockaddr *sockaddr, 
                     socklen_t sockaddr_len, 
                     int sock, dict_t *options)
{
        data_t *path_data = NULL;
        struct sockaddr_un *addr = NULL;
        int32_t ret = 0;

        path_data = dict_get (options, "transport.socket.bind-path");
        if (path_data) {
                char *path = data_to_str (path_data);
                if (!path || strlen (path) > UNIX_PATH_MAX) {
                        lz_log (__func__, LZ_LOG_TRACE,
                                "bind-path not specfied for unix socket, "
                                "letting connect to assign default value");
                        goto err;
                }

                addr = (struct sockaddr_un *) sockaddr;
                strcpy (addr->sun_path, path);
                ret = bind (sock, (struct sockaddr *)addr, sockaddr_len);
                if (ret == -1) {
                        lz_log (__func__, LZ_LOG_ERROR,
                                "cannot bind to unix-domain socket %d (%s)", 
                                sock, strerror (errno));
                        goto err;
                }
        } else {
                lz_log (__func__, LZ_LOG_TRACE,
                        "bind-path not specfied for unix socket, "
                        "letting connect to assign default value");
        }

err:
        return ret;
}

int32_t
client_fill_address_family (transport_t *this, sa_family_t *sa_family, dict_t *options)
{
        data_t  *address_family_data = NULL;
        int32_t  ret                 = -1;

        if (sa_family == NULL) {
                goto out;
        }

        address_family_data = dict_get (options, "transport.address-family");
        if (!address_family_data) {
                data_t *remote_host_data = NULL, *connect_path_data = NULL;
                remote_host_data = dict_get (options, "remote-host");
                connect_path_data = dict_get (options, 
                                              "transport.socket.connect-path");

                if (!(remote_host_data || connect_path_data) || 
                    (remote_host_data && connect_path_data)) {
                        lz_log (__func__, LZ_LOG_ERROR,
                                "transport.address-family not specified and "
                                "not able to determine the "
                                "same from other options (remote-host:%s and "
                                "transport.unix.connect-path:%s)", 
                                data_to_str (remote_host_data), 
                                data_to_str (connect_path_data));
                        goto out;
                } 

                if (remote_host_data) {
                        lz_log (__func__, LZ_LOG_DEBUG,
                                "address-family not specified, guessing it "
                                "to be inet/inet6");
                        *sa_family = AF_UNSPEC;
                } else {
                        lz_log (__func__, LZ_LOG_DEBUG,
                                "address-family not specified, guessing it "
                                "to be unix");
                        *sa_family = AF_UNIX;
                }

        } else {
                char *address_family = data_to_str (address_family_data);
                if (!strcasecmp (address_family, "unix")) {
                        *sa_family = AF_UNIX;
                } else if (!strcasecmp (address_family, "inet")) {
                        *sa_family = AF_INET;
                } else if (!strcasecmp (address_family, "inet6")) {
                        *sa_family = AF_INET6;
                } else if (!strcasecmp (address_family, "inet-sdp")) {
                        *sa_family = AF_INET_SDP;
                } else if (!strcasecmp (address_family, "inet/inet6")
                           || !strcasecmp (address_family, "inet6/inet")) {
                        *sa_family = AF_UNSPEC;
                } else {
                        lz_log (__func__, LZ_LOG_ERROR,
                                "unknown address-family (%s) specified", 
                                address_family);
                        goto out;
                }
        }

        ret = 0;

out:
        return ret;
}

static int32_t
af_inet_client_get_remote_sockaddr (transport_t *this, 
                                    struct sockaddr *sockaddr, 
                                    socklen_t *sockaddr_len, dict_t *options)
{
        data_t *remote_host_data = NULL;
        data_t *remote_port_data = NULL;
        char *remote_host = NULL;
        uint16_t remote_port = 0;
        struct addrinfo *addr_info = NULL;
        int32_t ret = 0;

        remote_host_data = dict_get (options, "remote-host");
        if (remote_host_data == NULL)
        {
                lz_log (__func__, LZ_LOG_ERROR,
                        "option remote-host missing");
                ret = -1;
                goto err;
        }

        remote_host = data_to_str (remote_host_data);
        if (remote_host == NULL)
        {
                lz_log (__func__, LZ_LOG_ERROR,
                        "option remote-host has data NULL");
                ret = -1;
                goto err;
        }

        remote_port_data = dict_get (options, "remote-port");
        if (remote_port_data == NULL)
        {
                lz_log (__func__, LZ_LOG_TRACE,
                        "option remote-port missing. Defaulting to %d",
                        DEFAULT_SOCKET_LISTEN_PORT);

                remote_port = DEFAULT_SOCKET_LISTEN_PORT;
        }
        else
        {
                remote_port = data_to_uint16 (remote_port_data);
        }

        if (remote_port == (uint16_t)-1)
        {
                lz_log (__func__, LZ_LOG_ERROR,
                        "option remote-port has invalid port in volume %s",
                        __func__);
                ret = -1;
                goto err;
        }

        /* TODO: gf_resolve is a blocking call. kick in some
           non blocking dns techniques */
        ret = gf_resolve_ip6 (remote_host, remote_port,
                              sockaddr->sa_family, &this->dnscache, &addr_info);
        if (ret == -1) {
                lz_log (__func__, LZ_LOG_ERROR,
                        "DNS resolution failed on host %s", remote_host);
                goto err;
        }

        memcpy (sockaddr, addr_info->ai_addr, addr_info->ai_addrlen);
        *sockaddr_len = addr_info->ai_addrlen;

err:
        return ret;
}

static int32_t
af_unix_client_get_remote_sockaddr (transport_t *this, 
                                    struct sockaddr *sockaddr, 
                                    socklen_t *sockaddr_len, dict_t *options)
{
        struct sockaddr_un *sockaddr_un = NULL;
        char *connect_path = NULL;
        data_t *connect_path_data = NULL;
        int32_t ret = 0;

        connect_path_data = dict_get (options, 
                                      "transport.socket.connect-path");
        if (!connect_path_data) {
                lz_log (__func__, LZ_LOG_ERROR,
                        "option transport.unix.connect-path not specified for "
                        "address-family unix");
                ret = -1;
                goto err;
        }

        connect_path = data_to_str (connect_path_data);
        if (!connect_path) {
                lz_log (__func__, LZ_LOG_ERROR,
                        "transport.unix.connect-path is null-string");
                ret = -1;
                goto err;
        }

        if (strlen (connect_path) > UNIX_PATH_MAX) {
                lz_log (__func__, LZ_LOG_ERROR,
                        "connect-path value length %"LZ_PRI_SIZET" > %d octets", 
                        strlen (connect_path), UNIX_PATH_MAX);
                ret = -1;
                goto err;
        }

        lz_log (__func__, LZ_LOG_TRACE,
                "using connect-path %s", connect_path);
        sockaddr_un = (struct sockaddr_un *)sockaddr;
        strcpy (sockaddr_un->sun_path, connect_path);
        *sockaddr_len = sizeof (struct sockaddr_un);

err:
        return ret;
}

static int32_t
af_unix_server_get_local_sockaddr (transport_t *this,
                                   struct sockaddr *addr,
                                   socklen_t *addr_len, dict_t *options)
{
        data_t *listen_path_data = NULL;
        char *listen_path = NULL;
        int32_t ret = 0;
        struct sockaddr_un *sunaddr = (struct sockaddr_un *)addr;


        listen_path_data = dict_get (options, 
                                     "transport.socket.listen-path");
        if (!listen_path_data) {
                lz_log (__func__, LZ_LOG_ERROR,
                        "missing option transport.socket.listen-path");
                ret = -1;
                goto err;
        }

        listen_path = data_to_str (listen_path_data);

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

        if (strlen (listen_path) > UNIX_PATH_MAX) {
                lz_log (__func__, LZ_LOG_ERROR,
                        "option transport.unix.listen-path has value length "
                        "%"LZ_PRI_SIZET" > %d",
                        strlen (listen_path), UNIX_PATH_MAX);
                ret = -1;
                goto err;
        }

        sunaddr->sun_family = AF_UNIX;
        strcpy (sunaddr->sun_path, listen_path);
        *addr_len = sizeof (struct sockaddr_un);

err:
        return ret;
}

static int32_t 
af_inet_server_get_local_sockaddr (transport_t *this, 
                                   struct sockaddr *addr, 
                                   socklen_t *addr_len, dict_t *options)
{
        struct addrinfo hints, *res = 0;
        data_t *listen_port_data = NULL, *listen_host_data = NULL;
        uint16_t listen_port = -1;
        char service[NI_MAXSERV], *listen_host = NULL;
        int32_t ret = 0;

        listen_port_data = dict_get (options, "transport.socket.listen-port");
        listen_host_data = dict_get (options, "transport.socket.bind-address");

        if (listen_port_data)
        {
                listen_port = data_to_uint16 (listen_port_data);
        }

        if (listen_port == (uint16_t) -1)
                listen_port = DEFAULT_SOCKET_LISTEN_PORT;


        if (listen_host_data)
        {
                listen_host = data_to_str (listen_host_data);
        } else {
		if (addr->sa_family == AF_INET6) {
			struct sockaddr_in6 *in = (struct sockaddr_in6 *) addr;
			in->sin6_addr = in6addr_any;
			in->sin6_port = htons(listen_port);
			*addr_len = sizeof(struct sockaddr_in6);
                        goto out;
		} else if (addr->sa_family == AF_INET) {
			struct sockaddr_in *in = (struct sockaddr_in *) addr;
			in->sin_addr.s_addr = htonl(INADDR_ANY);
			in->sin_port = htons(listen_port);
			*addr_len = sizeof(struct sockaddr_in);
			goto out;
		}
	}

        memset (service, 0, sizeof (service));
        sprintf (service, "%d", listen_port);

        memset (&hints, 0, sizeof (hints));
        hints.ai_family = addr->sa_family;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags    = AI_ADDRCONFIG | AI_PASSIVE;

        ret = getaddrinfo(listen_host, service, &hints, &res);
        if (ret != 0) {
                lz_log (__func__, LZ_LOG_ERROR,
                        "getaddrinfo failed for host %s, service %s (%s)", 
                        listen_host, service, gai_strerror (ret));
                ret = -1;
                goto out;
        }

        memcpy (addr, res->ai_addr, res->ai_addrlen);
        *addr_len = res->ai_addrlen;

        freeaddrinfo (res);

out:
        return ret;
}

int32_t 
client_bind (transport_t *this, 
             struct sockaddr *sockaddr, 
             socklen_t *sockaddr_len, 
             int sock, dict_t *options)
{
        int ret = 0;

        *sockaddr_len = sizeof (struct sockaddr_in6);
        switch (sockaddr->sa_family)
        {
        case AF_INET_SDP:
        case AF_INET:
                *sockaddr_len = sizeof (struct sockaddr_in);

        case AF_INET6:
                ret = af_inet_bind_to_port_lt_ceiling (sock, sockaddr, 
                                                       *sockaddr_len, CLIENT_PORT_CEILING);
                if (ret == -1) {
                        lz_log (__func__, LZ_LOG_WARNING,
                                "cannot bind inet socket (%d) to port less than %d (%s)", 
                                sock, CLIENT_PORT_CEILING, strerror (errno));
                        ret = 0;
                }
                break;

        case AF_UNIX:
                *sockaddr_len = sizeof (struct sockaddr_un);
                ret = af_unix_client_bind (this, (struct sockaddr *)sockaddr, 
                                           *sockaddr_len, sock, options);
                break;

        default:
                lz_log (__func__, LZ_LOG_ERROR,
                        "unknown address family %d", sockaddr->sa_family);
                ret = -1;
                break;
        }

        return ret;
}

int32_t
socket_client_get_remote_sockaddr (transport_t *this,
                                   struct sockaddr *sockaddr, 
                                   socklen_t *sockaddr_len,
                                   sa_family_t *sa_family, dict_t *options)
{
        int32_t ret = 0;

        if ((sockaddr == NULL) || (sockaddr_len == NULL)
            || (sa_family == NULL)) {
                ret = -1;
                goto err;
        }


        ret = client_fill_address_family (this, &sockaddr->sa_family, options);
        if (ret) {
                ret = -1;
                goto err;
        }
 
        *sa_family = sockaddr->sa_family;

        switch (sockaddr->sa_family)
        {
        case AF_INET_SDP:
                sockaddr->sa_family = AF_INET;

        case AF_INET:
        case AF_INET6:
        case AF_UNSPEC:
                ret = af_inet_client_get_remote_sockaddr (this, sockaddr,
                                                          sockaddr_len, options);
                break;

        case AF_UNIX:
                ret = af_unix_client_get_remote_sockaddr (this, sockaddr,
                                                          sockaddr_len, options);
                break;

        default:
                lz_log (__func__, LZ_LOG_ERROR,
                        "unknown address-family %d", sockaddr->sa_family);
                ret = -1;
        }
  
        if (*sa_family == AF_UNSPEC) {
                *sa_family = sockaddr->sa_family;
        }

err:
        return ret;
}


int32_t
server_fill_address_family (transport_t *this, sa_family_t *sa_family, dict_t *options)
{
        data_t  *address_family_data = NULL;
        int32_t  ret                 = -1;
        
        if (sa_family == NULL) {
                goto out;
        }

        address_family_data = dict_get (options, 
                                        "transport.address-family");
        if (address_family_data) {
                char *address_family = NULL;
                address_family = data_to_str (address_family_data);

                if (!strcasecmp (address_family, "inet")) {
                        *sa_family = AF_INET;
                } else if (!strcasecmp (address_family, "inet6")) {
                        *sa_family = AF_INET6;
                } else if (!strcasecmp (address_family, "inet-sdp")) {
                        *sa_family = AF_INET_SDP;
                } else if (!strcasecmp (address_family, "unix")) {
                        *sa_family = AF_UNIX;
                } else if (!strcasecmp (address_family, "inet/inet6")
                           || !strcasecmp (address_family, "inet6/inet")) {
                        *sa_family = AF_UNSPEC;
                } else {
                        lz_log (__func__, LZ_LOG_ERROR,
                                "unknown address family (%s) specified", address_family);
                        goto out;
                }
        } else {
                lz_log (__func__, LZ_LOG_DEBUG,
                        "option address-family not specified, defaulting to inet/inet6");
                *sa_family = AF_UNSPEC;
        }

        ret = 0;
out:
        return ret;
}


int32_t
socket_server_get_local_sockaddr (transport_t *this, struct sockaddr *addr, 
                                  socklen_t *addr_len, sa_family_t *sa_family, dict_t *options)
{
        int32_t ret = -1;

        if ((addr == NULL) || (addr_len == NULL) || (sa_family == NULL)) {
                goto err;
        }

        ret = server_fill_address_family (this, &addr->sa_family, options);
        if (ret == -1) {
                goto err;
        }

        *sa_family = addr->sa_family;

        switch (addr->sa_family)
        {
        case AF_INET_SDP:
                addr->sa_family = AF_INET;

        case AF_INET:
        case AF_INET6:
        case AF_UNSPEC:
                ret = af_inet_server_get_local_sockaddr (this, addr, addr_len, options);
                break;

        case AF_UNIX:
                ret = af_unix_server_get_local_sockaddr (this, addr, addr_len, options);
                break;
        }

        if (*sa_family == AF_UNSPEC) {
                *sa_family = addr->sa_family;
        }

err:
        return ret;
}

int32_t 
fill_inet6_inet_identifiers (transport_t *this, struct sockaddr_storage *addr, 
                             int32_t addr_len, char *identifier)
{
        int32_t ret = 0, tmpaddr_len = 0;
        char service[NI_MAXSERV], host[NI_MAXHOST];
        struct sockaddr_storage tmpaddr;

        memset (&tmpaddr, 0, sizeof (tmpaddr));
        tmpaddr = *addr;
        tmpaddr_len = addr_len;

        if (((struct sockaddr *) &tmpaddr)->sa_family == AF_INET6) {
                int32_t one_to_four, four_to_eight, twelve_to_sixteen;
                int16_t eight_to_ten, ten_to_twelve;
    
                one_to_four = four_to_eight = twelve_to_sixteen = 0;
                eight_to_ten = ten_to_twelve = 0;
    
                one_to_four = ((struct sockaddr_in6 *) &tmpaddr)->sin6_addr.s6_addr32[0];
                four_to_eight = ((struct sockaddr_in6 *) &tmpaddr)->sin6_addr.s6_addr32[1];
#ifdef LZ_SOLARIS_HOST_OS
                eight_to_ten = S6_ADDR16(((struct sockaddr_in6 *) &tmpaddr)->sin6_addr)[4];
#else
                eight_to_ten = ((struct sockaddr_in6 *) &tmpaddr)->sin6_addr.s6_addr16[4];
#endif

#ifdef LZ_SOLARIS_HOST_OS
                ten_to_twelve = S6_ADDR16(((struct sockaddr_in6 *) &tmpaddr)->sin6_addr)[5];
#else
                ten_to_twelve = ((struct sockaddr_in6 *) &tmpaddr)->sin6_addr.s6_addr16[5];
#endif

                twelve_to_sixteen = ((struct sockaddr_in6 *) &tmpaddr)->sin6_addr.s6_addr32[3];

                /* ipv4 mapped ipv6 address has
                   bits 0-80: 0
                   bits 80-96: 0xffff
                   bits 96-128: ipv4 address 
                */
 
                if (one_to_four == 0 &&
                    four_to_eight == 0 &&
                    eight_to_ten == 0 &&
                    ten_to_twelve == -1) {
                        struct sockaddr_in *in_ptr = (struct sockaddr_in *)&tmpaddr;
                        memset (&tmpaddr, 0, sizeof (tmpaddr));
      
                        in_ptr->sin_family = AF_INET;
                        in_ptr->sin_port = ((struct sockaddr_in6 *)addr)->sin6_port;
                        in_ptr->sin_addr.s_addr = twelve_to_sixteen;
                        tmpaddr_len = sizeof (*in_ptr);
                }
        }

        ret = getnameinfo ((struct sockaddr *) &tmpaddr,
                           tmpaddr_len,
                           host, sizeof (host),
                           service, sizeof (service),
                           NI_NUMERICHOST | NI_NUMERICSERV);
        if (ret != 0) {
                lz_log (__func__, LZ_LOG_ERROR,
                        "getnameinfo failed (%s)", gai_strerror (ret));
        }

        sprintf (identifier, "%s:%s", host, service);

        return ret;
}

int32_t
get_transport_identifiers (transport_t *this)
{
        int32_t ret = 0;
        char is_inet_sdp = 0;

        switch (((struct sockaddr *) &this->myinfo.sockaddr)->sa_family)
        {
        case AF_INET_SDP:
                is_inet_sdp = 1;
                ((struct sockaddr *) &this->peerinfo.sockaddr)->sa_family = ((struct sockaddr *) &this->myinfo.sockaddr)->sa_family = AF_INET;

        case AF_INET:
        case AF_INET6:
        {
                ret = fill_inet6_inet_identifiers (this, 
                                                   &this->myinfo.sockaddr, 
                                                   this->myinfo.sockaddr_len,
                                                   this->myinfo.identifier);
                if (ret == -1) {
                        lz_log (__func__, LZ_LOG_ERROR,
                                "cannot fill inet/inet6 identifier for server");
                        goto err;
                }

                ret = fill_inet6_inet_identifiers (this,
                                                   &this->peerinfo.sockaddr,
                                                   this->peerinfo.sockaddr_len,
                                                   this->peerinfo.identifier);
                if (ret == -1) {
                        lz_log (__func__, LZ_LOG_ERROR,
                                "cannot fill inet/inet6 identifier for client");
                        goto err;
                }

                if (is_inet_sdp) {
                        ((struct sockaddr *) &this->peerinfo.sockaddr)->sa_family = ((struct sockaddr *) &this->myinfo.sockaddr)->sa_family = AF_INET_SDP;
                }
        }
        break;

        case AF_UNIX:
        {
                struct sockaddr_un *sunaddr = NULL;

                sunaddr = (struct sockaddr_un *) &this->myinfo.sockaddr;
                strcpy (this->myinfo.identifier, sunaddr->sun_path);

                sunaddr = (struct sockaddr_un *) &this->peerinfo.sockaddr;
                strcpy (this->peerinfo.identifier, sunaddr->sun_path);
        }
        break;

        default:
                lz_log (__func__, LZ_LOG_ERROR,
                        "unknown address family (%d)", 
                        ((struct sockaddr *) &this->myinfo.sockaddr)->sa_family);
                ret = -1;
                break;
        }

err:
        return ret;
}
