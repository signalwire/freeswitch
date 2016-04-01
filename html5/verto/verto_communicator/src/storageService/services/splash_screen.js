'use strict';

  angular
    .module('storageService')
    .service('splashscreen', ['$rootScope', '$q', 'storage', 'config', 'verto', '$translate',
      function($rootScope, $q, storage, config, verto, $translate) {

        var checkBrowser = function() {
          return $q(function(resolve, reject) {
            var activity = 'browser-upgrade';
            var result = {
              'activity': activity,
              'soft': false,
              'status': 'success',
              'message': $translate.instant('BROWSER_COMPATIBILITY')
            };
            navigator.getUserMedia = navigator.getUserMedia ||
              navigator.webkitGetUserMedia ||
              navigator.mozGetUserMedia;

            if (!navigator.getUserMedia) {
              result['status'] = 'error';
              result['message'] = $translate.instant('BROWSER_WITHOUT_WEBRTC');
              reject(result);
            }

            resolve(result);

          });
        };

        var checkMediaPerm = function() {
          return $q(function(resolve, reject) {
            var activity = 'media-perm';
            var result = {
              'activity': activity,
              'soft': false,
              'status': 'success',
              'message': $translate.instant('CHECK_PERMISSION_MEDIA')
            };

            verto.mediaPerm(function(status) {
              if(!status) {
                result['status'] = 'error';
                result['message'] = $translate.instant('ERROR_PERMISSION_MEDIA');
                verto.data.mediaPerm = false;
                reject(result);
              }
              verto.data.mediaPerm = true;
              resolve(result);
            });
          });
        };

        var refreshMediaDevices = function() {
          return $q(function(resolve, reject) {
            var activity = 'refresh-devices';
            var result = {
              'status': 'success',
              'soft': true,
              'activity': activity,
              'message': $translate.instant('REFRESH_MEDIA_DEVICES')
            };

            verto.refreshDevices(function(status) {
              verto.refreshDevicesCallback(function() {
                resolve(result);
              });
            });

          });
        };

        var checkConnectionSpeed = function() {
          return $q(function(resolve, reject) {
            var activity = 'check-connection-speed';
            var result = {
              'status': 'success',
              'soft': true,
              'activity': activity,
              'message': $translate.instant('CHECK_CONNECTION_SPEED')
            };

            if (storage.data.autoBand && verto.data.instance) {
              verto.testSpeed(cb);
            } else {
              resolve(result);
            }

            function cb(data) {
              resolve(result);
            }
          });
        };

        var provisionConfig = function() {
          return $q(function(resolve, reject) {
            var activity = 'provision-config';
            var result = {
              'status': 'promise',
              'soft': true,
              'activity': activity,
              'message': $translate.instant('CHECK_PROVISIONING_CONF')
            };

            var configResponse = config.configure();

            var configPromise = configResponse.then(
              function(response) {
                /**
                 * from angular docs:
                 * A response status code between 200 and 299 is considered a success status and will result in the success callback being called
                 */
                if(response.status >= 200 && response.status <= 299) {
                  return result;
                } else {
                  result['status'] = 'error';
                  result['message'] = $translate.instant('ERROR_PROVISIONING_CONF');
                  return result;
                }
              });

              result['promise'] = configPromise;

              resolve(result);
          });
        };

        var checkLogin = function() {
          return $q(function(resolve, reject) {
            var activity = 'check-login';
            var result = {
              'status': 'success',
              'soft': true,
              'activity': activity,
              'message': $translate.instant('CHECK_LOGIN'),
            };

            if(verto.data.connecting || verto.data.connected) {
              resolve(result);
              return;
            };

            var checkUserStored = function() {
              /**
               * if user data saved, use stored data for logon and not connecting
               * not connecting prevent two connects
               */
              if (storage.data.ui_connected && storage.data.ws_connected && !verto.data.connecting) {
                verto.data.name = storage.data.name;
                verto.data.email = storage.data.email;
                verto.data.login = storage.data.login;
                verto.data.password = storage.data.password;

                verto.data.connecting = true;
                verto.connect(function(v, connected) {
                  verto.data.connecting = false;
                  resolve(result);
                });
              };
            };

            if(storage.data.ui_connected && storage.data.ws_connected) {
              checkUserStored();
            } else {
              resolve(result);
            };
          });
        };

        var progress = [
          checkBrowser,
          checkMediaPerm,
          refreshMediaDevices,
          provisionConfig,
          checkLogin,
          checkConnectionSpeed
        ];

        var progress_message = [
          $translate.instant('BROWSER_COMPATIBILITY'),
          $translate.instant('CHECK_PERMISSION_MEDIA'),
          $translate.instant('REFRESH_MEDIA_DEVICES'),
          $translate.instant('CHECK_PROVISIONING_CONF'),
          $translate.instant('CHECK_LOGIN'),
          $translate.instant('CHECK_CONNECTION_SPEED'),
        ];

        var getProgressMessage = function(current_progress) {
          if(progress_message[current_progress] != undefined) {
            return progress_message[current_progress];
          } else {
            return $translate.instant('PLEASE_WAIT');
          }
        };

        var current_progress = -1;
        var progress_percentage = 0;

        var calculateProgress = function(index) {
          var _progress;

          _progress = index + 1;
          progress_percentage = (_progress / progress.length) * 100;
          return progress_percentage;
        };

        var nextProgress = function() {
          var fn, fn_return, status, interrupt, activity, soft, message, promise;
          interrupt = false;
          current_progress++;

          if(current_progress >= progress.length) {
            $rootScope.$emit('progress.complete', current_progress);
            return;
          }

          fn = progress[current_progress];
          fn_return = fn();

          var emitNextProgress = function(fn_return) {
            if(fn_return['promise'] != undefined) {
              promise = fn_return['promise'];
            }

            status = fn_return['status'];
            soft = fn_return['soft'];
            activity = fn_return['activity'];
            message = fn_return['message'];

            if(status != 'success') {
              interrupt = true;
            }

            $rootScope.$emit('progress.next', current_progress, status, promise, activity, soft, interrupt, message);

          };

          fn_return.then(
            function(fn_return) {
              emitNextProgress(fn_return);
            },
            function(fn_return) {
              emitNextProgress(fn_return);
            }
          );

        };

        return {
          'next': nextProgress,
          'getProgressMessage': getProgressMessage,
          'progress_percentage': progress_percentage,
          'calculate': calculateProgress
        };

      }]);
