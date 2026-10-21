-- Capture only host artifacts; the logical EEPROM shadow is never modified.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])')..'nsb7_service_observe.lua')
local machine=manager.machine
local memory=machine.devices[':maincpu'].spaces.program
local capture=coroutine.create(function()
    assert(emu.wait(6.1))
    local bytes={}
    for offset=0,0x7fff do bytes[#bytes+1]=string.char(memory:read_u8(0x10d5c8+offset)) end
    local output=assert(io.open('nsb7_nv_shadow.bin','wb'))
    output:write(table.concat(bytes))
    output:close()
    machine:logerror(string.format('nsb7_storage: base=0010d5c8 bytes=32768 t=%.9f observer_writes=0\n',machine.time:as_double()))
    machine:exit()
end)
_G.nsb7_storage_observer=capture
assert(coroutine.resume(capture))
