var SPNAPI = (function(SPNAPI, $, undefined) {

    SPNAPI.methods = {};
    SPNAPI.pages = ["Settings", "Debug","Wallet", "Tradebots","PAX","MGW","Atomic", "Jumblr", "Pangea", "InstantDEX"];
    SPNAPI.pageContent = {};
    SPNAPI.page = "welcome";


    $(document).ready(function() {

        //load Pages into the navbar
        $.each(SPNAPI.pages, function( index, value ) {
            $("#welcome").after('<li class="navigation" data-page="'+value+'"><a href="#">'+value+'</a></li>');
        });


        $(".navigation").on("click", function () {

            var page = $(this).data("page");
            $(".navigation").removeClass("active");
            $(".hljs").html("JSON response");
            SPNAPI.loadSite(page);

        });

        $(".page").hide();
        $("#welcome_page").show();

        $(".submit_api_request").on("click", function () {

            SPNAPI.submitRequest();

        });

        $(".clear-response").on("click", function () {

            $(".hljs").html("JSON response");

        });

    });

    SPNAPI.submitRequest = function(e) {

        var request = $(".json_submit_url").html();

        postCall('SuperNET', request, function(jsonstr)
        {
            $(".debuglogdebuglog").append(jsonstr);
            common.logMessage(jsonstr + '\n');

            $(".hljs").html(jsonstr);

        });
    };


    return SPNAPI;
}(SPNAPI || {}, jQuery));
