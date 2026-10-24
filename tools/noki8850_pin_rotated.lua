_G.nsm2_pin_digits={{3,'Keypad 2'},{2,'Keypad 4'},{4,'Keypad 6'},
                  {3,'Keypad 8'},{1,'Menu'}}
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki8850_security_input.lua')
