-- Reuse the physical NSM-3 security input and shared native replay fixture.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
_G.noki8210_security_only=true
dofile(directory..'noki8210_security_input.lua')
local run=dofile(directory..'native_ui_state_fixture.lua')
run({prefix='nsm3r6',ram_end=0x17fffc,checkpoint=24,verify_endpoint=true,io_stream=true,
    contact=function(machine) return machine.ioport.ports[':COL.1'].fields['Menu'] end})
