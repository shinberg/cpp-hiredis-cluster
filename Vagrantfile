# -*- mode: ruby -*-
# vi: set ft=ruby :

# All Vagrant configuration is done below. The "2" in Vagrant.configure
# configures the configuration version (we support older styles for
# backwards compatibility). Please don't change it unless you know what
# you're doing.
Vagrant.configure(2) do |config|
  # The most common configuration options are documented and commented below.
  # For a complete reference, please see the online documentation at
  # https://docs.vagrantup.com.

  # Every Vagrant development environment requires a box. You can search for
  # boxes at https://atlas.hashicorp.com/search.
  config.vm.box = "puphpet/debian75-x64"

  # Disable automatic box update checking. If you disable this, then
  # boxes will only be checked for updates when the user runs
  # `vagrant box outdated`. This is not recommended.
  # config.vm.box_check_update = false

  # Create a forwarded port mapping which allows access to a specific port
  # within the machine from a port on the host machine. In the example below,
  # accessing "localhost:8080" will access port 80 on the guest machine.
  # config.vm.network "forwarded_port", guest: 80, host: 8080

  # Create a private network, which allows host-only access to the machine
  # using a specific IP.
  # config.vm.network "private_network", ip: "192.168.33.10"

  # Create a public network, which generally matched to bridged network.
  # Bridged networks make the machine appear as another physical device on
  # your network.
  # config.vm.network "public_network"

  # Share an additional folder to the guest VM. The first argument is
  # the path on the host to the actual folder. The second argument is
  # the path on the guest to mount the folder. And the optional third
  # argument is a set of non-required options.
  # config.vm.synced_folder "../data", "/vagrant_data"

  # Provider-specific configuration so you can fine-tune various
  # backing providers for Vagrant. These expose provider-specific options.
  # Example for VirtualBox:
  #
  # config.vm.provider "virtualbox" do |vb|
  #   # Display the VirtualBox GUI when booting the machine
  #   vb.gui = true
  #
  #   # Customize the amount of memory on the VM:
  #   vb.memory = "1024"
  # end
  #
  # View the documentation for the provider you are using for more
  # information on available options.

  # Define a Vagrant Push strategy for pushing to Atlas. Other push strategies
  # such as FTP and Heroku are also available. See the documentation at
  # https://docs.vagrantup.com/v2/push/atlas.html for more information.
  # config.push.define "atlas" do |push|
  #   push.app = "YOUR_ATLAS_USERNAME/YOUR_APPLICATION_NAME"
  # end

  # Enable provisioning with a shell script. Additional provisioners such as
  # Puppet, Chef, Ansible, Salt, and Docker are also available. Please see the
  # documentation for more information about their specific syntax and use.
  config.vm.provision "shell", privileged: false, inline: <<-SHELL
#!/bin/sh
sudo mkdir -p /var/log/redis/
sudo mkdir -p /var/run/redis/
sudo mkdir -p /var/lib/redis/
sudo mkdir -p /etc/redis/
mkdir -p /home/vagrant/etc/redis/

sudo adduser redis --no-create-home --disabled-password --disabled-login --gecos ""
sudo chown redis:redis /var/log/redis/
sudo chown redis:redis /var/run/redis/
sudo chown redis:redis /var/lib/redis/

mkdir redis
wget http://download.redis.io/releases/redis-3.0.2.tar.gz
tar xf redis-3.0.2.tar.gz -C redis --strip-components=1
rm redis-3.0.2.tar.gz
cd redis
make -j4

for N in 0 1 2
do
	cat | sudo tee /etc/redis/redis-cluster-$N.conf <<-EOF
	bind 0.0.0.0
	port 700$N
	cluster-enabled yes
	cluster-config-file nodes-$N.conf
	cluster-node-timeout 5000
	unixsocket /tmp/redis$N.sock
	unixsocketperm 755
	logfile /var/log/redis/redis-cluster-$N.log
	pidfile /var/run/redis/redis-cluster-$N.pid    
	dir /var/lib/redis
	dbfilename dump-cluster-$N.rdb
	daemonize yes
	EOF

	cat | sudo tee /etc/init.d/redis-cluster-$N <<-EOL
	#! /bin/sh
	PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
	DAEMON=/home/vagrant/redis/src/redis-server
	DAEMON_ARGS=/etc/redis/redis-cluster-$N.conf
	NAME=redis-cluster-$N
	DESC=redis-cluster-$N

	RUNDIR=/var/run/redis
	PIDFILE=\\$RUNDIR/redis-cluster-$N.pid

	test -x \\$DAEMON || exit 0

	if [ -r /etc/default/\\$NAME ]
	then
		. /etc/default/\\$NAME
	fi

	. /lib/lsb/init-functions

	set -e

	case "\\$1" in
	start)
	echo -n "Starting \\$DESC: "
	mkdir -p \\$RUNDIR
	touch \\$PIDFILE
	chown redis:redis \\$RUNDIR \\$PIDFILE
	chmod 755 \\$RUNDIR

	if [ -n "\\$ULIMIT" ]
	then
		ulimit -n \\$ULIMIT
	fi

	if start-stop-daemon --start --quiet --umask 007 --pidfile \\$PIDFILE --chuid redis:redis --exec \\$DAEMON -- 	\\$DAEMON_ARGS
	then
		echo "\\$NAME."
	else
		echo "failed"
	fi
	;;
	stop)
		echo -n "Stopping \\$DESC: "
		if start-stop-daemon --stop --retry forever/TERM/1 --quiet --oknodo --pidfile \\$PIDFILE --exec \\$DAEMON
		then
			echo "\\$NAME."
		else
			echo "failed"
		fi
		rm -f \\$PIDFILE
		sleep 1
		;;

	restart|force-reload)
		\\${0} stop
		\\${0} start
		;;

	status)
	status_of_proc -p \\${PIDFILE} \\${DAEMON} \\${NAME}
		;;

	*)
		echo "Usage: /etc/init.d/\\$NAME {start|stop|restart|force-reload|status}" >&2
		exit 1
		;;
	esac

	exit 0
	EOL



	sudo chmod +x /etc/init.d/redis-cluster-$N
	sudo update-rc.d redis-cluster-$N defaults
	sudo service redis-cluster-$N start
done

sudo chown -R vagrant:vagrant /usr/local/rvm
gem install redis
printf 'yes\\n' | /home/vagrant/redis/src/redis-trib.rb create 127.0.0.1:7000 127.0.0.1:7001 127.0.0.1:7002
sudo apt-get install cmake -y
sudo apt-get install libevent-dev -y
cd /home/vagrant/ && git clone http://github.com/redis/hiredis && cd hiredis && make && sudo make install && sudo ldconfig
cd /home/vagrant && mkdir -p build && cd build && cmake /vagrant/ && make -j4 
SHELL
end
