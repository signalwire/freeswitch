'use strict';

var storageService = angular.module('storageService', ['ngStorage']);

storageService.service('storage', ['$rootScope', '$localStorage',
  function($rootScope, $localStorage) {
    var data = $localStorage;

    data.$default({
      ui_connected: false,
      ws_connected: false,
      cur_call: 0,
      called_number: '',
      useVideo: true,
      call_history: [],
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
      useVideo: '',
      useCamera: '',
      useVideo: '',
      useCamera: '',
      useStereo: '',
      useSTUN: '',
      useDedenc: '',
      mirrorInput: '',
      outgoingBandwidth: '',
      incomingBandwidth: '',
      vidQual: '',
      askRecoverCall: true
    });

    function changeData(verto_data) {
      data.selectedVideo = verto_data.data.selectedVideo;
      data.selectedAudio = verto_data.data.selectedAudio;
      data.selectedShare = verto_data.data.selectedShare;
      data.useVideo = verto_data.data.useVideo;
      data.useCamera = verto_data.data.useCamera;
      data.useStereo = verto_data.data.useStereo;
      data.useDedenc = verto_data.data.useDedenc;
      data.useSTUN = verto_data.data.useSTUN;
      data.vidQual = verto_data.data.vidQual;
      data.mirrorInput = verto_data.data.mirrorInput;
      data.outgoingBandwidth = verto_data.data.outgoingBandwidth;
      data.incomingBandwidth = verto_data.data.incomingBandwidth;
    }

    return {
      data: data,
      changeData: changeData,
      reset: function() {
        data.ui_connected = false;
        data.ws_connected = false;
        data.cur_call = 0;
        data.userStatus = 'disconnected';
      },
    };
  }
]);
