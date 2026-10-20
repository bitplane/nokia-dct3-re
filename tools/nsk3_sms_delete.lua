-- Physical Options/Erase/confirmation of the persisted offline draft.
_G.nsk3_sms_delete=true
_G.nsk3_sms_delete_steps=3
_G.nsk3_sms_frames={19,21,23}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_sms_reload.lua')
