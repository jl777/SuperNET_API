# SuperNET_API

make sure you have google-chrome, chromium is having different instructions
http://askubuntu.com/questions/510056/how-to-install-google-chrome

launch chrome with  --allow-nacl-socket-api=localhost command line arg

git clone https://github.com/jl777/SuperNET_API;
cd SuperNET_API;
tools/httpd.py -C . -p 7777 &

now you can go to http://localhost:7777 

To issue API calls use SNapi:
gcc -o SNapi SNapi.c plugins/utils/cJSON.c -lnanomsg -lm

./SNapi "{\"plugin\":\"InstantDEX\",\"method\":\"allexchanges\"}"


You can change any of the html or JS files and just refresh to test it. After the first clone, just a git pull and refresh the page.

To recompile the pexe files from scratch, you need to setup a nacl_sdk toolchain (https://developer.chrome.com/native-client/sdk/download) and copy the toolchain directory into the SuperNET_API directory. From there make will recompile, make serve will recompile and launch httpd.py localhost server.

There are five ways of accessing the SuperNET_API:
	a) via JS function bindings that will send JSON to SuperNET core and get a JSON return back
	b) via SNapi to send in a JSON request to SuperNET core and it will print to stdout the JSON return
    c) from SuperNET console
    d) using ./BitcoinDarkd SuperNET '{<json request>}'
	e) via C code that is linked into the SuperNET_API itself

On unix systems, you can use ./m_unix to build a standalone SuperNET, SNapi and BitcoinDarkd. It shares the same codebase, but on most unix (or mac osx) systems it will be 64bit code vs the 32bit bytecodes and of course an entirely different toolchain, so there could be subtle differences in behavior other than the ~2x performance the native code will get

The nanomsg comms between the native and pnacl should be compatible as long as control messages are not used and the transports are limited to inproc, ipc and tcp

James
