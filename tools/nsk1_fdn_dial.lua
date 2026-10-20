-- Single-Navi physical dial; never inject a call or registration event.
_G.nsk1_fdn_keys=_G.nsk1_fdn_unlisted and {'five','four','three','two','one','menu'}
    or {'one','two','three','four','six','menu'}
_G.nsk1_fdn_frame_delay=1.5
_G.nsk1_fdn_final_time=46
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk1_fdn_input.lua')
