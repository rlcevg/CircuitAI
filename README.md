Circuit AI
=========
Framework for Spring RTS local native AIs.

### Requirements
* gcc 5.4+
* spring 104.0-dev
* SDL2 (optional)

### Compiling
Build process of native AI described in the [wiki](https://springrts.com/wiki/AI:Development:Lang:Cpp) of Spring RTS engine.
Required steps on linux:
```
$ git clone https://github.com/spring/spring.git
$ cd spring && git checkout maintenance
$ git clone https://github.com/rlcevg/CircuitAI.git AI/Skirmish/CircuitAI
$ cmake . && make CircuitAI
```

### Installing
To install the AI, put files into proper directory, see CppTestAI or Shard for reference.
An example location of `libSkirmishAI.so` on linux would be `/home/<user>/.spring/engine/<engine version>/AI/Skirmish/CircuitAI/<AI version>/libSkirmishAI.so`

### Linux troubleshooting
Dead AI upon match start: ensure that `libSkirmishAI.so` is compatible with `AI/Interfaces/C/0.1/libAIInterface.so` (i.e. replace it with own build)

For those who lost all hope, behold: [Vagrant](https://docs.vagrantup.com/v2/).
Just navigate to Vagrantfile and do "vagrant up". It will take some time to warm up, install all dependencies and compile Circuit for the first time.
Subsequent builds should be done manually, see Vagrantfile for reference.
