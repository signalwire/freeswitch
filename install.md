run it as Sudo user, on VM and in Production.

```bash
TOKEN=pat_kUQdordkVe6tyqDw

apt-get update && apt-get install -yq gnupg2 wget lsb-release

wget --http-user=signalwire --http-password=$TOKEN -O /usr/share/keyrings/signalwire-freeswitch-repo.gpg https://freeswitch.signalwire.com/repo/deb/debian-release/signalwire-freeswitch-repo.gpg

echo "machine freeswitch.signalwire.com login signalwire password $TOKEN" > /etc/apt/auth.conf

echo "deb [signed-by=/usr/share/keyrings/signalwire-freeswitch-repo.gpg] https://freeswitch.signalwire.com/repo/deb/debian-release/ `lsb_release -sc` main" > /etc/apt/sources.list.d/freeswitch.list

echo "deb-src [signed-by=/usr/share/keyrings/signalwire-freeswitch-repo.gpg] https://freeswitch.signalwire.com/repo/deb/debian-release/ `lsb_release -sc` main" >> /etc/apt/sources.list.d/freeswitch.list

apt-get -y update
apt-get -y build-dep freeswitch

cd /usr/src
git clone https://github.com/Veeivs/freeswitch
cd freeswitch


sudo ./bootstrap.sh -j
sudo ./configure

sudo make
sudo make install
sudo make cd-sounds-install cd-moh-install```