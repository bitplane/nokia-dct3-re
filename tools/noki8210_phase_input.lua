local directory = assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
dofile(directory..'noki8210_registration_input.lua')
dofile(directory..'noki8210_phase_observe.lua')
manager.machine.debugger:command('g')
