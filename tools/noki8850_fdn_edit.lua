local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
_G.nsm2_fdn_record_steps=dofile(directory..'noki8850_fdn_record_options.lua')
for _,step in ipairs({{'Menu','name_editor'},{'Menu','number_editor'},
        {'Names / C','number_1234'},{'Keypad 6','number_12346'},{'Menu','edited'}}) do
    table.insert(_G.nsm2_fdn_record_steps,step)
end
_G.nsm2_fdn_record_finish=60
dofile(directory..'noki8850_fdn.lua')
