-- Header-publication observation only; does not replace the recorder acceptance gate.
_G.mu4_record_header_full_session = true
local source = debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'mu4_recorder_header_observe.lua')
emu.register_frame_done(function()
    if manager.machine.time:as_double() > 15.1 then manager.machine:exit() end
end)
