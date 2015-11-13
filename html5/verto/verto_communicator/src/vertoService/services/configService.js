'use strict';

var vertoService = angular.module('vertoService');

vertoService.service('config', ['$rootScope', '$http', '$location', 'storage', 'verto',
  function($rootScope, $http, $location, storage, verto) {
    var configure = function() {
      /**
       * Load stored user info into verto service
       */
      if(storage.data.name) {
        verto.data.name = storage.data.name;
      }
      if(storage.data.email) {
        verto.data.email = storage.data.email;
      }
      if(storage.data.login) {
        verto.data.login = storage.data.login;
      }
      if(storage.data.password) {
        verto.data.password = storage.data.password;
      }

      /*
       * Load the Configs before logging in
       * with cache buster
       */
      var url = window.location.origin + window.location.pathname;
      var httpRequest = $http.get(url + 'config.json?cachebuster=' + Math.floor((Math.random()*1000000)+1));

      var httpReturn = httpRequest.then(function(response) {
        var data = response.data;

        /* save these for later as we're about to possibly over write them */
        var name = verto.data.name;
        var email = verto.data.email;

        console.debug("googlelogin: " + data.googlelogin);
        if (data.googlelogin){
          verto.data.googlelogin = data.googlelogin;
          verto.data.googleclientid = data.googleclientid;
        }

        angular.extend(verto.data, data);

        /**
         * use stored data (localStorage) for login, allow config.json to take precedence
         */

        if (name != '' && data.name == '') {
          verto.data.name = name;
        }
        if (email != '' && data.email == '') {
          verto.data.email = email;
        }
        if (verto.data.login == '' && verto.data.password == '' && storage.data.login != '' && storage.data.password != '') {
          verto.data.login = storage.data.login;
          verto.data.password = storage.data.password;
        }

        if (verto.data.autologin == "true" && !verto.data.autologin_done) {
          console.debug("auto login per config.json");
          verto.data.autologin_done = true;
        }

        if(verto.data.autologin && storage.data.name.length && storage.data.email.length && storage.data.login.length && storage.data.password.length) {
          $rootScope.$emit('config.http.success', data);
        };
        return response;
      }, function(response) {
        $rootScope.$emit('config.http.error', response);
        return response;
      });

      return httpReturn;
    };

    return {
      'configure': configure
    };
  }]);
