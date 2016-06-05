# -*- mode: ruby -*-
# vi: set ft=ruby :

Vagrant.configure(2) do |config|
  # For a complete reference, please see the online documentation at
  # https://docs.vagrantup.com.

  config.vm.box = "ubuntu/trusty64"
  config.vm.network "public_network"

  config.vm.provider "virtualbox" do |v|
    v.memory = 2048
    v.cpus = 2
#    v.customize ["setextradata", :id, "VBoxInternal2/SharedFoldersEnableSymlinksCreate/v-root", "1"]
  end

  config.vm.provision "shell", name: "install", inline: <<-SHELL
    add-apt-repository -y ppa:ubuntu-toolchain-r/test
    apt-get update
    
    apt-get install -y build-essential cmake git
    apt-get install -y libglew-dev libsdl2-dev libdevil-dev libopenal-dev \
      libogg-dev libvorbis-dev libfreetype6-dev p7zip-full libxcursor-dev \
      libboost-thread-dev libboost-regex-dev libboost-system-dev \
      libboost-program-options-dev libboost-signals-dev \
      libboost-chrono-dev libboost-filesystem-dev libunwind8-dev
    
    apt-get install -y gcc-5 g++-5
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 60 --slave /usr/bin/g++ g++ /usr/bin/g++-5
  SHELL

  $script = <<-SCRIPT
    if [ ! -d ~/spring ]; then
      cd ~ && git clone https://github.com/spring/spring.git spring
      
      mkdir spring/AI/Skirmish/CircuitAI
      cp -r /vagrant/src spring/AI/Skirmish/CircuitAI/src
      cp /vagrant/CMakeLists.txt spring/AI/Skirmish/CircuitAI/
      cp /vagrant/VERSION spring/AI/Skirmish/CircuitAI/
      
      cd spring && git checkout tags/102.0
      cmake . && make CircuitAI -j2 > ~/CircuitAI.log 2>&1
      cp AI/Skirmish/CircuitAI/libSkirmishAI.so /vagrant/
    fi
  SCRIPT
  config.vm.provision "shell", name: "startup", inline: $script, privileged: false

end
