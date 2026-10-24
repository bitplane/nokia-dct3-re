-- Own NSB-1 login, transport observation and Navi contact (not NSM-1's cell).
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nsb1_toolkit_input.lua')
local menu
for _,field in pairs(manager.machine.ioport.ports[':COL.1'].fields) do
    if field.mask==4 then menu=field end
end
dofile(directory..'sim_toolkit_interactive_input.lua')('nsb1',menu)
