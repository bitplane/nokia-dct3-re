_G.nsm2_fdn_record_steps={
    {'Scroll Down','number_option_1'},{'Scroll Down','number_option_2'},{'Menu','numbers'},
    {'Scroll Down','add'},{'Menu','record_pin2'},
    {'Keypad 5','record_pin2_1'},{'Keypad 6','record_pin2_2'},
    {'Keypad 7','record_pin2_3'},{'Keypad 8','record_pin2_4'},{'Menu','name_editor'},
    {'Keypad 2','name_A'},{'Menu','number_editor'},
    {'Keypad 1','number_1'},{'Keypad 2','number_12'},{'Keypad 3','number_123'},
    {'Keypad 4','number_1234'},{'Keypad 5','number_12345'},{'Menu','saved'},
}
_G.nsm2_fdn_record_finish=60
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki8850_fdn.lua')
