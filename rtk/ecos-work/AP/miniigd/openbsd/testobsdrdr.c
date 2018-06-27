/* $Id: testobsdrdr.c,v 1.1.1.1 2007-08-06 10:04:43 root Exp $ */
/* project: miniupnp
 * webpage: http://miniupnp.free.fr/
 * (c) 2006 Thomas Bernard
 * This software is subject to the conditions detailed in the
 * LICENCE file provided in this distribution     */
/* Test program for the add_redirect functions under OpenBSD
 * netinet/in.h is necessary for getting #define of IPPROTO_*
 */
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "obsdrdr.h"

int logpackets = 1;

void list_rules(void);

void test_index(void)
{
	char ifname[16/*IFNAMSIZ*/];
	char iaddr[32];
	char desc[64];
	unsigned short iport = 0;
	unsigned short eport = 0;
	int proto = 0;
	ifname[0] = '\0';
	iaddr[0] = '\0';
	if(get_redirect_rule_by_index(0, ifname, &eport, iaddr, sizeof(iaddr),
	                              &iport, &proto, desc) < 0)
	{
		printf("get.._by_index : no rule\n");
	}
	else
	{
		printf("%s %u -> %s:%u proto %d\n", ifname, (unsigned int)eport,
		       iaddr, (unsigned int)iport, proto);
		printf("description: \"%s\"\n", desc);
	}
}

int main(int arc, char * * argv)
{
	char buf[32];
	char desc[64];
	unsigned short iport;
	//add_redirect_rule("ep0", 12123, "192.168.1.23", 1234);
	//add_redirect_rule2("ep0", 12155, "192.168.1.155", 1255, IPPROTO_TCP);
	add_redirect_rule2("ep0", 12123, "192.168.1.125", 1234,
	                   IPPROTO_UDP, "test description");

	list_rules();

	if(get_redirect_rule("ep0", 54321, IPPROTO_TCP, buf, 32, &iport, desc) < 0)
		printf("get_redirect_rule() failed\n");
	else
		printf("\n%s:%d\n", buf, (int)iport);

	if(delete_redirect_rule("ep0", 12123, IPPROTO_UDP) < 0)
		printf("delete_redirect_rule() failed\n");
	else
		printf("delete_redirect_rule() succeded\n");

	if(delete_redirect_rule("ep0", 12123, IPPROTO_UDP) < 0)
		printf("delete_redirect_rule() failed\n");
	else
		printf("delete_redirect_rule() succeded\n");

	test_index();

	clear_redirect_rules();
	list_rules();

	return 0;
}


