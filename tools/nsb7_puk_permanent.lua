-- Cold permanent-card rejection: observe without retrying a credential.
_G.nsb7_observe_only=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(assert(source:match('^(.*[/\\])'))..'nsb7_sim_observe.lua')
local machine=manager.machine
local capture=coroutine.create(function()
    assert(emu.wait(12))
    machine.screens[':screen']:snapshot('nsb7_puk_permanent.png')
    machine:logerror('nsb7_puk_permanent: complete t=12.000000000\n')
    machine:exit()
end)
_G.nsb7_puk_permanent_capture=capture
assert(coroutine.resume(capture))
