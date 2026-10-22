-- Observe the exhausted card after cold boot; do not retry any credential.
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_staged_observe.lua')
local runner=coroutine.create(function()
    assert(emu.wait(14))
    manager.machine.screens[':screen']:snapshot('6210_puk_permanent.png')
    manager.machine:logerror('6210_puk_permanent: complete=1 credential_input=0 firmware_state_forcing=0\n')
end)
_G.npe3_puk_permanent_runner=runner
assert(coroutine.resume(runner))
