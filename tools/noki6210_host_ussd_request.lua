-- Answer opens the numeric reply editor directly on NPE-3.
_G.npe3_host_ussd_capture='6210_host_ussd_request.png'
_G.npe3_host_ussd_actions={{26,'Left Softkey / Menu'},
    {30,'Keypad 2'},{32,'Left Softkey / Menu'},{36,'End'},{38,'Left Softkey / Menu'}}
_G.npe3_host_ussd_finish=41
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'noki6210_host_ussd_notify.lua')
