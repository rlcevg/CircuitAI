Circuit AI
=========
Spring RTS local native AI for Zero-K.

### Requirements
* gcc 4.9+ (`<regex>` support)*
* spring 102.0
* boost 1.54+
* SDL2 (optional)

*gcc 4.9.3 recommended, [gcc 5+](https://gcc.gnu.org/gcc-5/changes.html) had issues as official spring sits on 4.8

### Compiling
Build process of native AI described in the [wiki](https://springrts.com/wiki/AI:Development:Lang:Cpp) of Spring RTS engine.
Required steps on linux:
```
$ git clone https://github.com/spring/spring.git
$ cd spring && git checkout tags/102.0
$ git clone https://github.com/rlcevg/CircuitAI.git AI/Skirmish/CircuitAI
$ cd AI/Skirmish/CircuitAI && git checkout v<AI version> && cd ../../..
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
([Regarding long startup](http://stackoverflow.com/questions/29012531/package-a-new-base-box))
