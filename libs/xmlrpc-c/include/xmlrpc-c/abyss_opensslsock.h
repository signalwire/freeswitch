/* This is just a sub-file for abyss.h */

#include <sys/socket.h>

struct abyss_openssl_chaninfo {
    /* TODO: figure out useful information to put in here.
       Maybe client IP address and port.  Maybe authenticated host name.
       Maybe authentication level.
       Maybe a certificate.
    */
    int dummy;
};

void
ChanSwitchOpensslCreate(unsigned short const portNumber,
                        TChanSwitch ** const chanSwitchPP,
                        const char **  const errorP);

void
ChanSwitchOpensslCreateFd(int            const fd,
                          TChanSwitch ** const chanSwitchPP,
                          const char **  const errorP);

void
ChannelOpensslCreateSsl(SSL *                            const sslP,
                        TChannel **                      const channelPP,
                        struct abyss_openssl_chaninfo ** const channelInfoPP,
                        const char **                    const errorP);
