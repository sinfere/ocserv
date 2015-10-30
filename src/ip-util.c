/*
 * Copyright (C) 2013-2015 Nikos Mavrogiannopoulos
 * Copyright (C) 2015 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "ip-util.h"
#include <string.h>
#include <stdio.h>
#include <talloc.h>
/* for inet_ntop */
#include <arpa/inet.h>

int ip_cmp(const struct sockaddr_storage *s1, const struct sockaddr_storage *s2)
{
	if (((struct sockaddr*)s1)->sa_family == AF_INET) {
		return memcmp(SA_IN_P(s1), SA_IN_P(s2), sizeof(struct in_addr));
	} else { /* inet6 */
		return memcmp(SA_IN6_P(s1), SA_IN6_P(s2), sizeof(struct in6_addr));
	}
}

/* returns an allocated string with the mask to apply for the prefix
 */
char* ipv4_prefix_to_strmask(void *pool, unsigned prefix)
{
	struct in_addr in;
	char str[MAX_IP_STR];

	if (prefix == 0 || prefix > 32)
		return NULL;

	in.s_addr = ntohl(((uint32_t)0xFFFFFFFF) << (32 - prefix));
	if (inet_ntop(AF_INET, &in, str, sizeof(str)) == NULL)
		return NULL;

	return talloc_strdup(pool, str);
}

unsigned ipv6_prefix_to_mask(struct in6_addr *in6, unsigned prefix)
{
	int i, j;

	if (prefix == 0 || prefix > 128)
		return 0;

	memset(in6, 0x0, sizeof(*in6));
	for (i = prefix, j = 0; i > 0; i -= 8, j++) {
		if (i >= 8) {
			in6->s6_addr[j] = 0xff;
		} else {
			in6->s6_addr[j] = (unsigned long)(0xffU << ( 8 - i ));
		}
	}

	return 1;
}

/* check whether a route is on the expected format, and if it cannot be
 * fixed, then returns a negative code.
 *
 * The expected format by clients for IPv4 is xxx.xxx.xxx.xxx/xxx.xxx.xxx.xxx, i.e.,
 * this function converts xxx.xxx.xxx.xxx/prefix to the above for IPv4.
 */
int ip_route_sanity_check(void *pool, char **_route)
{
	char *p;
	unsigned prefix;
	char *route = *_route, *n;
	char *slash_ptr, *pstr;

	/* this check is valid for IPv4 only */
	p = strchr(route, '.');
	if (p == NULL)
		return 0;

	p = strchr(p, '/');
	if (p == NULL) {
		fprintf(stderr, "route '%s' in wrong format, use xxx.xxx.xxx.xxx/xxx.xxx.xxx.xxx\n", route);
		return -1;
	}
	slash_ptr = p;
	p++;

	/* if we are in dotted notation exit */
	if (strchr(p, '.') != 0)
		return 0;

	/* we are most likely in the xxx.xxx.xxx.xxx/prefix format */
	prefix = atoi(p);

	pstr = ipv4_prefix_to_strmask(pool, prefix);
	if (pstr == NULL) {
		fprintf(stderr, "cannot figure format of route '%s'\n", route);
		return -1;
	}

	*slash_ptr = 0;

	n = talloc_asprintf(pool, "%s/%s", route, pstr);
	if (n == NULL) {
		fprintf(stderr, "memory error\n");
		return -1;
	}
	*_route = n;

	talloc_free(pstr);
	talloc_free(route);
	return 0;
}