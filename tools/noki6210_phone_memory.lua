local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_staged_observe.lua')
dofile(directory..'dct3_phone_memory_input.lua')('6210',_G.npe3_phone_memory_empty)
