'use strict';

var storageService = angular.module('storageService', ['ngStorage']);

storageService.service('storage', ['$rootScope', '$localStorage', 'verto',
  function($rootScope, $localStorage, verto) {
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
      verto: angular.toJson(verto)
    });

    return {
      data: data,
      reset: function() {
        data.ui_connected = false;
        data.ws_connected = false;
        data.cur_call = 0;
        data.userStatus = 'disconnected';
      },
    };
  }
]);
