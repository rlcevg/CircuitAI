BARbarIAn config
=========
AI recides in folder `Beyond All Reason/engine/<engine-version>/AI/Skirmish/BARb/stable/`, further on referenced as `BARb/stable/`.
### Points of interest and new custom profile
##### 1) BARb/stable/AIOptions.lua
its `profile` key contains list of available profiles. In order to add new profile named `customlol` add it into list
```
{
	key  = 'customlol',
	name = 'My custom profile',
	desc = 'Funny ally',
},
```

##### 2) BARb/satble/script/
Contains per-profile scripts main entry points:
* `<profile>/init.as` - describes low-level AI data for threat-maps, surface categories and json data parts to be used with this `<profile>`; typically `init.as` references such config parts as behaviour.json, block_map.json, build_chain.json, commander.json, economy.json, factory.json and response.json that will be used as its data source.
```
@data.profile = @(array<string> = {"behaviour", "block_map", "build_chain", "commander", "economy", "factory", "response"});
```
* `<profile>/main.as` - contains AI callback handlers (docs = src).

Fastest way to create initial scripts for `customlol` is to copy `dev` folder and rename it to `customlol`.

##### 3) BARb/stable/config/
Root contains fallback AI config parts. Folders contain configs for each `<profile>`. Parts listed in `data.profile` from `BARb/satble/script/<profile>/init.as` but not available in `BARb/stable/config/<profile>` folder are replaced by fallback part if such exists.
`easy` for example has own `economy.json` and uses default fallback for other parts.
As with script fastest way to create initial config parts for `customlol` is to copy `dev` folder and rename it to `customlol`.

After those steps new `My custom profile` should appear in the list of available profiles for BARbarIAn in the lobby.
