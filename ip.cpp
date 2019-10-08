#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <errno.h>

std::string getIp()
{
    struct ifaddrs *myaddrs, *ifa;
    void *in_addr;
    char buf[64];

    if(getifaddrs(&myaddrs) != 0)
    {
        perror("getifaddrs");
        exit(1);
    }

    for (ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next)
    {
        if(strncmp(ifa->ifa_name, "en", 2) == 0) {
            if (ifa->ifa_addr == NULL)
                continue;
            if (!(ifa->ifa_flags & IFF_UP))
                continue;

            if (ifa->ifa_addr->sa_family == AF_INET) {
                struct sockaddr_in *s4 = (struct sockaddr_in *) ifa->ifa_addr;
                in_addr = &s4->sin_addr;
                if (!inet_ntop(ifa->ifa_addr->sa_family, in_addr, buf, sizeof(buf))) {
                    printf("%s: inet_ntop failed!\n", ifa->ifa_name);
                } else {
                    printf("%s: %s\n", ifa->ifa_name, buf);

                    std::string str(buf, strlen(buf));
                    return str;
                }
            }
        }
    }
    return nullptr;
}
