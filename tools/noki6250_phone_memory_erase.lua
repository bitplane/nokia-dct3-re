local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6250_runtime_observe.lua')
dofile(directory..'dct3_phone_memory_lifecycle.lua')('6250','erase')
