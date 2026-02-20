FROM debian:bookworm-20241202
MAINTAINER Andrey Volk <andrey@signalwire.com>

ARG REPOTOKEN
ARG COVERITYTOKEN

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get -yq install \
    wget gnupg2 dos2unix apt-transport-https lsb-release git
RUN echo "machine freeswitch.signalwire.com login signalwire password ${REPOTOKEN}" > /etc/apt/auth.conf
RUN wget --http-user=signalwire --http-password=${REPOTOKEN} -O /usr/share/keyrings/signalwire-freeswitch-repo.gpg https://freeswitch.signalwire.com/repo/deb/debian-release/signalwire-freeswitch-repo.gpg 
RUN echo "deb [signed-by=/usr/share/keyrings/signalwire-freeswitch-repo.gpg] https://freeswitch.signalwire.com/repo/deb/debian-release/ `lsb_release -sc` main" > /etc/apt/sources.list.d/freeswitch.list
RUN echo "deb-src [signed-by=/usr/share/keyrings/signalwire-freeswitch-repo.gpg] https://freeswitch.signalwire.com/repo/deb/debian-release/ `lsb_release -sc` main" >> /etc/apt/sources.list.d/freeswitch.list
RUN apt-get update
RUN apt-get -y build-dep freeswitch
RUN mkdir /data

ENV PATH="/coverity/cov-analysis-linux64/bin:${PATH}"

RUN echo "#!/bin/bash\n\
wget https://scan.coverity.com/download/linux64 --post-data \"token=${COVERITYTOKEN}&project=FreeSWITCH\" -O /coverity_tool.tgz\n\
COVERITYFOLDER=\$(tar -tf /coverity_tool.tgz | head -n1)\n\
mkdir -p /coverity\n\
tar zxvf /coverity_tool.tgz -C /coverity/\n\
cd /coverity\n\
mv \$COVERITYFOLDER cov-analysis-linux64\n\
apt-get update && apt-get -y build-dep freeswitch\n\
apt-get install -y libv8-6.1-dev\n\
cd /\n\
git clone https://github.com/signalwire/freeswitch -b "\$FSBRANCH"\n\
cd /freeswitch\n\
git status\n\
mkdir modules\n\
cd modules\n\
../debian/bootstrap.sh -c bookworm\n\
cd ..\n\
./bootstrap.sh\n\
cp ./modules/modules_.conf ./modules.conf\n\
./configure\n\
cov-build --dir cov-int make -j\$(nproc)\n\
tar czvf /data/freeswitch.tgz cov-int\n "\
>> /run.sh

RUN echo /run.sh

WORKDIR /data
RUN chmod 755 /run.sh
RUN dos2unix /run.sh
CMD ["/run.sh"]