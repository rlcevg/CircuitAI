# -*- mode: ruby -*-
# vi: set ft=ruby :

Vagrant.configure(2) do |config|
  # For a complete reference, please see the online documentation at
  # https://docs.vagrantup.com.

  config.vm.box = "ubuntu/xenial64"
  config.vm.network "public_network"
  config.vm.synced_folder ".", "/vagrant"

  config.vm.provider "virtualbox" do |v|
    v.memory = 2048
    v.cpus = 2
#    v.customize ["setextradata", :id, "VBoxInternal2/SharedFoldersEnableSymlinksCreate/v-root", "1"]
  end

  config.vm.provision "shell", name: "install", inline: <<-SHELL
    apt-get update

    apt-get install -y build-essential cmake git gcc-5 g++-5
    apt-get install -y libglew-dev libsdl2-dev libdevil-dev libopenal-dev \
      libogg-dev libvorbis-dev libfreetype6-dev p7zip-full libboost-dev \
      libunwind8-dev libcurl4-openssl-dev
  SHELL

  $script = <<-SCRIPT
    if [ ! -d ~/spring ]; then
      cd ~ && git clone https://github.com/spring/spring.git spring

      mkdir spring/AI/Skirmish/CircuitAI
      cp -r /vagrant/src spring/AI/Skirmish/CircuitAI/src
      cp /vagrant/CMakeLists.txt spring/AI/Skirmish/CircuitAI/
      cp /vagrant/VERSION spring/AI/Skirmish/CircuitAI/

      cd spring && git checkout maintenance
      git submodule init && git submodule update
      cmake -DAI_TYPES=NATIVE .
      make CircuitAI -j2 > ~/CircuitAI.log 2>&1
      strip AI/Skirmish/CircuitAI/data/libSkirmishAI.so
      cp AI/Skirmish/CircuitAI/data/libSkirmishAI.so /vagrant/
    fi
  SCRIPT
  config.vm.provision "shell", name: "startup", inline: $script, privileged: false

end
