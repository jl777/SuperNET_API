var SPNAPI = (function(SPNAPI, $, undefined) {

    SPNAPI.methods.InstantDEX = [
        {"id":1,"method":"allorderbooks","base":"","rel":"","exchange":"","price":"","volume":""},
        {"id":2,"method":"allexchanges","base":"","rel":"","exchange":"","price":"","volume":""},
        {"id":2,"method":"openorders","base":"","rel":"","exchange":"","price":"","volume":""},
        {"id":3,"method":"orderbook","base":"base","rel":"rel","exchange":"active","price":"","volume":""},
        {"id":4,"method":"placeask","base":"base","rel":"rel","exchange":"active","price":"price","volume":"volume"},
        {"id":5,"method":"placebid","base":"base","rel":"rel","exchange":"active","price":"price","volume":"volume"},
        {"id":6,"method":"orderstatus","base":"","rel":"","exchange":"","price":"","volume":"","orderid":"orderid"},
        {"id":7,"method":"cancelorder","base":"","rel":"","exchange":"","price":"","volume":"","orderid":"orderid"},
        {"id":8,"method":"enablequotes","base":"base","rel":"rel","exchange":"exchange","price":"","volume":""},
        {"id":9,"method":"disablequotes","base":"base","rel":"rel","exchange":"exchange","price":"","volume":""},
        {"id":10,"method":"lottostats","base":"","rel":"","exchange":"","price":"","volume":""},
        {"id":11,"method":"tradehistory","base":"","rel":"","exchange":"","price":"","volume":""},
        {"id":12,"method":"balance","base":"","rel":"","exchange":"exchange","price":"","volume":""},
        {"id":13,"method":"peggyrates","base":"base","rel":"","exchange":"","price":"","volume":""}
    ];

    SPNAPI.methods.Pangea = [
        {"id":1,"method":"start","base":"base","maxplayers":"maxplayers","bigblind":"bigblind","ante":"ante","hostrake":"hostrake"},
        {"id":2,"method":"status","tableid":"tableid"},
        {"id":3,"method":"turn","tableid":"tableid"},
        {"id":4,"method":"mode"},
        {"id":5,"method":"buyin","tableid":"tableid"},
        {"id":6,"method":"history","tableid":"tableid","handid":"handid"},
        {"id":7,"method":"rates","base":"base"},
        {"id":8,"method":"lobby"},
        {"id":9,"method":"tournaments"},
        {"id":10,"method":"rosetta","base":"base"}
    ];

    SPNAPI.methods.Jumblr = [
        {"id":1,"method":"jumblr","base":"base","maxplayers":"maxplayers","bigblind":"bigblind","ante":"ante"},
        {"id":2,"method":"status","tableid":"tableid"}
    ];

    SPNAPI.methods.MGW =[
        {"id":1,"method":"MGW","base":"base","maxplayers":"maxplayers","bigblind":"bigblind","ante":"ante"},
        {"id":2,"method":"status","tableid":"tableid"}
    ];

    SPNAPI.methods.Atomic = [
        {"id":1,"method":"atomic","base":"base","maxplayers":"maxplayers","bigblind":"bigblind","ante":"ante"},
        {"id":2,"method":"status","tableid":"tableid"}
    ];

SPNAPI.methods.PAX = [
{"id":1,"method":"peggy","base":"base","maxplayers":"maxplayers","bigblind":"bigblind","ante":"ante"},
{"id":2,"method":"status","tableid":"tableid"}
];

SPNAPI.methods.Tradebots = [
{"id":1,"method":"tradebots","base":"base","maxplayers":"maxplayers","bigblind":"bigblind","ante":"ante"},
{"id":2,"method":"status","tableid":"tableid"}
];
              
    SPNAPI.methods.Wallet = [
        {"id":1,"method":"wallet","base":"base","maxplayers":"maxplayers","bigblind":"bigblind","ante":"ante"},
        {"id":2,"method":"status","tableid":"tableid"}
    ];


    SPNAPI.loadApiBox = function (agent, methods) {

        methods = methods[0];
        $(".api-panel-title").html(methods.method);

        var json = { "agent" : agent, "method" : methods.method };

        var rows = '';
        rows += '<input type="hidden" name="agent" value="'+agent+'">';
        rows += '<input type="hidden" name="method" value="'+methods.method+'">';
        rows += '<table class="table">' +
        '<thead>' +
        '<tr><th>Agent</th><th>'+agent+'</th></tr>' +
        '<tr><td>Method</th><td>'+methods.method+'</td></tr>';



        $.each(methods, function (index, value) {

            if(index !== "id") {
                if( index !== "method") {
                    rows += '<tr><td>' + index + '</td><td><input type="text" class="api_control" class="form-control" name="' + index + '" value="' + value + '" style="width:100%;min-width:200px;"></td></tr>';
                    json[index] = value;
                }
            }

        });


        rows += '</table><hr>';

        json = JSON.stringify(json);
        $(".json_submit_url").html(json);

        $(".api_formfill").html(rows);


    };






    return SPNAPI;
}(SPNAPI || {}, jQuery));


/*
var api_request = function(agent)
{
    var jsonstr = '';//$$("apirequest").getValues().jsonstr;
    var base = $$("formA").getValues().base;
    var rel = $$("formB").getValues().rel;
    var exchange = $$("formC").getValues().exchange;
    var price = $$("formD").getValues().price;
    var volume = $$("formE").getValues().volume;
    var orderid = $$("formF").getValues().orderid;
    var method = $$("method").getValues().method;
    var request = '{"agent":"' + agent + '","method":"' + method + '","base":"' + base + '","rel":"' + rel + '","exchange":"' + exchange + '","price":"' + price + '","volume":"' + volume + '","orderid":"' + orderid + '"' + jsonstr + '}';
    return(request);
}
*/
