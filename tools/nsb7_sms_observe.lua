-- Extend passive protocol observation; no handset input after security code.
_G.nsb7_security_continue=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_page_observe.lua')
local finish=coroutine.create(function()
    assert(emu.wait(35-manager.machine.time:as_double()))
    manager.machine.screens[':screen']:snapshot('nsb7_sms_final.png')
    manager.machine:exit()
end)
_G.nsb7_sms_finish=finish
assert(coroutine.resume(finish))
