# SuperNET_API

launch chrome with  --allow-nacl-socket-api=localhost command line arg

git clone https://github.com/jl777/SuperNET_API
cd SuperNET_API
tools/httpd.py -C . -p 7777 &

now you can go to http://localhost:7777 

To issue API calls use SNapi:
gcc -o SNapi SNapi.c plugins/utils/cJSON.c -lnanomsg

./SNapi "{\"plugin\":\"InstantDEX\",\"method\":\"allexchanges\"}"


You can change any of the html or JS files and just refresh to test it. After the first clone, just a git pull and refresh the page.

