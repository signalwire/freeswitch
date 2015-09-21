'use strict';

  angular
  .module('storageService')
  .service('storage', ['$rootScope', '$localStorage',
  function($rootScope, $localStorage) {
    var data = $localStorage,
	defaultSettings = {
	  ui_connected: false,
          ws_connected: false,
          cur_call: 0,
          called_number: '',
          useVideo: true,
          call_history: {},
          history_control: [],
          call_start: false,
          name: '',
          email: '',
          login: '',
          password: '',
          userStatus: 'disconnected',
          mutedVideo: false,
          mutedMic: false,
          selectedVideo: null,
          selectedAudio: null,
          selectedShare: null,
          useStereo: true,
          useSTUN: true,
          useDedenc: false,
          mirrorInput: false,
          outgoingBandwidth: 'default',
          incomingBandwidth: 'default',
          vidQual: undefined,
          askRecoverCall: false,
          googNoiseSuppression: true,
          googHighpassFilter: true,
          googEchoCancellation: true
       };

    data.$default(defaultSettings);

    function changeData(verto_data) {
      jQuery.extend(true, data, verto_data);
    }

    return {
      data: data,
      changeData: changeData,
      reset: function() {
        data.$reset(defaultSettings);
      },
    };
  }
]);
