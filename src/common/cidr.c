/* $Id$ */

/*
 * Copyright (c) 2001-2004 Aaron Turner.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright owners nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"
#include "defines.h"
#include "common.h"
#include "lib/strlcpy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* required for inet_aton() */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#ifdef DEBUG
extern int debug;
#endif

static tcpr_cidr_t *cidr2cidr(char *);

/*
 * prints to the given fd all the entries in mycidr
 */
void
print_cidr(tcpr_cidr_t * mycidr)
{
    tcpr_cidr_t *cidr_ptr;

    fprintf(stderr, "Cidr List: ");

    cidr_ptr = mycidr;
    while (cidr_ptr != NULL) {
        /* print it */
        fprintf(stderr, "%s/%d, ",
                get_addr2name4(cidr_ptr->network, RESOLVE),
                cidr_ptr->masklen);

        /* go to the next */
        if (cidr_ptr->next != NULL) {
            cidr_ptr = cidr_ptr->next;
        }
        else {
            break;
        }
    }
    fprintf(stderr, "\n");
}

/*
 * deletes all entries in a cidr and destroys the datastructure
 */
void
destroy_cidr(tcpr_cidr_t * cidr)
{

    if (cidr != NULL)
        if (cidr->next != NULL)
            destroy_cidr(cidr->next);

    free(cidr);
    return;

}

/*
 * adds a new tcpr_cidr_t entry to cidrdata
 */
void
add_cidr(tcpr_cidr_t ** cidrdata, tcpr_cidr_t ** newcidr)
{
    tcpr_cidr_t *cidr_ptr;
    dbg(1, "Running new_cidr()");

    if (*cidrdata == NULL) {
        *cidrdata = *newcidr;
    }
    else {
        cidr_ptr = *cidrdata;

        while (cidr_ptr->next != NULL) {
            cidr_ptr = cidr_ptr->next;
        }

        cidr_ptr->next = *newcidr;
    }
}

/*
 * takes in an IP and masklen, and returns a string in
 * cidr format: x.x.x.x/y.  This malloc's memory.
 */
u_char *
ip2cidr(const unsigned long ip, const int masklen)
{
    u_char *network;
    char mask[3];

    network = (u_char *)safe_malloc(20);

    strlcpy((char *)network, (char *)get_addr2name4(ip, RESOLVE),
            sizeof(network));

    strcat((char *)network, "/");
    if (masklen < 10) {
        snprintf(mask, 1, "%d", masklen);
        strncat((char *)network, mask, 1);
    }
    else {
        snprintf(mask, 2, "%d", masklen);
        strncat((char *)network, mask, 2);
    }

    return (network);
}

/*
 * Mallocs and sets to sane defaults a tcpr_cidr_t structure
 */

tcpr_cidr_t *
new_cidr(void)
{
    tcpr_cidr_t *newcidr;

    newcidr = (tcpr_cidr_t *)safe_malloc(sizeof(tcpr_cidr_t));
    
    memset(newcidr, '\0', sizeof(tcpr_cidr_t));
    newcidr->masklen = 99;
    newcidr->next = NULL;

    return (newcidr);
}

tcpr_cidrmap_t *
new_cidr_map(void)
{
    tcpr_cidrmap_t *new;

    new = (tcpr_cidrmap_t *)safe_malloc(sizeof(tcpr_cidrmap_t));
    
    memset(new, '\0', sizeof(tcpr_cidrmap_t));
    new->next = NULL;

    return (new);
}


/*
 * Converts a single cidr (string) in the form of x.x.x.x/y into a
 * tcpr_cidr_t structure.  Will malloc the tcpr_cidr_t structure.
 */

static tcpr_cidr_t *
cidr2cidr(char *cidr)
{
    int count = 0;
    unsigned int octets[4];     /* used in sscanf */
    tcpr_cidr_t *newcidr;
    char networkip[16], tempoctet[4], ebuf[EBUF_SIZE];

    assert(cidr);
    assert(strlen(cidr) <= EBUF_SIZE);

    newcidr = new_cidr();

    /*
     * scan it, and make sure it scanned correctly, also copy over the
     * masklen
     */
    count = sscanf(cidr, "%u.%u.%u.%u/%d", &octets[0], &octets[1],
                   &octets[2], &octets[3], &newcidr->masklen);
    if (count == 4) {
        newcidr->masklen = 32;
    } else if (count != 5) {
        goto error;
    }

    /* masklen better be 0 =< masklen <= 32 */
    if (newcidr->masklen > 32)
        goto error;

    /* copy in the ip address */
    memset(networkip, '\0', 16);
    for (count = 0; count < 4; count++) {
        if (octets[count] > 255)
            goto error;

        snprintf(tempoctet, sizeof(octets[count]), "%d", octets[count]);
        strcat(networkip, tempoctet);
        /* we don't want a '.' at the end of the last octet */
        if (count < 3)
            strcat(networkip, ".");
    }

    /* copy over the network address and return */
#ifdef HAVE_INET_ATON
    inet_aton(networkip, (struct in_addr *)&newcidr->network);
#elif HAVE_INET_ADDR
    newcidr->network = inet_addr(networkip);
#endif

    return (newcidr);

    /* we only get here on error parsing input */
  error:
    memset(ebuf, '\0', EBUF_SIZE);
    strcpy(ebuf, "Unable to parse as a vaild CIDR: ");
    strlcat(ebuf, cidr, EBUF_SIZE);
    errx(1, "%s", ebuf);
    return NULL;
}

/*
 * parses a list of tcpr_cidr_t's input from the user which should be in the form
 * of x.x.x.x/y,x.x.x.x/y...
 * returns 1 for success, or fails to return on failure (exit 1)
 * since we use strtok to process cidr, it gets zeroed out.
 */

int
parse_cidr(tcpr_cidr_t ** cidrdata, char *cidrin, char *delim)
{
    tcpr_cidr_t *cidr_ptr;             /* ptr to current cidr record */
    char *network = NULL;
    char *token = NULL;

    /* first itteration of input using strtok */
    network = strtok_r(cidrin, delim, &token);

    *cidrdata = cidr2cidr(network);
    cidr_ptr = *cidrdata;

    /* do the same with the rest of the input */
    while (1) {
        network = strtok_r(NULL, delim, &token);
        /* if that was the last CIDR, then kickout */
        if (network == NULL)
            break;

        /* next record */
        cidr_ptr->next = cidr2cidr(network);
        cidr_ptr = cidr_ptr->next;
    }
    return 1;

}

/*
 * parses a pair of IP addresses: <IP1>:<IP2> and processes it like:
 * -N 0.0.0.0/0:<IP1> -N 0.0.0.0/0:<IP2>
 * returns 1 for success or returns 0 on failure
 * since we use strtok to process optarg, it gets zeroed out
 */
int
parse_endpoints(tcpr_cidrmap_t ** cidrmap1, tcpr_cidrmap_t ** cidrmap2, const char *optarg)
{
#define NEWMAP_LEN 32
    char *map = NULL, newmap[NEWMAP_LEN];
    char *token = NULL;
    char *string;

    string = safe_strdup(optarg);

    memset(newmap, '\0', NEWMAP_LEN);
    map = strtok_r(string, ":", &token);

    strlcpy(newmap, "0.0.0.0/0:", NEWMAP_LEN);
    strlcat(newmap, map, NEWMAP_LEN);
    if (! parse_cidr_map(cidrmap1, newmap))
        return 0;
    
    /* do again with the second IP */
    memset(newmap, '\0', NEWMAP_LEN);
    map = strtok_r(NULL, ":", &token);
    
    strlcpy(newmap, "0.0.0.0/0:", NEWMAP_LEN);
    strlcat(newmap, map, NEWMAP_LEN);
    if (! parse_cidr_map(cidrmap2, newmap))
        return 0;
    
    free(string);
    return 1; /* success */
}


/*
 * parses a list of tcpr_cidrmap_t's input from the user which should be in the form
 * of x.x.x.x/y:x.x.x.x/y,...
 * returns 1 for success, or returns 0 on failure
 * since we use strtok to process optarg, it gets zeroed out.
 */
int
parse_cidr_map(tcpr_cidrmap_t **cidrmap, const char *optarg)
{
    tcpr_cidr_t *cidr = NULL;
    char *map = NULL;
    char *token = NULL, *string = NULL;
    tcpr_cidrmap_t *ptr;
    
    string = safe_strdup(optarg);

    /* first iteration */
    map = strtok_r(string, ",", &token);
    if (! parse_cidr(&cidr, map, ":"))
        return 0;

    /* must return a linked list of two */
    if (cidr->next == NULL)
        return 0;

    /* copy over */
    *cidrmap = new_cidr_map();
    ptr = *cidrmap;

    ptr->from = cidr;
    ptr->to = cidr->next;
    ptr->from->next = NULL;

    /* do the same with the reset of the input */
    while(1) {
        map = strtok_r(NULL, ",", &token);
        if (map == NULL)
            break;

        if (! parse_cidr(&cidr, map, ":"))
            return 0;

        /* must return a linked list of two */
        if (cidr->next == NULL)
            return 0;

        /* copy over */
        ptr->next = new_cidr_map();
        ptr = ptr->next;
        ptr->from = cidr;
        ptr->to = cidr->next;
        ptr->from->next = NULL;

    }
    free(string);
    return 1; /* success */
}

/*
 * checks to see if the ip address is in the cidr
 * returns CACHE_PRIMARY for true, CACHE_SECONDARY for false
 */

int
ip_in_cidr(const tcpr_cidr_t * mycidr, const unsigned long ip)
{
    unsigned long ipaddr = 0, network = 0, mask = 0;
    int ret;
    
    /* always return 1 if 0.0.0.0/0 */
    if (mycidr->masklen == 0 && mycidr->network == 0)
        return CACHE_PRIMARY;

    mask = ~0;                  /* turn on all the bits */

    /* shift over by the correct number of bits */
    mask = mask << (32 - mycidr->masklen);

    /* apply the mask to the network and ip */
    ipaddr = ntohl(ip) & mask;

    network = htonl(mycidr->network) & mask;

    /* if they're the same, then ip is in network */
    if (network == ipaddr) {

        dbgx(1, "The ip %s is inside of %s/%d",
            get_addr2name4(ip, RESOLVE),
            get_addr2name4(htonl(network), RESOLVE), mycidr->masklen);

        ret = CACHE_PRIMARY;
    }
    else {

        dbgx(1, "The ip %s is not inside of %s/%d",
            get_addr2name4(ip, RESOLVE),
            get_addr2name4(htonl(network), RESOLVE), mycidr->masklen);

        ret = CACHE_SECONDARY;
    }
    return ret;
}

/*
 * iterates over cidrdata to find if a given ip matches
 * returns CACHE_PRIMARY for true, CACHE_SECONDARY for false
 */

int
check_ip_cidr(tcpr_cidr_t * cidrdata, const unsigned long ip)
{
    tcpr_cidr_t *mycidr;

    /* if we have no cidrdata, of course it isn't in there 
     * this actually should happen occasionally, so don't put an assert here
     */
    if (cidrdata == NULL) {
        return CACHE_SECONDARY;
    }

    mycidr = cidrdata;

    /* loop through cidr */
    while (1) {

        /* if match, return 1 */
        if (ip_in_cidr(mycidr, ip) == CACHE_PRIMARY) {
            dbgx(3, "Found %s in cidr", get_addr2name4(ip, RESOLVE));
            return CACHE_PRIMARY;
        }
        /* check for next record */
        if (mycidr->next != NULL) {
            mycidr = mycidr->next;
        }
        else {
            break;
        }
    }

    /* if we get here, no match */
    dbgx(3, "Didn't find %s in cidr", get_addr2name4(ip, RESOLVE));
    return CACHE_SECONDARY;
}


/*
 * cidr2ip takes a tcpr_cidr_t and a delimiter
 * and returns a string which lists all the IP addresses in the cidr
 * deliminated by the given char
 */
char *
cidr2iplist(tcpr_cidr_t * cidr, char delim)
{
    char *list = NULL;
    char ipaddr[16];
    u_int32_t size, addr, first, last, numips;
    struct in_addr in;

    /* 
     * 16 bytes per IP + delim
     * # of IP's = 2^(32-masklen)
     */
    numips = 2;
    for (int i = 2; i <= (32 - cidr->masklen); i++) {
        numips *= 2;
    }
    size = 16 * numips;

    list = (char *)safe_malloc(size);

    memset(list, 0, size);

    /* first and last should not include network or broadcast */
    first = ntohl(cidr->network) + 1;
    last = first + numips - 3;

    dbgx(1, "First: %u\t\tLast: %u", first, last);

    /* loop through all but the last one */
    for (addr = first; addr < last; addr++) {
        in.s_addr = htonl(addr);
        snprintf(ipaddr, 17, "%s%c", inet_ntoa(in), delim);
        dbgx(2, "%s", ipaddr);
        strlcat(list, ipaddr, size);
    }

    /* last is a special case, end in \0 */
    in.s_addr = htonl(addr);
    snprintf(ipaddr, 16, "%s", inet_ntoa(in));
    strlcat(list, ipaddr, size);

    return list;
}

/*
 Local Variables:
 mode:c
 indent-tabs-mode:nil
 c-basic-offset:4
 End:
*/
