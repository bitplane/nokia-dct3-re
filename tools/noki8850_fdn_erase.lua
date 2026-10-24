local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
_G.nsm2_fdn_record_steps=dofile(directory..'noki8850_fdn_record_options.lua')
for _,step in ipairs({{'Scroll Down','erase_option'},{'Menu','erase_confirm'},
        {_G.nsm2_fdn_cancel and 'Names / C' or 'Menu',_G.nsm2_fdn_cancel and 'cancelled' or 'erased'}}) do
    table.insert(_G.nsm2_fdn_record_steps,step)
end
_G.nsm2_fdn_record_finish=58
dofile(directory..'noki8850_fdn.lua')
