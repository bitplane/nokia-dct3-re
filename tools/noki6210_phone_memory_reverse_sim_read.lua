local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_phone_memory_bank_read.lua')('sim',false,'sim')
