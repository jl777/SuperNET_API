var SPNAPI = (function(SPNAPI, $, undefined) {

    SPNAPI.settings = {};

    SPNAPI.getCheckBoxDetails = function(agent) {

        var extraInfo = '';

        switch (agent) {

            case 'InstantDEX':
                extraInfo = 'Extra Info on this '+agent+' Agent';
                break;
            case 'pangea':
                extraInfo = 'Extra Info on this '+agent+' Agent';
                break;
            case 'Jumblr':
                extraInfo = 'Extra Info on this '+agent+' Agent';
                break;
            case 'MGW':
                extraInfo = 'Extra Info on this '+agent+' Agent';
                break;
            case 'Atomic':
                extraInfo = 'Extra Info on this '+agent+' Agent';
                break;
            case 'PAX':
                extraInfo = 'Extra Info on this '+agent+' Agent';
                break;
            case 'Tradebots':
                extraInfo = 'Extra Info on this '+agent+' Agent';
                break;
              case 'Wallet':
              extraInfo = 'Extra Info on this '+agent+' Agent';
              break;
              case 'Jay':
              extraInfo = 'Extra Info on this '+agent+' Agent';
              break;

        }

        return extraInfo;


    }

    chrome.storage.sync.get("settings", function(items) {
        if (!chrome.runtime.error) {
            //console.log(items);
            //document.getElementById("data").innerText = items.data;
            SPNAPI.settings = items;
        }
    });

    SPNAPI.pageContent.Settings = function () {

        var filehandle_map = {};
        var dirhandle_map = {};

        var rows = '<h3>Agents</h3>';

        $.each(SPNAPI.methods, function (index, value) {

            var this_state;

            $.each(SPNAPI.settings, function (settings_index, settings_value) {

                $.each(settings_value, function (this_setting_index, this_setting_value) {

                    if(this_setting_value.agent == index) {
                        this_state = this_setting_value.state;
                    }

                })

            });

            var checkbox_text = "";
            var checkbox_checked = "";
            var extraDetails = SPNAPI.getCheckBoxDetails(index);
            if(this_state == 'inactive') { checkbox_text = '<i>Disabled</i>'; checkbox_checked = ''; extraDetails = "";  }
            else {
                checkbox_text = 'Enabled'; checkbox_checked = 'checked="checked"';

            }


            rows += '' +
            '<div class="panel panel-default">'+
            '<div class="panel-body">'+
            '<div class="col-xs-6 col-md-6 col-lg-6">'+index+'</div>'+
            '<div class="col-xs-6 col-md-6 col-lg-6" style="text-align: right;">' +
            '<div class="checkbox">'+
            '<label>'+
            '<input type="checkbox" id="'+index+'_checkbox" '+checkbox_checked+' class="agent_checkbox" value="'+index+'" aria-label="Activate/Deactivate Agent"> <span class="checkbox_'+index+'_text">'+checkbox_text+'</span>'+
            '</label>'+
            '</div>' +
            '</div>' +
            '<div class="row"><div class="'+index+'_extra_info col-xs-10 col-md-10 col-lg-10">'+extraDetails+'</div></div>'+
            '</div>'+
            '</div>';

        });


        var filename = "/persistent/SuperNET.conf";
        var access = "w+";
        postCall('fopen', filename, access, function(filename_return, filehandle) {
            filehandle_map[filehandle] = filename_return;
            common.logMessage('File ' + filename_return + ' opened successfully.');

            console.log(filehandle + " and "+filename_return);
        });
        /*

         var data = "SuperNETconfigurationsdaaaaa TES TEST TEST TEST";
         postCall('fwrite', 0, data, function(filehandle, bytesWritten) {
         var filename = filehandle_map[filehandle];
         common.logMessage('Wrote ' + bytesWritten + ' bytes to file ' + filename +
         '.');
         });


         var filesize = "";
         postCall('stat', filename, function(filename, size) {
         common.logMessage('File ' + filename + ' has size ' + size + '.');
         filesize = size;

         });


         var filehandle = parseInt(filehandle_map, 10);
         var numBytes = parseInt(filesize, 10);
         postCall('fread', 0, 0, function(filehandle, data) {
         var filename = filehandle_map[filehandle];
         common.logMessage('Read "' + data + '" from file ' + filename + '.');
         });
         */

        $("#agent_settings").html(rows);

        var config = '<h3>Config</h3>';


        $("#config_settings").html(config);


        var agent_checkbox = $('.agent_checkbox');

        agent_checkbox.on("click", function () {

            var checkbox_agent = $(this).val();

            var thisCheck = $(this);
            if (thisCheck.is (':checked'))
            {
                $('.checkbox_'+checkbox_agent+'_text').html("Enabled");
                var extraDetails = SPNAPI.getCheckBoxDetails(checkbox_agent);

                $("."+checkbox_agent+"_extra_info").html(extraDetails);

            } else {
                $("."+checkbox_agent+"_extra_info").html('');
                $('.checkbox_'+checkbox_agent+'_text').html("<i>Disabled</i>");
            }


        });

        $("#save_settings").on("click", function () {

            var agent_checkbox = $('.agent_checkbox');
            var settings = [];

            $.each(agent_checkbox, function(index, value) {

                var agent = value.value;
                console.log(agent);
                var thisCheck = $("#"+agent+"_checkbox");
                var state;
                if (thisCheck.is (':checked'))
                {
                    state = "active";

                } else {
                    state = 'inactive';
                }

                var json = { "agent" : "InstantDEX", "state" : "inactive" };
                json.agent = agent;
                json.state = state;
                settings.push(json);


            });

            //console.log(settings);

            var filename = "/persistent/SuperNET.conf";
            var access = "w";
            postCall('fopen', filename, access, function(filename_return, filehandle) {
                filehandle_map[filehandle] = filename_return;
                common.logMessage('File ' + filename_return + ' opened successfully.');

            });


            //var settings = { "agent" : "InstantDEX", "state" : "active" };
            chrome.storage.sync.set({ "settings" : settings }, function() {
                if (chrome.runtime.error) {
                    console.log("Runtime error.");
                }
            });

            chrome.storage.sync.get("settings", function(items) {
                if (!chrome.runtime.error) {
                    //console.log(items);
                    //document.getElementById("data").innerText = items.data;
                    SPNAPI.settings = items;
                }
            });

        });

    };

    return SPNAPI;
}(SPNAPI || {}, jQuery));
