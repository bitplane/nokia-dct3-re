-- PIN and phone code are physical; radio messages remain firmware-owned.
_G.nsb7_security_with_pin=true
_G.nsb7_security_continue=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_page_observe.lua')
local finish=coroutine.create(function()
    assert(emu.wait(45-manager.machine.time:as_double()))
    manager.machine.screens[':screen']:snapshot('nsb7_pin_registered.png')
    manager.machine:logerror('nsb7_pin_registration: complete\n')
    manager.machine:exit()
end)
_G.nsb7_pin_registration=finish
assert(coroutine.resume(finish))
