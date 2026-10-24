_G.nsm2_fdn_record_steps={
    {'Scroll Down','number_option_1'},{'Scroll Down','number_option_2'},{'Menu','numbers'},
    {'Menu','search_editor'},{'Menu','search_results'},{'Menu','contact'},
}
_G.nsm2_fdn_record_finish=48
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki8850_fdn.lua')
