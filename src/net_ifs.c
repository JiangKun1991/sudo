/*
 * SPDX-License-Identifier: ISC
 *
 * Copyright (c) 1996, 1998-2005, 2007-2015, 2018-2021
 *	Todd C. Miller <Todd.Miller@sudo.ws>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

/*
 * This is an open source non-commercial project. Dear PVS-Studio, please check it.
 * PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
 */

/*
 * Suppress a warning w/ gcc on Digital UN*X.
 * The system headers should really do this....
 */
#if defined(__osf__) && !defined(__cplusplus)
struct mbuf;
struct rtentry;
#endif

/* Avoid a compilation problem with gcc and machine/sys/getppdp.h */
#define _MACHINE_SYS_GETPPDP_INCLUDED

#include <config.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#if defined(HAVE_SYS_SOCKIO_H) && !defined(SIOCGIFCONF)
# include <sys/sockio.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# include "compat/stdbool.h"
#endif /* HAVE_STDBOOL_H */
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#ifdef _ISC
# include <sys/stream.h>
# include <sys/sioctl.h>
# include <sys/stropts.h>
# define STRSET(cmd, param, len) {strioctl.ic_cmd=(cmd);\
				 strioctl.ic_dp=(param);\
				 strioctl.ic_timout=0;\
				 strioctl.ic_len=(len);}
#endif /* _ISC */
#ifdef _MIPS
# include <net/soioctl.h>
#endif /* _MIPS */
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef NEED_RESOLV_H
# include <arpa/nameser.h>
# include <resolv.h>
#endif /* NEED_RESOLV_H */
#include <net/if.h>
#ifdef HAVE_GETIFADDRS
# include <ifaddrs.h>
#endif

#define NEED_INET_NTOP		/* to expose sudo_inet_ntop in sudo_compat.h */

#define DEFAULT_TEXT_DOMAIN	"sudo"

#include "sudo_compat.h"
#include "sudo_conf.h"
#include "sudo_debug.h"
#include "sudo_fatal.h"
#include "sudo_gettext.h"

/* Minix apparently lacks IFF_LOOPBACK */
#ifndef IFF_LOOPBACK
# define IFF_LOOPBACK	0
#endif

#ifndef INET_ADDRSTRLEN
# define INET_ADDRSTRLEN 16
#endif
#ifndef INET6_ADDRSTRLEN
# define INET6_ADDRSTRLEN 46
#endif

/* HP-UX SIOCGLIFCONF is not compatible (and may only include IPv6?). */
#ifdef __hpux
# undef SIOCGLIFCONF
#endif

#ifdef HAVE_GETIFADDRS

/*
 * Fill in the interfaces string with the machine's ip addresses and netmasks
 * and return the number of interfaces found.  Returns -1 on error.
 */
int
get_net_ifs(char **addrinfo)
{
    struct ifaddrs *ifa, *ifaddrs;
    struct sockaddr_in *sin;
# ifdef HAVE_STRUCT_IN6_ADDR
    struct sockaddr_in6 *sin6;
# endif
    char addrstr[INET6_ADDRSTRLEN], maskstr[INET6_ADDRSTRLEN];
    int len, num_interfaces = 0;
    size_t ailen;
    char *cp;
    debug_decl(get_net_ifs, SUDO_DEBUG_NETIF);

    if (!sudo_conf_probe_interfaces())
	debug_return_int(0);

    if (getifaddrs(&ifaddrs) == -1)
	debug_return_int(-1);

    /* Allocate space for the interfaces info string. */
    for (ifa = ifaddrs; ifa != NULL; ifa = ifa -> ifa_next) {
	/* Skip interfaces marked "down" and "loopback". */
	if (ifa->ifa_addr == NULL || ifa->ifa_netmask == NULL ||
	    !ISSET(ifa->ifa_flags, IFF_UP) || ISSET(ifa->ifa_flags, IFF_LOOPBACK))
	    continue;

	switch (ifa->ifa_addr->sa_family) {
	    case AF_INET:
# ifdef HAVE_STRUCT_IN6_ADDR
	    case AF_INET6:
# endif
		num_interfaces++;
		break;
	}
    }
    if (num_interfaces == 0)
	goto done;
    ailen = num_interfaces * 2 * INET6_ADDRSTRLEN;
    if ((cp = malloc(ailen)) == NULL) {
	sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
	    "unable to allocate memory");
	num_interfaces = -1;
	goto done;
    }
    *addrinfo = cp;

    for (ifa = ifaddrs; ifa != NULL; ifa = ifa -> ifa_next) {
	/* Skip interfaces marked "down" and "loopback". */
	if (!ISSET(ifa->ifa_flags, IFF_UP) || ISSET(ifa->ifa_flags, IFF_LOOPBACK))
		continue;

	switch (ifa->ifa_addr->sa_family) {
	case AF_INET:
	    sin = (struct sockaddr_in *)ifa->ifa_addr;
	    if (sin->sin_addr.s_addr == INADDR_ANY || sin->sin_addr.s_addr == INADDR_NONE) {
		sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
		    "ignoring unspecified AF_INET addr for %s", ifa->ifa_name);
		continue;
	    }
	    if (inet_ntop(AF_INET, &sin->sin_addr, addrstr, sizeof(addrstr)) == NULL) {
		sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
		    "ignoring bad AF_INET addr for %s", ifa->ifa_name);
		continue;
	    }
	    sin = (struct sockaddr_in *)ifa->ifa_netmask;
	    if (inet_ntop(AF_INET, &sin->sin_addr, maskstr, sizeof(maskstr)) == NULL) {
		sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
		    "ignoring bad AF_INET mask for %s", ifa->ifa_name);
		continue;
	    }
	    break;
# ifdef HAVE_STRUCT_IN6_ADDR
	case AF_INET6:
	    sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
	    if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
		sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
		    "ignoring unspecified AF_INET6 addr for %s", ifa->ifa_name);
		continue;
	    }
	    if (inet_ntop(AF_INET6, &sin6->sin6_addr, addrstr, sizeof(addrstr)) == NULL) {
		sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
		    "ignoring bad AF_INET6 addr for %s", ifa->ifa_name);
		continue;
	    }
	    sin6 = (struct sockaddr_in6 *)ifa->ifa_netmask;
	    if (inet_ntop(AF_INET6, &sin6->sin6_addr, maskstr, sizeof(maskstr)) == NULL) {
		sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
		    "ignoring bad AF_INET6 mask for %s", ifa->ifa_name);
		continue;
	    }
	    break;
# endif /* HAVE_STRUCT_IN6_ADDR */
	default:
	    continue;
	}

	/* Store the IP addr/netmask pairs. */
	len = snprintf(cp, ailen, "%s%s/%s",
	    cp == *addrinfo ? "" : " ", addrstr, maskstr);
	if (len < 0 || (size_t)len >= ailen) {
	    sudo_warnx(U_("internal error, %s overflow"), __func__);
	    goto done;
	}
	cp += len;
	ailen -= len;
    }

done:
# ifdef HAVE_FREEIFADDRS
    freeifaddrs(ifaddrs);
# else
    free(ifaddrs);
# endif
    debug_return_int(num_interfaces);
}

#elif defined(SIOCGLIFCONF) && !defined(STUB_LOAD_INTERFACES)

/*
 * Fill in the interfaces string with the machine's ip addresses and netmasks
 * and return the number of interfaces found.  Returns -1 on error.
 */
int
get_net_ifs(char **addrinfo)
{
    struct lifreq *lifr, lifr_tmp;
    struct lifconf lifconf;
    struct lifnum lifn;
    struct sockaddr_in *sin;
# ifdef HAVE_STRUCT_IN6_ADDR
    struct sockaddr_in6 *sin6;
# endif
    char addrstr[INET6_ADDRSTRLEN], maskstr[INET6_ADDRSTRLEN];
    int i, n, sock, sock4, sock6 = -1;
    int num_interfaces = -1;
    size_t ailen, buflen;
    char *cp;
    debug_decl(get_net_ifs, SUDO_DEBUG_NETIF);

    if (!sudo_conf_probe_interfaces())
	debug_return_int(0);

    /* We need both INET4 and INET6 sockets to get flags and netmask. */
    sock4 = socket(AF_INET, SOCK_DGRAM, 0);
# ifdef HAVE_STRUCT_IN6_ADDR
    sock6 = socket(AF_INET6, SOCK_DGRAM, 0);
# endif
    if (sock4 == -1 && sock6 == -1)
	debug_return_int(-1);

    /* Use INET6 socket with SIOCGLIFCONF if possible (may not matter). */
    sock = sock6 != -1 ? sock6 : sock4;

    /* Get number of interfaces if possible. */
    memset(&lifn, 0, sizeof(lifn));
    if (ioctl(sock, SIOCGLIFNUM, &lifn) != -1) {
	sudo_debug_printf(SUDO_DEBUG_INFO|SUDO_DEBUG_LINENO,
	    "SIOCGLIFNUM reports %d interfaces", lifn.lifn_count);
	lifn.lifn_count += 4;		/* in case new interfaces come up */
    } else {
	lifn.lifn_count = 512;
    }

    /* Allocate space for the interface buffer. */
    memset(&lifconf, 0, sizeof(lifconf));
    lifconf.lifc_len = lifn.lifn_count * sizeof(struct lifreq);
    lifconf.lifc_buf = malloc(lifconf.lifc_len);
    if (lifconf.lifc_buf == NULL) {
	sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
	    "unable to allocate memory");
	goto done;
    }
    if (ioctl(sock, SIOCGLIFCONF, &lifconf) < 0) {
	sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO|SUDO_DEBUG_ERRNO,
	    "unable to get interface list (SIOCGLIFCONF)");
	goto done;
    }

    /* Allocate space for the maximum number of interfaces that could exist. */
    if ((n = lifconf.lifc_len / sizeof(struct lifreq)) > 0) {
	ailen = n * 2 * INET6_ADDRSTRLEN;
	if ((cp = malloc(ailen)) == NULL) {
	    sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
		"unable to allocate memory");
	    goto done;
	}
	*addrinfo = cp;
    }

    /* For each interface, store the ip address and netmask. */
    num_interfaces = 0;
    for (i = 0; i < lifconf.lifc_len; ) {
	/* Get a pointer to the current interface. */
	lifr = (struct lifreq *)&lifconf.lifc_buf[i];

	/* Set i to the subscript of the next interface (no sa_len). */
	i += sizeof(struct lifreq);

	/* Store the address. */
	switch (lifr->lifr_addr.ss_family) {
	case AF_INET:
	    sin = (struct sockaddr_in *)&lifr->lifr_addr;
	    if (sin->sin_addr.s_addr == INADDR_ANY || sin->sin_addr.s_addr == INADDR_NONE) {
		sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
		    "ignoring unspecified AF_INET addr for %s", lifr->lifr_name);
		continue;
	    }
	    if (inet_ntop(AF_INET, &sin->sin_addr, addrstr, sizeof(addrstr)) == NULL) {
		sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
		    "ignoring bad AF_INET addr for %s", lifr->lifr_name);
		continue;
	    }
	    sock = sock4;
	    break;
# ifdef HAVE_STRUCT_IN6_ADDR
	case AF_INET6:
	    sin6 = (struct sockaddr_in6 *)&lifr->lifr_addr;
	    if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
		sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
		    "ignoring unspecified AF_INET6 addr for %s", lifr->lifr_name);
		continue;
	    }
	    if (inet_ntop(AF_INET6, &sin6->sin6_addr, addrstr, sizeof(addrstr)) == NULL) {
		sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
		    "ignoring bad AF_INET6 addr for %s", lifr->lifr_name);
		continue;
	    }
	    sock = sock6;
	    break;
# endif /* HAVE_STRUCT_IN6_ADDR */
	default:
	    sudo_debug_printf(SUDO_DEBUG_INFO|SUDO_DEBUG_LINENO,
		"ignoring address with family %d for %s",
		lifr->lifr_addr.ss_family, lifr->lifr_name);
	    continue;
	}

	/* Skip interfaces marked "down" and "loopback". */
	memset(&lifr_tmp, 0, sizeof(lifr_tmp));
	memcpy(lifr_tmp.lifr_name, lifr->lifr_name, sizeof(lifr_tmp.lifr_name));
	if (ioctl(sock, SIOCGLIFFLAGS, &lifr_tmp) < 0) {
	    sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO|SUDO_DEBUG_ERRNO,
		"SIOCGLIFFLAGS for %s", lifr->lifr_name);
	    continue;
	}
	if (!ISSET(lifr_tmp.lifr_flags, IFF_UP) ||
	    ISSET(lifr_tmp.lifr_flags, IFF_LOOPBACK))
		continue;

	/* Fetch and store the netmask. */
	memset(&lifr_tmp, 0, sizeof(lifr_tmp));
	memcpy(lifr_tmp.lifr_name, lifr->lifr_name, sizeof(lifr_tmp.lifr_name));
	if (ioctl(sock, SIOCGLIFNETMASK, &lifr_tmp) < 0) {
	    sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO|SUDO_DEBUG_ERRNO,
		"SIOCGLIFNETMASK for %s", lifr->lifr_name);
	    continue;
	}
	switch (lifr->lifr_addr.ss_family) {
	case AF_INET:
	    sin = (struct sockaddr_in *)&lifr_tmp.lifr_addr;
	    if (inet_ntop(AF_INET, &sin->sin_addr, maskstr, sizeof(maskstr)) == NULL) {
		sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
		    "ignoring bad AF_INET mask for %s", lifr->lifr_name);
		continue;
	    }
	    break;
# ifdef HAVE_STRUCT_IN6_ADDR
	case AF_INET6:
	    sin6 = (struct sockaddr_in6 *)&lifr_tmp.lifr_addr;
	    if (inet_ntop(AF_INET6, &sin6->sin6_addr, maskstr, sizeof(maskstr)) == NULL) {
		sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
		    "ignoring bad AF_INET6 mask for %s", lifr->lifr_name);
		continue;
	    }
	    break;
# endif /* HAVE_STRUCT_IN6_ADDR */
	default:
	    sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
		"unexpected address family %d for %s",
		lifr->lifr_addr.ss_family, lifr->lifr_name);
	    continue;
	}

	n = snprintf(cp, ailen, "%s%s/%s",
	    cp == *addrinfo ? "" : " ", addrstr, maskstr);
	if (n < 0 || (size_t)n >= ailen) {
	    sudo_warnx(U_("internal error, %s overflow"), __func__);
	    goto done;
	}
	cp += n;
	ailen -= n;

	num_interfaces++;
    }

done:
    free(lifconf.lifc_buf);
    if (sock4 != -1)
	close(sock4);
    if (sock6 != -1)
	close(sock6);

    debug_return_int(num_interfaces);
}

#elif defined(SIOCGIFCONF) && !defined(STUB_LOAD_INTERFACES)

/*
 * Fill in the interfaces string with the machine's ip addresses and netmasks
 * and return the number of interfaces found.  Returns -1 on error.
 */
int
get_net_ifs(char **addrinfo)
{
    long ifr_tmpbuf[1024 / sizeof(long)];
    struct ifreq *ifr, *ifr_tmp = (struct ifreq *)ifr_tmpbuf;
    struct ifconf *ifconf;
    struct sockaddr_in *sin;
# ifdef HAVE_STRUCT_IN6_ADDR
    struct sockaddr_in6 *sin6;
# endif
    char addrstr[INET6_ADDRSTRLEN], maskstr[INET6_ADDRSTRLEN];
    int i, n, sock, sock4, sock6 = -1, num_interfaces = 0;
    size_t ailen, buflen = sizeof(struct ifconf) + BUFSIZ;
    char *cp, *ifconf_buf = NULL;
#ifdef _ISC
    struct strioctl strioctl;
#endif /* _ISC */
    debug_decl(get_net_ifs, SUDO_DEBUG_NETIF);

    if (!sudo_conf_probe_interfaces())
	debug_return_int(0);

    sock4 = socket(AF_INET, SOCK_DGRAM, 0);
# ifdef HAVE_STRUCT_IN6_ADDR
    sock6 = socket(AF_INET6, SOCK_DGRAM, 0);
# endif
    if (sock4 == -1 && sock6 == -1)
	debug_return_int(-1);

    /* Use INET6 socket with SIOCGIFCONF if possible (may not matter). */
    sock = sock6 != -1 ? sock6 : sock4;

    /*
     * Get interface configuration or return.
     */
    for (;;) {
	if ((ifconf_buf = malloc(buflen)) == NULL) {
	    sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
		"unable to allocate memory");
	    num_interfaces = -1;
	    goto done;
	}
	ifconf = (struct ifconf *)ifconf_buf;
	ifconf->ifc_len = buflen - sizeof(struct ifconf);
	ifconf->ifc_buf = (caddr_t)(ifconf_buf + sizeof(struct ifconf));

#ifdef _ISC
	STRSET(SIOCGIFCONF, (caddr_t)ifconf, buflen);
	if (ioctl(sock, I_STR, (caddr_t)&strioctl) < 0)
#else
	/* Note that some kernels return EINVAL if the buffer is too small */
	if (ioctl(sock, SIOCGIFCONF, ifconf) < 0 && errno != EINVAL)
#endif /* _ISC */
	{
	    num_interfaces = -1;
	    goto done;
	}

	/* Break out of loop if we have a big enough buffer. */
	if (ifconf->ifc_len + sizeof(struct ifreq) < buflen)
	    break;
	buflen += BUFSIZ;
	free(ifconf_buf);
    }

    /*
     * Allocate space for the maximum number of interfaces that could exist.
     * We walk the list for systems with sa_len in struct sockaddr.
     */
    for (i = 0, n = 0; i < ifconf->ifc_len; n++) {
	/* Set i to the subscript of the next interface. */
	i += sizeof(struct ifreq);
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
	ifr = (struct ifreq *)&ifconf->ifc_buf[i];
	if (ifr->ifr_addr.sa_len > sizeof(ifr->ifr_addr))
	    i += ifr->ifr_addr.sa_len - sizeof(struct sockaddr);
#endif /* HAVE_STRUCT_SOCKADDR_SA_LEN */
    }
    if (n == 0)
	goto done;
    ailen = n * 2 * INET6_ADDRSTRLEN;
    if ((cp = malloc(ailen)) == NULL) {
	sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
	    "unable to allocate memory");
	num_interfaces = -1;
	goto done;
    }
    *addrinfo = cp;

    /* For each interface, store the ip address and netmask. */
    for (i = 0; i < ifconf->ifc_len; ) {
	/* Get a pointer to the current interface. */
	ifr = (struct ifreq *)&ifconf->ifc_buf[i];

	/* Set i to the subscript of the next interface. */
	i += sizeof(struct ifreq);
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
	if (ifr->ifr_addr.sa_len > sizeof(ifr->ifr_addr))
	    i += ifr->ifr_addr.sa_len - sizeof(struct sockaddr);
#endif /* HAVE_STRUCT_SOCKADDR_SA_LEN */

	/* Store the address. */
	switch (ifr->ifr_addr.sa_family) {
	case AF_INET:
	    sin = (struct sockaddr_in *)&ifr->ifr_addr;
	    if (sin->sin_addr.s_addr == INADDR_ANY || sin->sin_addr.s_addr == INADDR_NONE) {
		sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
		    "ignoring unspecified AF_INET addr for %s", ifr->ifr_name);
		continue;
	    }
	    if (inet_ntop(AF_INET, &sin->sin_addr, addrstr, sizeof(addrstr)) == NULL) {
		sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
		    "ignoring bad AF_INET addr for %s", ifr->ifr_name);
		continue;
	    }
	    sock = sock4;
	    break;
# ifdef HAVE_STRUCT_IN6_ADDR
	case AF_INET6:
	    sin6 = (struct sockaddr_in6 *)&ifr->ifr_addr;
	    if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
		sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
		    "ignoring unspecified AF_INET6 addr for %s", ifr->ifr_name);
		continue;
	    }
	    if (inet_ntop(AF_INET6, &sin6->sin6_addr, addrstr, sizeof(addrstr)) == NULL) {
		sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
		    "ignoring bad AF_INET6 addr for %s", ifr->ifr_name);
		continue;
	    }
	    sock = sock6;
	    break;
# endif /* HAVE_STRUCT_IN6_ADDR */
	default:
	    sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
		"unexpected address family %d for %s",
		ifr->ifr_addr.sa_family, ifr->ifr_name);
	    continue;
	}

	/* Skip interfaces marked "down" and "loopback". */
#ifdef SIOCGIFFLAGS
	memset(ifr_tmp, 0, sizeof(*ifr_tmp));
	memcpy(ifr_tmp->ifr_name, ifr->ifr_name, sizeof(ifr_tmp->ifr_name));
	if (ioctl(sock, SIOCGIFFLAGS, ifr_tmp) < 0)
#endif
	    memcpy(ifr_tmp, ifr, sizeof(*ifr_tmp));
	if (!ISSET(ifr_tmp->ifr_flags, IFF_UP) ||
	    ISSET(ifr_tmp->ifr_flags, IFF_LOOPBACK))
		continue;

	/* Fetch and store the netmask. */
	memset(ifr_tmp, 0, sizeof(*ifr_tmp));
	memcpy(ifr_tmp->ifr_name, ifr->ifr_name, sizeof(ifr_tmp->ifr_name));
#ifdef _ISC
	STRSET(SIOCGIFNETMASK, (caddr_t)ifr_tmp, sizeof(*ifr_tmp));
	if (ioctl(sock, I_STR, &strioctl) < 0)
#else
	if (ioctl(sock, SIOCGIFNETMASK, ifr_tmp) < 0)
#endif /* _ISC */
	    continue;

	/* Convert the mask to string form. */
	switch (ifr->ifr_addr.sa_family) {
	case AF_INET:
	    sin = (struct sockaddr_in *)&ifr_tmp->ifr_addr;
	    if (inet_ntop(AF_INET, &sin->sin_addr, maskstr, sizeof(maskstr)) == NULL) {
		sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
		    "ignoring bad AF_INET mask for %s", ifr->ifr_name);
		continue;
	    }
	    break;
# ifdef HAVE_STRUCT_IN6_ADDR
	case AF_INET6:
	    sin6 = (struct sockaddr_in6 *)&ifr_tmp->ifr_addr;
	    if (inet_ntop(AF_INET6, &sin6->sin6_addr, maskstr, sizeof(maskstr)) == NULL) {
		sudo_debug_printf(SUDO_DEBUG_ERROR|SUDO_DEBUG_LINENO,
		    "ignoring bad AF_INET6 mask for %s", ifr->ifr_name);
		continue;
	    }
	    break;
# endif /* HAVE_STRUCT_IN6_ADDR */
	default:
	    continue;
	}

	n = snprintf(cp, ailen, "%s%s/%s",
	    cp == *addrinfo ? "" : " ", addrstr, maskstr);
	if (n < 0 || (size_t)n >= ailen) {
	    sudo_warnx(U_("internal error, %s overflow"), __func__);
	    goto done;
	}
	cp += n;
	ailen -= n;

	num_interfaces++;
    }

done:
    free(ifconf_buf);
    if (sock4 != -1)
        close(sock4);
    if (sock6 != -1)
        close(sock6);

    debug_return_int(num_interfaces);
}

#else /* !SIOCGIFCONF || STUB_LOAD_INTERFACES */

/*
 * Stub function for those without SIOCGIFCONF or getifaddrs()
 */
int
get_net_ifs(char **addrinfo)
{
    debug_decl(get_net_ifs, SUDO_DEBUG_NETIF);
    debug_return_int(0);
}

#endif /* SIOCGIFCONF && !STUB_LOAD_INTERFACES */
