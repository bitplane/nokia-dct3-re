-- Own NSM-1 login, transport observation and confirmation contact.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nsm1_toolkit_input.lua')
local menu
for _,field in pairs(manager.machine.ioport.ports[':COL.1'].fields) do
    if field.mask==2 then menu=field end
end
dofile(directory..'sim_toolkit_interactive_input.lua')('nsm1',menu)
